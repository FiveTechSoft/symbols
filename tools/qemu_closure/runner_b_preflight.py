#!/usr/bin/env python3
"""No-boot hosted-runner namespace capability probe. Never accredits isolation."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

NAMESPACES=('user','mnt','net','pid','ipc','uts')


def refuse(reason):
    return {'schema':'symbols.runner-b-preflight.v1','runtime_complete':False,
            'isolation_accredited':False,'classification':'blocked','reason':reason}


def verify(host, child):
    """Classify only capability evidence; never claim confinement or a pin."""
    if not isinstance(host,dict) or not isinstance(child,dict):return refuse('schema')
    if host.get('euid')==0 or host.get('uid')==0:return refuse('host_root')
    if host.get('runner_image')!=host.get('expected_runner_image') or not re.fullmatch(r'[0-9]{8}\.[0-9]+\.[0-9]+',str(host.get('runner_image',''))):return refuse('runner_image')
    if (child.get('euid')!=0 or child.get('pid')!=1 or
        child.get('mount_propagation_private') is not True or
        child.get('staged_proc') is not True or
        child.get('staged_empty_sys') is not True):return refuse('namespace_effects')
    before=host.get('namespace_ids');after=child.get('namespace_ids')
    if not isinstance(before,dict) or not isinstance(after,dict):return refuse('namespace_ids')
    for name in NAMESPACES:
        if not (re.fullmatch(name+r':\[[0-9]+\]',str(before.get(name,''))) and
                re.fullmatch(name+r':\[[0-9]+\]',str(after.get(name,''))) and
                before[name]!=after[name]):return refuse('namespace_'+name)
    return {'schema':'symbols.runner-b-preflight.v1','runtime_complete':False,
            'isolation_accredited':False,'classification':'measured_only','reason':'namespace_capability_only',
            'host_nonroot':True,'runner_image':host['runner_image'],
            'namespaces_distinct':list(NAMESPACES),'staged_proc_mount':True,
            'staged_empty_sys_directory':True,
            'limitation':'Staged proc/empty sys are NOT remapped as /proc and /sys for QEMU; no QEMU install/run, KVM check, /dev policy, host FD identity, egress test, limits or adversarial isolation proof'}


def namespace_ids():
    return {name:os.readlink('/proc/self/ns/'+name) for name in NAMESPACES}


def child_main(root):
    # The child has private user/mount/net/ipc/uts/pid namespaces and PID 1.
    # No QEMU, chroot, bind of host /sys, or mutation of host mount namespace.
    if root.is_symlink() or root.exists() or not root.parent.is_dir() or root.parent.is_symlink():
        raise ValueError('child_root')
    root.mkdir(mode=0o700)
    tmpfs=False;proc=False
    try:
        subprocess.run(['/usr/bin/mount','--make-rprivate','/'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        subprocess.run(['/usr/bin/mount','-t','tmpfs','-o','size=1m,mode=0700,nosuid,nodev,noexec','tmpfs',str(root)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        tmpfs=True
        (root/'proc').mkdir();(root/'sys').mkdir()
        subprocess.run(['/usr/bin/mount','-t','proc','-o','nosuid,nodev,noexec,hidepid=2','proc',str(root/'proc')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        proc=True
        proc_ok=(root/'proc'/'self'/'stat').is_file() and (root/'proc'/'1'/'stat').is_file()
        result={'euid':os.geteuid(),'pid':os.getpid(),'namespace_ids':namespace_ids(),
                'mount_propagation_private':True,'staged_proc':proc_ok,
                'staged_empty_sys':not any((root/'sys').iterdir())}
    finally:
        if proc:subprocess.run(['/usr/bin/umount',str(root/'proc')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        if tmpfs:subprocess.run(['/usr/bin/umount',str(root)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
        root.rmdir()
    print(json.dumps(result,sort_keys=True),flush=True)


def main():
    import tempfile
    parser=argparse.ArgumentParser()
    parser.add_argument('--child-root',type=Path)
    parser.add_argument('--expected-runner-image')
    parser.add_argument('--expected-target-sha')
    a=parser.parse_args()
    if a.child_root:
        child_main(a.child_root);return
    result=refuse('preflight_incomplete')
    try:
        if sys.platform!='linux':raise ValueError('not_linux')
        if os.getuid()==0 or os.geteuid()==0:raise ValueError('host_root')
        if not a.expected_runner_image or os.environ.get('ImageVersion')!=a.expected_runner_image:raise ValueError('runner_image')
        if not re.fullmatch(r'[0-9a-f]{40}',a.expected_target_sha or '') or os.environ.get('GITHUB_SHA')!=a.expected_target_sha or os.environ.get('GITHUB_REF')!='refs/heads/master':raise ValueError('workflow_identity')
        host={'uid':os.getuid(),'euid':os.geteuid(),'namespace_ids':namespace_ids(),
              'runner_image':os.environ['ImageVersion'],'expected_runner_image':a.expected_runner_image}
        # No network, guest boot or subprocess from an untrusted task input.
        with tempfile.TemporaryDirectory(prefix='symbols-b-preflight-',dir=os.environ.get('RUNNER_TEMP')) as tmp:
            root=Path(tmp)/'synthetic'
            command=['/usr/bin/unshare','--user','--map-root-user','--mount','--net','--pid',
                     '--ipc','--uts','--fork','--','/usr/bin/python3',str(Path(__file__).resolve()),'--child-root',str(root)]
            p=subprocess.run(command,capture_output=True,text=True,timeout=15)
            if p.returncode or len(p.stdout)>4096:raise ValueError('namespace_setup')
            child=json.loads(p.stdout)
            result=verify(host,child)
    except (ValueError, OSError, subprocess.SubprocessError, json.JSONDecodeError) as exc:
        reason=str(exc)
        # No exception string/path/host data in report.
        result=refuse(reason if reason in {'not_linux','host_root','namespace_setup','runner_image','workflow_identity'} else 'preflight_error')
    print(json.dumps(result,sort_keys=True))
    raise SystemExit(0 if result['classification']=='measured_only' else 2)


if __name__=='__main__':main()
