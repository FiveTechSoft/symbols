#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_move.h"
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
 if(!exists){ck(FsReadStat(r,name,&m)==FS_READ_MISSING,"absent");return;}
 ck(FsReadFile(r,name,&b,&n,&m)==FS_READ_OK&&n==7&&
    !memcmp(b,"payload",7),"intact");free(b);}
static void clean(void)
{DIR *d=opendir("test_fs_move_scratch");struct dirent *e;char p[256];
 ck(d!=NULL,"list");while((e=readdir(d))){if(!strcmp(e->d_name,".")||
 !strcmp(e->d_name,".."))continue;
 snprintf(p,sizeof(p),"test_fs_move_scratch/%s",e->d_name);
 ck(unlink(p)==0,"unlink fixture");}
 ck(closedir(d)==0&&rmdir("test_fs_move_scratch")==0,"root cleanup");}
int main(void)
{FS_READ_ROOT *r;FS_READ_META m;int status;pid_t pid;
 ck(mkdir("test_fs_move_scratch",0700)==0,"root");
 ck(FsReadOpen("test_fs_move_scratch",&r)==FS_READ_OK,"open");
 put("test_fs_move_scratch/source","payload");
 ck(FsMoveFile(r,"source","target","wrong",5)==FS_READ_DENIED,"stale");
 target(r,"source",1);target(r,"target",0);
 ck(FsMoveFile(r,"source","target","payload",7)==FS_READ_OK,"move");
 target(r,"source",0);target(r,"target",1);
 ck(FsMoveRecover(r)==FS_READ_OK,"clean recovery");
 ck(FsMoveFile(r,"../escape","new","x",1)==FS_READ_INVALID,"traversal");
 for(int step=20;step<=23;step++){
    char src[32],dst[32],v[4],full[96];
    snprintf(src,sizeof(src),"src-%d",step);
    snprintf(dst,sizeof(dst),"dst-%d",step);
    snprintf(full,sizeof(full),"test_fs_move_scratch/%s",src);
    put(full,"payload");snprintf(v,sizeof(v),"%d",step);
    pid=fork();ck(pid>=0,"fork");
    if(pid==0){setenv("FS_CREATE_TEST_CRASH",v,1);
      (void)FsMoveFile(r,src,dst,"payload",7);_exit(30);}
    ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
       WEXITSTATUS(status)==90+step,"crash point");
    target(r,src,step!=23);target(r,dst,step>=22);
    if(step==20)ck(FsMoveRecover(r)==FS_READ_OK,"pre-intent noop");
    else{
      ck(FsReadStat(r,".fstxn.move",&m)==FS_READ_OK,"intent");
      ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
         "pending move blocks create");
      ck(FsMoveRecover(r)==FS_READ_OK,"rollback");
      target(r,src,1);target(r,dst,0);
    }
 }
 /* Committed move and marker-only cleanup boundary. */
 for(int step=24;step<=25;step++){
    char src[32],dst[32],v[4],full[96];
    snprintf(src,sizeof(src),"committed-src-%d",step);
    snprintf(dst,sizeof(dst),"committed-dst-%d",step);
    snprintf(full,sizeof(full),"test_fs_move_scratch/%s",src);
    put(full,"payload");snprintf(v,sizeof(v),"%d",step);
    pid=fork();ck(pid>=0,"commit fork");
    if(pid==0){setenv("FS_CREATE_TEST_CRASH",v,1);
      (void)FsMoveFile(r,src,dst,"payload",7);_exit(30);}
    ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
       WEXITSTATUS(status)==90+step,"commit crash");
    target(r,src,0);target(r,dst,1);
    if(step==25)ck(FsMoveRecover(r)==FS_READ_OK,"committed recovery");
    else{
      ck(FsReadStat(r,".fstxn.move",&m)==FS_READ_MISSING,"journal retired");
      ck(FsReadStat(r,".fstxn.mcommit",&m)==FS_READ_OK,"marker survives");
      ck(FsMoveRecover(r)==FS_READ_DENIED,"marker-only fails closed");
      ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
         "marker-only blocks writes");
      ck(unlink("test_fs_move_scratch/.fstxn.mcommit")==0,"test-only marker cleanup");
    }
 }
 /* An unrelated target at a rollback name is never removed. */
 put("test_fs_move_scratch/foreign-src","payload");
 pid=fork();ck(pid>=0,"foreign fork");
 if(pid==0){setenv("FS_CREATE_TEST_CRASH","21",1);
   (void)FsMoveFile(r,"foreign-src","foreign-dst","payload",7);_exit(30);}
 ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&
    WEXITSTATUS(status)==111,"foreign crash");
 put("test_fs_move_scratch/foreign-dst","foreign");
 ck(FsMoveRecover(r)==FS_READ_DENIED,"foreign target refused");
 target(r,"foreign-src",1);
 {unsigned char *b;size_t n;
  ck(FsReadFile(r,"foreign-dst",&b,&n,&m)==FS_READ_OK&&n==7&&
     !memcmp(b,"foreign",7),"foreign bytes intact");free(b);}
 ck(unlink("test_fs_move_scratch/foreign-dst")==0,"foreign test cleanup");
 ck(FsMoveRecover(r)==FS_READ_OK,"recover after foreign removed");
 target(r,"foreign-src",1);target(r,"foreign-dst",0);
 put("test_fs_move_scratch/.fstxn.move","bad");
 ck(FsMoveRecover(r)==FS_READ_DENIED,"malformed refused");
 ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"malformed blocks");
 ck(unlink("test_fs_move_scratch/.fstxn.move")==0,"clear malformed fixture");
 FsReadClose(r);clean();puts("move crash recovery passed");return 0;
}
#else
int main(void){return 0;}
#endif
