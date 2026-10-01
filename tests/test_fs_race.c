#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void clean(void)
{DIR *d=opendir("test_fs_race_scratch");struct dirent *e;char p[256];
 ck(d!=NULL,"list");while((e=readdir(d))){if(!strcmp(e->d_name,".")||
 !strcmp(e->d_name,".."))continue;
 snprintf(p,sizeof(p),"test_fs_race_scratch/%s",e->d_name);
 ck(unlink(p)==0,"unlink fixture");}
 ck(closedir(d)==0&&rmdir("test_fs_race_scratch")==0,"root cleanup");}
int main(void)
{FS_READ_ROOT *r;FS_READ_META m;pid_t pid[8];int status,success=0;
 ck(mkdir("test_fs_race_scratch",0700)==0,"root");
 ck(FsReadOpen("test_fs_race_scratch",&r)==FS_READ_OK,"open");
 for(int round=0;round<20;round++){
    char path[40];snprintf(path,sizeof(path),"new-%d",round);
    for(int k=0;k<8;k++){
       pid[k]=fork();ck(pid[k]>=0,"fork create");
       if(pid[k]==0){FS_READ_STATUS s=FsCreateFile(r,path,"value",5,0600);
          _exit(s==FS_READ_OK?0:s==FS_READ_DENIED?1:2);}
    }
    success=0;for(int k=0;k<8;k++){
       ck(waitpid(pid[k],&status,0)==pid[k]&&WIFEXITED(status),"wait create");
       if(WEXITSTATUS(status)==0)success++;
       else ck(WEXITSTATUS(status)==1,"loser denied");
    }
    ck(success==1,"exactly one create winner");
    {unsigned char *b;size_t n;
     ck(FsReadFile(r,path,&b,&n,&m)==FS_READ_OK&&n==5&&
        !memcmp(b,"value",5),"winner bytes");free(b);}
 }
 put("test_fs_race_scratch/source","payload");
 /* Copy and move contend for the same destination. Either winner is allowed;
    no cooperating writer clobbers an existing target. */
 pid[0]=fork();ck(pid[0]>=0,"copy fork");
 if(pid[0]==0)_exit(FsCopyFile(r,"source","shared","payload",7)==FS_READ_OK?0:1);
 pid[1]=fork();ck(pid[1]>=0,"move fork");
 if(pid[1]==0)_exit(FsMoveFile(r,"source","shared","payload",7)==FS_READ_OK?0:1);
 success=0;for(int k=0;k<2;k++){
    ck(waitpid(pid[k],&status,0)==pid[k]&&WIFEXITED(status),"wait shared");
    success+=WEXITSTATUS(status)==0;
 }
 ck(success==1,"one destination winner");
 {unsigned char *b;size_t n;
  ck(FsReadFile(r,"shared",&b,&n,&m)==FS_READ_OK&&n==7&&
     !memcmp(b,"payload",7),"shared bytes");free(b);}
 ck(FsMoveRecover(r)==FS_READ_OK&&FsCreateRecover(r)==FS_READ_OK,
    "no pending intent from race");
 ck(FsReadStat(r,".fstxn.move",&m)==FS_READ_MISSING&&
    FsReadStat(r,".fstxn.intent",&m)==FS_READ_MISSING,"no pending records");
 FsReadClose(r);clean();puts("cooperating writer races passed");return 0;
}
#else
/* Windows port of the cooperating-writer race test. Until now this main was
   empty, so test_fs_race passed vacuously on Windows.
   Ported: 8 processes contend to create the same path (exactly one winner,
   all losers FS_READ_DENIED), then a copy and a move contend for one
   destination (exactly one winner, bytes intact), then no pending records.
   Rounds are 10 instead of 20 because starting a Windows process is slow.
   BLIND ASSUMPTION, not yet observed on Windows: a create loser reports
   FS_READ_DENIED. It is inferred from the code (writers serialize on an
   exclusive LockFileEx lock, then see the existing target and return DENIED;
   every failed NtCreateFile open maps to DENIED) and from the "existing name"
   case in test_fs_create_windows. A different loser status is a finding to
   report, not something to loosen here.
   Not ported: cross-writer recovery races (Remove/Move/Replace in parallel).
   Children are re-executions of this binary, as in test_fs_win_ops. */
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ROOT "test_fs_race_scratch"
#define CHILDREN 8
#define ROUNDS 10
#define STATUS_BASE 10 /* child exit code = STATUS_BASE + FS_READ_STATUS */
#define CODE(s) ((DWORD)(STATUS_BASE+(int)(s)))
static void ck(int x,const char *m)
{if(!x){fprintf(stderr,"FAIL %s (%lu)\n",m,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void clean(void)
{WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(ROOT "\\*",&d);char p[512];
 ck(h!=INVALID_HANDLE_VALUE,"clean find");
 do{
   if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
   snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);
   if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)ck(_rmdir(p)==0,"clean directory");
   else ck(DeleteFileA(p),"clean file");
 }while(FindNextFileA(h,&d));
 ck(GetLastError()==ERROR_NO_MORE_FILES,"clean enumerate");
 FindClose(h);ck(_rmdir(ROOT)==0,"root cleanup");}
/* op 0: create <arg>; op 1: copy source->shared; op 2: move source->shared */
static void child(int op,const char *arg)
{FS_READ_ROOT *r;FS_READ_STATUS s;
 if(FsReadOpen(ROOT,&r)!=FS_READ_OK)ExitProcess(90);
 s=op==0?FsCreateFile(r,arg,"value",5,0600):
   op==1?FsCopyFile(r,"source","shared","payload",7):
         FsMoveFile(r,"source","shared","payload",7);
 FsReadClose(r);ExitProcess((UINT)(STATUS_BASE+(int)s));}
static void spawn(const char *exe,int op,const char *arg,HANDLE *out)
{char cmd[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
 snprintf(cmd,sizeof(cmd),"\"%s\" child %d %s",exe,op,arg);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn child");
 CloseHandle(pi.hThread);*out=pi.hProcess;}
static void collect(HANDLE *procs,int n,DWORD *codes)
{ck(WaitForMultipleObjects((DWORD)n,procs,TRUE,60000)==WAIT_OBJECT_0,"wait children");
 for(int k=0;k<n;k++){
   ck(GetExitCodeProcess(procs[k],&codes[k]),"child exit code");
   CloseHandle(procs[k]);
 }}
int main(int argc,char **argv)
{FS_READ_ROOT *r;FS_READ_META m;char exe[768];DWORD got;
 HANDLE procs[CHILDREN];DWORD codes[CHILDREN];
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),argv[3]);return 90;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe path");
 ck(_mkdir(ROOT)==0,"root");
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
 /* Anti-vacuity guard: a plain create must work in this scratch, otherwise
    "exactly one winner" below could never hold or would hold for the wrong
    reason. */
 ck(FsCreateFile(r,"guard","ok",2,0600)==FS_READ_OK,"guard: create works here");
 for(int round=0;round<ROUNDS;round++){
    char path[40];int success=0;
    snprintf(path,sizeof(path),"new-%d",round);
    for(int k=0;k<CHILDREN;k++)spawn(exe,0,path,&procs[k]);
    collect(procs,CHILDREN,codes);
    for(int k=0;k<CHILDREN;k++){
       if(codes[k]==CODE(FS_READ_OK))success++;
       else{
          if(codes[k]!=CODE(FS_READ_DENIED))
             fprintf(stderr,"round %d child %d unexpected exit code %lu\n",
                     round,k,(unsigned long)codes[k]);
          ck(codes[k]==CODE(FS_READ_DENIED),"loser denied");
       }
    }
    ck(success==1,"exactly one create winner");
    {unsigned char *b=NULL;size_t n=0;
     ck(FsReadFile(r,path,&b,&n,&m)==FS_READ_OK&&n==5&&
        !memcmp(b,"value",5),"winner bytes");free(b);}
 }
 put(ROOT "\\source","payload");
 /* Copy and move contend for the same destination. Either winner is allowed;
    no cooperating writer clobbers an existing target. */
 {int success=0;
  spawn(exe,1,"-",&procs[0]);spawn(exe,2,"-",&procs[1]);
  collect(procs,2,codes);
  for(int k=0;k<2;k++){
     if(codes[k]!=CODE(FS_READ_OK))
        fprintf(stderr,"shared contender %d exit code %lu\n",k,(unsigned long)codes[k]);
     success+=codes[k]==CODE(FS_READ_OK);
  }
  ck(success==1,"one destination winner");}
 {unsigned char *b=NULL;size_t n=0;
  ck(FsReadFile(r,"shared",&b,&n,&m)==FS_READ_OK&&n==7&&
     !memcmp(b,"payload",7),"shared bytes");free(b);}
 ck(FsMoveRecover(r)==FS_READ_OK&&FsCreateRecover(r)==FS_READ_OK,
    "no pending intent from race");
 ck(FsReadStat(r,".fstxn.intent",&m)==FS_READ_MISSING&&
    FsReadStat(r,".fstxn.ccommit",&m)==FS_READ_MISSING,"no pending records");
 FsReadClose(r);clean();puts("cooperating writer races passed");return 0;
}
#endif
