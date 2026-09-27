#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_txn_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#define UNLINK(p) _unlink(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0700)
#define RMDIR(p) rmdir(p)
#define UNLINK(p) unlink(p)
#endif
static void ck(int yes,const char *why)
{if(!yes){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static void put(const char *p,const char *data)
{FILE *f=fopen(p,"wb");ck(f!=NULL,"fixture");ck(fwrite(data,1,strlen(data),f)==strlen(data),"write");ck(!fclose(f),"close");}
int main(void)
{
    FS_READ_ROOT *root;FS_TXN_PLAN p={0};FS_READ_META m;
    unsigned char *b;size_t n;
    ck(MKDIR("test_fs_txn_scratch")==0,"root");
    ck(MKDIR("test_fs_txn_scratch/a")==0,"child");
    put("test_fs_txn_scratch/a/old.txt","original");
    put("test_fs_txn_scratch/a/other.txt","other");
    ck(FsReadOpen("test_fs_txn_scratch",&root)==FS_READ_OK,"open root");
    FS_TXN_REQUEST req[]={
        {{FS_OP_REPLACE,NULL,"a/old.txt"},NULL,0,"original",8},
        {{FS_OP_COPY,"a/other.txt","a/new.txt"},"other",5,NULL,0}
    };
    ck(FsTxnPlan(root,req,2,&p)==FS_READ_OK&&p.state==FS_TXN_PLANNED&&
       p.effects.count==4&&p.count==3,"plan batch");
    ck(!strcmp(p.before[0].path,"a/old.txt")&&p.before[0].existed&&
       p.before[0].len==8&&!memcmp(p.before[0].bytes,"original",8),"original image");
    ck(!strcmp(p.before[1].path,"a/other.txt")&&p.before[1].existed&&
       p.before[1].len==5,"copy source image");
    ck(!strcmp(p.before[2].path,"a/new.txt")&&!p.before[2].existed,
       "new target absence recorded");
    FsTxnFree(&p);
    ck(!p.before&&!p.effects.items&&!p.count&&!p.state,"free clears plan");
    req[1].expected_source="wrong";
    ck(FsTxnPlan(root,req,2,&p)==FS_READ_DENIED&&!p.before&&!p.effects.items,
       "stale batch all-or-nothing");
    req[1].expected_source="other";
    FS_TXN_REQUEST unversioned[]={{{FS_OP_REPLACE,NULL,"a/old.txt"},NULL,0,NULL,0}};
    ck(FsTxnPlan(root,unversioned,1,&p)==FS_READ_INVALID,"replace requires expected image");
    FS_TXN_REQUEST duplicate[]={
        {{FS_OP_CREATE,NULL,"a/new.txt"},NULL,0,NULL,0},
        {{FS_OP_COPY,"a/other.txt","a/new.txt"},"other",5,NULL,0}
    };
    ck(FsTxnPlan(root,duplicate,2,&p)==FS_READ_DENIED,"conflicting target");
    FS_TXN_REQUEST traversal[]={{{FS_OP_REMOVE,"../outside.txt",NULL},"x",1,NULL,0}};
    ck(FsTxnPlan(root,traversal,1,&p)==FS_READ_INVALID,"traversal");
    ck(FsReadFile(root,"a/old.txt",&b,&n,&m)==FS_READ_OK&&n==8&&
       !memcmp(b,"original",8),"unchanged existing image");free(b);
    ck(FsReadStat(root,"a/new.txt",&m)==FS_READ_MISSING,"no new file");
    FsReadClose(root);
    UNLINK("test_fs_txn_scratch/a/old.txt");
    UNLINK("test_fs_txn_scratch/a/other.txt");
    RMDIR("test_fs_txn_scratch/a");RMDIR("test_fs_txn_scratch");
    puts("read-only transaction planning tests passed");return 0;
}
