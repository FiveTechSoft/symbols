#ifdef NDEBUG
#undef NDEBUG
#endif
#include "agent_runner.h"
#include "engineering_episode.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR _mkdir
#define SETENV(k,v) _putenv_s(k,v)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(d) mkdir(d,0700)
#define SETENV(k,v) setenv(k,v,1)
#endif
static void write_text(const char *p,const char *text)
{
    FILE *f=fopen(p,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(fclose(f)==0);
}
static void read_text(const char *p,char *out,size_t cap)
{
    FILE *f=fopen(p,"rb");assert(f);size_t n=fread(out,1,cap-1,f);assert(!ferror(f));assert(fclose(f)==0);out[n]=0;
}
static void run(const char *name,int failing,int locked)
{
    char a[256],b[256],fa[320],fb[320],store_path[400],lock[420];
    snprintf(a,sizeof(a),"build/runner_%s_off",name);
    snprintf(b,sizeof(b),"build/runner_%s_on",name);
    MKDIR(a);MKDIR(b);
    snprintf(fa,sizeof(fa),"%s/target.c",a);snprintf(fb,sizeof(fb),"%s/target.c",b);
    snprintf(store_path,sizeof(store_path),"%s/.symbols/engineering_episodes.v1",b);
    snprintf(lock,sizeof(lock),"%s.lock",store_path);
    remove(store_path);
    const char *original="int Value(void) { return 1; }\n";
    write_text(fa,original);write_text(fb,original);
    if(locked) {
        char dir[320];snprintf(dir,sizeof(dir),"%s/.symbols",b);MKDIR(dir);
        write_text(store_path,"old bytes");assert(MKDIR(lock)==0);
    }
    SWE_BENCH_RESULT ra,rb;
    for(int i=0;i<2;++i) {
        SWE_BENCH_TASK task={0};
        snprintf(task.task_id,sizeof(task.task_id),"SHADOW-%s",name);
        snprintf(task.issue_description,sizeof(task.issue_description),"Change Value to 2");
        snprintf(task.target_file,sizeof(task.target_file),"%s",i?fb:fa);
        task.target_line=1;
        snprintf(task.buggy_snippet,sizeof(task.buggy_snippet),"%s",original);
        snprintf(task.fixed_snippet,sizeof(task.fixed_snippet),"int Value(void) { return 2; }\n");
        if(failing)snprintf(task.test_command,sizeof(task.test_command),"exit 1");
        SETENV("SYMBOLS_ENGINEERING_EPISODES",i?"1":"0");
        AGENT_RUNNER *runner=AgentRunnerCreate(i?b:a,1);assert(runner);
        int result=AgentRunnerSolveTask(runner,&task,i?&rb:&ra);
        assert(result==(failing?0:1));AgentRunnerDestroy(runner);
    }
    /* task_id and absolute target paths are the same shape within the two
       runs; runner reports do not embed those paths except in task metadata. */
    assert(ra.is_solved==rb.is_solved && ra.attempts_executed==rb.attempts_executed &&
           ra.replans_triggered==rb.replans_triggered && ra.reflection_count==rb.reflection_count &&
           ra.repairs_applied==rb.repairs_applied && ra.total_tool_calls==rb.total_tool_calls &&
           (failing ? (!ra.unified_diff[0] && !rb.unified_diff[0]) :
            (strstr(ra.unified_diff,"@@") && strstr(rb.unified_diff,"@@") &&
             !strcmp(strstr(ra.unified_diff,"@@"),strstr(rb.unified_diff,"@@")))));
    char x[2048],y[2048];read_text(fa,x,sizeof(x));read_text(fb,y,sizeof(y));assert(!strcmp(x,y));
    if(locked) {
        read_text(store_path,y,sizeof(y));assert(!strcmp(y,"old bytes"));
#ifdef _WIN32
        assert(_rmdir(lock)==0);
#else
        assert(rmdir(lock)==0);
#endif
    } else {
        EPISODE_STORE *s=calloc(1,sizeof(*s));assert(s);
        assert(EpisodeLoad(store_path,s)==1);
        assert(s->count==(failing?2:1));
        for(size_t i=0;i<s->count;++i) {
            const ENGINEERING_EPISODE *e=&s->rows[i];
            assert(!strcmp(e->engine,"agent_runner") && !strcmp(e->goal_provenance,"task_text_unverified"));
            assert(!strncmp(e->workspace_before,"fnv64:",6) && !strncmp(e->workspace_after,"fnv64:",6));
            if(failing)assert(!strcmp(e->workspace_before,e->workspace_after));
            else assert(strcmp(e->workspace_before,e->workspace_after));
            if(failing)assert(!strcmp(e->outcome,"refuted") && !strcmp(e->rollback,"confirmed"));
            else assert(!strcmp(e->outcome,"verified") && !strcmp(e->rollback,"not_needed"));
            if(i)assert(!strcmp(e->parent_id,"1"));
        }
        free(s);
    }
}
int main(void)
{
    run("success",0,0);run("refuted",1,0);run("locked",0,1);
    SETENV("SYMBOLS_ENGINEERING_EPISODES","0");
    puts("runner shadow checks passed");return 0;
}
