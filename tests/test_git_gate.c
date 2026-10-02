#ifndef _WIN32
#include "git_gate.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
/* Phase 2 step 5a: the command line front end (symbols_git_gate) over the Git
   contracts, and the manifest derived from a patch. Real repositories, POSIX
   only. Nothing in the workflow uses it yet. */
#define S "test_git_gate_scratch"
#define R S "/r"
#define P S "/p"
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
/* Run the in-process front end with a built argument vector; collect output. */
static char gout[2048],gerr[2048];
static int Gate(const char *a1,const char *a2,const char *a3,const char *a4,const char *a5,const char *a6,const char *a7)
{
    char *argv[9];int argc=0;FILE *o,*e;int rc;size_t n;
    argv[argc++]="gate";
    if(a1)argv[argc++]=(char*)a1;if(a2)argv[argc++]=(char*)a2;if(a3)argv[argc++]=(char*)a3;
    if(a4)argv[argc++]=(char*)a4;if(a5)argv[argc++]=(char*)a5;if(a6)argv[argc++]=(char*)a6;if(a7)argv[argc++]=(char*)a7;
    o=tmpfile();e=tmpfile();
    rc=GitGateRun(argc,argv,o,e);
    rewind(o);rewind(e);
    n=fread(gout,1,sizeof gout-1,o);gout[n]=0;
    n=fread(gerr,1,sizeof gerr-1,e);gerr[n]=0;
    fclose(o);fclose(e);return rc;
}
static int Derive(const char *text,GIT_GATE_MANIFEST *m,char *err)
{
    Put(P,text);
    return GitGateDeriveManifest(P,m,err,GIT_ERROR_MAX);
}
static int Has(const GIT_GATE_MANIFEST *m,const char *path,char st)
{
    size_t i;for(i=0;i<m->count;i++)if(!strcmp(m->path[i],path))return m->status[i]==st;return 0;
}
int main(void)
{
    static GIT_GATE_MANIFEST m;
    char err[GIT_ERROR_MAX]="",head[80];
    SHELL_EXEC_RESULT r;
    printf("=== symbols_git_gate: manifest from the patch and the four commands ===\n");
    (void)system("rm -rf " S);
    CHECK(mkdir(S,0755)==0&&Run(S,"git init -q -b main r",0),"repo");
    CHECK(Run(R,"git config user.name F",0)&&Run(R,"git config user.email f@example.invalid",0),"identity");
    CHECK(Put(R "/a.txt","one\ntwo\nthree\nfour\nfive\nsix\n")&&Put(R "/b.txt","bee\n")&&Put(R "/old.txt","line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\n")&&Put(R "/x.sh","echo\n"),"files");
    CHECK(Run(R,"git add -A && git commit -q -m base",0),"base commit");
    CHECK(Run(R,"git rev-parse HEAD",0)&&AgentShellExec("git rev-parse HEAD",R,20000,&r),"head");
    snprintf(head,sizeof head,"%.40s",r.stdout_buf);

    /* A real patch: modify, delete, add, binary add, rename with edit, mode change. */
    CHECK(Put(R "/a.txt","one\nTWO\nthree\nfour\nfive\nsix\n")&&Run(R,"git rm -q b.txt",0)&&Put(R "/new.txt","n\n")&&Run(R,"printf '\\000\\001\\002bin' > blob.bin",0)
          &&Run(R,"git mv old.txt moved.txt",0)&&Put(R "/moved.txt","line1\nline2\nline3\nline4\nline5\nline6\nline7\nEIGHT\n")&&Run(R,"chmod +x x.sh",0)
          &&Run(R,"git add -A",0),"stage the change");
    CHECK(Run(R,"git diff --cached --binary -M > ../p.patch",0),"patch with rename detection");
    CHECK(GitGateDeriveManifest(S "/p.patch",&m,err,sizeof err),"derive from a real patch");
    CHECK(m.count==7,"seven entries (rename counts twice)");
    CHECK(Has(&m,"a.txt",'M')&&Has(&m,"b.txt",'D')&&Has(&m,"new.txt",'A')&&Has(&m,"blob.bin",'A'),"modify, delete, add, binary add");
    CHECK(Has(&m,"old.txt",'D')&&Has(&m,"moved.txt",'A'),"rename is D for the old path and A for the new one");
    CHECK(Has(&m,"x.sh",'M'),"mode-only change is M");

    /* Hand written headers: refusals, all fail closed. */
    CHECK(!Derive("diff --git \"a/q\" \"b/q\"\n",&m,err)&&strstr(err,"Unsupported"),"quoted header refused");
    CHECK(!Derive("diff --git a/a b.txt b/a b.txt\n",&m,err),"space in a path refused");
    CHECK(!Derive("diff --git a/x.txt b/y.txt\n--- a/x.txt\n+++ b/y.txt\n",&m,err)&&strstr(err,"differ"),"different paths without a rename header refused");
    CHECK(!Derive("diff --git a/../x b/../x\n",&m,err),"dot dot refused");
    CHECK(!Derive("diff --git a/-x b/-x\n",&m,err),"leading dash refused");
    CHECK(!Derive("diff --git a/q.txt b/q.txt\n@@ -1 +1 @@\n-a\n+b\ndiff --git a/q.txt b/q.txt\n",&m,err)&&strstr(err,"twice"),"same path twice refused");
    CHECK(!Derive("not a patch\n",&m,err)&&strstr(err,"no files"),"text without diff headers: no files");
    CHECK(!Derive("",&m,err),"empty file refused");
    CHECK(!GitGateDeriveManifest(S "/missing.patch",&m,err,sizeof err),"missing file refused");
    CHECK(Derive("diff --git a/c.txt b/c.txt\nindex 1..2 100644\n--- a/c.txt\n+++ b/c.txt\n@@ -1 +1 @@\n-x\n+diff --git a/trick b/trick\n",&m,err)&&m.count==1,"a body line that looks like a header is not a header");
    {
        char big[200000];size_t n=0;int i;
        for(i=0;i<300;i++)n+=(size_t)snprintf(big+n,sizeof big-n,"diff --git a/f%d b/f%d\nnew file mode 100644\n",i,i);
        CHECK(!Derive(big,&m,err)&&strstr(err,"more than"),"more than the supported number of paths refused");
    }
    {
        GIT_GATE_MANIFEST *mm=&m;
        CHECK(Derive("diff --git a/s.txt b/s.txt\nsimilarity index 90%\ncopy from s.txt\ncopy to t.txt\n",mm,err)&&Has(mm,"t.txt",'A')&&mm->count==1,"copy is A for the destination");
    }

    /* The front end. Patch in the repository at a relative path. */
    CHECK(Run(R,"git reset -q --hard HEAD && git clean -fdq",0),"back to the base commit");
    CHECK(Run(S,"cp p.patch r/.git/change.patch",0),"patch copied to .git/change.patch");
    CHECK(Gate("preflight","--expected-head",head,"--dir",R,NULL,NULL)==0&&strstr(gout,"ready"),"preflight: ready");
    CHECK(Gate("preflight","--expected-head","0000000000000000000000000000000000000000","--dir",R,NULL,NULL)==10+3&&strstr(gout,"stale_head")&&gerr[0],"preflight: stale head, exit 13, explanation on stderr");
    CHECK(Gate("preflight","--expected-head",head,"--branch","other","--dir",R)==10+4&&strstr(gout,"wrong_branch"),"preflight: wrong branch, exit 14");
    CHECK(Put(R "/dirty.txt","d\n")&&Gate("preflight","--expected-head",head,"--dir",R,NULL,NULL)==10+7&&strstr(gout,"dirty_tree"),"preflight: untracked file is dirty, exit 17");
    CHECK(Run(R,"rm dirty.txt",0)&&Gate("preflight","--expected-head",head,"--dir",R,NULL,NULL)==0,"preflight: ready again");
    CHECK(Gate("preflight","--dir",R,NULL,NULL,NULL,NULL)==GIT_GATE_USAGE,"preflight without --expected-head: usage");
    CHECK(Gate("preflight","--expected-head",head,"--remote-sync","--dir",R,NULL)==10+8&&strstr(gout,"no_upstream"),"preflight --remote-sync without an upstream: exit 18");

    CHECK(Gate("patch-state",".git/change.patch","--dir",R,NULL,NULL,NULL)==0&&strstr(gout,"not applied"),"patch-state: not applied, exit 0");
    CHECK(Gate("patch-state","/tmp/x.patch","--dir",R,NULL,NULL,NULL)==5,"patch-state: absolute path refused, exit 5");
    CHECK(Gate("patch-state","nonexistent.patch","--dir",R,NULL,NULL,NULL)==5,"patch-state: missing patch, exit 5");
    CHECK(Gate("verify-staged",".git/change.patch","--dir",R,NULL,NULL,NULL)==1,"verify-staged before applying: mismatch, exit 1");

    /* The workflow sequence: apply, stage, verify, commit, verify. */
    CHECK(Run(R,"git apply .git/change.patch",0),"git apply");
    CHECK(Gate("patch-state",".git/change.patch","--dir",R,NULL,NULL,NULL)==3&&strstr(gout,"already applied"),"patch-state after applying: already applied, exit 3");
    /* The built executable returns the same codes. */
#ifdef GATE_EXE
    {
        char cmd[1024];
        snprintf(cmd,sizeof cmd,"%s patch-state .git/change.patch --dir r",GATE_EXE);
        CHECK(Run(S,cmd,3),"executable: patch-state on the applied patch exits 3");
        snprintf(cmd,sizeof cmd,"%s",GATE_EXE);
        CHECK(Run(S,cmd,64),"executable: no arguments exits 64");
    }
#endif
    CHECK(Run(R,"git add -A",0)&&Gate("verify-staged",".git/change.patch","--dir",R,NULL,NULL,NULL)==0&&strstr(gout,"match (7 paths)"),"verify-staged: match");
    CHECK(Put(R "/extra.c","e\n")&&Run(R,"git add extra.c",0)&&Gate("verify-staged",".git/change.patch","--dir",R,NULL,NULL,NULL)==1&&strstr(gerr,"extra.c"),"an extra staged file: mismatch names it");
    CHECK(Run(R,"git rm -q --cached extra.c && rm extra.c",0)&&Gate("verify-staged",".git/change.patch","--dir",R,NULL,NULL,NULL)==0,"unstaged: match");
    CHECK(Run(R,"git commit -q -m change",0)&&Gate("verify-head",".git/change.patch","--dir",R,NULL,NULL,NULL)==0,"verify-head: match");
    CHECK(Run(R,"git commit -q --allow-empty -m empty",0)&&Gate("verify-head",".git/change.patch","--dir",R,NULL,NULL,NULL)==1,"verify-head on a different HEAD: mismatch");
    CHECK(Put(R "/a.txt","one\nQQ\nthree\nfour\nfive\nsix\n")&&Run(R,"git commit -q -am diverge",0)&&Gate("patch-state",".git/change.patch","--dir",R,NULL,NULL,NULL)==4&&strstr(gout,"neither")&&gerr[0],"patch-state: diverged tree, applies neither way, exit 4");
    CHECK(Gate("verify-head","garbage.patch","--dir",R,NULL,NULL,NULL)==6,"verify-head with an unreadable patch: exit 6");
    CHECK(Put(P,"junk\n")&&Gate("verify-staged","../p","--dir",R,NULL,NULL,NULL)==6,"verify-staged with a patch that has no headers: exit 6");
    CHECK(Gate(NULL,NULL,NULL,NULL,NULL,NULL,NULL)==GIT_GATE_USAGE&&Gate("bogus",NULL,NULL,NULL,NULL,NULL,NULL)==GIT_GATE_USAGE,"no command, unknown command: usage");

    printf("\n%d/%d passed\n",pass_n,run_n);
    (void)system("rm -rf " S);
    return pass_n==run_n?0:1;
}
#else
#include <stdio.h>
int main(void){printf("SKIP: test_git_gate is POSIX only\n");return 0;}
#endif
