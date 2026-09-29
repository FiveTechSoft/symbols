"""Fail-closed, offline strace %file reconciliation for a fixed QEMU boot.

No process is launched. This parser reports unresolved observations, not a pin.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import re

class Refusal(ValueError): pass
# Exact basic line grammar for the selected strace version/flags. Unsupported
# syntax fails; no partial log becomes a closure result.
OPEN = re.compile(r'^(?:\[pid +\d+\] )?(open|openat|openat2)\((.*)\) += +(-?\d+)(?:<[^>]*>)?(?: .*)?$')
EXEC = re.compile(r'^(?:\[pid +\d+\] )?execve\((.*)\) += +(-?\d+)(?: .*)?$')
PATH = re.compile(r'^"(/[ -~]*)"$')
_ALLOWED_PREFIXES=('usr/lib/x86_64-linux-gnu/','lib/x86_64-linux-gnu/','usr/share/qemu/','usr/lib/x86_64-linux-gnu/qemu/')


def quoted(arg: str) -> str:
    m=PATH.fullmatch(arg)
    if not m or '\\' in m[1] or '"' in m[1] or '..' in m[1].split('/') or '//' in m[1]:
        raise Refusal('trace_path')
    return m[1]


def events(lines: list[str]) -> dict:
    if not isinstance(lines,list) or len(lines)>20000:raise Refusal('trace_size')
    successful=[]; missing=[]; seen_exec=0; exited=0
    for line in lines:
        if not isinstance(line,str) or len(line)>4096:raise Refusal('trace_line')
        if line=='+++ exited with 0 +++' or re.fullmatch(r'\[pid +\d+\] \+\+\+ exited with 0 \+\+\+',line):
            exited+=1;continue
        if 'exited with' in line or 'killed by' in line:raise Refusal('trace_exit')
        if '<unfinished ...>' in line or '<... ' in line or line.startswith('--- ') or 'SIG' in line:
            raise Refusal('trace_incomplete')
        m=OPEN.fullmatch(line)
        if m:
            call,args,fd=m.groups()
            if call=='open': path=quoted(args.split(', ',1)[0])
            else:
                parts=args.split(', ',2)
                if len(parts)<2 or parts[0]!='AT_FDCWD':raise Refusal('trace_dirfd')
                path=quoted(parts[1])
            if int(fd)<0:missing.append(path)
            else:successful.append(path)
            continue
        m=EXEC.fullmatch(line)
        if m:
            args,status=m.groups();path=quoted(args.split(', ',1)[0])
            if int(status)!=0:raise Refusal('trace_exec')
            successful.append(path);seen_exec+=1
            continue
        # Other traced path operations may affect runtime; never ignore them.
        raise Refusal('trace_unknown')
    if seen_exec!=1:raise Refusal('trace_no_exec')
    if exited!=1:raise Refusal('trace_no_exit')
    return {'opened':sorted(set(successful)),'unsuccessful_probes':sorted(set(missing))}


def reconcile(observed: dict, *, prefix: str, static: dict, inventory: dict, host_read) -> dict:
    if not prefix.startswith('/') or prefix.endswith('/') or '..' in prefix.split('/'):
        raise Refusal('trace_prefix')
    if static.get('schema')!='symbols.qemu-static-closure-candidate.v1' or static.get('runtime_complete') is not False:
        raise Refusal('static_candidate')
    if set(observed)!={'opened','unsuccessful_probes'}:raise Refusal('trace_schema')
    opened=observed['opened']
    if not isinstance(opened,list) or opened!=sorted(set(opened)):raise Refusal('trace_order')
    if (not isinstance(observed['unsuccessful_probes'],list) or
            observed['unsuccessful_probes']!=sorted(set(observed['unsuccessful_probes']))):raise Refusal('trace_order')
    source=inventory['source']; result={}
    for path in opened:
        if not isinstance(path,str) or not path.startswith(prefix+'/'):
            raise Refusal('host_external_file')
        rel=path[len(prefix)+1:]
        if not (rel=='usr/bin/qemu-system-x86_64' or rel.startswith(_ALLOWED_PREFIXES)):
            raise Refusal('runtime_path')
        row=source.get(rel)
        if row is None:raise Refusal('inventory_missing')
        # Follow only inventory relative basename symlinks, never the host FS.
        seen=set(); target=rel
        while 'link' in row:
            if target in seen:raise Refusal('link_cycle')
            seen.add(target)
            link=row['link']
            if not isinstance(link,str) or not link or '/' in link or link in ('.','..'):
                raise Refusal('link_target')
            target=target.rsplit('/',1)[0]+'/'+link;row=source.get(target)
            if not isinstance(row,dict):raise Refusal('link_missing')
        if not isinstance(row.get('size'),int) or not isinstance(row.get('sha256'),str):
            raise Refusal('inventory_file')
        # Keep this read scoped to the path observed by strace; the caller
        # must separately ensure no replacement race between trace and read.
        data=host_read(path)
        if len(data)!=row['size'] or hashlib.sha256(data).hexdigest()!=row['sha256']:
            raise Refusal('host_drift')
        result[rel]={'resolved':target,'sha256':row['sha256'],'size':row['size']}
    # PT_INTERP loader is mapped by the kernel, not necessarily opened by
    # QEMU's own open syscalls. Keep it as a static pin to measure separately.
    expected=set(static['files'])-{'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'}
    if not expected.issubset({x['resolved'] for x in result.values()}):
        raise Refusal('unseen_static_dependency')
    # An observed open isn't proof that there were no invisible loads, nor can
    # successful open paths alone close runtime data and firmware.
    missing=['usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'] if 'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2' in static['files'] else []
    return {'schema':'symbols.qemu-trace-candidate.v1','runtime_complete':False,
            'observed':{x:result[x] for x in sorted(result)},
            'unseen_static_files':missing,'unsuccessful_probes':observed['unsuccessful_probes'],
            'limitation':'Requires complete trace, closed dynamic/data set and on-host immutable measurement before pinning.'}
