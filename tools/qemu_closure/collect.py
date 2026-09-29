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
MARKER=b'SYMBOLS_BOOT_ONLY_SUPERVISOR_READY_v1\r\n'
MAX_TRACE=16_000_000
MAX_OUTPUT=65536
MAX_PREFLIGHT=8192
SONAME=re.compile(rb'[A-Za-z0-9_+.-]{1,80}\.so(?:\.[0-9]{1,8}){0,4}')
# This diagnostic treats every non-staged successful file operation as a blocker,
# not as evidence for promoting a static closure.
PID_PREFIX=r'(?:[0-9]{1,12} |\[pid +[0-9]{1,12}\] )?'
LINE=re.compile(r'^'+PID_PREFIX+r'([a-z][a-z0-9_]*)\((.*)\) += +(.+)$')
QUOTED=re.compile(r'"(/[^"\\]*)"')
FD=re.compile(r'^[0-9]+(?:<[^>]*>)?$')
ERROR=re.compile(r'^-1 [A-Z][A-Z0-9_]+ ')


def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


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
    ):
        if needle in lower:labels.append(label)
    # Only a loader-formatted missing-library line may supply a soname.
    sonames=sorted({m.decode('ascii') for m in re.findall(
        rb'error while loading shared libraries: ('+SONAME.pattern+rb'):',clipped)})[:12]
    return {'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),
            'truncated':len(data)>MAX_OUTPUT,'categories':labels or ['unclassified'],
            'sonames':sonames}


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
        serial=(stage/'serial.bin').read_bytes()
        result['serial_marker_once']=(serial.count(MARKER)==1 or serial.count(MARKER[:-2]+b'\n')==1)
        result['serial_sha256']=hashlib.sha256(serial).hexdigest()
        # Distinguish successful functional boot from unresolved host closure.
        if ret or not result['serial_marker_once']:result['reason']='boot_or_marker'
        elif observed['outside']:result['reason']='ambient_files'
        elif observed['unsupported_count']:result['reason']='unsupported_trace'
        else:result['reason']='coverage_and_host_race_unproven'
        # Stage comparison shows candidate files, not a race-free immutable host pin.
        result['staged_opened']={}
        for path in observed['opened']:
            if path.startswith(str(stage)+'/'):
                rel=path[len(str(stage))+1:]
                if rel=='initramfs.cpio':
                    result['staged_opened'][rel]=digest(initrd)
                    if result['staged_opened'][rel]!=INITRD_SHA:result['reason']='opened_byte_drift'
                    continue
                row=src.get(rel)
                if row is None:result['reason']='opened_not_in_inventory';continue
                target=stage/rel
                if not target.is_file() or not target.resolve().is_relative_to(stage) or target.stat().st_size>40_000_000:
                    result['reason']='opened_unmeasurable';continue
                measured=digest(target);result['staged_opened'][rel]=measured
                resolved=target.resolve().relative_to(stage).as_posix()
                expected=src.get(resolved)
                if not isinstance(expected,dict) or expected.get('sha256')!=measured or expected.get('size')!=target.stat().st_size:
                    result['reason']='opened_byte_drift'
        result['static_files_not_observed']=sorted(set(static['files'])-set(result['staged_opened']))
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
