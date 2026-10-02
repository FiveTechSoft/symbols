#ifndef _WIN32
#include "agent_git.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* Phase 2: submodule states seen by AgentGitPreflight. Real repositories:
   a library repo used as a file:// submodule of a super repo. Every state
   that differs from the commit recorded in the super repo must be refused
   as a dirty tree; a recorded and committed state must be ready. POSIX
   only (fixtures use sh); the Windows build prints a skip. */
#define S "test_agent_git_submodule_scratch"
#define SUP S "/super"
#define SUB S "/super/lib"
static int run_n,pass_n;
#define CHECK(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else printf("  [FAIL] %s (line %d)\n",m,__LINE__);}while(0)
static int Run(const char *cwd,const char *cmd,int want)
{
    SHELL_EXEC_RESULT r;
    if(!AgentShellExec(cmd,cwd,20000,&r))return 0;
    return !r.execution_failed&&!r.timed_out&&r.exit_code==want;
}
static int Put(const char *path,const char *text,const char *mode)
{
    FILE *f=fopen(path,mode);int ok=f&&fputs(text,f)>=0;
    if(f&&fclose(f)!=0)ok=0;
    return ok;
}
static GIT_PREFLIGHT_STATUS pre(const char *repo,GIT_REPOSITORY_STATE *st)
{
    GIT_PRECONDITIONS q;char err[GIT_ERROR_MAX]="";
    memset(&q,0,sizeof(q));q.require_clean=true;
    return AgentGitPreflight(repo,&q,st,err,GIT_ERROR_MAX);
}
static void expect(const char *repo,GIT_PREFLIGHT_STATUS want,unsigned staged,unsigned unstaged,const char *msg)
{
    GIT_REPOSITORY_STATE st;GIT_PREFLIGHT_STATUS got=pre(repo,&st);
    if(got!=want)printf("    got %s want %s\n",AgentGitPreflightStatusName(got),AgentGitPreflightStatusName(want));
    CHECK(got==want&&st.staged_paths==staged&&st.unstaged_paths==unstaged,msg);
}
#define SMADD "git -c protocol.file.allow=always submodule add -q ../lib.git lib"
int main(void)
{
    printf("=== Submodule preflight ===\n");
    (void)system("rm -rf " S);
    CHECK(mkdir(S,0755)==0,"scratch");
    CHECK(Run(S,"git init -q -b main lib.git.work",0),"library repo");
    CHECK(Run(S "/lib.git.work","git config user.name F",0)&&Run(S "/lib.git.work","git config user.email f@example.invalid",0),"library identity");
    CHECK(Put(S "/lib.git.work/l.txt","one\n","wb")&&Run(S "/lib.git.work","git add l.txt",0)&&Run(S "/lib.git.work","git commit -q -m one",0),"library commit");
    CHECK(Run(S,"git clone -q --bare lib.git.work lib.git",0),"library bare");
    CHECK(Run(S,"git init -q -b main super",0),"super repo");
    CHECK(Run(SUP,"git config user.name F",0)&&Run(SUP,"git config user.email f@example.invalid",0),"super identity");
    CHECK(Put(SUP "/r.txt","root\n","wb")&&Run(SUP,"git add r.txt",0)&&Run(SUP,"git commit -q -m root",0),"root commit (preflight needs a HEAD)");
    CHECK(Run(SUP,SMADD,0),"submodule add (file protocol allowed for this command only)");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,2,0,"submodule just added (.gitmodules and pointer): staged, refused");
    CHECK(Run(SUP,"git commit -q -m add-lib",0),"commit pointer");
    expect(SUP,GIT_PREFLIGHT_READY,0,0,"submodule at the recorded commit: ready");

    CHECK(Put(SUB "/l.txt","edit\n","ab"),"edit tracked file in submodule");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,0,1,"modified content inside submodule: refused");
    CHECK(Run(SUB,"git checkout -q -- l.txt",0),"revert edit");
    expect(SUP,GIT_PREFLIGHT_READY,0,0,"reverted: ready");

    CHECK(Put(SUB "/new.txt","x\n","wb"),"untracked file in submodule");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,0,1,"untracked file inside submodule: refused");
    CHECK(Run(SUB,"rm new.txt",0),"remove it");
    expect(SUP,GIT_PREFLIGHT_READY,0,0,"removed: ready");

    CHECK(Run(SUB,"git config user.name F",0)&&Run(SUB,"git config user.email f@example.invalid",0),"submodule identity");
    CHECK(Run(SUB,"git commit -q --allow-empty -m moved",0),"new commit in submodule, not recorded");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,0,1,"submodule moved past the recorded commit: refused");
    CHECK(Run(SUP,"git add lib",0),"stage the pointer");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,1,0,"pointer staged: refused");
    CHECK(Run(SUP,"git commit -q -m bump",0),"commit the pointer");
    expect(SUP,GIT_PREFLIGHT_READY,0,0,"pointer committed: ready");

    CHECK(Run(SUB,"git checkout -q HEAD~1",0),"submodule back to the older commit");
    expect(SUP,GIT_PREFLIGHT_DIRTY_TREE,0,1,"submodule behind the recorded commit: refused");
    CHECK(Run(SUB,"git checkout -q main",0),"restore");
    expect(SUP,GIT_PREFLIGHT_READY,0,0,"restored: ready");

    /* A clone without --recurse-submodules leaves the submodule uninitialised. */
    CHECK(Run(S,"git clone -q super clone",0),"plain clone, submodule not initialised");
    expect(S "/clone",GIT_PREFLIGHT_READY,0,0,"uninitialised submodule: ready (nothing to compare)");

    (void)system("rm -rf " S);
    printf("\n=== %d/%d passed ===\n",pass_n,run_n);
    return pass_n==run_n?0:1;
}
#else
#include <stdio.h>
int main(void){printf("test_agent_git_submodule: skipped on Windows\n");return 0;}
#endif
