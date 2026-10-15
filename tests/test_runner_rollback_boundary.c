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
#define MKDIR(p) _mkdir(p)
#define SETENV(k,v) _putenv_s(k,v)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0700)
#define SETENV(k,v) setenv(k,v,1)
#endif
void AgentRunnerTestRollbackMode(unsigned mode);
static const char *before="int Value(void) { return 1; }\n";
static const char *after="int Value(void) { return 2; }\n";
static void put(const char*p,const char*t){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(t,1,strlen(t),f)==strlen(t));assert(!fclose(f));}
static int get(const char*p,char*b,size_t n){FILE*f=fopen(p,"rb");if(!f){b[0]=0;return 0;}size_t k=fread(b,1,n-1,f);assert(!ferror(f));assert(!fclose(f));b[k]=0;return 1;}
int main(void){
 const char*outcomes[]={"refuted","rollback_failed","rollback_failed","verification_incomplete","verification_incomplete"};
 const char*rollbacks[]={"confirmed","failed","failed","unknown","unknown"};
 MKDIR("build");
 for(unsigned mode=0;mode<5;mode++){
  SWE_BENCH_RESULT results[2];char final[2][128];int exists[2];
  for(unsigned audit=0;audit<2;audit++){
   char dir[128],file[180],store[220],sub[180];snprintf(dir,sizeof(dir),"build/runner_rb_%u_%u",mode,audit);MKDIR(dir);snprintf(sub,sizeof(sub),"%s/.symbols",dir);MKDIR(sub);snprintf(file,sizeof(file),"%s/target.c",dir);snprintf(store,sizeof(store),"%s/engineering_episodes.v1",sub);remove(store);put(file,before);
   SWE_BENCH_TASK task={0};snprintf(task.task_id,sizeof(task.task_id),"RB-%u",mode);snprintf(task.issue_description,sizeof(task.issue_description),"Change Value to 2");snprintf(task.target_file,sizeof(task.target_file),"%s",file);task.target_line=1;snprintf(task.buggy_snippet,sizeof(task.buggy_snippet),"%s",before);snprintf(task.fixed_snippet,sizeof(task.fixed_snippet),"%s",after);snprintf(task.test_command,sizeof(task.test_command),"exit 1");
   SETENV("SYMBOLS_ENGINEERING_EPISODES",audit?"1":"0");AgentRunnerTestRollbackMode(mode);AGENT_RUNNER*r=AgentRunnerCreate(dir,1);assert(r);r->max_replans=(mode==1||mode==2)?2:0;assert(!AgentRunnerSolveTask(r,&task,&results[audit]));AgentRunnerDestroy(r);assert(!results[audit].is_solved&&results[audit].attempts_executed==1&&results[audit].replans_triggered==0&&results[audit].repairs_applied==0);exists[audit]=get(file,final[audit],sizeof(final[audit]));
   if(mode==4)assert(!exists[audit]);else assert(exists[audit]&&!strcmp(final[audit],(mode==1||mode==3)?after:before));
   if(audit){EPISODE_STORE*s=calloc(1,sizeof(*s));assert(s);assert(EpisodeLoad(store,s)==1&&s->count==1);assert(!strcmp(s->rows[0].outcome,outcomes[mode]));assert(!strcmp(s->rows[0].rollback,rollbacks[mode]));assert(!strcmp(s->rows[0].goal_provenance,"task_text_unverified"));if(mode==2)assert(!strcmp(s->rows[0].workspace_before,s->rows[0].workspace_after));free(s);}else{char b[4];assert(!get(store,b,sizeof(b)));}
  }
  assert(exists[0]==exists[1]&&!strcmp(final[0],final[1]));assert(results[0].attempts_executed==results[1].attempts_executed&&results[0].reflection_count==results[1].reflection_count&&results[0].total_tool_calls==results[1].total_tool_calls);
 }
 AgentRunnerTestRollbackMode(0);SETENV("SYMBOLS_ENGINEERING_EPISODES","0");puts("runner rollback boundary: 5 cases x audit off/on PASS");return 0;
}
