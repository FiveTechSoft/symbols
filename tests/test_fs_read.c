#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#define CHDIR(p) _chdir(p)
#define UNLINK(p) _unlink(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0700)
#define RMDIR(p) rmdir(p)
#define CHDIR(p) chdir(p)
#define UNLINK(p) unlink(p)
#endif
static void check(int pass,const char *why)
{ if(!pass){fprintf(stderr,"FAIL: %s\n",why);exit(1);} }
static void put(const char *p,const void *b,size_t n)
{ FILE *f=fopen(p,"wb");check(f!=NULL,"create fixture");check(fwrite(b,1,n,f)==n,"write fixture");check(fclose(f)==0,"close fixture"); }
int main(void)
{
    const char *base="test_fs_read_scratch";
    FS_READ_ROOT *root=NULL;FS_READ_META m;FS_READ_ENTRY *list=NULL;
    unsigned char *b=NULL;size_t n=0,count=0;
    MKDIR(base);MKDIR("test_fs_read_scratch/inside");
    put("test_fs_read_scratch/inside/lf.txt","a\nb\n",4);
    put("test_fs_read_scratch/inside/crlf.txt","a\r\nb\r\n",6);
    put("test_fs_read_scratch/inside/binary.bin","a\0b",3);
    put("test_fs_read_outside.txt","outside",7);
    check(FsReadOpen(base,&root)==FS_READ_OK,"open workspace");
    check(FsReadStat(root,"",&m)==FS_READ_OK && m.kind==FS_KIND_DIR,"stat root");
    check(FsReadStat(root,"inside/lf.txt",&m)==FS_READ_OK && m.size==4 && m.binary==-1,"stat metadata");
    check(FsReadStat(root,"inside/missing",&m)==FS_READ_MISSING,"missing leaf");
    const char *bad[]={"../test_fs_read_outside.txt","/etc/passwd","inside/../lf.txt",
        "inside//lf.txt","inside/./lf.txt","inside/lf.txt/","inside\\lf.txt",
        "C:/Windows/win.ini",NULL};
    for(int i=0;bad[i];i++)
        check(FsReadStat(root,bad[i],&m)==FS_READ_INVALID,"bad relative path refused");
    check(FsReadFile(root,"inside/lf.txt",&b,&n,&m)==FS_READ_OK && n==4 &&
          !memcmp(b,"a\nb\n",4) && m.binary==0 && m.newline==FS_NEWLINE_LF,"read LF");free(b);
    check(FsReadFile(root,"inside/crlf.txt",&b,&n,&m)==FS_READ_OK &&
          m.newline==FS_NEWLINE_CRLF,"read CRLF");free(b);
    check(FsReadFile(root,"inside/binary.bin",&b,&n,&m)==FS_READ_OK &&
          n==3 && m.binary==1,"read binary bytes");free(b);
    check(FsReadList(root,"inside",&list,&count)==FS_READ_OK && count==3 &&
          !strcmp(list[0].name,"binary.bin") && !strcmp(list[2].name,"lf.txt"),
          "sorted direct listing");FsReadFreeList(list,count);
#ifndef _WIN32
    check(symlink("../../test_fs_read_outside.txt","test_fs_read_scratch/inside/outside-link")==0,"create outside symlink");
    check(symlink("inside","test_fs_read_scratch/dir-link")==0,"create directory symlink");
    check(FsReadStat(root,"inside/outside-link",&m)!=FS_READ_OK,"outside symlink refused");
    check(FsReadStat(root,"dir-link/lf.txt",&m)!=FS_READ_OK,"directory symlink refused");
    check(FsReadList(root,"inside",&list,&count)==FS_READ_DENIED,"symlink entry blocks listing");
    UNLINK("test_fs_read_scratch/inside/outside-link");UNLINK("test_fs_read_scratch/dir-link");
#else
    { char target[MAX_PATH],cmd[2*MAX_PATH+128];DWORD len;
      len=GetFullPathNameA("test_fs_read_scratch/inside",MAX_PATH,target,NULL);
      check(len>0 && len<MAX_PATH,"resolve junction target");
      check(snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"test_fs_read_scratch\\dir-junction\" \"%s\" >NUL",target)>0,
            "format junction fixture");
      check(system(cmd)==0,"create junction fixture");
      check(FsReadStat(root,"dir-junction/lf.txt",&m)==FS_READ_DENIED,"junction traversal refused");
      check(FsReadList(root,"",&list,&count)==FS_READ_DENIED,"junction in listing refused");
      check(RMDIR("test_fs_read_scratch/dir-junction")==0,"remove junction fixture"); }
#endif
    /* The held root stays authoritative even when the old name is reused. */
    check(rename(base,"test_fs_read_moved")==0,"rename held root");
    check(MKDIR(base)==0,"replace old root pathname");
    put("test_fs_read_scratch/lf.txt","wrong",5);
    check(FsReadFile(root,"inside/lf.txt",&b,&n,&m)==FS_READ_OK && n==4 &&
          !memcmp(b,"a\nb\n",4),"held root survives path replacement");free(b);
    UNLINK("test_fs_read_scratch/lf.txt");RMDIR(base);
    check(rename("test_fs_read_moved",base)==0,"restore held root name");
    FsReadClose(root);
    UNLINK("test_fs_read_scratch/inside/lf.txt");
    UNLINK("test_fs_read_scratch/inside/crlf.txt");
    UNLINK("test_fs_read_scratch/inside/binary.bin");
    RMDIR("test_fs_read_scratch/inside");RMDIR(base);UNLINK("test_fs_read_outside.txt");
    puts("read-only workspace path and metadata tests passed");return 0;
}
