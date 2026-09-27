#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_replace.h"
#include "fs_write.h"
#include "fs_remove.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static void check(FS_READ_ROOT *r,const char *p,const char *v)
{unsigned char *b=NULL;size_t n=0;FS_READ_META m;
 ck(FsReadFile(r,p,&b,&n,&m)==FS_READ_OK&&n==strlen(v)&&!memcmp(b,v,n),"target content");free(b);}
static void clean(void)
{DIR *d=opendir("test_fs_replace_scratch");struct dirent *e;char p[512];
 ck(d!=NULL,"opendir");while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
 snprintf(p,sizeof(p),"test_fs_replace_scratch/%s",e->d_name);
 ck(unlink(p)==0,"unlink");}closedir(d);ck(rmdir("test_fs_replace_scratch")==0,"rmdir");}
static void crash(FS_READ_ROOT *r,const char *path,int point)
{char v[16];int status;pid_t pid;snprintf(v,sizeof(v),"%d",point);
 pid=fork();ck(pid>=0,"fork");if(!pid){setenv("FS_CREATE_TEST_CRASH",v,1);
 (void)FsReplaceFile(r,path,"old",3,"new",3);_exit(10);}
 ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==90+point,"crash point reached");}
int main(void)
{FS_READ_ROOT *r;FS_READ_META m;char p[48],full[128];
 ck(mkdir("test_fs_replace_scratch",0700)==0,"mkdir");
 ck(FsReadOpen("test_fs_replace_scratch",&r)==FS_READ_OK,"open");
 put("test_fs_replace_scratch/basic","old");
 ck(FsReplaceFile(r,"basic","stale",5,"new",3)==FS_READ_DENIED,"expected bytes");
 check(r,"basic","old");
 ck(FsReplaceFile(r,"basic","old",3,"new",3)==FS_READ_OK,"replace");
 check(r,"basic","new");ck(FsReplaceRecover(r)==FS_READ_OK,"clean recovery");
 ck(FsReplaceFile(r,"basic","new",3,"",0)==FS_READ_OK,"empty replacement");
 check(r,"basic","");
 ck(FsReplaceFile(r,"basic","",0,"new",3)==FS_READ_OK,"restore from empty");
 ck(FsReplaceFile(r,"../escape","old",3,"new",3)==FS_READ_INVALID,"traversal");
 for(int point=40;point<=53;point++){
   if(point==47||point==49||point==50||point==44||point==45||point==46)continue;
   snprintf(p,sizeof(p),"crash-%d",point);snprintf(full,sizeof(full),"test_fs_replace_scratch/%s",p);put(full,"old");
   crash(r,p,point);
   check(r,p,point>=43?"new":"old");
   if(point!=40){ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"pending blocks create");}
   ck(FsReplaceRecover(r)==FS_READ_OK,"recover");
   check(r,p,(point==51||point==53)?"new":"old");
   ck(FsReplaceRecover(r)==FS_READ_OK,"idempotent replay");
 }
 /* Generate rollback checkpoints by first crashing after publication. */
 for(int point=44;point<=55;point++){
   if(point==48||point==51||point==52||point==53)continue;
   snprintf(p,sizeof(p),"replay-%d",point);snprintf(full,sizeof(full),"test_fs_replace_scratch/%s",p);put(full,"old");
   if(point==49||point==50||point==54)crash(r,p,51);else crash(r,p,43);
   {char v[16];int status;pid_t pid;snprintf(v,sizeof(v),"%d",point);
    pid=fork();ck(pid>=0,"replay fork");if(!pid){setenv("FS_CREATE_TEST_CRASH",v,1);
      (void)FsReplaceRecover(r);_exit(10);}
    ck(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==90+point,"replay point reached");}
   {FS_READ_STATUS rs=FsReplaceRecover(r);if(rs!=FS_READ_OK)fprintf(stderr,"replay step %d status %d\n",point,rs);ck(rs==FS_READ_OK,"resume interrupted replay");}
   check(r,p,(point==49||point==50||point==54)?"new":"old");
   ck(FsReplaceRecover(r)==FS_READ_OK,"replay idempotent");
 }
 put("test_fs_replace_scratch/fsync","old");
 setenv("FS_REPLACE_TEST_FAIL_SYNC","1",1);
 ck(FsReplaceFile(r,"fsync","old",3,"new",3)==FS_READ_PENDING,"post-publish pending");
 unsetenv("FS_REPLACE_TEST_FAIL_SYNC");check(r,"fsync","new");
 ck(FsReplaceRecover(r)==FS_READ_OK,"pending rollback");check(r,"fsync","old");
 ck(FsReplaceFile(r,"fsync","old",3,"new",3)==FS_READ_OK,"replace after recovery");
 check(r,"fsync","new");
 /* A swapped foreign target may not be overwritten by rollback. */
 put("test_fs_replace_scratch/foreign","old");crash(r,"foreign",43);
 {struct stat published;ck(stat("test_fs_replace_scratch/foreign",&published)==0,"published stat");
 /* Keep its inode for the explicit fixture restoration below. */
 FILE *f=fopen("test_fs_replace_scratch/foreign-inode","wb");
 ck(f&&fwrite(&published.st_ino,1,sizeof(published.st_ino),f)==sizeof(published.st_ino)&&fclose(f)==0,"inode fixture");}
 ck(unlink("test_fs_replace_scratch/foreign")==0,"remove published name");
 put("test_fs_replace_scratch/foreign","bad");
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"foreign target blocks rollback");
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"foreign retry still blocked");
 check(r,"foreign","bad");
 ck(FsReadStat(r,".fstxn.replace",&m)==FS_READ_OK,"intent preserved");
 /* Restore the published inode from its pinned stage only in this fixture,
    then replay; a real caller cannot assume this repair is safe. */
 {
  DIR *d=opendir("test_fs_replace_scratch");struct dirent *e;char stage[512]={0};
  ino_t published_ino=0;FILE *id=fopen("test_fs_replace_scratch/foreign-inode","rb");
  ck(id&&fread(&published_ino,1,sizeof(published_ino),id)==sizeof(published_ino)&&fclose(id)==0,"inode fixture read");
  ck(unlink("test_fs_replace_scratch/foreign-inode")==0,"inode fixture cleanup");
  ck(d!=NULL,"fixture dir");while((e=readdir(d)))if(!strncmp(e->d_name,".fsrp-",6)){
   char candidate[512];struct stat st;
   snprintf(candidate,sizeof(candidate),"test_fs_replace_scratch/%s",e->d_name);
   ck(stat(candidate,&st)==0,"stage stat");
   if(st.st_ino==published_ino && st.st_size==3){FILE *f=fopen(candidate,"rb");char v[4]={0};
    ck(f&&fread(v,1,3,f)==3&&fclose(f)==0,"stage read");
    if(!memcmp(v,"new",3)){strcpy(stage,candidate);break;}
   }
  }
  closedir(d);
  ck(*stage,"new stage found");
  ck(unlink("test_fs_replace_scratch/foreign")==0&&
     link(stage,"test_fs_replace_scratch/foreign")==0,"fixture restore identity");
 }
 ck(FsReplaceRecover(r)==FS_READ_OK,"recover restored identity");
 check(r,"foreign","old");
 /* Invalid marker is never interpreted as commit or deleted. */
 put("test_fs_replace_scratch/marker","old");crash(r,"marker",43);
 put("test_fs_replace_scratch/.fstxn.pcommit","bad");
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"bad marker rejected");
 check(r,"marker","new");
 ck(FsReadStat(r,".fstxn.replace",&m)==FS_READ_OK,"bad marker keeps intent");
 ck(unlink("test_fs_replace_scratch/.fstxn.pcommit")==0,"fixture remove bad marker");
 ck(FsReplaceRecover(r)==FS_READ_OK,"recover after bad marker removed");
 check(r,"marker","old");
 FsReadClose(r);clean();puts("replace crash recovery passed");return 0;
}
#else
int main(void){return 0;}
#endif
