#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
static void ck(int yes,const char *m){if(!yes){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void put(const char *p,const char *b)
{FILE *f=fopen(p,"wb");ck(f!=NULL&&fwrite(b,1,strlen(b),f)==strlen(b)&&fclose(f)==0,"fixture");}
int main(void)
{
    FS_READ_ROOT *r;unsigned char *b;size_t n;FS_READ_META m;int fd,pipes[2],st;pid_t child;char token;
    ck(mkdir("test_fs_copy_scratch",0700)==0,"root");
    ck(mkdir("test_fs_copy_scratch/sub",0700)==0,"child");
    put("test_fs_copy_scratch/sub/src","source");
    ck(FsReadOpen("test_fs_copy_scratch",&r)==FS_READ_OK,"open");
    ck(FsCopyFile(r,"sub/src","sub/out","source",6)==FS_READ_OK,"copy");
    ck(FsReadFile(r,"sub/out",&b,&n,&m)==FS_READ_OK&&n==6&&
       !memcmp(b,"source",6),"copied bytes");free(b);
    ck(FsCopyFile(r,"sub/src","sub/out","source",6)==FS_READ_DENIED,
       "no replace");
    ck(FsCopyFile(r,"sub/src","sub/stale","wrong",5)==FS_READ_DENIED,
       "expected source mismatch");
    ck(FsReadStat(r,"sub/stale",&m)==FS_READ_MISSING,"no stale target");
    ck(FsCopyFile(r,"../escape","sub/escape","source",6)==FS_READ_INVALID,
       "traversal");
    ck(symlink("src","test_fs_copy_scratch/sub/link")==0,"symlink");
    ck(FsCopyFile(r,"sub/link","sub/escape","source",6)!=FS_READ_OK,
       "source symlink refused");
    ck(FsCopyFile(r,"sub/src",".fstxn.intent","source",6)==FS_READ_DENIED,
       "journal name reserved");
    fd=open("test_fs_copy_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
    ck(fd>=0&&pipe(pipes)==0&&flock(fd,LOCK_EX)==0,"hold lock");
    child=fork();ck(child>=0,"fork");
    if(child==0){FS_READ_ROOT *other;int contender;
        close(fd);close(pipes[0]);
        contender=open("test_fs_copy_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
        ck(contender>=0&&flock(contender,LOCK_EX|LOCK_NB)<0,"contention");
        close(contender);token='r';(void)write(pipes[1],&token,1);
        if(FsReadOpen("test_fs_copy_scratch",&other)!=FS_READ_OK)_exit(11);
        if(FsCopyFile(other,"sub/src","sub/waited","source",6)!=FS_READ_OK)_exit(12);
        FsReadClose(other);token='x';(void)write(pipes[1],&token,1);_exit(0);
    }
    close(pipes[1]);ck(read(pipes[0],&token,1)==1&&token=='r',"child ready");
    ck(FsReadStat(r,"sub/waited",&m)==FS_READ_MISSING,"not published before lock");
    ck(flock(fd,LOCK_UN)==0,"unlock");close(fd);
    alarm(10);ck(read(pipes[0],&token,1)==1&&token=='x',"child done");
    ck(waitpid(child,&st,0)==child&&WIFEXITED(st)&&WEXITSTATUS(st)==0,
       "child success");alarm(0);close(pipes[0]);
    ck(FsReadFile(r,"sub/waited",&b,&n,&m)==FS_READ_OK&&n==6&&
       !memcmp(b,"source",6),"serialized copy");free(b);
    FsReadClose(r);
    ck(unlink("test_fs_copy_scratch/sub/src")==0&&
       unlink("test_fs_copy_scratch/sub/out")==0&&
       unlink("test_fs_copy_scratch/sub/waited")==0&&
       unlink("test_fs_copy_scratch/sub/link")==0&&
       unlink("test_fs_copy_scratch/.fstxn.lock")==0&&
       rmdir("test_fs_copy_scratch/sub")==0&&
       rmdir("test_fs_copy_scratch")==0,"teardown");
    puts("copy safety passed");return 0;
}
#else
int main(void){return 0;}
#endif
