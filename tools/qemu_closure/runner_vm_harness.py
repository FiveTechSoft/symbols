#!/usr/bin/env python3
"""Trusted bounded child supervisor. No arbitrary executable or arguments."""
import json
import os
from pathlib import Path
import selectors
import signal
import stat
import subprocess
import sys
import time
from runner_vm_surface import identity

EXPECTED = {'schema':'symbols.runner-vm-harness-child.v2','classification':'measured_only',
            'limits_readback':True,'all_five_ceiling_raises_denied':True,'allow_getpid':True,'deny_classes':True}
REASONS = ('identity','binary','child_failure','timeout','output_bounds','child_schema',
           'supervisor_error','capabilities_only_not_enforcement')


def report(reason):
    return {'schema':'symbols.runner-vm-harness.v2','classification':'blocked',
            'reason':reason if reason in REASONS else 'supervisor_error',
            'boot_attempted':False,'runtime_complete':False,'isolation_accredited':False,
            'gates':[{'gate':g,'status':'not_proven'} for g in ('egress','resources','process_cleanup','workspace_cleanup')],
            'child_checks':'measured_only' if reason=='capabilities_only_not_enforcement' else 'not_proven',
            'ceiling_raise_denials':[{'limit':k,'status':
                'measured_only' if reason=='capabilities_only_not_enforcement' else 'not_proven'}
                for k in ('as','cpu','fsize','nofile','core')],
            'consumption_violations':'not_tested'}


def validate(raw):
    try:
        def unique(pairs):
            d={}
            for k,v in pairs:
                if k in d: raise ValueError('duplicate')
                d[k]=v
            return d
        value=json.loads(raw,object_pairs_hook=unique)
        return type(value) is dict and set(value)==set(EXPECTED) and all(
            type(value[k]) is type(v) and value[k]==v for k,v in EXPECTED.items())
    except (ValueError,UnicodeError,RecursionError):
        return False


def supervise(binary):
    child=None; handle=None; result='supervisor_error'
    try:
        # No shell, no payload env, no inherited descriptors except stdout pipe.
        child=subprocess.Popen([str(binary)],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL,env={},close_fds=True)
        if hasattr(os,'pidfd_open'):
            try: handle=os.pidfd_open(child.pid)
            except OSError:
                # Direct unreaped child remains stable; optional pidfd unavailable.
                handle=None
        raw=bytearray(); deadline=time.monotonic()+3
        with selectors.DefaultSelector() as poll:
            poll.register(child.stdout,selectors.EVENT_READ)
            while True:
                left=deadline-time.monotonic()
                if left<=0:
                    result='timeout'; break
                if not poll.select(left):
                    result='timeout'; break
                piece=os.read(child.stdout.fileno(),4096)
                if not piece:
                    try: rc=child.wait(timeout=max(0.01,deadline-time.monotonic()))
                    except subprocess.TimeoutExpired:
                        result='timeout';break
                    result=('child_failure' if rc else 'capabilities_only_not_enforcement' if validate(raw) else 'child_schema')
                    break
                raw.extend(piece)
                if len(raw)>1024:
                    result='output_bounds';break
    except (OSError,ValueError,subprocess.SubprocessError):
        result='supervisor_error'
    finally:
        if child is not None:
            try:
                if child.poll() is None:
                    if handle is not None and hasattr(signal,'pidfd_send_signal'):
                        signal.pidfd_send_signal(handle,signal.SIGKILL)
                    else:
                        # Unreaped direct child PID cannot be recycled; no descendants.
                        child.kill()
                child.wait(timeout=2)
            except (OSError,subprocess.SubprocessError):
                result='supervisor_error'
            if child.stdout is not None: child.stdout.close()
        if handle is not None: os.close(handle)
    return report(result)


def main(argv):
    result=report('identity')
    try:
        if len(argv)==2 and identity(os.environ,*argv):
            # Fixed location produced by reviewed workflow, never arbitrary input.
            binary=Path(os.environ['RUNNER_TEMP'])/'symbols-vm-harness'
            info=binary.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or info.st_mode&0o022:
                result=report('binary')
            else: result=supervise(binary)
    except (OSError,ValueError,KeyError): result=report('supervisor_error')
    print(json.dumps(result,sort_keys=True,separators=(',',':')))
    return 2


if __name__=='__main__': raise SystemExit(main(sys.argv[1:]))
