#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_batch.h"
#include "fs_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void target(FS_READ_ROOT *r,const char *name,int exists)
{
    FS_READ_META m;unsigned char *b;size_t n;
    if(!exists){ck(FsReadStat(r,name,&m)==FS_READ_MISSING,"absent");return;}
    ck(FsReadFile(r,name,&b,&n,&m)==FS_READ_OK&&n==4&&
       !memcmp(b,"data",4),"published bytes");free(b);
}
static void teardown(void)
{
    DIR *d=opendir("test_fs_batch_scratch");struct dirent *e;char path[256];
    ck(d!=NULL,"list teardown");
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        snprintf(path,sizeof(path),"test_fs_batch_scratch/%s",e->d_name);
        ck(unlink(path)==0,"unlink teardown");
    }
    ck(closedir(d)==0&&rmdir("test_fs_batch_scratch")==0,"teardown");
}
int main(void)
{
    FS_READ_ROOT *r;FS_BATCH_CREATE e[2]={{"a","data",4,0600},{"b","data",4,0600}};
    FS_READ_META m;int st;
    ck(mkdir("test_fs_batch_scratch",0700)==0,"root");
    ck(FsReadOpen("test_fs_batch_scratch",&r)==FS_READ_OK,"root open");
    ck(FsBatchCreate(r,e,2)==FS_READ_OK,"normal create");
    target(r,"a",1);target(r,"b",1);
    ck(FsBatchRecover(r)==FS_READ_OK,"idempotent clean recovery");
    ck(unlink("test_fs_batch_scratch/a")==0&&
       unlink("test_fs_batch_scratch/b")==0,"reset");
    for(int step=5;step<=7;step++){
        pid_t pid;char v[4];snprintf(v,sizeof(v),"%d",step);
        pid=fork();ck(pid>=0,"fork");
        if(pid==0){setenv("FS_CREATE_TEST_CRASH",v,1);
            (void)FsBatchCreate(r,e,2);_exit(20);}
        ck(waitpid(pid,&st,0)==pid&&WIFEXITED(st)&&
           WEXITSTATUS(st)==90+step,"crash point");
        ck(FsReadStat(r,".fstxn.batch",&m)==FS_READ_OK,"batch intent");
        ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
           "single create blocked");
        ck(FsBatchCreate(r,e,2)!=FS_READ_OK,"batch blocked");
        target(r,"a",step>=6);target(r,"b",step==7);
        ck(FsBatchRecover(r)==FS_READ_OK,"recovery");
        target(r,"a",step==7);target(r,"b",step==7);
        ck(FsReadStat(r,".fstxn.batch",&m)==FS_READ_MISSING,"intent retired");
        if(step==7)ck(unlink("test_fs_batch_scratch/a")==0&&
                      unlink("test_fs_batch_scratch/b")==0,"reset committed");
    }
    /* A foreign target at a name due for rollback prevents any partial
       cleanup, including removal of an earlier matching target. */
    {pid_t pid=fork();ck(pid>=0,"foreign fork");
     if(pid==0){setenv("FS_CREATE_TEST_CRASH","6",1);
         (void)FsBatchCreate(r,e,2);_exit(20);}
     ck(waitpid(pid,&st,0)==pid&&WIFEXITED(st)&&WEXITSTATUS(st)==96,
        "foreign crash");
     {FILE *f=fopen("test_fs_batch_scratch/b","wb");
      ck(f!=NULL&&fwrite("foreign",1,7,f)==7&&fclose(f)==0,"foreign fixture");}
     ck(FsBatchRecover(r)==FS_READ_DENIED,"foreign target refused");
     target(r,"a",1);
     {unsigned char *b;size_t n;FS_READ_META meta;
      ck(FsReadFile(r,"b",&b,&n,&meta)==FS_READ_OK&&n==7&&
         !memcmp(b,"foreign",7),"foreign untouched");free(b);}
     ck(unlink("test_fs_batch_scratch/b")==0,"remove foreign fixture");
     ck(FsBatchRecover(r)==FS_READ_OK,"recover after conflict removed");
     target(r,"a",0);target(r,"b",0);
    }
    ck(FsBatchRecover(r)==FS_READ_OK,"idempotent replay");
    {pid_t pid=fork();ck(pid>=0,"marker fork");
     if(pid==0){setenv("FS_CREATE_TEST_CRASH","8",1);
         (void)FsBatchCreate(r,e,2);_exit(20);}
     ck(waitpid(pid,&st,0)==pid&&WIFEXITED(st)&&WEXITSTATUS(st)==98,
        "marker-only crash");
     ck(FsReadStat(r,".fstxn.batch",&m)==FS_READ_MISSING,
        "journal retired at marker crash");
     ck(FsReadStat(r,".fstxn.commit",&m)==FS_READ_OK,
        "commit marker survives");
     target(r,"a",1);target(r,"b",1);
     ck(FsBatchRecover(r)==FS_READ_DENIED,"marker-only fails closed");
     ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
        "marker-only blocks create");
     ck(FsBatchCreate(r,e,2)!=FS_READ_OK,"marker-only blocks batch");
     ck(unlink("test_fs_batch_scratch/.fstxn.commit")==0,
        "test-only manual marker cleanup");
    }
    /* Malformed journal is never interpreted as permission to delete. */
    {FILE *f=fopen("test_fs_batch_scratch/.fstxn.batch","wb");
     ck(f!=NULL&&fwrite("bad",1,3,f)==3&&fclose(f)==0,"malformed fixture");}
    ck(FsBatchRecover(r)==FS_READ_DENIED,"malformed batch fails closed");
    ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
       "malformed batch blocks create");
    ck(unlink("test_fs_batch_scratch/.fstxn.batch")==0,"manual fixture cleanup");
    FsReadClose(r);teardown();puts("batch create recovery passed");return 0;
}
#else
int main(void){return 0;}
#endif
