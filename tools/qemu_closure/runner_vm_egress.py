#!/usr/bin/env python3
"""Fixed trusted numeric-loopback experiment. Not an adversarial egress proof."""
import json
import os
from pathlib import Path
import selectors
import signal
import socket
import stat
import subprocess
import sys
import time
from runner_vm_surface import identity, anchors

MARK=b'LOCL'
DNS=bytes([0x12,0x34,1,0,0,1,0,0,0,0,0,0,0,0,1,0,1])
FRAMED=bytes([0,17])+DNS
CLASSES=('tcp4','tcp6','udp4','udp6','dns_udp4','dns_udp6','dns_tcp4','dns_tcp6',
         'inherited_tcp','inherited_udp','process_samples')
CONTROL={'schema':'symbols.runner-vm-egress-control.v1','classification':'measured_only',
         'loopback_baselines':True,'inherited_baselines':True,'fork_reaped':True}
CHILD={'schema':'symbols.runner-vm-egress-child.v1','classification':'measured_only',
       'limits_readback':True,'hard_raise_denied':True,'allow_getpid':True,'deny_classes':True,
       'known_fds_closed':True,'transport_samples':True}
REASONS=('identity','fixture_setup','baseline','fd_identity','negative_result','fixture_protocol',
         'timeout','output_bounds','child_schema','supervisor_error','cleanup_refusal',
         'capabilities_only_not_enforcement')
BINARIES=('symbols-vm-egress-control','symbols-vm-egress-child')
SCRATCH='symbols-vm-egress-owned'
REPORT='qemu-runner-vm-egress.json'
CLEANUP='qemu-runner-vm-egress-cleanup.json'
SUPERVISOR='qemu-runner-vm-egress-supervisor-cleanup.json'

class Refusal(Exception):
    def __init__(self,reason): self.reason=reason


def exact(raw,expected):
    def unique(pairs):
        d={}
        for k,v in pairs:
            if k in d: raise ValueError()
            d[k]=v
        return d
    try:
        j=json.loads(raw,object_pairs_hook=unique)
        return type(j) is dict and set(j)==set(expected) and all(
            type(j[k]) is type(v) and j[k]==v for k,v in expected.items())
    except (ValueError,UnicodeError,RecursionError): return False


def report(reason):
    good=reason=='capabilities_only_not_enforcement'
    return {'schema':'symbols.runner-vm-egress.v1','classification':'blocked',
            'reason':reason if reason in REASONS else 'supervisor_error',
            'boot_attempted':False,'runtime_complete':False,'isolation_accredited':False,
            'classes':[{ 'class':k,'baseline':good,'negative':good} for k in CLASSES],
            'gates':[{'gate':g,'status':'not_proven'} for g in
                     ('egress','resources','process_cleanup','workspace_cleanup')]}


def clean_report(reaped=False,closed=False,paths=False,refusal=False):
    return {'schema':'symbols.runner-vm-egress-cleanup.v1',
            'direct_children':'confirmed_owned_direct' if reaped else 'not_proven',
            'fixture_descriptors':'confirmed_owned_direct' if closed else 'not_proven',
            'owned_paths':'confirmed_owned_direct' if paths else 'not_proven',
            'outcome':'refusal' if refusal else 'measured_only',
            'aggregate_process_cleanup':'not_proven','aggregate_workspace_cleanup':'not_proven'}


def write_report(path,value):
    # Fixed report path supplied only by internal callers; O_NOFOLLOW, bounded content.
    raw=json.dumps(value,sort_keys=True,separators=(',',':')).encode()+b'\n'
    if len(raw)>8192: raise Refusal('output_bounds')
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_TRUNC|os.O_NOFOLLOW,0o600)
    try:
        if os.write(fd,raw)!=len(raw): raise Refusal('output_bounds')
    finally: os.close(fd)


class Fixtures:
    def __init__(self):
        self.poll=selectors.DefaultSelector();self.owned=[];self.ports=[]
        self.receipts={};self.accepts=0;self.bytes=0;self.negative=False
        try:
            for stream in (True,False):
                for v6 in (False,True):
                    s=socket.socket(socket.AF_INET6 if v6 else socket.AF_INET,
                                    socket.SOCK_STREAM if stream else socket.SOCK_DGRAM)
                    self.owned.append(s)
                    if v6:s.setsockopt(socket.IPPROTO_IPV6,socket.IPV6_V6ONLY,1)
                    address='::1' if v6 else '127.0.0.1'
                    s.bind((address,0));s.setblocking(False)
                    if stream:s.listen(1)
                    self.ports.append(s.getsockname()[1])
                    tag=('tcp' if stream else 'udp')+('6' if v6 else '4')
                    self.poll.register(s,selectors.EVENT_READ,('listen' if stream else 'udp',tag,b''))
        except (OSError,ValueError):
            self.close();raise Refusal('fixture_setup')

    def prepare_inherited(self):
        endpoints=[]
        try:
            for stream,port in ((True,self.ports[0]),(False,self.ports[2])):
                s=socket.socket(socket.AF_INET,socket.SOCK_STREAM if stream else socket.SOCK_DGRAM)
                endpoints.append(s);s.settimeout(1)
                s.connect(('127.0.0.1',port))
                if stream:
                    peer,_=self.owned[0].accept();peer.setblocking(False);self.owned.append(peer)
                    self.poll.register(peer,selectors.EVENT_READ,('stream','inherited_tcp',b''))
                else:
                    # Source port classifies only this owned connected UDP endpoint.
                    self.inherited_udp=s.getsockname()[1]
            if any(s.fileno()<3 or s.fileno()>=16 for s in endpoints):raise Refusal('fd_identity')
            return endpoints
        except (OSError,ValueError,Refusal):
            for s in endpoints:s.close()
            raise Refusal('fd_identity')

    def receipt(self,tag,data):
        self.bytes+=len(data)
        if self.negative or self.bytes>512:raise Refusal('negative_result' if self.negative else 'output_bounds')
        if data==MARK:key=tag
        elif data==DNS and tag.startswith('udp'):key='dns_'+tag
        elif data==FRAMED and tag.startswith('tcp'):key='dns_'+tag
        else:raise Refusal('fixture_protocol')
        self.receipts[key]=self.receipts.get(key,0)+1

    def service(self,s,key):
        kind,tag,pending=key
        if kind=='listen':
            if self.negative:raise Refusal('negative_result')
            peer,addr=s.accept()
            if addr[0] not in ('127.0.0.1','::1') or self.accepts>=4:
                peer.close();raise Refusal('fixture_protocol')
            self.accepts+=1;peer.setblocking(False);self.owned.append(peer)
            self.poll.register(peer,selectors.EVENT_READ,('stream',tag,b''))
        elif kind=='udp':
            data,addr=s.recvfrom(65)
            if addr[0] not in ('127.0.0.1','::1'):raise Refusal('fixture_protocol')
            actual='inherited_udp' if addr[1]==getattr(self,'inherited_udp',None) else tag
            self.receipt(actual,data)
            if s.sendto(b'R',addr)!=1:raise Refusal('fixture_protocol')
        elif kind=='stream':
            data=s.recv(65)
            if not data:
                if pending:raise Refusal('fixture_protocol')
                self.poll.unregister(s);s.close();return
            pending+=data
            if len(pending)>64:raise Refusal('output_bounds')
            size=4 if pending.startswith(MARK[:min(len(pending),4)]) else 19
            if len(pending)>size:raise Refusal('fixture_protocol')
            if len(pending)==size:
                self.receipt(tag,pending)
                if s.send(b'R')!=1:raise Refusal('fixture_protocol')
                pending=b''
            self.poll.modify(s,selectors.EVENT_READ,('stream',tag,pending))

    def baseline_complete(self):
        expected={k:1 for k in CLASSES if k!='process_samples'}
        expected['inherited_tcp']=2;expected['inherited_udp']=2
        return self.receipts==expected and self.accepts==4

    def close(self):
        ok=True
        for s in self.owned:
            try:s.close();ok=ok and s.fileno()==-1
            except OSError:ok=False
        self.poll.close();return ok


def run_child(binary,fixtures,expected,negative,deadline):
    child=None;handle=None;passed=[];raw=bytearray();reaped=False
    try:
        passed=fixtures.prepare_inherited();fds=tuple(s.fileno() for s in passed)
        args=[str(binary),*(str(p) for p in fixtures.ports),*(str(fd) for fd in fds)]
        child=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL,env={},close_fds=True,pass_fds=fds)
        for s in passed:s.close()
        passed=[]
        if hasattr(os,'pidfd_open'):
            try:handle=os.pidfd_open(child.pid)
            except OSError:pass
        fixtures.poll.register(child.stdout,selectors.EVENT_READ,('output','',b''))
        end=min(deadline,time.monotonic()+3)
        eof=False
        while not eof:
            left=end-time.monotonic()
            if left<=0:raise Refusal('timeout')
            events=fixtures.poll.select(left)
            if not events:raise Refusal('timeout')
            for key,_ in events:
                if key.data[0]=='output':
                    data=os.read(child.stdout.fileno(),4096)
                    if not data:eof=True;continue
                    raw.extend(data)
                    if len(raw)>1024:raise Refusal('output_bounds')
                else:fixtures.service(key.fileobj,key.data)
        if child.wait(timeout=max(0.01,end-time.monotonic()))!=0:raise Refusal('negative_result' if negative else 'baseline')
        reaped=True
        if not exact(raw,expected):raise Refusal('child_schema')
        # Drain queued owned events once, including EOF; no new traffic is sent.
        for key,_ in fixtures.poll.select(0):
            if key.data[0]!='output':fixtures.service(key.fileobj,key.data)
        if not negative and not fixtures.baseline_complete():raise Refusal('baseline')
        if negative:
            for key,_ in fixtures.poll.select(min(0.1,max(0,deadline-time.monotonic()))):
                if key.data[0]!='output':fixtures.service(key.fileobj,key.data)
        return True
    finally:
        for s in passed:s.close()
        if child is not None:
            try:
                if child.poll() is None:
                    if handle is not None and hasattr(signal,'pidfd_send_signal'):
                        signal.pidfd_send_signal(handle,signal.SIGKILL)
                    else:child.kill()
                child.wait(timeout=2);reaped=True
            except (OSError,subprocess.SubprocessError):reaped=False
            if child.stdout:
                try:fixtures.poll.unregister(child.stdout)
                except (KeyError,ValueError):pass
                child.stdout.close()
        if handle is not None:os.close(handle)
        # No child or a fully reaped direct child is a closed owned result.
        fixtures.last_reaped=child is None or reaped
        if not fixtures.last_reaped:raise Refusal('cleanup_refusal')


def remove_owned(root):
    scratch=root/SCRATCH
    try:
        if scratch.exists() or scratch.is_symlink():
            info=scratch.lstat()
            if not stat.S_ISDIR(info.st_mode) or info.st_uid!=os.getuid():return False
            if list(scratch.iterdir()):return False
            scratch.rmdir()
        for name in BINARIES:
            p=root/name
            try:info=p.lstat()
            except FileNotFoundError:continue
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid():return False
            p.unlink()
        return all(not (root/n).exists() and not (root/n).is_symlink() for n in (*BINARIES,SCRATCH))
    except OSError:return False


def experiment(root):
    def terminate(signum,frame):raise Refusal('supervisor_error')
    previous={s:signal.signal(s,terminate) for s in (signal.SIGTERM,signal.SIGINT)}
    fixtures=None;reason='supervisor_error';reaped=False;closed=False
    deadline=time.monotonic()+15
    try:
        write_report(root/SUPERVISOR,clean_report())
        for name in BINARIES:
            p=root/name;info=p.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or info.st_mode&0o022:
                raise Refusal('fd_identity')
        (root/SCRATCH).mkdir(mode=0o700)
        fixtures=Fixtures()
        run_child(root/BINARIES[0],fixtures,CONTROL,False,deadline)
        reaped=fixtures.last_reaped
        fixtures.negative=True
        run_child(root/BINARIES[1],fixtures,CHILD,True,deadline)
        reaped=reaped and fixtures.last_reaped
        reason='capabilities_only_not_enforcement'
    except Refusal as exc:reason=exc.reason
    except (OSError,ValueError,subprocess.SubprocessError):reason='supervisor_error'
    finally:
        if fixtures is not None:
            reaped=getattr(fixtures,'last_reaped',False) and (reaped or reason!='capabilities_only_not_enforcement')
            closed=fixtures.close()
        paths=remove_owned(root)
        if not closed or not paths or not reaped:reason='cleanup_refusal' if reason=='capabilities_only_not_enforcement' else reason
        try:write_report(root/SUPERVISOR,clean_report(reaped,closed,paths,not (reaped and closed and paths)))
        finally:
            for sig,handler in previous.items():signal.signal(sig,handler)
    return report(reason)


def finalizer(root):
    # Missing supervisor evidence never implies direct child or socket cleanup.
    result=clean_report()
    try:
        raw=(root/SUPERVISOR).read_bytes()
        if len(raw)<=8192:
            value=json.loads(raw)
            candidates=[clean_report(a,b,c,d) for a in (False,True) for b in (False,True)
                        for c in (False,True) for d in (False,True)]
            if any(exact(raw,v) for v in candidates):result=value
    except (OSError,ValueError):pass
    paths=remove_owned(root)
    result['owned_paths']='confirmed_owned_direct' if paths else 'not_proven'
    if not paths:result['outcome']='refusal'
    write_report(root/CLEANUP,result)
    return 0 if paths else 2


def main(argv):
    root=Path(os.environ.get('RUNNER_TEMP','/nonexistent'))
    if len(argv)==1 and argv[0]=='finalize':return finalizer(root)
    result=report('identity')
    if len(argv)==2 and identity(os.environ,*argv) and anchors(os.environ) is not None:
        try:result=experiment(root)
        except (OSError,ValueError,Refusal):result=report('supervisor_error')
    print(json.dumps(result,sort_keys=True,separators=(',',':')))
    return 2

if __name__=='__main__':raise SystemExit(main(sys.argv[1:]))
