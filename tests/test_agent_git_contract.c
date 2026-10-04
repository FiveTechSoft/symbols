#include "agent_git.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMTREE(p) "if exist " p " rmdir /s /q " p
#define MKDIR_DIR "if not exist dir mkdir dir"
#define COPY_HOSTILE "copy p.patch \"p.patch;true\" >nul && copy p.patch \"-p.patch\" >nul"
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMTREE(p) "rm -rf " p
#define MKDIR_DIR "mkdir -p dir"
#define COPY_HOSTILE "cp p.patch 'p.patch;true' && cp p.patch ./-p.patch"
#endif
/* Phase 2: commit contract (staged index and HEAD commit equal the reviewed
   manifest) and "already applied" detection for a retry. Real repositories.
   Runs on POSIX and, since m114, on Windows (fixture helpers differ in rmdir, mkdir, copy, quoting). */
#define S "test_agent_git_contract_scratch"
#define R S "/r"
static int run_n,pass_n;
#define CHECK(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else printf("  [FAIL] %s (line %d)\n",m,__LINE__);}while(0)
static int Run(const char *cwd,const char *cmd,int want)
{
    SHELL_EXEC_RESULT r;
    if(!AgentShellExec(cmd,cwd,20000,&r))return 0;
    return !r.execution_failed&&!r.timed_out&&r.exit_code==want;
}
static int Put(const char *path,const char *text)
{
    FILE *f=fopen(path,"wb");int ok=f&&fputs(text,f)>=0;
    if(f&&fclose(f)!=0)ok=0;
    return ok;
}
static int Snapshot(char *out,size_t n)
{
    SHELL_EXEC_RESULT r;
    if(!AgentShellExec("git rev-parse HEAD && git status --porcelain=v2 --untracked-files=all && git diff --cached --stat",R,20000,&r)||r.exit_code!=0)return 0;
    snprintf(out,n,"%s",r.stdout_buf);return 1;
}
static GIT_CHANGES_STATUS staged(const GIT_EXPECTED_CHANGE *e,size_t n,char *err)
{ return AgentGitStagedMatches(R,e,n,err,GIT_ERROR_MAX); }
static GIT_CHANGES_STATUS head(const GIT_EXPECTED_CHANGE *e,size_t n,char *err)
{ return AgentGitHeadCommitMatches(R,e,n,err,GIT_ERROR_MAX); }
static GIT_PATCH_STATE pstate(const char *p,char *err)
{ return AgentGitPatchState(R,p,err,GIT_ERROR_MAX); }
int main(void)
{
    char err[GIT_ERROR_MAX]="",before[8192],after[8192];
    GIT_EXPECTED_CHANGE all[3]={{"a.txt",'M'},{"b.txt",'D'},{"dir/c y.txt",'A'}};
    GIT_EXPECTED_CHANGE anyk[3]={{"a.txt",0},{"b.txt",0},{"dir/c y.txt",0}};
    printf("=== Commit contract and already-applied detection ===\n");
    (void)system(RMTREE(S));
    CHECK(MKDIR(S)==0&&Run(S,"git init -q -b main r",0),"repo");
    CHECK(Run(R,"git config user.name F",0)&&Run(R,"git config user.email f@example.invalid",0)&&Run(R,"git config core.autocrlf false",0),"identity");
    CHECK(Put(R "/a.txt","one\ntwo\nthree\n")&&Put(R "/b.txt","bee\n")&&Run(R,"git add a.txt b.txt",0)&&Run(R,"git commit -q -m base",0),"base commit");

    /* Index vs the manifest. */
    CHECK(staged(NULL,0,err)==GIT_CHANGES_MATCH,"empty index, empty manifest: match");
    CHECK(Put(R "/a.txt","one\nTWO\nthree\n")&&Run(R,"git rm -q b.txt",0)&&Run(R,MKDIR_DIR,0)&&Put(R "/dir/c y.txt","c\n")&&Run(R,"git add a.txt \"dir/c y.txt\"",0),"stage modify, delete, add (path with a space)");
    CHECK(staged(all,3,err)==GIT_CHANGES_MATCH,"exact manifest with kinds: match");
    CHECK(staged(anyk,3,err)==GIT_CHANGES_MATCH,"exact manifest, kinds not given: match");
    {GIT_EXPECTED_CHANGE w[3]={{"a.txt",'A'},{"b.txt",'D'},{"dir/c y.txt",'A'}};
     CHECK(staged(w,3,err)==GIT_CHANGES_MISMATCH&&strstr(err,"a.txt"),"wrong kind: mismatch names the path");}
    CHECK(staged(all,2,err)==GIT_CHANGES_MISMATCH&&strstr(err,"Unexpected"),"manifest missing a staged path: mismatch");
    {GIT_EXPECTED_CHANGE x[4]={{"a.txt",'M'},{"b.txt",'D'},{"dir/c y.txt",'A'},{"zzz.txt",'A'}};
     CHECK(staged(x,4,err)==GIT_CHANGES_MISMATCH&&strstr(err,"zzz.txt"),"manifest names a path not staged: mismatch");}
    CHECK(Put(R "/extra.txt","e\n")&&staged(all,3,err)==GIT_CHANGES_MATCH,"untracked file is not part of the index: still match");
    CHECK(Run(R,"git add extra.txt",0)&&staged(all,3,err)==GIT_CHANGES_MISMATCH&&strstr(err,"Unexpected")&&strstr(err,"extra.txt"),"extra staged file: mismatch names it as unexpected");
    CHECK(Run(R,"git rm -q --cached extra.txt",0)&&staged(all,3,err)==GIT_CHANGES_MATCH,"unstaged again: match");
    {GIT_EXPECTED_CHANGE d[2]={{"a.txt",'M'},{"a.txt",'M'}};
     CHECK(staged(d,2,err)==GIT_CHANGES_INSPECTION_FAILED,"manifest repeating a path: refused");}
    {GIT_EXPECTED_CHANGE d[1]={{"bad\"name",'A'}};
     CHECK(staged(d,1,err)==GIT_CHANGES_INSPECTION_FAILED,"quote in a manifest path: refused");}
    {GIT_EXPECTED_CHANGE d[1]={{"a.txt",'X'}};
     CHECK(staged(d,1,err)==GIT_CHANGES_INSPECTION_FAILED,"unknown kind letter: refused");}
    CHECK(Snapshot(before,sizeof(before)),"snapshot");
    (void)staged(all,3,err);(void)staged(NULL,0,err);
    CHECK(Snapshot(after,sizeof(after))&&!strcmp(before,after),"staged checks changed nothing");

    /* The produced commit vs the manifest. */
    CHECK(Run(R,"git commit -q -m change",0),"commit");
    CHECK(head(all,3,err)==GIT_CHANGES_MATCH,"HEAD commit equals the manifest: match");
    CHECK(head(anyk,3,err)==GIT_CHANGES_MATCH,"HEAD commit, kinds not given: match");
    CHECK(head(all,2,err)==GIT_CHANGES_MISMATCH,"HEAD commit has a path the manifest lacks: mismatch");
    {GIT_EXPECTED_CHANGE x[4]={{"a.txt",'M'},{"b.txt",'D'},{"dir/c y.txt",'A'},{"q.txt",'A'}};
     CHECK(head(x,4,err)==GIT_CHANGES_MISMATCH&&strstr(err,"q.txt"),"manifest names a path the commit lacks: mismatch");}
    CHECK(Run(R,"git commit -q --allow-empty -m empty",0)&&head(NULL,0,err)==GIT_CHANGES_MATCH,"empty commit, empty manifest: match");
    CHECK(head(all,3,err)==GIT_CHANGES_MISMATCH,"previous manifest against an empty commit: mismatch");
    CHECK(Run(R,"git checkout -q -b side HEAD~1",0)&&Put(R "/s.txt","s\n")&&Run(R,"git add s.txt",0)&&Run(R,"git commit -q -m side",0)&&Run(R,"git checkout -q main",0)&&Run(R,"git merge -q --no-ff -m merge side",0),"merge commit");
    {GIT_EXPECTED_CHANGE m[1]={{"s.txt",'A'}};
     CHECK(head(m,1,err)==GIT_CHANGES_MISMATCH&&strstr(err,"merge"),"merge commit never matches");}

    /* Already applied. */
    CHECK(Run(S,"git init -q -b main r2",0),"second repo");
    CHECK(Run(S "/r2","git config user.name F",0)&&Run(S "/r2","git config user.email f@example.invalid",0)&&Run(S "/r2","git config core.autocrlf false",0),"identity 2");
    CHECK(Put(S "/r2/f.txt","1\n2\n3\n4\n5\n")&&Run(S "/r2","git add f.txt",0)&&Run(S "/r2","git commit -q -m base",0),"base 2");
    CHECK(Put(S "/r2/f.txt","1\n2\nTHREE\n4\n5\n")&&Run(S "/r2","git diff > p.patch",0)&&Run(S "/r2","git checkout -q -- f.txt",0),"patch made, tree restored");
    {char snap1[4096],snap2[4096];SHELL_EXEC_RESULT r;
     CHECK(AgentShellExec("git rev-parse HEAD && git status --porcelain=v2",S "/r2",20000,&r)&&r.exit_code==0,"snapshot r2");snprintf(snap1,sizeof(snap1),"%s",r.stdout_buf);
     CHECK(AgentGitPatchState(S "/r2","p.patch",err,GIT_ERROR_MAX)==GIT_PATCH_NOT_APPLIED,"fresh patch: not applied");
     CHECK(AgentShellExec("git rev-parse HEAD && git status --porcelain=v2",S "/r2",20000,&r),"snapshot r2 again");snprintf(snap2,sizeof(snap2),"%s",r.stdout_buf);
     CHECK(!strcmp(snap1,snap2),"state check changed nothing");}
    CHECK(Run(S "/r2","git apply p.patch",0)&&AgentGitPatchState(S "/r2","p.patch",err,GIT_ERROR_MAX)==GIT_PATCH_ALREADY_APPLIED,"applied, uncommitted: already applied");
    CHECK(Run(S "/r2","git commit -q -am applied",0)&&AgentGitPatchState(S "/r2","p.patch",err,GIT_ERROR_MAX)==GIT_PATCH_ALREADY_APPLIED,"applied and committed: already applied");
    CHECK(Put(S "/r2/f.txt","1\n2\nELSE\n4\n5\n")&&AgentGitPatchState(S "/r2","p.patch",err,GIT_ERROR_MAX)==GIT_PATCH_NO_MATCH,"same lines changed again: neither direction");
    CHECK(Run(S "/r2","git checkout -q -- f.txt",0)&&AgentGitPatchState(S "/r2","nope.patch",err,GIT_ERROR_MAX)==GIT_PATCH_CHECK_FAILED,"missing patch file: check failed");
    CHECK(AgentGitPatchState(S "/r2","p.patch; echo x",err,GIT_ERROR_MAX)==GIT_PATCH_CHECK_FAILED,"unsafe patch path: check failed");
    CHECK(Run(S "/r2",COPY_HOSTILE,0),"patch copies with hostile names");
    CHECK(AgentGitPatchState(S "/r2","p.patch;true",err,GIT_ERROR_MAX)==GIT_PATCH_CHECK_FAILED,"existing file with a shell metacharacter name: check failed");
    CHECK(AgentGitPatchState(S "/r2","-p.patch",err,GIT_ERROR_MAX)==GIT_PATCH_CHECK_FAILED,"leading dash: check failed");
    CHECK(Put(S "/r2/bad.patch","this is not a patch\n")&&AgentGitPatchState(S "/r2","bad.patch",err,GIT_ERROR_MAX)==GIT_PATCH_CHECK_FAILED,"garbage file: check failed");

    (void)system(RMTREE(S));
    printf("\n=== %d/%d passed ===\n",pass_n,run_n);
    return pass_n==run_n?0:1;
}
