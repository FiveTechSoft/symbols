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
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
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
    ck(FsCreateFile(root,".fstxn.lock","bad",3,0600)==FS_READ_DENIED,
       "lock control name reserved");
    ck(FsCreateFile(root,".fstxn/escape","bad",3,0600)==FS_READ_DENIED,
       "journal control subtree reserved");
    ck(FsCreateFile(root,"inside/new.txt","new-data",8,0600)==FS_READ_OK,"create new");
    {int fd=open("test_fs_create_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
     struct stat st;ck(fd>=0&&fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&
       (st.st_mode&077)==0,"private lock exists");
     close(fd);}
    /* Another process holds the lock. A child creator must wait, not publish. */
    {int fd=open("test_fs_create_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
     int signal_pipe[2],status;pid_t child;char token;
     ck(fd>=0&&pipe(signal_pipe)==0&&flock(fd,LOCK_EX)==0,"hold lock");
     child=fork();ck(child>=0,"fork");
     if(child==0){FS_READ_ROOT *other;int competing;close(fd);close(signal_pipe[0]);
       competing=open("test_fs_create_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
       if(competing<0||flock(competing,LOCK_EX|LOCK_NB)!=-1||
          (errno!=EWOULDBLOCK&&errno!=EAGAIN))_exit(12);
       close(competing);token='r';(void)write(signal_pipe[1],&token,1);
       if(FsReadOpen("test_fs_create_scratch",&other)!=FS_READ_OK)_exit(10);
       if(FsCreateFile(other,"inside/serialized.txt","yes",3,0600)!=FS_READ_OK)_exit(11);
       FsReadClose(other);token='x';(void)write(signal_pipe[1],&token,1);_exit(0);
     }
     close(signal_pipe[1]);
     ck(read(signal_pipe[0],&token,1)==1&&token=='r',"child observed contention");
     ck(FsReadStat(root,"inside/serialized.txt",&m)==FS_READ_MISSING,
        "no pre-lock publication");
     ck(flock(fd,LOCK_UN)==0,"release lock");close(fd);
     alarm(10);ck(read(signal_pipe[0],&token,1)==1&&token=='x',"child committed");
     ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0,
        "serialized child successful");alarm(0);close(signal_pipe[0]);
     ck(FsReadFile(root,"inside/serialized.txt",&b,&n,&m)==FS_READ_OK&&n==3&&
        !memcmp(b,"yes",3),"serialized bytes");free(b);
    }
    {int fd=open("test_fs_create_scratch/.fstxn.lock",O_RDWR|O_NOFOLLOW);
     ck(fd>=0,"open lock");close(fd);
     ck(UNLINK("test_fs_create_scratch/.fstxn.lock")==0,"remove lock for attack");
     ck(symlink("../test_fs_create_outside.txt","test_fs_create_scratch/.fstxn.lock")==0,
        "symlink lock attack");
     ck(FsCreateFile(root,"inside/rejected.txt","no",2,0600)!=FS_READ_OK,
        "symlinked lock fails closed");
     ck(FsReadStat(root,"inside/rejected.txt",&m)==FS_READ_MISSING,"no attack publish");
     ck(UNLINK("test_fs_create_scratch/.fstxn.lock")==0,"remove attack link");
    }
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
    UNLINK("test_fs_create_scratch/inside/serialized.txt");
    UNLINK("test_fs_create_scratch/.fstxn.lock");
    UNLINK("test_fs_create_scratch/inside/after.txt");
    UNLINK("test_fs_create_scratch/inside/link");
    UNLINK("test_fs_create_scratch/dirlink");
#else
    {FS_READ_STATUS status=FsCreateFile(root,"inside/new.txt","new-data",8,0600);
     if(status!=FS_READ_OK)fprintf(stderr,"Windows create FS_READ_STATUS=%d\n",(int)status);
     ck(status==FS_READ_OK,"Windows handle-relative create");}
    ck(FsReadFile(root,"inside/new.txt",&b,&n,&m)==FS_READ_OK&&n==8&&
       !memcmp(b,"new-data",8),"Windows created bytes");free(b);
    ck((m.mode&0777)==0644,"Windows writable mode maps to 0644");
    ck(FsCreateFile(root,"inside/existing.txt","wrong",5,0600)==FS_READ_DENIED,
       "Windows existing name refused");
    ck(UNLINK("test_fs_create_scratch/inside/new.txt")==0,"Windows fixture cleanup");
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
