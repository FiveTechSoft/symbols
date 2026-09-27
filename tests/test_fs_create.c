#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_write.h"
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
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f!=NULL,"fixture");ck(fwrite(v,1,strlen(v),f)==strlen(v),"write");ck(!fclose(f),"close");}
int main(void)
{
    FS_READ_ROOT *root;FS_READ_META m;unsigned char *b;size_t n;
    ck(MKDIR("test_fs_create_scratch")==0,"root");
    ck(MKDIR("test_fs_create_scratch/inside")==0,"inside");
    put("test_fs_create_scratch/inside/existing.txt","original");
    put("test_fs_create_outside.txt","outside");
    ck(FsReadOpen("test_fs_create_scratch",&root)==FS_READ_OK,"open root");
#ifndef _WIN32
    ck(FsCreateFile(root,"inside/new.txt","new-data",8,0600)==FS_READ_OK,"create new");
    ck(FsReadFile(root,"inside/new.txt",&b,&n,&m)==FS_READ_OK&&n==8&&
       !memcmp(b,"new-data",8),"created bytes");free(b);
    ck((m.mode&0777)==0600,"mode");
    ck(FsCreateFile(root,"inside/existing.txt","wrong",5,0600)!=FS_READ_OK,"no replace");
    ck(FsReadFile(root,"inside/existing.txt",&b,&n,&m)==FS_READ_OK&&n==8&&
       !memcmp(b,"original",8),"existing bytes intact");free(b);
    ck(FsCreateFile(root,"../test_fs_create_outside.txt","wrong",5,0600)==FS_READ_INVALID,
       "traversal refused");
    ck(symlink("../../test_fs_create_outside.txt","test_fs_create_scratch/inside/link")==0,
       "symlink fixture");
    ck(FsCreateFile(root,"inside/link","wrong",5,0600)!=FS_READ_OK,"symlink leaf refused");
    ck(symlink("inside","test_fs_create_scratch/dirlink")==0,"directory link");
    ck(FsCreateFile(root,"dirlink/escape.txt","wrong",5,0600)!=FS_READ_OK,
       "symlink traversal refused");
    ck(rename("test_fs_create_scratch","test_fs_create_moved")==0,"rename held root");
    ck(MKDIR("test_fs_create_scratch")==0,"replacement root");
    ck(FsCreateFile(root,"inside/after.txt","held",4,0600)==FS_READ_OK,
       "create through held root");
    ck(FsReadFile(root,"inside/after.txt",&b,&n,&m)==FS_READ_OK&&n==4&&
       !memcmp(b,"held",4),"created in held root");free(b);
    ck(RMDIR("test_fs_create_scratch")==0,"remove replacement");
    ck(rename("test_fs_create_moved","test_fs_create_scratch")==0,"restore root");
    UNLINK("test_fs_create_scratch/inside/new.txt");
    UNLINK("test_fs_create_scratch/inside/after.txt");
    UNLINK("test_fs_create_scratch/inside/link");
    UNLINK("test_fs_create_scratch/dirlink");
#else
    ck(FsCreateFile(root,"inside/new.txt","new-data",8,0600)==FS_READ_UNSUPPORTED,
       "Windows fails closed without handle-relative publish");
    ck(FsReadStat(root,"inside/new.txt",&m)==FS_READ_MISSING,"no new file");
#endif
    FsReadClose(root);
    UNLINK("test_fs_create_scratch/inside/existing.txt");
    RMDIR("test_fs_create_scratch/inside");RMDIR("test_fs_create_scratch");
    {FILE *f=fopen("test_fs_create_outside.txt","rb");char out[8]={0};
     ck(f!=NULL&&fread(out,1,7,f)==7&&!memcmp(out,"outside",7),"outside unchanged");
     ck(!fclose(f),"close outside");}
    UNLINK("test_fs_create_outside.txt");
    puts("create-new safety tests passed");return 0;
}
