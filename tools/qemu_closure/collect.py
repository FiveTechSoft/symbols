#!/usr/bin/env python3
"""One diagnostic hosted KVM boot. Never emits a complete closure or pin."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import stat
from pathlib import Path
import re
import signal
import subprocess
import tarfile
import time

from closure import BINARY, LOADER, LIB, records, Refusal
from elf import dependencies

KERNEL='boot/vmlinuz-6.8.0-142-generic'
BUSYBOX='usr/bin/busybox'
INITRD_SHA='21d1a476d2e561c4d6f506b198b401772d3600a6d16ef2398e023612cf3ae38d'
MICROVM_BIOS_SOURCE='usr/share/seabios/bios-microvm.bin'
MICROVM_BIOS_DATA='usr/share/qemu/bios-microvm.bin'
MICROVM_BIOS_SHA='3dc79b28380ae79a014060ac23052f32293bc80cdbb39e347811965807cfee1f'
MICROVM_BIOS_SIZE=131072
MARKER=b'SYMBOLS_BOOT_ONLY_SUPERVISOR_READY_v1\r\n'
MAX_TRACE=16_000_000
MAX_OUTPUT=65536
MAX_PREFLIGHT=8192
SONAME=re.compile(rb'[A-Za-z0-9_+.-]{1,80}\.so(?:\.[0-9]{1,8}){0,4}')
# This diagnostic treats every non-staged successful file operation as a blocker,
# not as evidence for promoting a static closure.
PID_PREFIX=r'(?:[0-9]{1,12} {1,2}|\[pid +[0-9]{1,12}\] )?'
LINE=re.compile(r'^'+PID_PREFIX+r'([a-z][a-z0-9_]*)\((.*)\) += +(.+)$')
QUOTED=re.compile(r'"(/[^"\\]*)"')
FD=re.compile(r'^[0-9]+(?:<[^>]*>)?$')
ERROR=re.compile(r'^-1 [A-Z][A-Z0-9_]+ ')


def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def stage_microvm_bios(src, stage):
    """Map one verified snapshot BIOS into QEMU's fixed -L data directory."""
    row=src.get(MICROVM_BIOS_SOURCE)
    if (not isinstance(row,dict) or row.get('sha256')!=MICROVM_BIOS_SHA or
        row.get('size')!=MICROVM_BIOS_SIZE or MICROVM_BIOS_DATA in src):
        raise Refusal('bios_inventory')
    source=stage/MICROVM_BIOS_SOURCE
    target=stage/MICROVM_BIOS_DATA
    if (not source.is_file() or source.is_symlink() or source.stat().st_size!=MICROVM_BIOS_SIZE or
        digest(source)!=MICROVM_BIOS_SHA or target.exists() or target.is_symlink()):
        raise Refusal('bios_source_or_destination')
    target.parent.mkdir(parents=True,exist_ok=True)
    # The source was read from a digest-checked archive member in the stage loop.
    with target.open('xb') as out:
        out.write(source.read_bytes())
    target.chmod(0o644)
    if target.stat().st_size!=MICROVM_BIOS_SIZE or digest(target)!=MICROVM_BIOS_SHA:
        raise Refusal('bios_stage_digest')


def diagnostic(lines, root):
    opened=set(); probes=set(); unsupported=[]; outside=set()
    for index,line in enumerate(lines,1):
        if re.fullmatch(PID_PREFIX+r'\+\+\+ exited with [0-9]+ \+\+\+',line):
            continue
        m=LINE.fullmatch(line)
        if not m:
            unsupported.append(index);continue
        call,args,status=m.groups()
        paths=QUOTED.findall(args)
        if not paths or '\\' in args or '<unfinished ...>' in args or '<... ' in args:
            unsupported.append(index);continue
        if call in ('open','openat','openat2','execve') and (FD.fullmatch(status) or (call=='execve' and status=='0')):
            opened.update(paths[:1] if call in ('open','execve') else paths[-1:])
        elif ERROR.match(status):probes.update(paths)
        elif call not in ('open','openat','openat2','execve') and re.fullmatch(r'[0-9]+(?:<[^>]*>)?',status):
            # Metadata reads do not prove an open, but remain visible.
            probes.update(paths)
        else:unsupported.append(index)
    if len(opened)+len(probes)>5000:unsupported.append('path_cap')
    for path in opened | probes:
        if not path.startswith(str(root)+'/'):outside.add(path)
    return {'opened':sorted(opened),'probes':sorted(probes),'outside':sorted(outside),
            'unsupported_line_numbers':unsupported[:100],'unsupported_count':len(unsupported)}


SAFE_WORDS=frozenset(('qemu strace kvm kernel initrd rom firmware memory machine cpu device accelerator accel '
    'failed failure error invalid unsupported unable cannot could not open load initialize access permission denied '
    'no such file or directory operation allowed available requested specified argument option bus pci microvm '
    'network block serial address space size out of for with to from on at in the a an is was '
    'configuration config module library plugin rom bios image binary executable format elf '
    'migration state qboot firmware requested').split())


def redacted_stderr(data):
    """Diagnostic words from a fixed lexicon only; paths, numbers and unknown words are hidden."""
    if len(data)>256 or not data.isascii():return {'status':'suppressed'}
    text=data.decode('ascii','strict')
    # Reject control characters except conventional line endings; never emit text verbatim.
    if any(ord(c)<32 and c not in '\r\n\t' for c in text):return {'status':'suppressed'}
    tokens=re.findall(r'[^\s:]+',text)
    out=[]
    for token in tokens[:40]:
        if '/' in token or '\\' in token or any(c.isdigit() for c in token):
            out.append('[value]');continue
        word=token.strip('.,;:!?()[]').lower()
        out.append(word if word in SAFE_WORDS else '[unknown]')
    return {'status':'redacted','words':out,'truncated':len(tokens)>40}


TRACE_CALLS=frozenset(('execve open openat openat2 access faccessat faccessat2 newfstatat '
    'stat lstat statx statfs statfs64 readlink readlinkat getcwd chdir mkdir unlink rename').split())
PROBE_PATHS=frozenset(('/dev/sgx_vepc','/etc/ld.so.preload','/etc/libnl/classid',
    '/etc/selinux/config','/proc/self/exe','/selinux','/sys/fs/selinux'))
AMBIENT_PATHS=frozenset(('/dev/kvm','/dev/urandom','/etc/gnutls/config',
    '/proc/filesystems','/proc/self/status','/sys/bus/nd/devices',
    '/sys/devices/system/cpu/possible','/sys/devices/system/node'))
TRACE_KNOWN_PATHS=PROBE_PATHS | AMBIENT_PATHS
TRACE_ERRNOS=frozenset(('ENOENT','EACCES','EPERM','ENODEV','ENOTDIR','EINVAL','EIO'))


def trace_status(status):
    if FD.fullmatch(status):return ('integer_or_fd','none')
    if status=='0':return ('zero','none')
    error=re.match(r'^-1 ([A-Z][A-Z0-9_]+) ',status)
    if error:return ('negative_errno',error.group(1) if error.group(1) in TRACE_ERRNOS else 'other')
    return ('other','none')


def missed_path_shape(call, args):
    # The first quoted operand is the pathname for these exact syscall forms;
    # escaping prevents accepting a broken quoted token as a path.
    supported=frozenset(('open','openat','openat2','access','faccessat','faccessat2',
        'newfstatat','stat','lstat','statx','statfs','statfs64',
        'readlink','readlinkat','execve'))
    if call not in supported:return ('not_applicable','not_parsed','unknown',False)
    dirfd='not_applicable'
    if call in ('openat','openat2','faccessat','faccessat2','newfstatat','statx','readlinkat'):
        first=args.split(',',1)[0].strip()
        if first=='AT_FDCWD':dirfd='AT_FDCWD'
        elif re.fullmatch(r'[0-9]+(?:<[^>]*>)?',first):dirfd='descriptor'
        else:dirfd='other'
    if dirfd!='not_applicable' and ',' not in args:return (dirfd,'not_parsed','unknown',False)
    operand=args.split(',',1)[1] if dirfd!='not_applicable' else args
    token=re.match(r'^\s*"((?:[^"\\]|\\.)*)"',operand)
    if not token:return (dirfd,'not_parsed','unknown',False)
    value=token.group(1)
    escaped='\\' in value
    path_class=('empty' if not value else 'absolute' if value.startswith('/') else
                'relative' if not value.startswith('\\') else 'other')
    length=('0' if not value else '1-16' if len(value)<=16 else '17-64' if len(value)<=64
            else '65-256' if len(value)<=256 else 'over_256')
    return (dirfd,path_class,length,escaped)


def structured_trace_details(lines, indices):
    """Bounded parser misses and allowlisted host path attempts only; never raw text."""
    unsupported=[];host_attempts=[];host_attempt_count=0;unsupported_set=set(indices)
    for n,line in enumerate(lines,1):
        m=LINE.fullmatch(line)
        stripped=re.sub(r'^'+PID_PREFIX,'',line)
        call_match=re.match(r'^([a-z][a-z0-9_]{0,31})\(',stripped)
        call=call_match.group(1) if call_match and call_match.group(1) in TRACE_CALLS else 'other'
        reason='grammar';ret='absent';errno='none';paths=[]
        dirfd='not_applicable';path_class='not_parsed';path_length='unknown';has_escape=False;event='none'
        if m:
            raw_call,args,status=m.groups()
            paths=QUOTED.findall(args)
            ret,errno=trace_status(status)
            dirfd,path_class,path_length,has_escape=missed_path_shape(raw_call,args)
            if '<unfinished ...>' in args or '<... ' in args:reason='unfinished'
            elif '\\' in args:reason='escape'
            elif not paths:reason='no_absolute_quoted_path'
            elif raw_call in ('open','openat','openat2','execve'):reason='return_or_dirfd'
            else:reason='metadata_return'
            # Report only exact allowlisted path strings; never extract or echo other args.
            for path in sorted(set(paths)&TRACE_KNOWN_PATHS):
                host_attempt_count+=1
                if len(host_attempts)<100:
                    host_attempts.append({'line':n,'path':path,'call':raw_call if raw_call in TRACE_CALLS else 'other',
                                          'return_class':ret,'errno_class':errno,
                                          'parser_supported':n not in unsupported_set})
        elif stripped.startswith('+++ exited with '):reason='exit_other';event='exit'
        elif stripped.startswith('+++ killed by '):reason='killed';event='killed'
        elif stripped.startswith('--- '):
            reason='signal';event='signal_delimited' if stripped.endswith(' ---') else 'signal_unterminated'
        elif stripped.startswith('<... '):reason='resumed'
        elif '<unfinished ...>' in stripped:reason='unfinished'
        elif re.match(r'^[a-z][a-z0-9_]{0,31}\(',stripped):reason='grammar_call'
        if n in unsupported_set and len(unsupported)<100:
            unsupported.append({'line':n,'call':call,'rejection':reason,'return_class':ret,
                                'errno_class':errno,'known_probe_paths':sorted(set(paths)&PROBE_PATHS),
                                'dirfd_class':dirfd,'path_class':path_class,'path_length':path_length,
                                'has_escape':has_escape,'event_shape':event})
    return {'unsupported':unsupported,'host_attempts':host_attempts,
            'host_attempts_truncated':host_attempt_count>100}


# Measurement only. No change to diagnostic() acceptance or runtime_complete.
# Never hash arbitrary path operands in the public artifact: short names can be
# recovered from an unkeyed digest. Fingerprints below cover closed vocabulary.
MEASURE_FLAGS=frozenset(('O_RDONLY','O_WRONLY','O_RDWR','O_PATH','O_CLOEXEC',
                         'O_DIRECTORY','O_NONBLOCK','O_NOFOLLOW','O_LARGEFILE'))
MEASURE_ACCESS=frozenset(('F_OK','R_OK','W_OK','X_OK'))
MEASURE_SIGNALS=frozenset(('SIGHUP','SIGINT','SIGQUIT','SIGILL','SIGTRAP','SIGABRT',
    'SIGBUS','SIGFPE','SIGKILL','SIGUSR1','SIGSEGV','SIGUSR2','SIGPIPE',
    'SIGALRM','SIGTERM','SIGCHLD','SIGCONT','SIGSTOP','SIGTSTP','SIGTTIN',
    'SIGTTOU','SIGURG','SIGXCPU','SIGXFSZ','SIGVTALRM','SIGPROF',
    'SIGWINCH','SIGIO','SIGPWR','SIGSYS'))
MEASURE_SIGNAL_KEYS=frozenset(('si_signo','si_code','si_pid','si_uid','si_status',
                               'si_addr','si_value','si_int','si_ptr'))


def policy_shape_measurement(lines, unsupported_indices, unsupported_count=None):
    """Closed classes only; never an acceptance decision or an arbitrary name."""
    entries=[]
    for n in unsupported_indices[:100]:
        if not isinstance(n,int) or n<1 or n>len(lines):continue
        line=lines[n-1];m=LINE.fullmatch(line)
        if m:
            call,args,status=m.groups()
            if call=='openat':
                # Full first operand and comma boundary; no ./secret or escape.
                dot=re.fullmatch(r'AT_FDCWD, "\.", ([A-Z0-9_|]+)',args)
                if dot:
                    bits=dot.group(1).split('|')
                    valid=(len(bits)==len(set(bits)) and all(x in MEASURE_FLAGS for x in bits) and
                           trace_status(status)[0]=='integer_or_fd')
                    entries.append({'line':n,'kind':'literal_dot','grammar':valid,
                                    'flag_classes':sorted(bits) if valid else [],
                                    'flag_fingerprint':hashlib.sha256('|'.join(sorted(bits)).encode()).hexdigest() if valid else None,
                                    'return_class':trace_status(status)[0]})
                    continue
            if call=='access':
                token=re.fullmatch(r'"([^"\\]{1,256})", ([A-Z_|]+)',args)
                if token:
                    name=token.group(1);parts=name.split('/')
                    bits=token.group(2).split('|')
                    valid=(len(bits)==len(set(bits)) and all(x in MEASURE_ACCESS for x in bits))
                    entries.append({'line':n,'kind':'relative_access',
                                    'path_class':'relative' if not name.startswith('/') else 'absolute',
                                    'length_bucket':'1-16' if len(name)<=16 else '17-64' if len(name)<=64 else '65-256',
                                    'components_bucket':'1' if len(parts)==1 else '2-4' if len(parts)<=4 else '5-8' if len(parts)<=8 else '9+',
                                    'dot':'.' in parts,'dotdot':'..' in parts,
                                    'empty_component':'' in parts,
                                    'flag_classes':sorted(bits) if valid else [],
                                    'grammar':valid and not name.startswith('/') and not any(x in ('','..') for x in parts) and
                                              trace_status(status)==('negative_errno','ENOENT'),
                                    'return_class':trace_status(status)[0],
                                    'errno_class':trace_status(status)[1]})
                    continue
        stripped=re.sub(r'^'+PID_PREFIX,'',line)
        signal_match=re.fullmatch(r'--- (SIG[A-Z0-9]+) (\{.{0,512}\}) ---',stripped)
        if signal_match:
            sig,payload=signal_match.groups()
            keys=re.findall(r'(?:^|[, {])([a-z_]+)=',payload)
            recognized=(sig in MEASURE_SIGNALS and bool(keys) and
                        len(keys)==len(set(keys)) and all(x in MEASURE_SIGNAL_KEYS for x in keys)
                        and '\\' not in payload and '"' not in payload)
            entries.append({'line':n,'kind':'signal','type':sig if recognized else 'other',
                            'payload_keys':sorted(keys) if recognized else [],
                            'grammar':recognized})
            continue
        entries.append({'line':n,'kind':'other','grammar':False})
    return {'entries':entries,'all_unsupported_represented':len(entries)==len(unsupported_indices) and (
                unsupported_count is None or unsupported_count==len(unsupported_indices)),
            'acceptance':False,'open_time_identity_verified':False,
            'limitation':'shape_measurement_only_no_arbitrary_path_or_signal_payload'}


# Shape screen only: this does not certify file identity, contents, isolation,
# relative probe names or a complete static closure. Counts are fixed to #17.
RUNNER_HOST_SHAPE={
    '/dev/kvm':(('openat','fd'),),
    '/dev/urandom':(('openat','fd'),),
    '/etc/gnutls/config':(('newfstatat','nonnegative'),('openat','fd')),
    '/proc/filesystems':(('openat','fd'),),
    '/proc/self/status':(('openat','fd'),('openat','fd')),
    '/sys/bus/nd/devices':(('newfstatat','nonnegative'),('newfstatat','nonnegative'),('openat','fd')),
    '/sys/devices/system/cpu/possible':(('openat','fd'),),
    '/sys/devices/system/node':(('openat','fd'),),
    '/dev/sgx_vepc':(('openat','ENOENT'),),
    '/etc/ld.so.preload':(('access','ENOENT'),),
    '/etc/libnl/classid':(('newfstatat','ENOENT'),('openat','ENOENT')),
    '/etc/selinux/config':(('access','ENOENT'),),
    '/proc/self/exe':(('readlink','nonnegative'),),
    '/selinux':(('statfs','ENOENT'),),
    '/sys/fs/selinux':(('statfs','ENOENT'),),
}
HOST_DIRFD_CALLS=frozenset(('openat','newfstatat'))
HOST_FIRST_PATH_CALLS=frozenset(('access','statfs','readlink'))
HOST_TOKEN=re.compile(r'^"(/[^"\\]{1,4096})"(?=,|$)')
HOST_NEGATIVE=re.compile(r'^-1 ENOENT \(No such file or directory\)$')


def ambient_shape_match(lines, stage, unsupported_indices=(), observed_outside=None):
    """Per-call/result screen. Unknown host path or malformed line is drift."""
    seen={name:[] for name in RUNNER_HOST_SHAPE}
    unknown=[]
    unsupported=set(unsupported_indices)
    for n,line in enumerate(lines,1):
        if n in unsupported:continue  # The separate trace-shape screen owns these lines.
        m=LINE.fullmatch(line)
        if not m:continue  # Other-line coverage is a separate blocker.
        call,args,status=m.groups()
        if call in HOST_DIRFD_CALLS:
            first,sep,rest=args.partition(', ')
            if not sep or first!='AT_FDCWD':
                # A descriptor path operation cannot be credited to this gate.
                unknown.append(n);continue
            operand=rest
        elif call in HOST_FIRST_PATH_CALLS:operand=args
        else:
            if call not in ('execve','open','openat2'):
                unknown.append(n)
            continue  # Exec/staged opens are separately checked; unknown call blocks.
        token=HOST_TOKEN.match(operand)
        if not token:
            # Unknown pathname operands cannot be considered accepted coverage.
            unknown.append(n);continue
        name=token.group(1)
        if name.startswith(str(stage)+'/'):continue
        if name=='.' and call=='openat':continue  # Owned by the dot screen.
        if name not in RUNNER_HOST_SHAPE:
            unknown.append(n);continue
        outcome=('ENOENT' if HOST_NEGATIVE.fullmatch(status) else
                 'fd' if call=='openat' and FD.fullmatch(status) else
                 'nonnegative' if call in ('newfstatat','readlink','statfs','access') and
                                  re.fullmatch(r'[0-9]+',status) else 'other')
        seen[name].append((call,outcome))
    drift=sorted(name for name,expected in RUNNER_HOST_SHAPE.items()
                 if sorted(seen[name])!=sorted(expected))
    outside_set_match=(observed_outside is None or
                       set(observed_outside)==set(RUNNER_HOST_SHAPE))
    return {'match':not drift and not unknown and outside_set_match,'drift_paths':drift,
            'unaccounted_count':len(unknown),
            'unaccounted_lines':[x for x in unknown[:100] if isinstance(x,int)],
            'outside_set_match':outside_set_match,
            'identity_and_semantics_verified':False,
            'limitation':'path_call_result_shape_only_not_dependency_acceptance'}


# Only an entire, bounded siginfo record can match. Never publish values.
SIGNAL_SHAPE=re.compile(
    r'^--- (SIG[A-Z0-9]+) \{si_signo=(SIG[A-Z0-9]+), si_code=([A-Z_]+), '
    r'si_pid=([0-9]{1,10}), si_uid=([0-9]{1,10})\} ---$')
SIGNAL_CODES=frozenset(('SI_USER','SI_TKILL','SI_QUEUE'))
DOT_FLAGS_FINGERPRINT='d3be21c00eac6af3696207e9b04fbccf41de1f31a5421be681ae6b4db3f43659'
ACCESS_PATTERN=re.compile(r'^"([^"\\/]{1,64})", R_OK$')
DOT_PATTERN=re.compile(r'^AT_FDCWD, "\.", O_RDONLY$')


def trace_shape_match(lines, observed, stage, serial_marker_once, process_exit_status):
    """Separate positive shape screen. diagnostic() remains unchanged/blocked."""
    indices=observed['unsupported_line_numbers']
    cwd_changed=any(re.match(r'^'+PID_PREFIX+r'(?:chdir|fchdir)\(',line) for line in lines)
    stage_dir=False
    if not cwd_changed:
        fd=os.open(stage,os.O_PATH|os.O_DIRECTORY|os.O_CLOEXEC)
        try:stage_dir=stat.S_ISDIR(os.fstat(fd).st_mode)
        finally:os.close(fd)
    counts={'dot':0,'relative_access_ENOENT':0,'SIGUSR1':0}
    bad=[]
    for n in indices[:100]:
        if not isinstance(n,int) or n<1 or n>len(lines):bad.append(n);continue
        line=lines[n-1];m=LINE.fullmatch(line)
        if m:
            call,args,status=m.groups()
            if call=='openat' and DOT_PATTERN.fullmatch(args) and FD.fullmatch(status) and stage_dir:
                counts['dot']+=1;continue
            if (call=='access' and ACCESS_PATTERN.fullmatch(args) and
                    HOST_NEGATIVE.fullmatch(status)):
                # Name stays private and is never hashed into a public report.
                counts['relative_access_ENOENT']+=1;continue
        else:
            stripped=re.sub(r'^'+PID_PREFIX,'',line)
            sig=SIGNAL_SHAPE.fullmatch(stripped)
            if (sig and sig.group(1)==sig.group(2)=='SIGUSR1' and
                    sig.group(3) in SIGNAL_CODES and
                    0<int(sig.group(4))<=4294967295 and
                    0<=int(sig.group(5))<=4294967295):
                counts['SIGUSR1']+=1;continue
        bad.append(n)
    # strace -qq suppresses normal exit lines. The observed wait status is
    # authoritative for the traced process, but any contradictory exit or
    # killed record still fails the shape screen.
    exit_lines=[re.sub(r'^'+PID_PREFIX,'',x) for x in lines
                if re.match(r'^'+PID_PREFIX+r'(?:\+\+\+ exited with |\+\+\+ killed by )',x)]
    clean_exit=(process_exit_status==0 and len(exit_lines)<=1 and
                all(x=='+++ exited with 0 +++' for x in exit_lines))
    expected={'dot':1,'relative_access_ENOENT':3,'SIGUSR1':10}
    complete=(len(indices)==observed['unsupported_count'] and
              len(indices)==14 and all(isinstance(n,int) for n in indices))
    return {'match':complete and counts==expected and not bad and
            not cwd_changed and stage_dir and clean_exit and serial_marker_once,
            'counts':counts,'mismatch_lines':bad[:100],
            'all_unsupported_represented':complete,'cwd_changed':cwd_changed,
            'stage_cwd_directory':stage_dir,'clean_exit':clean_exit,
            'dot_flag_fingerprint':DOT_FLAGS_FINGERPRINT if counts['dot']==1 else None,
            'directory_events':counts['dot'],
            'blockers':['relative_names_unreviewed','host_identity_and_semantics_unproven'],
            'limitation':'form_only_not_dependency_acceptance'}


def relative_path_components(name):
    parts=name.split('/')
    flags={'leading_dot_slash':name.startswith('./'),
           'dot_component':'.' in parts,
           'dotdot_component':'..' in parts,
           'empty_component':'' in parts,
           'absolute':name.startswith('/')}
    # Diagnostic-only normalization: remove dot segments, never resolve
    # traversal, empty segments, absolute paths or an all-dot pathname.
    remaining=[part for part in parts if part!='.']
    allowed=(not flags['absolute'] and len(name)<=256 and
             not flags['dotdot_component'] and not flags['empty_component'] and
             bool(remaining))
    normalized='/'.join(remaining) if allowed else None
    return flags,normalized


def literal_dot_diagnostic(lines, indices, stage):
    """Recognize only the exact dot operand; no arbitrary names or trace args."""
    entries=[]
    cwd_changed=any(re.match(r'^'+PID_PREFIX+r'(?:chdir|fchdir)\(',line) for line in lines)
    stage_dir=None
    if not cwd_changed:
        fd=os.open(stage,os.O_PATH|os.O_DIRECTORY|os.O_CLOEXEC)
        try:stage_dir=stat.S_ISDIR(os.fstat(fd).st_mode)
        finally:os.close(fd)
    for n in indices[:100]:
        if not isinstance(n,int) or n<1 or n>len(lines):continue
        m=LINE.fullmatch(lines[n-1]);entry={'line':n,'call':'other','literal_dot':False,
                                               'result':'unparsed','stage_cwd_directory':stage_dir,
                                               'cwd_change_seen':cwd_changed}
        if not m:
            entries.append(entry);continue
        call,args,status=m.groups()
        if call not in ('openat','access'):
            entries.append(entry);continue
        entry['call']=call
        if call=='openat':
            first,sep,operand=args.partition(',')
            if first.strip()!='AT_FDCWD' or not sep:
                entries.append(entry);continue
        else:operand=args
        # Exact first pathname token only. A quoted longer path or an escape
        # remains unclassified and is never output.
        match=re.match(r'^\s*"([^"\\]*)"(?=\s*(?:,|$))',operand)
        if match and match.group(1)=='.':entry['literal_dot']=True
        if entry['literal_dot'] and not cwd_changed:
            entry['result']=('successful_fd' if call=='openat' and FD.fullmatch(status) else
                             'negative_ENOENT' if ERROR.match(status) and status.startswith('-1 ENOENT ') else
                             'other')
        entries.append(entry)
    return {'entries':entries,'limitation':'diagnostic_only_unsupported_trace'}


def relative_open_diagnostic(lines, indices, stage, source):
    """Post-run diagnostic only; never accredits identity at QEMU's open time."""
    import ctypes  # Linux x86_64; do not import into offline tests on Windows.
    class OpenHow(ctypes.Structure):
        _fields_=[('flags',ctypes.c_uint64),('mode',ctypes.c_uint64),('resolve',ctypes.c_uint64)]
    result=[]
    # chdir/fchdir in this captured trace invalidates the launch cwd assumption.
    cwd_changed=any(re.match(r'^'+PID_PREFIX+r'(?:chdir|fchdir)\(',line) for line in lines)
    root_fd=os.open(stage,os.O_PATH|os.O_DIRECTORY|os.O_CLOEXEC)
    libc=ctypes.CDLL(None,use_errno=True)
    try:
        for n in indices[:100]:
            if not isinstance(n,int) or n<1 or n>len(lines):continue
            m=LINE.fullmatch(lines[n-1]);entry={'line':n,'state':'not_candidate'}
            if not m or m.group(1)!='openat' or not FD.fullmatch(m.group(3)):
                result.append(entry);continue
            args=m.group(2)
            dirfd,path_class,_,escaped=missed_path_shape('openat',args)
            if dirfd!='AT_FDCWD' or escaped:
                entry['state']='wrong_shape';result.append(entry);continue
            if cwd_changed:
                entry['state']='cwd_changed';result.append(entry);continue
            operand=args.split(',',1)[1]
            match=re.match(r'^\s*"([^"\\]*)"',operand)
            if not match:
                entry['state']='path_unparsed';result.append(entry);continue
            name=match.group(1)
            flags,normalized=relative_path_components(name)
            entry['path_flags']=flags
            if normalized is None:
                entry['state']='path_unsafe';result.append(entry);continue
            name=normalized
            # O_PATH prevents file content reads and side effects on special files;
            # BENEATH forbids escape; symlinks and magiclinks are forbidden.
            how=OpenHow(os.O_PATH|os.O_CLOEXEC,0,0x08|0x04|0x02)
            fd=libc.syscall(437,root_fd,ctypes.c_char_p(os.fsencode(name)),ctypes.byref(how),ctypes.sizeof(how))
            if fd<0:
                entry['state']='unresolved';result.append(entry);continue
            try:
                meta=os.fstat(fd)
                if stat.S_ISREG(meta.st_mode):kind='regular'
                elif stat.S_ISDIR(meta.st_mode):kind='directory'
                else:kind='other'
                entry.update(state='stage_candidate',kind=kind)
                if name in source:
                    entry['inventory_path']=name  # public authenticated inventory only
                    row=source[name] if isinstance(source[name],dict) else {}
                    if kind=='regular' and row.get('sha256') and row.get('size')==meta.st_size:
                        entry['inventory_metadata_match']=True
                    else:entry['inventory_metadata_match']=False
                if kind=='regular' and meta.st_size<=40_000_000:
                    # Reopen through the same no-symlink resolution rules; compare
                    # inode and device to the O_PATH fd before reading bounded bytes.
                    read_how=OpenHow(os.O_RDONLY|os.O_NONBLOCK|os.O_CLOEXEC,0,0x08|0x04|0x02)
                    read_fd=libc.syscall(437,root_fd,ctypes.c_char_p(os.fsencode(name)),ctypes.byref(read_how),ctypes.sizeof(read_how))
                    if read_fd<0:entry['state']='read_unresolved'
                    else:
                        try:
                            rmeta=os.fstat(read_fd)
                            if (rmeta.st_dev,rmeta.st_ino,rmeta.st_size)!=(meta.st_dev,meta.st_ino,meta.st_size):
                                entry['state']='changed_after_trace'
                            else:
                                h=hashlib.sha256();total=0
                                while True:
                                    block=os.read(read_fd,1024*1024)
                                    if not block:break
                                    total+=len(block)
                                    if total>40_000_000:break
                                    h.update(block)
                                if total==meta.st_size and total<=40_000_000:
                                    entry['sha256']=h.hexdigest()
                                    if name in source and isinstance(source[name],dict) and source[name].get('sha256')!=entry['sha256']:
                                        entry['state']='inventory_drift'
                                else:entry['state']='read_incomplete'
                        finally:os.close(read_fd)
                result.append(entry)
            finally:os.close(fd)
    finally:os.close(root_fd)
    return {'launch_cwd': 'stage', 'cwd_change_seen':cwd_changed,'candidates':result,
            'limitation':'post_run_resolution_not_open_time_identity'}


def trace_shapes(lines, unsupported_line_numbers):
    """Summarize parser misses using only bounded, fixed-vocabulary shapes."""
    shapes={};samples=[];unsupported=set(unsupported_line_numbers)
    for index,line in enumerate(lines,1):
        prefix='none'
        if re.match(r'^[0-9]{1,12} ',line):prefix='decimal_pid'
        elif re.match(r'^\[pid +[0-9]{1,12}\] ',line):prefix='bracket_pid'
        elif re.match(r'^[0-9]',line):prefix='other_numeric'
        stripped=re.sub(r'^'+PID_PREFIX,'',line)
        syscall=re.match(r'^([a-z][a-z0-9_]{0,31})\(',stripped)
        kind='syscall' if syscall else ('exit' if stripped.startswith('+++ ') else 'other')
        name=syscall.group(1) if syscall and syscall.group(1) in (
            'execve','open','openat','openat2','access','faccessat','faccessat2',
            'newfstatat','stat','lstat','statx','readlink','readlinkat',
            'getcwd','chdir','mkdir','unlink','rename') else 'other'
        key=(prefix,kind,name)
        shapes[key]=shapes.get(key,0)+1
        # Only first few parser misses; never a raw line, path, quoted value or arbitrary name.
        if len(samples)<8 and index in unsupported:
            # Only separator class/length and known syscall after it, never arguments.
            gap=re.match(r'^(?:[0-9]{1,12}|\[pid +[0-9]{1,12}\])(\s{1,16})',line)
            after=line[gap.end():] if gap else ''
            after_call=re.match(r'^([a-z][a-z0-9_]{0,31})\(',after)
            after_name=after_call.group(1) if after_call and after_call.group(1) in (
                'execve','open','openat','openat2','access','faccessat','faccessat2',
                'newfstatat','stat','lstat','statx','readlink','readlinkat') else 'other'
            samples.append({'line':index,'prefix':prefix,'kind':kind,'call':name,
                'gap_length':len(gap.group(1)) if gap else 0,
                'gap_class':('space' if gap and gap.group(1).strip(' ')=='' else
                             'tab_or_mixed' if gap else 'other'),
                'after_gap_call':after_name,
                'starts_parenthesis':bool(syscall),
                'has_return_delimiter':' = ' in line,'contains_quoted_value':'"' in line})
    return {'counts':[{'prefix':a,'kind':b,'call':c,'count':n}
            for (a,b,c),n in sorted(shapes.items())][:60],'samples':samples}


def bounded_failure(data):
    """Publish categories and hashes, not arbitrary stderr or guest text."""
    clipped=data[:MAX_OUTPUT]
    lower=clipped.lower()
    labels=[]
    for label,needle in (
        ('loader_missing_library',b'error while loading shared libraries:'),
        ('loader_missing_file',b'cannot open shared object file'),
        ('loader_version_mismatch',b'version `'),
        ('loader_bad_elf',b'wrong elf class'),
        ('exec_permission',b'permission denied'),
        ('exec_missing',b'no such file or directory'),
        ('strace_exec_failure',b'strace: exec:'),
        ('qemu_kvm_failure',b'failed to initialize kvm'),
        ('qemu_kernel_open',b'could not open kernel image'),
        ('qemu_kernel_load',b'could not load kernel'),
        ('qemu_initrd_open',b'could not open initrd'),
        ('qemu_initrd_load',b'could not load initrd'),
        ('qemu_firmware_open',b'failed to load rom'),
        ('qemu_machine_unsupported',b'unsupported machine type'),
        ('qemu_kvm_device',b'could not access kvm kernel module'),
        ('qemu_accel_unsupported',b'accelerator kvm not found'),
        ('qemu_argument_invalid',b'invalid option'),
        ('qemu_file_open',b'could not open'),
        # Fixed basename is present in the signed snapshot inventory. This
        # identifies a referenced candidate, not an observed open or cause.
        ('bios_qboot_named',b'qboot.rom'),
        ('strace_prefix',b'strace:'),
        ('qemu_prefix',b'qemu-system-x86_64:'),
        ('error_failed',b'failed'),
        ('error_error',b'error'),
        ('error_denied',b'denied'),
        ('error_invalid',b'invalid'),
        ('error_unsupported',b'unsupported'),
        ('error_not_found',b'not found'),
        ('error_mmap',b'mmap'),
        ('error_memory',b'memory'),
        ('error_kvm',b'kvm'),
        ('error_microvm',b'microvm'),
        ('error_kernel',b'kernel'),
        ('error_initrd',b'initrd'),
        ('error_rom',b'rom'),
        ('error_virtio',b'virtio'),
        ('error_accel',b'accel'),
        ('error_device',b'device'),
        ('error_machine',b'machine'),
        ('error_cpu',b'cpu'),
    ):
        if needle in lower:labels.append(label)
    # Only a loader-formatted missing-library line may supply a soname.
    sonames=sorted({m.decode('ascii') for m in re.findall(
        rb'error while loading shared libraries: ('+SONAME.pattern+rb'):',clipped)})[:12]
    return {'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),
            'truncated':len(data)>MAX_OUTPUT,'categories':labels or ['unclassified'],
            'sonames':sonames,'redacted':redacted_stderr(data)}


def exec_probe(lines, loader, binary):
    """Classify the exact launch exec transition without printing trace lines."""
    counts={'loader_success':0,'loader_failure':0,'qemu_success':0,'qemu_failure':0}
    for line in lines:
        m=re.fullmatch(PID_PREFIX+r'execve\("([^"\\]+)".*\) += +(0|-1 [A-Z][A-Z0-9_]+ .*|\?)',line)
        if m and m.group(1) in (str(loader),str(binary)):
            key=('loader' if m.group(1)==str(loader) else 'qemu')+('_success' if m.group(2)=='0' else '_failure')
            counts[key]+=1
    return counts


def loader_preflight(loader, library_path, binary, *, env, cwd):
    """Ask the pinned staged loader to list dependencies; never run QEMU main."""
    command=[str(loader),'--inhibit-cache','--library-path',library_path,'--list',str(binary)]
    def limit_output():
        import resource  # Linux-only bounded preflight output.
        resource.setrlimit(resource.RLIMIT_FSIZE,(MAX_PREFLIGHT,MAX_PREFLIGHT))
    out=Path(cwd)/'loader-list.stdout';err=Path(cwd)/'loader-list.stderr'
    with out.open('wb') as stdout,err.open('wb') as stderr:
        try:
            p=subprocess.run(command,cwd=cwd,env=env,stdout=stdout,stderr=stderr,
                             timeout=5,preexec_fn=limit_output)
        except subprocess.TimeoutExpired:
            return {'status':'timeout'}
    stdout=out.read_bytes();stderr=err.read_bytes()
    missing=sorted({m.decode('ascii') for m in re.findall(rb'(?m)^\s*('+SONAME.pattern+rb')\s+=>\s+not found\s*$',stdout)})[:12]
    return {'status':'ok' if p.returncode==0 and len(stdout)<MAX_PREFLIGHT and len(stderr)<MAX_PREFLIGHT and not missing else 'blocked',
            'exit_status':p.returncode,'stdout_size':len(stdout),'stderr':bounded_failure(stderr),
            'output_limit':len(stdout)>=MAX_PREFLIGHT or len(stderr)>=MAX_PREFLIGHT,
            'missing_sonames':missing}


def needed_aliases(files, source, read):
    """Stage signed SONAME aliases, not merely their resolved ELF bytes."""
    aliases=set()
    for path in files:
        _,needed=dependencies(read(path))
        for name in needed:
            options=[prefix+name for prefix in (LIB,'lib/x86_64-linux-gnu/') if prefix+name in source]
            if len(options)!=1:raise Refusal('dependency_resolution')
            alias=options[0]
            if alias not in files:
                if 'link' not in source[alias]:raise Refusal('dependency_alias')
                aliases.add(alias)
    return aliases



def static_alias_shape_match(static_files, source, stage, staged_opened):
    """Post-run, snapshot-bound ELF/SONAME name reconciliation only.

    This is not evidence that an open used this inode, loaded the ELF, or
    retained its bytes between observation and measurement.
    """
    failures=[];pairs=[];direct=0
    if not isinstance(static_files,dict) or not isinstance(source,dict) or not isinstance(staged_opened,dict):
        return {'match':False,'reason':'input_schema','direct':0,'alias_pairs':[]}
    for target, record in sorted(static_files.items()):
        row=source.get(target)
        if (not isinstance(record,dict) or not isinstance(row,dict) or
            record.get('sha256')!=row.get('sha256') or record.get('size')!=row.get('size')):
            failures.append('static_inventory');continue
        if target in staged_opened:
            if staged_opened[target]!=record['sha256']:failures.append('direct_digest')
            else:direct+=1
            continue
        # Require a SONAME specifically referenced by the signed static ELF
        # graph. Merely finding a like-named symlink in the inventory is not
        # enough. Reject ambiguous aliases and mismatched package provenance.
        candidates=[]
        for parent, parent_record in static_files.items():
            if not isinstance(parent_record,dict) or not isinstance(parent_record.get('needed'),list):
                failures.append('needed_schema');continue
            for name in parent_record['needed']:
                if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_+.-]{1,80}\.so(?:\.[A-Za-z0-9]{1,8}){0,4}',name):
                    failures.append('needed_name');continue
                for prefix in (LIB,'lib/x86_64-linux-gnu/'):
                    alias=prefix+name;link=source.get(alias)
                    if (isinstance(link,dict) and link.get('link')==target.rsplit('/',1)[-1] and
                        alias.rsplit('/',1)[0]==target.rsplit('/',1)[0]):
                        candidates.append(alias)
        candidates=sorted(set(candidates))
        if len(candidates)!=1:
            failures.append('alias_ambiguous_or_missing');continue
        alias=candidates[0];link=source[alias]
        if (set(link)!={'link','origin_package','payload_sha256'} or
            link['origin_package']!=row.get('origin_package') or
            link['payload_sha256']!=row.get('payload_sha256')):
            failures.append('alias_provenance');continue
        path=stage/alias;resolved=stage/target
        if (not path.is_symlink() or os.readlink(path)!=link['link'] or
            not resolved.is_file() or resolved.is_symlink() or
            resolved.stat().st_size!=record['size'] or
            digest(resolved)!=record['sha256'] or
            staged_opened.get(alias)!=record['sha256']):
            failures.append('alias_staged_byte_drift');continue
        pairs.append({'target':target,'observed_alias':alias,'sha256':record['sha256']})
    return {'match':not failures and direct+len(pairs)==len(static_files),
            'reason':'matched' if not failures and direct+len(pairs)==len(static_files) else
                     sorted(set(failures))[0] if failures else 'unaccounted',
            'direct':direct,'alias_pairs':pairs,
            'limitation':'post-run pathname and byte reconciliation only; not open-time FD identity, ELF loading, later loads, or host isolation'}


def checked_kvm_identity(*, uid, euid, egid, groups, device_gid, device_mode, kvm_gid):
    active=sorted(set(groups) | {egid})
    if (uid==0 or euid==0 or kvm_gid!=device_gid or kvm_gid not in active or
        not (device_mode & stat.S_IRGRP and device_mode & stat.S_IWGRP)):
        raise Refusal('kvm_identity')
    return {'uid':uid,'euid':euid,'egid':egid,'active_gids':active,
            'device_gid':device_gid,'device_mode':oct(device_mode),'kvm_gid':kvm_gid}

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--tar',type=Path,required=True)
    p.add_argument('--inventory',type=Path,required=True)
    p.add_argument('--stage',type=Path,required=True)
    p.add_argument('--strace',type=Path,required=True)
    p.add_argument('--strace-sha256',required=True)
    p.add_argument('--runner-image',required=True)
    args=p.parse_args()
    stage=args.stage.resolve();stage.mkdir(mode=0o700,parents=True,exist_ok=False)
    proc=None
    result={'schema':'symbols.qemu-runtime-collection-diagnostic.v1','runtime_complete':False,
            'classification':'blocked','reason':'incomplete','mode':'github-ubuntu-24.04-kvm'}
    def stop_child(*_):
        if proc is not None and proc.poll() is None:
            os.killpg(proc.pid,signal.SIGKILL)
            proc.wait(timeout=5)
        raise Refusal('interrupted')
    signal.signal(signal.SIGTERM,stop_child)
    signal.signal(signal.SIGINT,stop_child)
    def save():
        (stage/'report.json').write_text(json.dumps(result,sort_keys=True,separators=(',',':'))+'\n')
        print(json.dumps(result,sort_keys=True),flush=True)
    try:
        inv=json.loads(args.inventory.read_bytes());src=inv['source']
        if args.tar.stat().st_size!=420270080 or digest(args.tar)!='5f5e095dbe95d167cf6a141e9ea2bd3a22f8c82c67107f54a82cd96bde8608c9':raise Refusal('archive_drift')
        if inv['archive_sha256']!=digest(args.tar) or inv['archive_size']!=args.tar.stat().st_size:raise Refusal('inventory_drift')
        if src[KERNEL]['sha256']!='cd5fcfd260b91782637b7b4e221a48e656358f6eef549602540e62380b6f2f2c':raise Refusal('kernel_drift')
        if args.strace.is_symlink() or not args.strace.is_file():raise Refusal('strace_missing')
        result['strace_sha256']=digest(args.strace)
        if not re.fullmatch(r'[0-9a-f]{64}',args.strace_sha256) or result['strace_sha256']!=args.strace_sha256:raise Refusal('strace_drift')
        if not args.runner_image or os.environ.get('ImageVersion')!=args.runner_image:raise Refusal('runner_image_drift')
        # Stage only the static closure plus candidate QEMU module/data tree.
        # Unseen ambient paths are evidence of missing closure, never added here.
        packages={r['Package']:r for r in json.loads(Path(__file__).parents[1].joinpath('builder_rootfs/noble-20260927-source.json').read_bytes())['packages']}
        with tarfile.open(args.tar,'r:') as archive:
            members={m.name:m for m in archive}
            if len(members)!=len(archive.getmembers()) or set(members)!={r['path'] for r in inv['included']}:raise Refusal('archive_members')
            def read(path):
                m=members[path]
                if not m.isfile():raise Refusal('not_regular')
                return archive.extractfile(m).read(40_000_001)
            static=records(inv,read,packages)
            selected=set(static['files']) | {BINARY,LOADER,KERNEL,BUSYBOX}
            selected.update(needed_aliases(static['files'],src,read))
            selected.add(MICROVM_BIOS_SOURCE)
            selected.update(k for k in src if k.startswith(('usr/share/qemu/','usr/lib/x86_64-linux-gnu/qemu/')) and isinstance(src[k],dict) and ('sha256' in src[k] or 'link' in src[k]))
            if len(selected)>250:raise Refusal('selected_count')
            # Include relative symlink chain of selected files; all paths and
            # material bytes must match the signed-snapshot inventory.
            pending=list(selected)
            while pending:
                name=pending.pop()
                if len(selected)>300:raise Refusal('selected_count')
                if name not in src or name not in members:raise Refusal('selected_missing')
                row=src[name];m=members[name]
                if not re.fullmatch(r'[A-Za-z0-9._+,/-]+',name) or name.startswith('/') or '..' in name.split('/'):raise Refusal('selected_path')
                target=stage/name;target.parent.mkdir(parents=True,exist_ok=True)
                if 'link' in row:
                    link=row['link']
                    if not re.fullmatch(r'[A-Za-z0-9._+-]+',link) or link in ('.','..') or not m.issym() or m.linkname!=link:raise Refusal('selected_link')
                    destination=name.rsplit('/',1)[0]+'/'+link
                    if destination not in selected:selected.add(destination);pending.append(destination)
                    if not target.is_symlink():target.symlink_to(link)
                else:
                    if not m.isfile() or not isinstance(row.get('size'),int) or row['size']>40_000_000:raise Refusal('selected_file')
                    raw=read(name)
                    if len(raw)!=row['size'] or hashlib.sha256(raw).hexdigest()!=row['sha256']:raise Refusal('selected_digest')
                    target.write_bytes(raw)
                    target.chmod(0o755 if name in (BINARY,LOADER,BUSYBOX) else 0o644)
        stage_microvm_bios(src,stage)
        busybox=stage/BUSYBOX
        subprocess.run(['python3',str(Path(__file__).parents[1]/'minimal_initramfs/pack.py'),'--busybox',str(busybox),'--out',str(stage/'initramfs.cpio')],check=True,timeout=15)
        if digest(stage/'initramfs.cpio')!=INITRD_SHA:raise Refusal('initrd_drift')
        import grp  # Linux-only; pure tests remain importable on Windows.
        device=os.stat('/dev/kvm')
        result['kvm_identity']=checked_kvm_identity(
            uid=os.getuid(),euid=os.geteuid(),egid=os.getegid(),groups=os.getgroups(),
            device_gid=device.st_gid,device_mode=stat.S_IMODE(device.st_mode),
            kvm_gid=grp.getgrnam('kvm').gr_gid)
        if not os.access('/dev/kvm',os.R_OK|os.W_OK):raise Refusal('kvm_permission')
        fd=os.open('/dev/kvm',os.O_RDWR|os.O_CLOEXEC);os.close(fd)
        qemu=stage/BINARY;kernel=stage/KERNEL;initrd=stage/'initramfs.cpio'
        loader=stage/LOADER
        argv=[str(loader),'--inhibit-cache','--library-path',str(stage/'usr/lib/x86_64-linux-gnu')+':'+str(stage/'lib/x86_64-linux-gnu'),str(qemu),'-M','microvm','-accel','kvm','-cpu','host','-m','128M','-smp','1','-kernel',str(kernel),'-initrd',str(initrd),'-append','console=ttyS0 rdinit=/init panic=1','-nodefaults','-no-user-config','-nographic','-serial','stdio','-monitor','none','-net','none','-no-reboot','-L',str(stage/'usr/share/qemu')]
        result['argv']=argv
        result['runner_image']=args.runner_image
        result['kernel_sha256']=digest(kernel);result['initramfs_sha256']=digest(initrd);result['qemu_sha256']=digest(qemu)
        library_path=str(stage/'usr/lib/x86_64-linux-gnu')+':'+str(stage/'lib/x86_64-linux-gnu')
        child_env={'PATH':'/usr/bin:/bin','LC_ALL':'C','HOME':str(stage),'QEMU_AUDIO_DRV':'none'}
        result['loader_preflight']=loader_preflight(loader,library_path,qemu,env=child_env,cwd=str(stage))
        if result['loader_preflight']['status']!='ok':raise Refusal('loader_preflight')
        # Strace version is diagnostic metadata, not an implicit version pin.
        result['strace_version']=subprocess.run([str(args.strace),'-V'],capture_output=True,text=True,timeout=5,check=True).stdout.splitlines()[0][:120]
        def limits():
            import resource  # Linux-only; pure diagnostic() remains importable on Windows.
            resource.setrlimit(resource.RLIMIT_FSIZE,(MAX_TRACE,MAX_TRACE))
        command=[str(args.strace),'-f','-qq','-s','65535','-e','trace=%file,execve','-o',str(stage/'trace.txt'),'--',*argv]
        started=time.monotonic()
        with (stage/'serial.bin').open('wb') as stdout,(stage/'stderr.bin').open('wb') as stderr:
            proc=subprocess.Popen(command,cwd=str(stage),env=child_env,stdout=stdout,stderr=stderr,start_new_session=True,preexec_fn=limits,close_fds=True)
            try:ret=proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid,signal.SIGKILL);proc.wait(timeout=5);raise Refusal('boot_timeout')
        result['elapsed_seconds']=round(time.monotonic()-started,3);result['exit_status']=ret
        if (stage/'trace.txt').stat().st_size>=MAX_TRACE or (stage/'serial.bin').stat().st_size>MAX_OUTPUT or (stage/'stderr.bin').stat().st_size>MAX_OUTPUT:raise Refusal('output_limit')
        result['process_stderr']=bounded_failure((stage/'stderr.bin').read_bytes())
        trace=(stage/'trace.txt').read_bytes().decode('utf-8','strict').splitlines()
        result['exec_probe']=exec_probe(trace,loader,qemu)
        result['trace_sha256']=digest(stage/'trace.txt')
        observed=diagnostic(trace,stage)
        result['observed']=observed
        result['trace_shapes']=trace_shapes(trace,observed['unsupported_line_numbers'])
        result['structured_trace_details']=structured_trace_details(trace,observed['unsupported_line_numbers'])
        result['relative_open_diagnostic']=relative_open_diagnostic(trace,observed['unsupported_line_numbers'],stage,src)
        result['literal_dot_diagnostic']=literal_dot_diagnostic(trace,observed['unsupported_line_numbers'],stage)
        result['policy_shape_measurement']=policy_shape_measurement(
            trace,observed['unsupported_line_numbers'],observed['unsupported_count'])
        serial=(stage/'serial.bin').read_bytes()
        result['serial_marker_once']=(serial.count(MARKER)==1 or serial.count(MARKER[:-2]+b'\n')==1)
        result['serial_sha256']=hashlib.sha256(serial).hexdigest()
        # Distinguish successful functional boot from unresolved host closure.
        if ret or not result['serial_marker_once']:result['reason']='boot_or_marker'
        elif observed['outside']:result['reason']='ambient_files'
        elif observed['unsupported_count']:result['reason']='unsupported_trace'
        else:result['reason']='coverage_and_host_race_unproven'
        result['trace_shape_match']=trace_shape_match(
            trace,observed,stage,result['serial_marker_once'],ret)
        result['ambient_shape_match']=ambient_shape_match(
            trace,stage,observed['unsupported_line_numbers'],observed['outside'])
        # Stage comparison shows candidate files, not a race-free immutable host pin.
        result['staged_opened']={}
        for path in observed['opened']:
            if path.startswith(str(stage)+'/'):
                rel=path[len(str(stage))+1:]
                if rel=='initramfs.cpio':
                    result['staged_opened'][rel]=digest(initrd)
                    if result['staged_opened'][rel]!=INITRD_SHA:result['reason']='opened_byte_drift'
                    continue
                row=src.get(MICROVM_BIOS_SOURCE if rel==MICROVM_BIOS_DATA else rel)
                if row is None:result['reason']='opened_not_in_inventory';continue
                target=stage/rel
                if not target.is_file() or not target.resolve().is_relative_to(stage) or target.stat().st_size>40_000_000:
                    result['reason']='opened_unmeasurable';continue
                measured=digest(target);result['staged_opened'][rel]=measured
                resolved=target.resolve().relative_to(stage).as_posix()
                expected=src.get(MICROVM_BIOS_SOURCE if rel==MICROVM_BIOS_DATA and resolved==MICROVM_BIOS_DATA else resolved)
                if not isinstance(expected,dict) or expected.get('sha256')!=measured or expected.get('size')!=target.stat().st_size:
                    result['reason']='opened_byte_drift'
        result['static_files_not_observed']=sorted(set(static['files'])-set(result['staged_opened']))
        result['static_alias_shape_match']=static_alias_shape_match(
            static['files'],src,stage,result['staged_opened'])
        if result['static_files_not_observed'] and result['reason']=='coverage_and_host_race_unproven':result['reason']='unseen_static_files'
        save()
        raise SystemExit(2)
    except Exception as exc:
        result['reason']=type(exc).__name__+':'+str(exc)[:160]
        save()
        raise
    finally:
        if proc is not None and proc.poll() is None:
            os.killpg(proc.pid,signal.SIGKILL)
            proc.wait(timeout=5)

if __name__=='__main__':main()
