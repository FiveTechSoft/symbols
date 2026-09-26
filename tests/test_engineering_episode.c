#ifdef NDEBUG
#undef NDEBUG
#endif
#include "engineering_episode.h"
#include "atomic_store.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define UNLINK _unlink
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define UNLINK unlink
#endif
static ENGINEERING_EPISODE make(const char *id, const char *parent)
{
    ENGINEERING_EPISODE r={0};
    strcpy(r.episode_id,id);strcpy(r.run_id,"run-A");strcpy(r.attempt_id,id);
    if(parent) strcpy(r.parent_id,parent);
    strcpy(r.repo_sha,"source-head");strcpy(r.workspace_before,"state-before");
    strcpy(r.workspace_after,"state-after");strcpy(r.task_signature,"task-digest");
    strcpy(r.goal_provenance,"test_oracle");strcpy(r.engine,"task_ops");
    strcpy(r.op,"test-op");strcpy(r.patch_hash,"patch-digest");
    strcpy(r.oracle,"build-and-run");strcpy(r.oracle_version,"v1");
    strcpy(r.diagnostic,"none");strcpy(r.outcome,"verified");
    strcpy(r.rollback,"not_needed");r.candidate_builds=2;r.probes=3;r.tool_calls=4;
    return r;
}
static size_t readfile(const char *p,char *b,size_t cap)
{
    FILE *f=fopen(p,"rb");assert(f);size_t n=fread(b,1,cap,f);assert(!ferror(f));fclose(f);return n;
}
int main(void)
{
    const char *path="build/engineering-episode-test-dir/engineering-episode-test.v1";
    UNLINK(path);UNLINK("build/engineering-episode-test-dir/engineering-episode-test.v1.tmp");
    EPISODE_STORE *s=calloc(1,sizeof(*s));assert(s);
    assert(EpisodeLoad(path,s)==0);assert(!fopen(path,"rb"));
    ENGINEERING_EPISODE a=make("first",NULL),b=make("second","first");
    assert(!EpisodeAppend(path,&b));assert(EpisodeAppend(path,&a));
    assert(EpisodeAppend(path,&b));assert(EpisodeLoad(path,s)==1 && s->count==2);
    assert(!strcmp(s->rows[1].parent_id,"first"));
    /* Invoke the exact CLI report logic in process: no shell or child can
       wait forever on a wrong Debug/Release path under Windows ASan. */
    FILE *report=tmpfile(),*err=tmpfile();assert(report && err);
    assert(EpisodeReport(path,report,err)==0);
    rewind(report);
    char summary[512]={0};assert(fgets(summary,sizeof(summary),report));
    fclose(report);fclose(err);
    assert(strstr(summary,"records=2 verified=2") && strstr(summary,"candidate_builds=4"));
    char original[8192],current[8192];size_t n=readfile(path,original,sizeof(original));
    assert(!EpisodeAppend(path,&a));
    ENGINEERING_EPISODE c=make("third","missing");assert(!EpisodeAppend(path,&c));
    c=make("third",NULL);strcpy(c.diagnostic,"bad\nline");assert(!EpisodeAppend(path,&c));
    c=make("third",NULL);memset(c.diagnostic,'x',EE_STR);assert(!EpisodeAppend(path,&c));
    assert(readfile(path,current,sizeof(current))==n && !memcmp(original,current,n));
    assert(AtomicStoreReplace(path,"SYMBOLS-ENGINEERING-EPISODES\t2\n",strlen("SYMBOLS-ENGINEERING-EPISODES\t2\n")));
    assert(EpisodeLoad(path,s)==-1);assert(!EpisodeAppend(path,&a));
    assert(AtomicStoreReplace(path,original,n));
    assert(AtomicStoreReplace(path,original,n-1));assert(EpisodeLoad(path,s)==-1);
    assert(AtomicStoreReplace(path,original,n));
    /* Reject a broken percent escape (including checksum mismatch). */
    char escaped[8192];memcpy(escaped,original,n);
    char *field=strstr(escaped,"first");assert(field);*field='%';
    assert(AtomicStoreReplace(path,escaped,n));assert(EpisodeLoad(path,s)==-1);
    assert(AtomicStoreReplace(path,original,n));
#ifndef _WIN32
    /* An unwritable temporary path must preserve prior bytes. */
    const char *tmp="build/engineering-episode-test-dir/engineering-episode-test.v1.tmp";
    assert(mkdir(tmp,0700)==0);
    c=make("third",NULL);assert(!EpisodeAppend(path,&c));
    assert(readfile(path,current,sizeof(current))==n && !memcmp(original,current,n));
    assert(rmdir(tmp)==0);
    /* A competing writer holds the whole-cycle lock, never silently wins. */
    assert(mkdir("build/engineering-episode-test-dir/engineering-episode-test.v1.lock",0700)==0);
    assert(!EpisodeAppend(path,&c));
    assert(rmdir("build/engineering-episode-test-dir/engineering-episode-test.v1.lock")==0);
    /* Two independent processes race the read/validate/replace cycle:
       contention must refuse one writer, then retry without lost rows. */
    pid_t child=fork(); assert(child>=0);
    if(child==0) {ENGINEERING_EPISODE forked=make("forked",NULL);_exit(EpisodeAppend(path,&forked)?0:1);}
    int parent_ok=EpisodeAppend(path,&c);
    int status=0;assert(waitpid(child,&status,0)==child && WIFEXITED(status));
    if(!parent_ok) assert(EpisodeAppend(path,&c));
    if(WEXITSTATUS(status)!=0) {ENGINEERING_EPISODE forked=make("forked",NULL);assert(EpisodeAppend(path,&forked));}
    assert(EpisodeLoad(path,s)==1 && s->count==4);
    /* Replace a directory target fails at rename and leaves it intact. */
    assert(mkdir("build/engineering-episode-target-dir",0700)==0);
    assert(!AtomicStoreReplace("build/engineering-episode-target-dir","x",1));
    assert(rmdir("build/engineering-episode-target-dir")==0);
#endif
    original[40]^=1;assert(AtomicStoreReplace(path,original,n));assert(EpisodeLoad(path,s)==-1);
    free(s);UNLINK(path);puts("engineering episode strict-reader tests passed");return 0;
}
