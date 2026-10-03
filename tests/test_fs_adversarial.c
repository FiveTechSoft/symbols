#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_batch.h"
#include "fs_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdint.h>
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void verify_outside(void)
{FILE *f=fopen("test_fs_adversarial_outside","rb");char b[8]={0};
 ck(f!=NULL&&fread(b,1,7,f)==7&&!memcmp(b,"outside",7)&&fclose(f)==0,
    "outside untouched");}
static void teardown(void)
{DIR *d=opendir("test_fs_adversarial_scratch");struct dirent *e;char p[256];
 ck(d!=NULL,"teardown open");while((e=readdir(d))){
   if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
   snprintf(p,sizeof(p),"test_fs_adversarial_scratch/%s",e->d_name);
   ck(unlink(p)==0,"teardown file");
 }
 ck(closedir(d)==0&&rmdir("test_fs_adversarial_scratch")==0&&
    unlink("test_fs_adversarial_outside")==0,"teardown");}
/* Deterministic generated malformed relative strings, including invalid
   component boundaries. The assertion is confinement, not exact error code. */
static uint32_t rnd(uint32_t *state)
{*state=*state*1664525u+1013904223u;return *state;}
int main(void)
{
 FS_READ_ROOT *root;FS_READ_META m;uint32_t seed=0x20d1854u;
 const char *invalid[]={"../test_fs_adversarial_outside","/tmp/escape",
    "./escape","x/../escape","x//escape","x/./escape","x/",
    "\\\\server\\share","C:/escape","x\\..\\escape",".fstxn.intent",
    ".fstxn.remove",".fstxn.move",".fstxn.batch",".fstxn.rcommit",
    ".fstxn.mcommit",".fstxn.lock",".fst-stage",".fsrm-stage",
    ".fsmv-stage"};
 ck(mkdir("test_fs_adversarial_scratch",0700)==0,"root");
 put("test_fs_adversarial_scratch/source","source");
 put("test_fs_adversarial_outside","outside");
 ck(FsReadOpen("test_fs_adversarial_scratch",&root)==FS_READ_OK,"open root");
 for(size_t k=0;k<sizeof(invalid)/sizeof(*invalid);k++){
   FS_BATCH_CREATE batch[2]={{"safe-new","x",1,0600},{invalid[k],"x",1,0600}};
   FS_OP_REQUEST req={FS_OP_CREATE,NULL,invalid[k]};FS_MANIFEST plan={0};
   ck(FsCreateFile(root,invalid[k],"x",1,0600)!=FS_READ_OK,"create rejected");
   ck(FsCopyFile(root,"source",invalid[k],"source",6)!=FS_READ_OK,"copy rejected");
   ck(FsMoveFile(root,"source",invalid[k],"source",6)!=FS_READ_OK,"move rejected");
   ck(FsRemoveFile(root,invalid[k],"source",6)!=FS_READ_OK,"remove rejected");
   ck(FsBatchCreate(root,batch,2)!=FS_READ_OK,"batch rejected");
   {FS_READ_STATUS manifest_status=FsManifestPlan(root,&req,1,&plan);
    /* The manifest is a read-only dry run and may allow a reserved control
       name; executable writers still must reject it. */
    ck(manifest_status!=FS_READ_OK||!strncmp(invalid[k],".fst",4)||
       !strncmp(invalid[k],".fsrm",5)||!strncmp(invalid[k],".fsmv",5),
       "manifest malformed path rejected");
   }
   FsManifestFree(&plan);
   ck(FsReadStat(root,"source",&m)==FS_READ_OK,"source retained");
   ck(FsReadStat(root,"safe-new",&m)==FS_READ_MISSING,"no partial batch");
   verify_outside();
 }
 for(int k=0;k<400;k++){
   char path[64];const char *segments[]={"..",".","","safe","\\","C:"};
   unsigned a=rnd(&seed)%6,b=rnd(&seed)%6;
   snprintf(path,sizeof(path),"%s/%s",segments[a],segments[b]);
   if(a==3&&b==3)continue; /* a valid path may lack a parent here */
   ck(FsCreateFile(root,path,"x",1,0600)!=FS_READ_OK,"generated create rejected");
   ck(FsCopyFile(root,"source",path,"source",6)!=FS_READ_OK,
      "generated copy rejected");
   ck(FsMoveFile(root,"source",path,"source",6)!=FS_READ_OK,
      "generated move rejected");
 }
 verify_outside();
 FsReadClose(root);teardown();puts("malformed paths confined");return 0;
}
#else
/* Windows port of the malformed-path confinement test. Until now this main
   was empty, so test_fs_adversarial passed vacuously on Windows.
   Ported: the 20 malformed strings across create/copy/move/remove/manifest,
   the 400 deterministic generated strings, and "outside untouched".
   Not ported, by design:
   - batch: FsBatchCreate is real on Windows (W2); its tests are
     test_fs_win_batch_create and the fuzz test. Only the one-entry
     rejection is asserted here.
   - cross-operation recovery and writer races (separate tests). */
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_batch.h"
#include "fs_manifest.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SCRATCH "test_fs_adversarial_scratch"
#define OUTSIDE "test_fs_adversarial_outside"
static void ck(int x,const char *m)
{if(!x){fprintf(stderr,"FAIL %s (%lu)\n",m,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void verify_outside(void)
{FILE *f=fopen(OUTSIDE,"rb");char b[8]={0};
 ck(f!=NULL&&fread(b,1,7,f)==7&&!memcmp(b,"outside",7)&&fclose(f)==0,
    "outside untouched");}
static void teardown(void)
{WIN32_FIND_DATAA fd;HANDLE h=FindFirstFileA(SCRATCH "\\*",&fd);char p[512];
 ck(h!=INVALID_HANDLE_VALUE,"teardown find");
 do{
   if(!strcmp(fd.cFileName,".")||!strcmp(fd.cFileName,".."))continue;
   snprintf(p,sizeof(p),SCRATCH "\\%s",fd.cFileName);
   if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
      ck(_rmdir(p)==0,"teardown directory");
   else
      ck(DeleteFileA(p),"teardown file");
 }while(FindNextFileA(h,&fd));
 ck(GetLastError()==ERROR_NO_MORE_FILES,"teardown enumerate");
 FindClose(h);
 ck(_rmdir(SCRATCH)==0&&DeleteFileA(OUTSIDE),"teardown");}
static uint32_t rnd(uint32_t *state)
{*state=*state*1664525u+1013904223u;return *state;}
int main(void)
{
 FS_READ_ROOT *root;FS_READ_META m;uint32_t seed=0x20d1854u;
 unsigned char *bytes=NULL;size_t len=0;
 const char *invalid[]={"../test_fs_adversarial_outside","/tmp/escape",
    "./escape","x/../escape","x//escape","x/./escape","x/",
    "\\\\server\\share","C:/escape","x\\..\\escape",".fstxn.intent",
    ".fstxn.remove",".fstxn.move",".fstxn.batch",".fstxn.rcommit",
    ".fstxn.mcommit",".fstxn.lock",".fst-stage",".fsrm-stage",
    ".fsmv-stage"};
 ck(_mkdir(SCRATCH)==0,"root");
 put(SCRATCH "\\source","source");
 put(OUTSIDE,"outside");
 ck(FsReadOpen(SCRATCH,&root)==FS_READ_OK,"open root");
 /* Anti-vacuity guard: if this directory could not host writes (for example
    a volume without the required file system support), every "rejected"
    check below would pass for the wrong reason. A valid create and a valid
    copy must succeed here first. */
 ck(FsCreateFile(root,"valid-new","ok",2,0600)==FS_READ_OK,
    "guard: valid create succeeds in this scratch");
 ck(FsReadFile(root,"valid-new",&bytes,&len,&m)==FS_READ_OK&&len==2&&
    !memcmp(bytes,"ok",2),"guard: valid create is readable");
 free(bytes);
 ck(FsCopyFile(root,"source","valid-copy","source",6)==FS_READ_OK,
    "guard: valid copy succeeds in this scratch");
 /* FsBatchCreate is real on Windows (W2): a single entry is invalid. */
 {FS_BATCH_CREATE one[1]={{"batch-new","x",1,0600}};
  ck(FsBatchCreate(root,one,1)==FS_READ_INVALID,
     "batch of one entry is rejected on Windows");
  ck(FsReadStat(root,"batch-new",&m)==FS_READ_MISSING,"rejected batch wrote nothing");}
 for(size_t k=0;k<sizeof(invalid)/sizeof(*invalid);k++){
   FS_OP_REQUEST req={FS_OP_CREATE,NULL,invalid[k]};FS_MANIFEST plan={0};
   ck(FsCreateFile(root,invalid[k],"x",1,0600)!=FS_READ_OK,"create rejected");
   ck(FsCopyFile(root,"source",invalid[k],"source",6)!=FS_READ_OK,"copy rejected");
   ck(FsMoveFile(root,"source",invalid[k],"source",6)!=FS_READ_OK,"move rejected");
   ck(FsRemoveFile(root,invalid[k],"source",6)!=FS_READ_OK,"remove rejected");
   {FS_READ_STATUS manifest_status=FsManifestPlan(root,&req,1,&plan);
    /* The manifest is a read-only dry run and may allow a reserved control
       name; executable writers still must reject it. */
    ck(manifest_status!=FS_READ_OK||!strncmp(invalid[k],".fst",4)||
       !strncmp(invalid[k],".fsrm",5)||!strncmp(invalid[k],".fsmv",5),
       "manifest malformed path rejected");
   }
   FsManifestFree(&plan);
   ck(FsReadStat(root,"source",&m)==FS_READ_OK,"source retained");
   verify_outside();
 }
 for(int k=0;k<400;k++){
   char path[64];const char *segments[]={"..",".","","safe","\\","C:"};
   unsigned a=rnd(&seed)%6,b=rnd(&seed)%6;
   snprintf(path,sizeof(path),"%s/%s",segments[a],segments[b]);
   if(a==3&&b==3)continue; /* a valid path may lack a parent here */
   ck(FsCreateFile(root,path,"x",1,0600)!=FS_READ_OK,"generated create rejected");
   ck(FsCopyFile(root,"source",path,"source",6)!=FS_READ_OK,
      "generated copy rejected");
   ck(FsMoveFile(root,"source",path,"source",6)!=FS_READ_OK,
      "generated move rejected");
 }
 ck(FsReadStat(root,"source",&m)==FS_READ_OK,"source retained after fuzz");
 verify_outside();
 FsReadClose(root);teardown();puts("malformed paths confined");return 0;
}
#endif
