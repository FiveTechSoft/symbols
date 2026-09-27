#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
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
{ if(!yes){fprintf(stderr,"FAIL: %s\n",why);exit(1);} }
static void write_file(const char *p,const char *text)
{ FILE *f=fopen(p,"wb");ck(f!=NULL,"create fixture");ck(fwrite(text,1,strlen(text),f)==strlen(text),"write fixture");ck(!fclose(f),"close fixture"); }
static void unchanged(FS_READ_ROOT *root)
{
    FS_READ_META m;unsigned char *bytes;size_t n;FS_READ_ENTRY *e;size_t count;
    ck(FsReadFile(root,"a/old.txt",&bytes,&n,&m)==FS_READ_OK &&
       n==8&&!memcmp(bytes,"original",8),"source bytes unchanged");free(bytes);
    ck(FsReadFile(root,"a/keep.txt",&bytes,&n,&m)==FS_READ_OK &&
       n==4&&!memcmp(bytes,"keep",4),"other bytes unchanged");free(bytes);
    ck(FsReadStat(root,"a/new.txt",&m)==FS_READ_MISSING,"target not created");
    ck(FsReadList(root,"a",&e,&count)==FS_READ_OK && count==2 &&
       !strcmp(e[0].name,"keep.txt")&&!strcmp(e[1].name,"old.txt"),"tree unchanged");
    FsReadFreeList(e,count);
}
int main(void)
{
    FS_READ_ROOT *root;FS_MANIFEST m={0};FS_READ_STATUS s;FS_READ_META meta;
    ck(MKDIR("test_fs_manifest_scratch")==0,"mkdir root");
    ck(MKDIR("test_fs_manifest_scratch/a")==0,"mkdir child");
    write_file("test_fs_manifest_scratch/a/old.txt","original");
    write_file("test_fs_manifest_scratch/a/keep.txt","keep");
    write_file("test_fs_manifest_outside.txt","outside");
    ck(FsReadOpen("test_fs_manifest_scratch",&root)==FS_READ_OK,"open root");
    FS_OP_REQUEST success[]={{FS_OP_COPY,"a/old.txt","a/new.txt"}};
    s=FsManifestPlan(root,success,1,&m);
    ck(s==FS_READ_OK&&m.count==2&&m.items[0].kind==FS_EFFECT_READ&&
       !strcmp(m.items[0].path,"a/old.txt")&&m.items[1].kind==FS_EFFECT_WRITE&&
       !strcmp(m.items[1].path,"a/new.txt"),"dry-run effects");
    unchanged(root);FsManifestFree(&m);
    FS_OP_REQUEST replace[]={{FS_OP_REPLACE,NULL,"a/old.txt"}};
    ck(FsManifestPlan(root,replace,1,&m)==FS_READ_OK && m.count==2 &&
       m.items[0].kind==FS_EFFECT_READ && m.items[1].kind==FS_EFFECT_WRITE,
       "replace existing plan");unchanged(root);FsManifestFree(&m);
    FS_OP_REQUEST move[]={{FS_OP_MOVE,"a/old.txt","a/new.txt"}};
    ck(FsManifestPlan(root,move,1,&m)==FS_READ_OK && m.count==2 &&
       m.items[1].kind==FS_EFFECT_RENAME && !strcmp(m.items[1].other,"a/new.txt"),
       "move plan");unchanged(root);FsManifestFree(&m);
    FS_OP_REQUEST removal[]={{FS_OP_REMOVE,"a/old.txt",NULL}};
    ck(FsManifestPlan(root,removal,1,&m)==FS_READ_OK && m.count==2 &&
       m.items[1].kind==FS_EFFECT_DELETE,"remove plan");unchanged(root);FsManifestFree(&m);
    FS_OP_REQUEST creation[]={{FS_OP_CREATE,NULL,"a/new.txt"}};
    ck(FsManifestPlan(root,creation,1,&m)==FS_READ_OK && m.count==1 &&
       m.items[0].kind==FS_EFFECT_WRITE,"create plan");unchanged(root);FsManifestFree(&m);
    FS_OP_REQUEST reject[]={{FS_OP_CREATE,NULL,"a/new.txt"},
                            {FS_OP_REMOVE,"../test_fs_manifest_outside.txt",NULL}};
    ck(FsManifestPlan(root,reject,2,&m)==FS_READ_INVALID && !m.items && !m.count,
       "all-or-nothing malformed batch");unchanged(root);
    FS_OP_REQUEST conflicts[]={{FS_OP_CREATE,NULL,"a/new.txt"},
                               {FS_OP_COPY,"a/old.txt","a/new.txt"}};
    ck(FsManifestPlan(root,conflicts,2,&m)==FS_READ_DENIED,"overlap refused");unchanged(root);
    FS_OP_REQUEST missing[]={{FS_OP_REMOVE,"a/missing.txt",NULL}};
    ck(FsManifestPlan(root,missing,1,&m)==FS_READ_MISSING,"missing source refused");unchanged(root);
#ifndef _WIN32
    ck(symlink("../../test_fs_manifest_outside.txt","test_fs_manifest_scratch/a/link")==0,
       "symlink fixture");
    FS_OP_REQUEST link[]={{FS_OP_REMOVE,"a/link",NULL}};
    ck(FsManifestPlan(root,link,1,&m)!=FS_READ_OK,"symlink refused");
    ck(FsReadStat(root,"a/link",&meta)!=FS_READ_OK,"link remains untraversed");
    UNLINK("test_fs_manifest_scratch/a/link");
#else
    {char target[MAX_PATH],cmd[2*MAX_PATH+120];DWORD len;
     len=GetFullPathNameA("test_fs_manifest_scratch/a",MAX_PATH,target,NULL);
     ck(len>0&&len<MAX_PATH,"junction target");
     ck(snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"test_fs_manifest_scratch\\junction\" \"%s\" >NUL",target)>0,
        "junction command");
     ck(system(cmd)==0,"junction fixture");
     FS_OP_REQUEST link[]={{FS_OP_REMOVE,"junction/old.txt",NULL}};
     ck(FsManifestPlan(root,link,1,&m)!=FS_READ_OK,"junction refused");
     ck(RMDIR("test_fs_manifest_scratch/junction")==0,"remove junction");}
    FS_OP_REQUEST alias[]={{FS_OP_CREATE,NULL,"a/NEW.txt"},
                           {FS_OP_CREATE,NULL,"a/new.TXT"}};
    ck(FsManifestPlan(root,alias,2,&m)==FS_READ_DENIED,"case alias conflict");
    FS_OP_REQUEST reserved[]={{FS_OP_CREATE,NULL,"a/CON.txt"}};
    ck(FsManifestPlan(root,reserved,1,&m)==FS_READ_INVALID,"reserved device name refused");
#endif
    unchanged(root);FsReadClose(root);
    UNLINK("test_fs_manifest_scratch/a/old.txt");
    UNLINK("test_fs_manifest_scratch/a/keep.txt");
    RMDIR("test_fs_manifest_scratch/a");RMDIR("test_fs_manifest_scratch");
    UNLINK("test_fs_manifest_outside.txt");
    puts("dry-run manifest does not mutate tree");return 0;
}
