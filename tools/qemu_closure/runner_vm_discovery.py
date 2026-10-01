#!/usr/bin/env python3
"""Read-only self metadata, no enforcement or private-scope assertion."""
import json
import os
import re
import signal
import stat
import sys
import time

GATES=('runner_identity','payload_credentials','host_content','host_fds','egress',
       'resources','process_cleanup','workspace_cleanup','provider_lifecycle')
FIELDS=('cgroup.type','cgroup.controllers','cgroup.subtree_control')
META=('cgroup.procs','cgroup.threads','cgroup.kill')
REASONS=('metadata_only_not_enforcement','identity','bounds','anchor','metadata_error','timeout')

class Stop(Exception):
    def __init__(self,reason):self.reason=reason

class Reader:
    def __init__(self):
        self.ops=0;self.bytes=0;self.end=time.monotonic()+10;self.fds=[]
    def call(self,fn,*args,**kwargs):
        if time.monotonic()>=self.end:raise Stop('timeout')
        # Reserve the closing operation before allocating each descriptor.
        if self.ops+len(self.fds)>=64:raise Stop('bounds')
        self.ops+=1
        return fn(*args,**kwargs)
    def open(self,path,flags,dir_fd=None):
        if self.ops+len(self.fds)+2>64:raise Stop('bounds')
        fd=self.call(os.open,path,flags|os.O_NOFOLLOW|os.O_CLOEXEC,dir_fd=dir_fd)
        self.fds.append(fd);return fd
    def close(self,fd):
        # Closing owned read handles must work even after a deadline/bound refusal.
        self.ops+=1;os.close(fd);self.fds.remove(fd)
    def directory(self,path):
        if not isinstance(path,str) or len(path)>4096 or not path.startswith('/'):
            raise Stop('anchor')
        parts=path.split('/')[1:]
        if any(not x or x in ('.','..') for x in parts) or len(parts)>64:
            raise Stop('anchor')
        fd=self.open('/',os.O_RDONLY|os.O_DIRECTORY)
        try:
            for part in parts:
                child=self.open(part,os.O_RDONLY|os.O_DIRECTORY,fd)
                self.close(fd);fd=child
            return fd
        except BaseException:
            if fd in self.fds:self.close(fd)
            raise
    def text(self,root,name):
        if name not in (*FIELDS,'cgroup','status','mountinfo'):raise Stop('anchor')
        fd=self.open(name,os.O_RDONLY,root)
        try:
            info=self.call(os.fstat,fd)
            if not stat.S_ISREG(info.st_mode):raise Stop('anchor')
            if self.bytes+4097>32768:raise Stop('bounds')
            raw=self.call(os.read,fd,4097)
            self.bytes+=len(raw)
            if len(raw)>4096 or self.bytes>32768:raise Stop('bounds')
            return raw.decode('ascii')
        finally:self.close(fd)
    def finish(self):
        for fd in list(self.fds):self.close(fd)


def base(reason):
    return {'schema':'symbols.runner-vm-discovery.v1','classification':'blocked',
            'reason':reason,'boot_attempted':False,'runtime_complete':False,
            'isolation_accredited':False,'scope':{'membership':'unknown',
            'candidate':'not_provided','delegation':'not_proven','interfaces':[],
            'coverage':'prioritized_not_complete','other_delegations':'not_excluded'},
            'storage':{'anchor':'unknown','filesystem':'unknown','quota_option':'unknown',
            'bytes_enforcement':'not_proven','entries_enforcement':'not_proven'},
            'separation':{'controller_identity':'self_only','payload_identity':'not_instantiated',
            'identity_relation':'unknown','capabilities':'unknown','no_new_privs':'unknown',
            'seccomp':'unknown','separation':'not_proven'},
            'gates':[{'gate':g,'status':'not_proven'} for g in GATES],
            'bounds':{'operations':0,'input_bytes':0}}


def membership(raw):
    rows=raw.splitlines()
    if len(rows)!=1 or not rows[0].startswith('0::/'):raise Stop('metadata_error')
    path=rows[0][3:]
    if len(path)>4096 or any(x in ('.','..') for x in path.split('/')):
        raise Stop('anchor')
    return '/sys/fs/cgroup'+('' if path=='/' else path)


def value(name,raw):
    text=raw.strip()
    if name=='cgroup.type':return 'domain_candidate' if text=='domain' else 'threaded_incompatible' if text in ('threaded','domain threaded','domain invalid') else 'unknown'
    if name in ('cgroup.controllers','cgroup.subtree_control'):
        if not all(re.fullmatch('[a-z_]+',x) for x in text.split()):return 'unknown'
        return 'required_present' if {'cpu','memory','pids'}<=set(text.split()) else 'required_missing'
    return 'unknown'


def own_status(raw):
    result={'identity_relation':'unknown','capabilities':'unknown','no_new_privs':'unknown','seccomp':'unknown'}
    rows={}
    for line in raw.splitlines():
        k,sep,v=line.partition(':')
        if k in ('Uid','Gid','Groups','CapEff','NoNewPrivs','Seccomp'):
            if not sep or k in rows:raise Stop('metadata_error')
            rows[k]=v.split()
    uid=rows.get('Uid',[]);gid=rows.get('Gid',[])
    if len(uid)==len(gid)==4 and all(x.isdigit() for x in uid+gid):
        result['identity_relation']='ids_equal' if uid[0]==uid[1] and gid[0]==gid[1] else 'ids_differ'
    c=rows.get('CapEff',[])
    if len(c)==1 and re.fullmatch('[0-9a-fA-F]{1,16}',c[0]):result['capabilities']='none_reported' if int(c[0],16)==0 else 'present_reported'
    for name,key,states in (('NoNewPrivs','no_new_privs',{'0':'unset','1':'set'}),('Seccomp','seccomp',{'0':'disabled','1':'strict','2':'filter'})):
        v=rows.get(name,[])
        if len(v)==1:result[key]=states.get(v[0],'unknown')
    return result


def mount(raw):
    # Match ONLY the fixed cgroup anchor; do not emit or discover other mount roots.
    rows=[]
    for line in raw.splitlines():
        a,sep,b=line.partition(' - ');f=a.split();g=b.split()
        if sep and len(f)>=6 and len(g)>=3 and f[4]=='/sys/fs/cgroup':rows.append(g[0])
    return len(rows)==1 and rows[0]=='cgroup2'


def observe(r,env):
    result=base('metadata_only_not_enforcement')
    # /proc/self is a kernel symlink: open only this process's numeric directory,
    # constructed from getpid(), through the no-follow /proc anchor. No foreign PID.
    pid=r.call(os.getpid);proc=r.directory('/proc')
    own=r.open(str(pid),os.O_RDONLY|os.O_DIRECTORY,proc);r.close(proc)
    path=membership(r.text(own,'cgroup'))
    if not mount(r.text(own,'mountinfo')):raise Stop('metadata_error')
    result['separation'].update(own_status(r.text(own,'status')));r.close(own)
    cg=r.directory(path);result['scope']['membership']='v2_candidate'
    # Read only fixed files. procs/threads contents never opened.
    for name in FIELDS:
        try:state=value(name,r.text(cg,name))
        except FileNotFoundError:state='missing'
        result['scope']['interfaces'].append({'interface':name.replace('.','_'),'state':state})
    # Metadata-only presence of write-only kill and task-list interfaces.
    for name in META:
        try:
            info=r.call(os.stat,name,dir_fd=cg,follow_symlinks=False)
            state='regular_metadata_only' if stat.S_ISREG(info.st_mode) else 'unsupported_type'
        except FileNotFoundError:state='missing'
        result['scope']['interfaces'].append({'interface':name.replace('.','_'),'state':state})
    r.close(cg)
    tmp=r.directory(env.get('RUNNER_TEMP',''))
    info=r.call(os.fstat,tmp);r.call(os.fstatvfs,tmp)
    result['storage']['anchor']='directory_metadata_only' if stat.S_ISDIR(info.st_mode) else 'unknown'
    # statvfs exposes flags/capacity, not filesystem identity or quota enforcement.
    result['storage']['filesystem']='not_identified_by_statvfs'
    result['storage']['quota_option']='not_inspected'
    r.close(tmp)
    return result


def main(argv):
    r=Reader();result=base('identity')
    def alarm(signum,frame):raise Stop('timeout')
    old=signal.signal(signal.SIGALRM,alarm);signal.setitimer(signal.ITIMER_REAL,10)
    try:
        uid=r.call(os.getuid);euid=r.call(os.geteuid)
        if (len(argv)==2 and uid!=0 and euid!=0 and sys.platform=='linux' and
            re.fullmatch('[0-9a-f]{40}',argv[0]) and re.fullmatch(r'[0-9]{8}\.[0-9]+\.[0-9]+',argv[1]) and
            all(os.environ.get(k)==v for k,v in {'GITHUB_SHA':argv[0],'GITHUB_REF':'refs/heads/master',
                'RUNNER_ENVIRONMENT':'github-hosted','RUNNER_OS':'Linux','RUNNER_ARCH':'X64','ImageVersion':argv[1]}.items())):
            result=observe(r,os.environ)
    except Stop as exc:result=base(exc.reason)
    except (OSError,ValueError,UnicodeError):result=base('metadata_error')
    finally:
        signal.setitimer(signal.ITIMER_REAL,0);signal.signal(signal.SIGALRM,old)
        try:r.finish()
        except OSError:result=base('metadata_error')
    result['bounds']={'operations':r.ops,'input_bytes':r.bytes}
    print(json.dumps(result,sort_keys=True,separators=(',',':')))
    return 2

if __name__=='__main__':raise SystemExit(main(sys.argv[1:]))
