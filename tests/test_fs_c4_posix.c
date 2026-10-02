#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_manifest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
/* M1 criterion 4 on POSIX: case, permissions, path length, newlines and a
   foreign lock holder. Every assertion pins behaviour measured on Linux; it
   is not a claim about other filesystems or about Windows. The workspace
   lock is cooperative: only the order of events is asserted, never a
   duration. Compiled out on Windows, where criterion 4 is not shown. */
#define SCRATCH "test_fs_c4_posix_scratch"
static const char *g_what="setup";
static void ck(int x,const char *m)
{
    if(!x){fprintf(stderr,"FAIL %s (%s)\n",m,g_what);exit(1);}
}
static FS_READ_ROOT *R;
static void rec(void)
{
    (void)FsCreateRecover(R);(void)FsReplaceRecover(R);(void)FsRemoveRecover(R);
    (void)FsMoveRecover(R);(void)FsBatchRecover(R);
}
static void fresh(void)
{
    int rc;
    if(R){FsReadClose(R);R=NULL;}
    rc=system("chmod -R u+rwx " SCRATCH " 2>/dev/null; rm -rf " SCRATCH);(void)rc;
    ck(mkdir(SCRATCH,0700)==0,"mkdir scratch");
    ck(FsReadOpen(SCRATCH,&R)==FS_READ_OK,"open root");
}
static int mk(const char *rel,const void *b,size_t n,unsigned mode)
{return FsCreateFile(R,rel,b,n,mode)==FS_READ_OK;}
static int same(const char *rel,const void *b,size_t n)
{
    unsigned char *got=NULL;size_t len=0;FS_READ_META m;int ok;
    if(FsReadFile(R,rel,&got,&len,&m)!=FS_READ_OK)return 0;
    ok=len==n&&(n==0||memcmp(got,b,n)==0);free(got);return ok;
}
static int cmpname(const void *a,const void *b){return strcmp(*(char*const*)a,*(char*const*)b);}
/* Sorted names in a directory (OS view), without the workspace lock file. */
static void names(const char *dir,char *out,size_t cap)
{
    DIR *d=opendir(dir);struct dirent *e;char *v[64];size_t n=0,i;
    out[0]=0;ck(d!=NULL,"opendir");
    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||!strcmp(e->d_name,".fstxn.lock"))continue;
        ck(n<64,"too many");v[n++]=strdup(e->d_name);
    }
    closedir(d);qsort(v,n,sizeof(*v),cmpname);
    for(i=0;i<n;i++){
        ck(strlen(out)+strlen(v[i])+2<cap,"names cap");
        if(i)strcat(out,",");
        strcat(out,v[i]);free(v[i]);
    }
}
static int namesis(const char *dir,const char *want)
{char b[512];names(dir,b,sizeof(b));if(strcmp(b,want)){fprintf(stderr,"  names(%s)=[%s] want [%s]\n",dir,b,want);return 0;}return 1;}

/* a) Linux is case-sensitive: the names are distinct objects. */
static void t_case(void)
{
    FS_OP_REQUEST rq[2]={{FS_OP_CREATE,NULL,"New.txt"},{FS_OP_CREATE,NULL,"new.txt"}};
    FS_MANIFEST mf;FS_READ_ENTRY *e;size_t n;
    g_what="case";fresh();
    ck(mk("Safe.txt","upper",5,0644),"create Safe");
    ck(mk("safe.txt","lower",5,0644),"create safe (distinct name)");
    ck(mk("SAFE.TXT","caps",4,0644),"create SAFE");
    ck(same("Safe.txt","upper",5)&&same("safe.txt","lower",5)&&same("SAFE.TXT","caps",4),"three objects");
    ck(FsCreateFile(R,"safe.txt","x",1,0644)!=FS_READ_OK,"exact duplicate refused");
    ck(FsReplaceFile(R,"Safe.txt","upper",5,"UPPER",5)==FS_READ_OK,"replace one case");
    ck(same("safe.txt","lower",5)&&same("Safe.txt","UPPER",5),"other case untouched");
    {size_t i,seen=0;
     ck(FsReadList(R,"",&e,&n)==FS_READ_OK,"list");
     for(i=0;i<n;i++)if(!strcasecmp(e[i].name,"safe.txt"))seen++;
     ck(seen==3,"list shows 3 case-distinct names");FsReadFreeList(e,n);}
    ck(FsManifestPlan(R,rq,2,&mf)==FS_READ_OK&&mf.count>=2,"manifest keeps case-distinct creates");
    FsManifestFree(&mf);
    ck(namesis(SCRATCH,"SAFE.TXT,Safe.txt,safe.txt"),"no leftovers");
}

/* b) permissions. */
static void ro_snapshot_ok(void)
{
    ck(namesis(SCRATCH "/ro","f,g"),"ro dir names unchanged");
    ck(same("ro/f","FFF",3)&&same("ro/g","GGG",3),"ro dir bytes unchanged");
}
/* needs_rec=0: the writer rolls back in process, so the root is clean at once
   and no recovery call is needed. needs_rec=1: still pins the older contract
   (pending state until the explicit recovery call). */
static void ro_case(const char *label,int status,int needs_rec)
{
    g_what=label;
    ck(status!=FS_READ_OK,"op in read-only dir must fail");
    (void)chmod(SCRATCH "/ro",0500);
    ro_snapshot_ok();
    if(!needs_rec){
        ck(namesis(SCRATCH,"a,ro"),"root clean with no recovery call");
        ck(FsCreateFile(R,"next","N",1,0644)==FS_READ_OK,"next writer not blocked");
        ck(FsRemoveFile(R,"next","N",1)==FS_READ_OK,"next remove not blocked");
    }
    rec();
    /* After explicit recovery the workspace root holds nothing but its own files. */
    ck(namesis(SCRATCH,"a,ro"),"root clean after recovery");
    ro_snapshot_ok();
}
static void t_perms(void)
{
    FS_BATCH_CREATE bc[2]={{"ro/b1","1",1,0644},{"ro/b2","2",1,0644}};
    FS_BATCH_REPLACE br[2]={{"ro/f","FFF",3,"XXX",3},{"ro/g","GGG",3,"YYY",3}};
    FS_READ_META m;unsigned char *buf;size_t len;
    g_what="perms";
    if(geteuid()==0){printf("perms: SKIP (euid 0 bypasses mode bits)\n");return;}
    fresh();
    ck(mk("a","AAA",3,0644),"fixture a");
    ck(mkdir(SCRATCH "/ro",0700)==0,"mkdir ro");
    ck(mk("ro/f","FFF",3,0644)&&mk("ro/g","GGG",3,0644),"ro fixtures");
    ck(chmod(SCRATCH "/ro",0500)==0,"ro chmod");
    ro_case("ro create",FsCreateFile(R,"ro/n","N",1,0644),0);
    ro_case("ro replace",FsReplaceFile(R,"ro/f","FFF",3,"XXX",3),1);
    ro_case("ro remove",FsRemoveFile(R,"ro/f","FFF",3),0);
    ro_case("ro move out",FsMoveFile(R,"ro/f","mv","FFF",3),0);
    ro_case("move into ro",FsMoveFile(R,"a","ro/mv","AAA",3),0);
    ro_case("copy into ro",FsCopyFile(R,"a","ro/cp","AAA",3),0);
    ro_case("batch create in ro",FsBatchCreate(R,bc,2),1);
    ro_case("batch replace in ro",FsBatchReplace(R,br,2),1);
    ck(same("a","AAA",3),"a untouched");
    /* The block is the mode, not a stuck state: with write access back, work resumes. */
    g_what="ro resume";
    ck(chmod(SCRATCH "/ro",0700)==0,"ro chmod back");
    ck(FsReplaceFile(R,"ro/f","FFF",3,"XXX",3)==FS_READ_OK&&same("ro/f","XXX",3),"replace after chmod");

    /* A 0444 file is protected by its directory, not by its own bits: replace
       and remove succeed on POSIX, and replace keeps the 0444 mode. */
    g_what="0444";fresh();
    ck(mk("p","PPP",3,0444),"0444 fixture");
    ck(FsReplaceFile(R,"p","PPP",3,"QQQ",3)==FS_READ_OK&&same("p","QQQ",3),"0444 replace");
    ck(FsReadStat(R,"p",&m)==FS_READ_OK&&m.mode==0444,"0444 mode kept");
    ck(FsCopyFile(R,"p","q","QQQ",3)==FS_READ_OK&&same("q","QQQ",3),"0444 copy source");
    ck(FsMoveFile(R,"p","p2","QQQ",3)==FS_READ_OK&&same("p2","QQQ",3),"0444 move");
    ck(FsRemoveFile(R,"p2","QQQ",3)==FS_READ_OK,"0444 remove");

    /* A 0000 file cannot be read, so no operation that must verify its bytes may proceed. */
    g_what="0000";fresh();
    ck(mk("z","ZZZ",3,0),"0000 fixture");
    ck(FsReadFile(R,"z",&buf,&len,&m)==FS_READ_DENIED,"0000 read denied");
    ck(FsReplaceFile(R,"z","ZZZ",3,"YYY",3)==FS_READ_DENIED,"0000 replace denied");rec();
    ck(FsCopyFile(R,"z","z2","ZZZ",3)==FS_READ_DENIED,"0000 copy denied");rec();
    ck(FsRemoveFile(R,"z","ZZZ",3)==FS_READ_DENIED,"0000 remove denied");rec();
    ck(namesis(SCRATCH,"z"),"0000 tree unchanged");
    ck(chmod(SCRATCH "/z",0600)==0&&same("z","ZZZ",3),"0000 bytes unchanged");

    /* A directory without x is closed for stat and create. */
    g_what="no-x dir";fresh();
    ck(mkdir(SCRATCH "/nx",0700)==0&&mk("nx/f","F",1,0644),"nx fixture");
    ck(chmod(SCRATCH "/nx",0600)==0,"nx chmod");
    ck(FsReadStat(R,"nx/f",&m)==FS_READ_DENIED,"nx stat denied");
    ck(FsCreateFile(R,"nx/g","G",1,0644)==FS_READ_DENIED,"nx create denied");rec();
    ck(FsReplaceFile(R,"nx/f","F",1,"H",1)!=FS_READ_OK,"nx replace refused");rec();
    ck(chmod(SCRATCH "/nx",0700)==0&&namesis(SCRATCH "/nx","f")&&same("nx/f","F",1),"nx unchanged");
}

/* c) path length: one component up to 255 bytes, whole path up to 1023. */
static void deep_path(char *path,int total)
{
    int len=0,rest;
    while(total-len>200){memset(path+len,'d',199);path[len+199]='/';len+=200;}
    rest=total-len;memset(path+len,'e',rest);path[total]=0;
}
static void t_long(void)
{
    char c[300],path[1300],dir[1500];int L,total;char *s;
    g_what="long";fresh();
    for(L=254;L<=255;L++){
        memset(c,'a',L);c[L]=0;
        ck(mk(c,"x",1,0644)&&same(c,"x",1),"component create");
        ck(FsReplaceFile(R,c,"x",1,"y",1)==FS_READ_OK&&same(c,"y",1),"component replace");
        ck(FsRemoveFile(R,c,"y",1)==FS_READ_OK,"component remove");
    }
    for(L=256;L<=257;L++){
        memset(c,'a',L);c[L]=0;
        ck(FsCreateFile(R,c,"x",1,0644)!=FS_READ_OK,"overlong component refused");rec();
    }
    ck(namesis(SCRATCH,""),"component cases leave nothing");
    for(total=1022;total<=1023;total++){
        deep_path(path,total);
        snprintf(dir,sizeof(dir),"%s/%s",SCRATCH,path);s=strrchr(dir,'/');*s=0;
        {char cmd[1700];int rc;snprintf(cmd,sizeof(cmd),"mkdir -p %s",dir);rc=system(cmd);ck(rc==0,"mkdir -p deep");}
        ck(mk(path,"x",1,0644)&&same(path,"x",1),"deep create");
        ck(FsReplaceFile(R,path,"x",1,"y",1)==FS_READ_OK,"deep replace");
        ck(FsRemoveFile(R,path,"y",1)==FS_READ_OK,"deep remove");
    }
    for(total=1024;total<=1025;total++){
        deep_path(path,total);
        ck(FsCreateFile(R,path,"x",1,0644)==FS_READ_INVALID,"path >=1024 invalid");
    }
}

/* d) newline kinds and NUL bytes survive every write op byte for byte. */
typedef struct {const char *name;const char *b;size_t n;FS_READ_NEWLINE nl;int bin;} PAY;
static const PAY P[5]={
    {"crlf","a\r\nb\r\n",6,FS_NEWLINE_CRLF,0},
    {"lf","a\nb\n",4,FS_NEWLINE_LF,0},
    {"mixed","a\r\nb\nc\r\n",8,FS_NEWLINE_MIXED,0},
    {"none","no newline at end",17,FS_NEWLINE_NONE,0},
    {"nul","bin\0\r\n\0x\n",9,0,1}};
static void t_newline(void)
{
    int i,j;FS_READ_META m;unsigned char *b;size_t n;
    g_what="newline";fresh();
    for(i=0;i<5;i++){
        char f[32],g[32];snprintf(f,sizeof(f),"f_%s",P[i].name);snprintf(g,sizeof(g),"g_%s",P[i].name);
        ck(mk(f,P[i].b,P[i].n,0644)&&same(f,P[i].b,P[i].n),"create bytes");
        ck(FsReadFile(R,f,&b,&n,&m)==FS_READ_OK,"read");free(b);
        if(P[i].bin)ck(m.binary==1,"NUL file flagged binary");
        else ck(m.newline==P[i].nl&&m.binary==0,"newline kind detected");
        ck(FsCopyFile(R,f,g,P[i].b,P[i].n)==FS_READ_OK&&same(g,P[i].b,P[i].n),"copy bytes");
        j=(i+1)%5;
        ck(FsReplaceFile(R,g,P[i].b,P[i].n,P[j].b,P[j].n)==FS_READ_OK&&same(g,P[j].b,P[j].n),"replace bytes");
        ck(FsMoveFile(R,g,"mv",P[j].b,P[j].n)==FS_READ_OK&&same("mv",P[j].b,P[j].n),"move bytes");
        ck(FsRemoveFile(R,"mv",P[j].b,P[j].n)==FS_READ_OK,"remove expected exact");
    }
    {
        FS_BATCH_REPLACE br[2]={{"f_crlf",P[0].b,P[0].n,P[4].b,P[4].n},{"f_nul",P[4].b,P[4].n,P[0].b,P[0].n}};
        ck(FsBatchReplace(R,br,2)==FS_READ_OK,"batch replace");
        ck(same("f_crlf",P[4].b,P[4].n)&&same("f_nul",P[0].b,P[0].n),"batch replace bytes");
    }
    /* A near-miss expected image (LF for CRLF) must not be accepted. */
    ck(FsReplaceFile(R,"f_lf","a\r\nb\n",5,"x",1)!=FS_READ_OK,"expected CRLF/LF mismatch refused");rec();
    ck(same("f_lf",P[1].b,P[1].n),"mismatch left bytes");
}

/* e) a foreign holder of the workspace lock. Order only, never milliseconds. */
static pid_t child_hold(const char *file,int sleep_us,int *ready,int *released)
{
    int a[2],b[2];pid_t p;
    ck(pipe(a)==0&&pipe(b)==0,"pipe");
    p=fork();ck(p>=0,"fork");
    if(p==0){
        int fd=open(file,O_RDWR);char c='L';
        close(a[0]);close(b[0]);
        if(fd<0||flock(fd,LOCK_EX)<0)_exit(2);
        if(write(a[1],&c,1)!=1)_exit(3);
        {struct timespec ts;ts.tv_sec=sleep_us/1000000;ts.tv_nsec=(long)(sleep_us%1000000)*1000L;nanosleep(&ts,NULL);}
        c='R';if(write(b[1],&c,1)!=1)_exit(4);
        flock(fd,LOCK_UN);close(fd);_exit(0);
    }
    close(a[1]);close(b[1]);
    {char c=0;ck(read(a[0],&c,1)==1&&c=='L',"child holds lock");}
    close(a[0]);*ready=1;*released=b[0];
    return p;
}
static int released_before_return(int fd)
{
    char c=0;int fl=fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,fl|O_NONBLOCK);
    return read(fd,&c,1)==1&&c=='R';
}
static void reap(pid_t p,int rfd)
{int st=0;ck(waitpid(p,&st,0)==p&&WIFEXITED(st)&&WEXITSTATUS(st)==0,"child exit 0");close(rfd);}
static void t_lock(void)
{
    int ready=0,rfd;pid_t p;struct stat st;
    g_what="lock";fresh();
    ck(mk("t","T0",2,0644),"fixture (creates the lock file)");
    ck(stat(SCRATCH "/.fstxn.lock",&st)==0&&(st.st_mode&0777)==0600,"lock is 0600");
    p=child_hold(SCRATCH "/.fstxn.lock",300000,&ready,&rfd);
    ck(FsCreateFile(R,"blocked_create","B",1,0644)==FS_READ_OK,"create completes after release");
    ck(released_before_return(rfd),"create returned only after the holder released");
    reap(p,rfd);
    p=child_hold(SCRATCH "/.fstxn.lock",300000,&ready,&rfd);
    ck(FsReplaceFile(R,"t","T0",2,"T1",2)==FS_READ_OK,"replace completes after release");
    ck(released_before_return(rfd),"replace returned only after the holder released");
    reap(p,rfd);
    ck(same("t","T1",2)&&same("blocked_create","B",1),"both writes landed exactly once");
    ck(namesis(SCRATCH,"blocked_create,t"),"no leftovers");
    /* A lock file with group/other bits is not trusted. */
    g_what="lock mode";
    ck(chmod(SCRATCH "/.fstxn.lock",0644)==0,"lock chmod 0644");
    ck(FsCreateFile(R,"u","U",1,0644)==FS_READ_DENIED,"create denied on unsafe lock");
    ck(FsReplaceFile(R,"t","T1",2,"T2",2)==FS_READ_DENIED,"replace denied on unsafe lock");
    ck(namesis(SCRATCH,"blocked_create,t")&&same("t","T1",2),"unsafe lock changed nothing");
    ck(chmod(SCRATCH "/.fstxn.lock",0600)==0,"lock chmod back");
    ck(FsReplaceFile(R,"t","T1",2,"T2",2)==FS_READ_OK,"works again with safe lock");
}
/* A flock on a target file is NOT honoured: only the workspace lock is. */
static void t_foreign_file_lock(void)
{
    int a[2],b[2];pid_t p;char c;int st=0;
    g_what="foreign file lock";fresh();
    ck(mk("t","T0",2,0644),"fixture");
    ck(pipe(a)==0&&pipe(b)==0,"pipe");
    p=fork();ck(p>=0,"fork");
    if(p==0){
        int fd=open(SCRATCH "/t",O_RDWR);
        close(a[0]);close(b[1]);
        if(fd<0||flock(fd,LOCK_EX)<0)_exit(2);
        if(write(a[1],"L",1)!=1)_exit(3);
        if(read(b[0],&c,1)!=0)_exit(4); /* held until the parent closes its end */
        _exit(0);
    }
    close(a[1]);close(b[0]);
    ck(read(a[0],&c,1)==1&&c=='L',"child holds the file lock");
    ck(FsReplaceFile(R,"t","T0",2,"T1",2)==FS_READ_OK,"replace does not wait for a per-file lock");
    ck(same("t","T1",2),"replaced while foreign lock was held");
    close(b[1]);close(a[0]);
    ck(waitpid(p,&st,0)==p&&WIFEXITED(st)&&WEXITSTATUS(st)==0,"child exit 0");
}

int main(void)
{
    alarm(120); /* a hang here means a lock was not released: fail, never stall CI */
    t_case();t_perms();t_long();t_newline();t_lock();t_foreign_file_lock();
    {int rc;if(R)FsReadClose(R);rc=system("chmod -R u+rwx " SCRATCH " 2>/dev/null; rm -rf " SCRATCH);(void)rc;}
    printf("test_fs_c4_posix: OK\n");
    return 0;
}
#else
#include <stdio.h>
int main(void){printf("test_fs_c4_posix: skipped on Windows (criterion 4 not shown there)\n");return 0;}
#endif
