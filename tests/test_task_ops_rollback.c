/* Test-only fault seam compiles a separate symbolic archive. Production never
   exports these hooks or reads fault settings from the environment. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "task_ops.h"
#include "engineering_episode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define SETENV(k,v) _putenv_s(k,v)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0700)
#define SETENV(k,v) setenv(k,v,1)
#endif

static void put(const char *dir,const char *name,const char *text)
{
    char p[512]; snprintf(p,sizeof(p),"%s/%s",dir,name);
    FILE *f=fopen(p,"wb"); assert(f);
    assert(fwrite(text,1,strlen(text),f)==strlen(text)); assert(!fclose(f));
}
static int get(const char *dir,const char *name,char *out,size_t cap)
{
    char p[512]; snprintf(p,sizeof(p),"%s/%s",dir,name);
    FILE *f=fopen(p,"rb"); if(!f) {out[0]=0;return 0;}
    size_t n=fread(out,1,cap-1,f);assert(!ferror(f));assert(!fclose(f));out[n]=0;return 1;
}
static void setup(const char *dir,const char *source,const char *header)
{
    char p[512];MKDIR("build");MKDIR(dir);
    snprintf(p,sizeof(p),"%s/.symbols",dir);MKDIR(p);
    snprintf(p,sizeof(p),"%s/.symbols/engineering_episodes.v1",dir);remove(p);
    snprintf(p,sizeof(p),"%s/.symbols/engineering_episodes.v1.lock",dir);remove(p);
    snprintf(p,sizeof(p),"%s/t2.c",dir);remove(p);
    put(dir,"m.c",source);
    if(header)put(dir,"m.h",header);
}
static void episode(const char *dir,const char *outcome,const char *rollback)
{
    char p[512];snprintf(p,sizeof(p),"%s/.symbols/engineering_episodes.v1",dir);
    EPISODE_STORE *s=calloc(1,sizeof(*s));assert(s);
    assert(EpisodeLoad(p,s)==1 && s->count==1);
    assert(!strcmp(s->rows[0].outcome,outcome));
    assert(!strcmp(s->rows[0].rollback,rollback));
    free(s);
}
int main(void)
{
    const char *before="int old_name(void){return 1;} int main(void){return old_name()-1;}\n";
    const char *task="In missing.c, rename old_name to new_name.";
    char buf[4096];TASK_OPS_REPORT base,success,fail,lying;
    SETENV("SYMBOLS_REFLEXION","0");SETENV("SYMBOLS_TASK_OPS_MEMORY","0");
    SETENV("SYMBOLS_ENGINEERING_EPISODES","1");
    setup("build/rb_restore_base",before,NULL);
    TaskOpsTestFailRestore(0,0,0);
    assert(!TaskOpsSolve("build/rb_restore_base",task,&base));
    assert(base.applied && !base.verified && !base.rollback_failed &&
           !strncmp(base.reason,"verify failed",13));
    assert(get("build/rb_restore_base","m.c",buf,sizeof(buf)) && !strcmp(buf,before));
    episode("build/rb_restore_base","refuted","confirmed");

    setup("build/rb_restore_fail",before,NULL);
    TaskOpsTestFailRestore(1,0,0);
    assert(!TaskOpsSolve("build/rb_restore_fail",task,&fail));
    assert(fail.applied && !fail.verified && fail.rollback_failed &&
           !strcmp(fail.reason,"rollback failed; inspect workspace files"));
    assert(get("build/rb_restore_fail","m.c",buf,sizeof(buf)) && strcmp(buf,before));
    episode("build/rb_restore_fail","rollback_failed","failed");

    /* A failed return must win over an apparently clean byte readback. */
    setup("build/rb_restore_lie",before,NULL);
    TaskOpsTestFailRestore(1,0,1);
    assert(!TaskOpsSolve("build/rb_restore_lie",task,&lying));
    assert(lying.rollback_failed && !strcmp(lying.reason,"rollback failed; inspect workspace files"));
    assert(get("build/rb_restore_lie","m.c",buf,sizeof(buf)) && !strcmp(buf,before));
    episode("build/rb_restore_lie","rollback_failed","failed");

    const char *src="int twice(int v) { return 2 * v; }\n";
    const char *hdr="int twice(int v);\n";
    const char *test="Add t2.c asserting twice(2) == 5.";
    setup("build/rb_remove_base",src,hdr);
    TaskOpsTestFailRestore(0,0,0);
    assert(!TaskOpsSolve("build/rb_remove_base",test,&success));
    assert(success.applied && !success.verified && !success.rollback_failed);
    assert(!get("build/rb_remove_base","t2.c",buf,sizeof(buf)));
    episode("build/rb_remove_base","refuted","confirmed");

    setup("build/rb_remove_fail",src,hdr);
    TaskOpsTestFailRestore(0,1,0);
    assert(!TaskOpsSolve("build/rb_remove_fail",test,&fail));
    assert(fail.applied && !fail.verified && fail.rollback_failed &&
           !strcmp(fail.reason,"rollback failed; inspect workspace files"));
    assert(get("build/rb_remove_fail","t2.c",buf,sizeof(buf)));
    episode("build/rb_remove_fail","rollback_failed","failed");
    TaskOpsTestFailRestore(0,0,0);
    puts("rollback restore/remove success and injected failure checked");
    return 0;
}
