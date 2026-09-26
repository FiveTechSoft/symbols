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
#include <io.h>
#define MKDIR _mkdir
#define SETENV(k,v) _putenv_s(k,v)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(d) mkdir(d,0700)
#define SETENV(k,v) setenv(k,v,1)
#endif
static void write_text(const char *dir,const char *name,const char *text)
{
    char p[512];snprintf(p,sizeof(p),"%s/%s",dir,name);
    FILE *f=fopen(p,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));
}
static void read_text(const char *dir,const char *name,char *out,size_t cap)
{
    char p[512];snprintf(p,sizeof(p),"%s/%s",dir,name);
    FILE *f=fopen(p,"rb");assert(f);size_t n=fread(out,1,cap-1,f);assert(!ferror(f));assert(!fclose(f));out[n]=0;
}
static void remove_audit(const char *dir)
{
    char p[512];snprintf(p,sizeof(p),"%s/.symbols/engineering_episodes.v1",dir);remove(p);
    snprintf(p,sizeof(p),"%s/.symbols/engineering_episodes.v1.lock",dir);remove(p);
}
static void run_case(const char *name,const char *source,const char *task,
                     int wanted,const char *expected_outcome)
{
    char a[256],b[256],path[512];
    snprintf(a,sizeof(a),"build/shadow_%s_off",name);
    snprintf(b,sizeof(b),"build/shadow_%s_on",name);
    MKDIR(a);MKDIR(b);remove_audit(b);
    write_text(a,"main.c",source);write_text(b,"main.c",source);
    TASK_OPS_REPORT ra,rb;
    SETENV("SYMBOLS_ENGINEERING_EPISODES","0");
    int xa=TaskOpsSolve(a,task,&ra);
    SETENV("SYMBOLS_ENGINEERING_EPISODES","1");
    int xb=TaskOpsSolve(b,task,&rb);
    assert(xa==xb && xa==wanted);
    assert(!memcmp(&ra,&rb,sizeof(ra)));
    char fa[4096],fb[4096];read_text(a,"main.c",fa,sizeof(fa));read_text(b,"main.c",fb,sizeof(fb));
    assert(!strcmp(fa,fb));
    snprintf(path,sizeof(path),"%s/.symbols/engineering_episodes.v1",b);
    EPISODE_STORE *store=calloc(1,sizeof(*store));assert(store);
    assert(EpisodeLoad(path,store)==1 && store->count>=1 && store->count<=3);
    const ENGINEERING_EPISODE *last=&store->rows[store->count-1];
    assert(!strcmp(last->engine,"task_ops"));
    assert(!strcmp(last->goal_provenance,"task_text_unverified"));
    assert(strstr(last->workspace_before,"fnv64:")==last->workspace_before);
    assert(!strcmp(last->outcome,expected_outcome));
    for(size_t i=1;i<store->count;++i) assert(!strcmp(store->rows[i].parent_id,store->rows[i-1].attempt_id));
    free(store);
}
int main(void)
{
    SETENV("SYMBOLS_REFLEXION","1");SETENV("SYMBOLS_TASK_OPS_MEMORY","0");
    run_case("verified","int old_name(void){return 1;} int main(void){return old_name()-1;}\n",
             "Rename old_name to new_name.",1,"verified");
    run_case("abstain","int main(void){return 0;}\n","Do something better.",0,"abstained");
    run_case("ambiguous","int main(void){int x=1,y=1;return x+y;}\n",
             "Change x to z or y to z.",0,"abstained");
    /* A real Reflexion two-attempt path: the first candidate fails its
       named-file scope check, is rolled back, then the second is kept. */
    {
        const char *u="int max_of(const int *a, int n) { int m = a[0]; for (int i = 1; i < n; i++) if (a[i] < m) m = a[i]; return m; }\n";
        const char *o="int bonus(int k) { return k < 3 ? 8 : 0; }\n";
        const char *m="#include <stdio.h>\nint max_of(const int *a, int n);\nint bonus(int k);\n"
                      "int main(void) { int a[] = {3, 9, 1, 4}; printf(\"%d\\n\", max_of(a, 4) + bonus(3)); return 0; }\n";
        const char *t="The bug is in util.c: the program must print 9.";
        char a[256]="build/shadow_reflex_off",b[256]="build/shadow_reflex_on",path[512];
        MKDIR(a);MKDIR(b);remove_audit(b);
        write_text(a,"util.c",u);write_text(a,"other.c",o);write_text(a,"main.c",m);
        write_text(b,"util.c",u);write_text(b,"other.c",o);write_text(b,"main.c",m);
        TASK_OPS_REPORT ra,rb;
        SETENV("SYMBOLS_ENGINEERING_EPISODES","0");int xa=TaskOpsSolve(a,t,&ra);
        SETENV("SYMBOLS_ENGINEERING_EPISODES","1");int xb=TaskOpsSolve(b,t,&rb);
        assert(xa==xb && xa==1 && !memcmp(&ra,&rb,sizeof(ra)) && rb.reflections_written==1 && rb.attempts==2);
        char fa[4096],fb[4096];
        for(int i=0;i<3;++i) {
            const char *name=i==0?"util.c":i==1?"other.c":"main.c";
            read_text(a,name,fa,sizeof(fa));read_text(b,name,fb,sizeof(fb));assert(!strcmp(fa,fb));
        }
        snprintf(path,sizeof(path),"%s/.symbols/engineering_episodes.v1",b);
        EPISODE_STORE *store=calloc(1,sizeof(*store));assert(store);
        assert(EpisodeLoad(path,store)==1 && store->count==2);
        assert(!strcmp(store->rows[0].outcome,"refuted") && !strcmp(store->rows[0].rollback,"confirmed"));
        assert(!strcmp(store->rows[1].outcome,"verified") && !strcmp(store->rows[1].parent_id,"1"));
        free(store);
    }
    /* Preexisting bad image refuses append, but solve report and edit match. */
    {
        char a[256]="build/shadow_baddisk_off",b[256]="build/shadow_baddisk_on";
        MKDIR(a);MKDIR(b);remove_audit(b);
        const char *source="int old_name(void){return 1;} int main(void){return old_name()-1;}\n";
        write_text(a,"main.c",source);write_text(b,"main.c",source);
        MKDIR("build/shadow_baddisk_on/.symbols");
        write_text("build/shadow_baddisk_on/.symbols","engineering_episodes.v1","corrupt");
        TASK_OPS_REPORT ra,rb;
        SETENV("SYMBOLS_ENGINEERING_EPISODES","0");int xa=TaskOpsSolve(a,"Rename old_name to new_name.",&ra);
        SETENV("SYMBOLS_ENGINEERING_EPISODES","1");int xb=TaskOpsSolve(b,"Rename old_name to new_name.",&rb);
        assert(xa==xb && xa==1 && !memcmp(&ra,&rb,sizeof(ra)));
        char fa[4096],fb[4096];read_text(a,"main.c",fa,sizeof(fa));read_text(b,"main.c",fb,sizeof(fb));assert(!strcmp(fa,fb));
        char persisted[32];read_text("build/shadow_baddisk_on/.symbols","engineering_episodes.v1",persisted,sizeof(persisted));
        assert(!strcmp(persisted,"corrupt"));
    }
    /* Deterministic audit-lock contention refuses the append, while the
       original store, source and solve report stay unchanged. */
    {
        const char *source="int old_name(void){return 1;} int main(void){return old_name()-1;}\n";
        char a[256]="build/shadow_lock_off",b[256]="build/shadow_lock_on";
        MKDIR(a);MKDIR(b);remove_audit(b);
        write_text(a,"main.c",source);write_text(b,"main.c",source);
        MKDIR("build/shadow_lock_on/.symbols");
        write_text("build/shadow_lock_on/.symbols","engineering_episodes.v1","old bytes");
        char lock[512];snprintf(lock,sizeof(lock),"%s/.symbols/engineering_episodes.v1.lock",b);
        assert(MKDIR(lock)==0); /* deterministic lock contention, even as root */
        TASK_OPS_REPORT ra,rb;
        SETENV("SYMBOLS_ENGINEERING_EPISODES","0");int xa=TaskOpsSolve(a,"Rename old_name to new_name.",&ra);
        SETENV("SYMBOLS_ENGINEERING_EPISODES","1");int xb=TaskOpsSolve(b,"Rename old_name to new_name.",&rb);
        assert(xa==xb && xa==1 && !memcmp(&ra,&rb,sizeof(ra)));
        char fa[4096],fb[4096];read_text(a,"main.c",fa,sizeof(fa));read_text(b,"main.c",fb,sizeof(fb));assert(!strcmp(fa,fb));
        read_text("build/shadow_lock_on/.symbols","engineering_episodes.v1",fb,sizeof(fb));assert(!strcmp(fb,"old bytes"));
#ifdef _WIN32
        assert(_rmdir(lock)==0);
#else
        assert(rmdir(lock)==0);
#endif
    }
    /* Typed continuation's internal TaskOpsSolve is a scratch preflight,
       not a solve outcome: it must not create an audit file. */
    {
        char d[256]="build/shadow_continuation",q[256],audit[512];MKDIR(d);remove_audit(d);
        write_text(d,"main.c","#include <stdio.h>\nint main(void){for(int i=0;i<3;i++) printf(\"x\"); puts(\"\");}\n");
        TASK_OPS_REPORT ask,r;
        SETENV("SYMBOLS_ENGINEERING_EPISODES","0");
        assert(!TaskOpsSolve(d,"stdout-goal-missing",&ask));
        assert(TaskOpsClarification(d,"stdout-goal-missing",&ask,q,sizeof(q)));
        SETENV("SYMBOLS_ENGINEERING_EPISODES","1");
        assert(TaskOpsContinueStdout(d,"stdout-goal-missing",ask.clarification_key,"xxxx",&r));
        snprintf(audit,sizeof(audit),"%s/.symbols/engineering_episodes.v1",d);
        FILE *f=fopen(audit,"rb");assert(!f);
    }
    SETENV("SYMBOLS_ENGINEERING_EPISODES","0");
    puts("shadow behavior and provenance checks passed");return 0;
}
