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
int main(void){return 0;}
#endif
