#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_remove.h"
#include "fs_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void target(FS_READ_ROOT *r,const char *name,int exists)
{FS_READ_META m;unsigned char *b;size_t n;
 if(!exists){ck(FsReadStat(r,name,&m)==FS_READ_MISSING,"target absent");return;}
 ck(FsReadFile(r,name,&b,&n,&m)==FS_READ_OK&&n==7&&
    !memcmp(b,"payload",7),"target intact");free(b);}
static void clean(void)
{DIR *d=opendir("test_fs_remove_scratch");struct dirent *e;char p[256];
 ck(d!=NULL,"list");while((e=readdir(d))){if(!strcmp(e->d_name,".")||
 !strcmp(e->d_name,".."))continue;
 snprintf(p,sizeof(p),"test_fs_remove_scratch/%s",e->d_name);
 ck(unlink(p)==0,"unlink fixture");}
 ck(closedir(d)==0&&rmdir("test_fs_remove_scratch")==0,"root cleanup");}
int main(void)
{FS_READ_ROOT *r;FS_READ_META m;int status;pid_t pid;
 ck(mkdir("test_fs_remove_scratch",0700)==0,"root");
 ck(FsReadOpen("test_fs_remove_scratch",&r)==FS_READ_OK,"open");
 put("test_fs_remove_scratch/normal","payload");
 ck(FsRemoveFile(r,"normal","wrong",5)==FS_READ_DENIED,"stale refuses");
 target(r,"normal",1);
 ck(FsRemoveFile(r,"normal","payload",7)==FS_READ_OK,"remove");
 target(r,"normal",0);
 ck(FsRemoveRecover(r)==FS_READ_OK,"clean recovery");
 ck(FsRemoveFile(r,"../escape","x",1)==FS_READ_INVALID,"traversal");
 for(int step=10;step<=12;step++){
    char path[32],v[4];snprintf(path,sizeof(path),"target-%d",step);
    char full[96];snprintf(full,sizeof(full),"test_fs_remove_scratch/%s",path);
    put(full,"payload");snprintf(v,sizeof(v),"%d",step);
    pid=fork();ck(pid>=0,"fork");
    if(pid==0){setenv("FS_CREATE_TEST_CRASH",v,1);
      (void)FsRemoveFile(r,path,"payload",7);_exit(20);}
    ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
       WEXITSTATUS(status)==90+step,"crash point");
    if(step==10){target(r,path,1);ck(FsRemoveRecover(r)==FS_READ_OK,"no intent");}
    else{
      ck(FsReadStat(r,".fstxn.remove",&m)==FS_READ_OK,"intent");
      ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"create blocked");
      target(r,path,step==11);
      ck(FsRemoveRecover(r)==FS_READ_OK,"rollback before commit");
      target(r,path,1);
    }
 }
 /* Committed removal and marker-only retirement boundary. */
 for(int step=13;step<=14;step++){
    char path[32],full[96],v[4];snprintf(path,sizeof(path),"commit-%d",step);
    snprintf(full,sizeof(full),"test_fs_remove_scratch/%s",path);
    put(full,"payload");snprintf(v,sizeof(v),"%d",step);
    pid=fork();ck(pid>=0,"commit fork");
    if(pid==0){setenv("FS_CREATE_TEST_CRASH",v,1);
      (void)FsRemoveFile(r,path,"payload",7);_exit(20);}
    ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
       WEXITSTATUS(status)==90+step,"commit crash");
    target(r,path,0);
    if(step==14){ck(FsRemoveRecover(r)==FS_READ_OK,"committed recovery");}
    else{
       ck(FsReadStat(r,".fstxn.remove",&m)==FS_READ_MISSING,"record retired");
       ck(FsReadStat(r,".fstxn.rcommit",&m)==FS_READ_OK,"marker remains");
       ck(FsRemoveRecover(r)==FS_READ_DENIED,"marker-only fails closed");
       ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
          "marker-only blocks writes");
       ck(unlink("test_fs_remove_scratch/.fstxn.rcommit")==0,
          "test-only marker cleanup");
    }
 }
 /* A foreign replacement after unlink must not be removed during rollback. */
 put("test_fs_remove_scratch/foreign","payload");
 pid=fork();ck(pid>=0,"foreign fork");
 if(pid==0){setenv("FS_CREATE_TEST_CRASH","12",1);
   (void)FsRemoveFile(r,"foreign","payload",7);_exit(20);}
 ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
    WEXITSTATUS(status)==102,"foreign crash");
 put("test_fs_remove_scratch/foreign","foreign");
 ck(FsRemoveRecover(r)==FS_READ_DENIED,"foreign target refused");
 {unsigned char *b;size_t n;
  ck(FsReadFile(r,"foreign",&b,&n,&m)==FS_READ_OK&&n==7&&
     !memcmp(b,"foreign",7),"foreign bytes intact");free(b);}
 ck(unlink("test_fs_remove_scratch/foreign")==0,"foreign test cleanup");
 ck(FsRemoveRecover(r)==FS_READ_OK,"recover after conflict removed");
 target(r,"foreign",1);
 /* Malformed intent blocks recovery and new writes. */
 put("test_fs_remove_scratch/.fstxn.remove","bad");
 ck(FsRemoveRecover(r)==FS_READ_DENIED,"malformed refused");
 ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"malformed blocks");
 ck(unlink("test_fs_remove_scratch/.fstxn.remove")==0,"clear malformed fixture");
 FsReadClose(r);clean();puts("remove crash recovery passed");return 0;
}
#else
int main(void){return 0;}
#endif
