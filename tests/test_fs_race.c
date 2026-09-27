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
int main(void){return 0;}
#endif
