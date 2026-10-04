#include "agent_git.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMTREE(p) "if exist " p " rmdir /s /q " p
#define MOVE(a, b) "move " a " " b
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMTREE(p) "rm -rf " p
#define MOVE(a, b) "mv " a " " b
#endif
/* Phase 2: remote-advance detection in AgentGitPreflight (opt-in
   require_remote_in_sync). Real repositories: a bare local remote and two
   clones. The check is read only: every case also proves HEAD, the working
   tree, the remote-tracking ref and the index did not change. Runs on POSIX and, since m112,
   on Windows (the fixture helpers differ only in rmdir, mkdir and move). */
#define S "test_agent_git_remote_scratch"
static int run_n,pass_n;
#define CHECK(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else printf("  [FAIL] %s (line %d)\n",m,__LINE__);}while(0)
static int Run(const char *cwd,const char *cmd,int want)
{
    SHELL_EXEC_RESULT r;
    if(!AgentShellExec(cmd,cwd,20000,&r))return 0;
    return !r.execution_failed&&!r.timed_out&&r.exit_code==want;
}
static int Out(const char *cwd,const char *cmd,char *buf,size_t n)
{
    SHELL_EXEC_RESULT r;size_t k;
    if(!AgentShellExec(cmd,cwd,20000,&r)||r.execution_failed||r.exit_code!=0)return 0;
    snprintf(buf,n,"%s",r.stdout_buf);k=strlen(buf);
    while(k&&(buf[k-1]=='\n'||buf[k-1]=='\r'))buf[--k]=0;
    return 1;
}
#define ID "-c user.name=F -c user.email=f@example.invalid "
/* Everything a read-only check must leave alone. */
static int snapshot(const char *repo,char *out,size_t n)
{
    char h[128]="",t[256]="",s[512]="";
    if(!Out(repo,"git rev-parse HEAD",h,sizeof(h))||
       !Out(repo,"git for-each-ref refs/remotes",t,sizeof(t))||
       !Out(repo,"git status --porcelain=v2 --untracked-files=all",s,sizeof(s)))return 0;
    snprintf(out,n,"%s|%s|%s",h,t,s);return 1;
}
static GIT_PREFLIGHT_STATUS pre(const char *repo,int remote,GIT_REPOSITORY_STATE *st,char *err)
{
    GIT_PRECONDITIONS q;memset(&q,0,sizeof(q));
    q.require_clean=true;q.require_remote_in_sync=remote?true:false;
    return AgentGitPreflight(repo,&q,st,err,GIT_ERROR_MAX);
}
static void expect(const char *repo,int remote,GIT_PREFLIGHT_STATUS want,const char *msg)
{
    char before[1024],after[1024],err[GIT_ERROR_MAX]="";GIT_REPOSITORY_STATE st;
    GIT_PREFLIGHT_STATUS got;
    CHECK(snapshot(repo,before,sizeof(before)),"snapshot before");
    got=pre(repo,remote,&st,err);
    if(got!=want)printf("    got %s (%s), want %s\n",AgentGitPreflightStatusName(got),err,AgentGitPreflightStatusName(want));
    CHECK(got==want,msg);
    CHECK(snapshot(repo,after,sizeof(after))&&!strcmp(before,after),"check changed nothing (HEAD, refs, index, tree)");
}
int main(void)
{
    const char *a=S "/a";char head_b[128];
    printf("=== Remote-advance preflight ===\n");
    (void)system(RMTREE(S));
    CHECK(MKDIR(S)==0,"scratch");
    CHECK(Run(S,"git init --bare -b main remote.git",0),"bare remote");
    CHECK(Run(S,"git clone -q remote.git a",0)&&Run(a,"git checkout -q -b main",0),"clone a");
    CHECK(Run(S,"git clone -q remote.git b",0)&&Run(S "/b","git checkout -q -b main",0),"clone b");
    CHECK(Run(a,"git config user.name F",0)&&Run(a,"git config user.email f@example.invalid",0),"identity a");
    CHECK(Run(S "/b","git config user.name F",0)&&Run(S "/b","git config user.email f@example.invalid",0),"identity b");
    {FILE *f=fopen(S "/a/f.txt","wb");CHECK(f&&fputs("base\n",f)>=0&&fclose(f)==0,"write base");}
    CHECK(Run(a,"git add f.txt",0)&&Run(a,"git commit -q -m base",0)&&Run(a,"git push -q -u origin main",0),"base pushed with upstream");
    CHECK(Run(S "/b","git pull -q origin main",0)&&Run(S "/b","git branch -q --set-upstream-to=origin/main main",0),"b follows");

    expect(a,1,GIT_PREFLIGHT_READY,"in sync: ready");

    /* Another clone advances the remote. */
    {FILE *f=fopen(S "/b/g.txt","wb");CHECK(f&&fputs("other\n",f)>=0&&fclose(f)==0,"write other");}
    CHECK(Run(S "/b","git add g.txt",0)&&Run(S "/b","git commit -q -m other",0)&&Run(S "/b","git push -q origin main",0),"remote advanced by b");
    CHECK(Out(S "/b","git rev-parse HEAD",head_b,sizeof(head_b)),"remote tip");
    expect(a,0,GIT_PREFLIGHT_READY,"check off: advance not looked for (opt-in)");
    expect(a,1,GIT_PREFLIGHT_REMOTE_ADVANCED,"remote advanced: refused, nothing fetched or changed");
    {GIT_REPOSITORY_STATE st;char err[GIT_ERROR_MAX]="";
     GIT_PREFLIGHT_STATUS g=pre(a,1,&st,err);
     CHECK(g==GIT_PREFLIGHT_REMOTE_ADVANCED,"remote advanced again: refused");
     CHECK(!strcmp(st.remote_head,head_b)&&!strcmp(st.upstream,"origin/main"),"reports the remote tip and upstream");}

    /* Diverged: a local commit on top of the old base while the remote moved. */
    {FILE *f=fopen(S "/a/f.txt","ab");CHECK(f&&fputs("local\n",f)>=0&&fclose(f)==0,"write local");}
    CHECK(Run(a,"git add f.txt",0)&&Run(a,"git commit -q -m local",0),"local commit");
    expect(a,1,GIT_PREFLIGHT_REMOTE_ADVANCED,"diverged: refused");

    /* Catch up by an explicit merge: local ahead of the remote tip is fine. */
    CHECK(Run(a,"git pull -q --no-rebase --no-edit origin main",0),"explicit pull and merge");
    expect(a,1,GIT_PREFLIGHT_READY,"local ahead of the remote tip: ready");

    /* No upstream. */
    CHECK(Run(a,"git checkout -q -b topic",0),"topic branch without upstream");
    expect(a,1,GIT_PREFLIGHT_NO_UPSTREAM,"no upstream: refused");
    expect(a,0,GIT_PREFLIGHT_READY,"no upstream, check off: ready");
    CHECK(Run(a,"git checkout -q main",0),"back to main");

    CHECK(Run(a,"git checkout -q --detach",0),"detached HEAD");
    expect(a,1,GIT_PREFLIGHT_DETACHED_HEAD,"detached HEAD: refused by default");
    {GIT_PRECONDITIONS q;GIT_REPOSITORY_STATE st;char err[GIT_ERROR_MAX]="";
     memset(&q,0,sizeof(q));q.require_clean=true;q.allow_detached_head=true;q.require_remote_in_sync=true;
     CHECK(AgentGitPreflight(a,&q,&st,err,GIT_ERROR_MAX)==GIT_PREFLIGHT_NO_UPSTREAM,"detached allowed, remote check on: no upstream, refused");}
    CHECK(Run(a,"git checkout -q main",0),"back to main again");

    /* Unreachable remote fails closed, never ready. */
    CHECK(Run(S,MOVE("remote.git","remote.gone"),0),"remote moved away");
    expect(a,1,GIT_PREFLIGHT_INSPECTION_FAILED,"unreachable remote: refused");
    CHECK(Run(S,MOVE("remote.gone","remote.git"),0),"remote back");
    expect(a,1,GIT_PREFLIGHT_READY,"reachable again: ready");

    /* Dirty tree is still refused first. */
    {FILE *f=fopen(S "/a/new.txt","wb");CHECK(f&&fputs("x\n",f)>=0&&fclose(f)==0,"untracked file");}
    expect(a,1,GIT_PREFLIGHT_DIRTY_TREE,"dirty tree wins over remote state");

    (void)system(RMTREE(S));
    printf("\n=== %d/%d passed ===\n",pass_n,run_n);
    return pass_n==run_n?0:1;
}
