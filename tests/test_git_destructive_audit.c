/* Phase 2 criterion 4: inventory of destructive git invocations in the
   product sources (src/ and include/). Every line that names a destructive
   git command must match an entry in the whitelist below, with the reason
   written next to it. A new use fails the test until someone reviews it and
   adds it here. A whitelist entry that no longer matches also fails, so a
   removed use is noticed.
   Limits: this is a text scan. It does not see commands built from pieces
   at run time, scripts outside src/ and include/, or workflows. */
#ifdef _WIN32
#include <stdio.h>
int main(void){printf("SKIP: test_git_destructive_audit is POSIX only (directory walk)\n");return 0;}
#else
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static int run_n,pass_n;
#define CHECK(x,m) do{run_n++;if(x){pass_n++;printf("  [PASS] %s\n",m);}else printf("  [FAIL] %s (line %d)\n",m,__LINE__);}while(0)

static const char *const PATTERNS[]={
    "git reset","git clean","git checkout","git restore","git push","--force",
    "--hard","git stash","git rebase","git switch","git filter","git rm",
    "git branch -D","git revert","git gc","git prune","git update-ref",
    "git worktree","git reflog"};
#define NPAT (sizeof PATTERNS/sizeof PATTERNS[0])

typedef struct { const char *file; const char *fragment; const char *why; } ALLOW;
static const ALLOW WL[]={
 {"src/command_policy.c","has_arg(s, a, \"--force\")","detector: refuses forced operations"},
 {"src/command_policy.c","\"--hard\"","detector: refuses reset --hard"},
 {"src/command_policy.c","\"git %s --hard discards work\"","detector message"},
 {"src/command_policy.c","\"git %s remove --force discards a worktree\"","detector message"},
 {"include/command_policy.h","force push, reset --hard","comment describing the detector"},
 {"include/git_ops.h","git checkout HEAD~1 -- FILE","comment: undelete a file from HEAD~1 (no force, one path)"},
 {"include/git_ops.h","git revert --no-edit HEAD","comment: revert adds a commit, no history rewrite"},
 {"src/git_ops.c","\"git checkout -m -- %s\"","restores conflicted paths after a failed merge; path-scoped"},
 {"src/git_ops.c","\"git checkout HEAD~1 -- %s\"","undelete one path from HEAD~1; path-scoped"},
 {"src/git_ops.c","\"git rm -q --cached -- %s\"","index only, keeps the work tree file"},
 {"src/git_ops.c","\"git revert --no-edit HEAD\"","adds a revert commit, no history rewrite"},
 {"src/git_ops.c","\"git reset -q --hard %s\"","KNOWN EXCEPTION: discards uncommitted work; outside the delivery contracts; see audit doc"},
 {"src/git_ops.c","\"git revert --abort\"","aborts an own revert"},
 {"src/server_proto.c","\"git commit\", \"git push\"","phrase list that recognises mutation intent in a question; never executed"},
 {"src/server_proto.c","\"git merge\", \"git rebase\"","same phrase list"},
 {"src/server_proto.c","\"git stash\", \"git reset\"","same phrase list"},
 {"src/server_taskops.c","\"git checkout HEAD~1 -- '%s'\"","undelete one path from HEAD~1; path-scoped"},
 {"src/server_taskops.c","\"git revert --no-edit HEAD\"","adds a revert commit, no history rewrite"},
};
#define NWL (sizeof WL/sizeof WL[0])
static int wl_used[NWL];

static int Destructive(const char *line)
{
    size_t i;
    for(i=0;i<NPAT;i++) if(strstr(line,PATTERNS[i])) return 1;
    return 0;
}
static int Allowed(const char *file,const char *line,int mark)
{
    size_t i;int hit=0;
    for(i=0;i<NWL;i++)
        if(!strcmp(WL[i].file,file)&&strstr(line,WL[i].fragment)){hit=1;if(mark)wl_used[i]++;}
    return hit;
}
static int unlisted,scanned,hits;
static void ScanFile(const char *path)
{
    FILE *f=fopen(path,"rb");char line[4096];int n=0;
    if(!f){printf("  cannot open %s\n",path);unlisted++;return;}
    scanned++;
    while(fgets(line,sizeof line,f)){
        n++;
        if(!Destructive(line))continue;
        hits++;
        if(!Allowed(path,line,1)){unlisted++;printf("  UNLISTED %s:%d: %s",path,n,line);}
    }
    fclose(f);
}
static void Walk(const char *dir)
{
    DIR *d=opendir(dir);struct dirent *e;
    if(!d){printf("  cannot open dir %s\n",dir);unlisted++;return;}
    while((e=readdir(d))){
        char p[1024];struct stat st;size_t L;
        if(e->d_name[0]=='.')continue;
        snprintf(p,sizeof p,"%s/%s",dir,e->d_name);
        if(stat(p,&st)!=0)continue;
        if(S_ISDIR(st.st_mode)){Walk(p);continue;}
        L=strlen(e->d_name);
        if((L>2&&!strcmp(e->d_name+L-2,".c"))||(L>2&&!strcmp(e->d_name+L-2,".h"))||
           (L>4&&!strcmp(e->d_name+L-4,".inc")))ScanFile(p);
    }
    closedir(d);
}
/* agent_git.c is the delivery contract: every "git apply" it builds must be
   a dry run, and it must not name any destructive command. */
static int ApplyOnlyChecks(const char *path)
{
    FILE *f=fopen(path,"rb");char line[4096];int ok=1,seen=0;
    if(!f)return 0;
    while(fgets(line,sizeof line,f)){
        char *p=line;
        while((p=strstr(p,"git apply"))){
            seen++;
            if(strncmp(p,"git apply --check",17)!=0)ok=0;
            p+=9;
        }
    }
    fclose(f);
    return ok&&seen>0;
}
int main(void)
{
    size_t i;int stale=0;
    printf("git destructive-command audit (src/, include/)\n");
    /* matcher self-check */
    CHECK(Destructive("run(\"git reset --hard HEAD\");"),"matcher flags reset --hard");
    CHECK(Destructive("x = \"git push --force origin\";"),"matcher flags push --force");
    CHECK(Destructive("\"git clean -fdx\""),"matcher flags clean");
    CHECK(Destructive("\"git checkout -- .\""),"matcher flags checkout");
    CHECK(!Destructive("\"git status --porcelain=v2\""),"matcher ignores status");
    CHECK(!Destructive("\"git apply --check %s\""),"matcher ignores apply --check");
    CHECK(!Allowed("src/agent_git.c","\"git reset --hard\"",0),"a new file is not allowed by default");
    CHECK(!Allowed("src/git_ops.c","\"git reset --hard HEAD~5\"",0),"a different reset in a listed file is not allowed");
    Walk("src");Walk("include");
    CHECK(scanned>=100,"scanned the source tree (>=100 files)");
    CHECK(hits>0,"the scan sees the known uses");
    CHECK(unlisted==0,"every destructive git mention is on the whitelist");
    for(i=0;i<NWL;i++)
        if(!wl_used[i]){stale=1;printf("  STALE whitelist entry: %s %s\n",WL[i].file,WL[i].fragment);}
    CHECK(!stale,"every whitelist entry still matches a line");
    CHECK(ApplyOnlyChecks("src/agent_git.c"),"agent_git.c only builds git apply --check");
    CHECK(Allowed("src/git_ops.c","\"git reset -q --hard %s\"",0),"git_ops.c reset --hard is registered as the known exception");
    printf("\n%d/%d passed\n",pass_n,run_n);
    return pass_n==run_n?0:1;
}
#endif
