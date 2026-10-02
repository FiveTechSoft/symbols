#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_batch.h"
#include "fs_write.h"
#include "fs_replace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#define ROOT "test_fs_batch_replace_scratch"
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static const char *NAME[3]={"a","b","sub/c"};
static const char *OLD[3]={"old-a","old-bb","old-ccc"};
static const char *NEW[3]={"NEW-A-longer","N-b","NEW-c"};
static void put(const char *name,const char *data,mode_t mode)
{
    char path[256];FILE *f;snprintf(path,sizeof(path),ROOT "/%s",name);
    f=fopen(path,"wb");ck(f&&fwrite(data,1,strlen(data),f)==strlen(data)&&fclose(f)==0,"put");
    ck(chmod(path,mode)==0,"put mode");
}
static void fixtures(void)
{
    put("a",OLD[0],0640);put("b",OLD[1],0600);put("sub/c",OLD[2],0644);
}
/* which[k]: 0 = original bytes, 1 = replacement bytes */
static void bytes(FS_READ_ROOT *r,const int *which,const char *m)
{
    for(int k=0;k<3;k++){
        FS_READ_META meta;unsigned char *b=NULL;size_t n=0;
        const char *want=which[k]?NEW[k]:OLD[k];
        ck(FsReadFile(r,NAME[k],&b,&n,&meta)==FS_READ_OK,m);
        ck(n==strlen(want)&&!memcmp(b,want,n),m);free(b);
    }
}
static void modes(void)
{
    struct stat st;
    ck(stat(ROOT "/a",&st)==0&&(st.st_mode&0777)==0640,"mode a kept");
    ck(stat(ROOT "/b",&st)==0&&(st.st_mode&0777)==0600,"mode b kept");
    ck(stat(ROOT "/sub/c",&st)==0&&(st.st_mode&0777)==0644,"mode c kept");
}
static int control_files(void)
{
    DIR *d=opendir(ROOT);struct dirent *e;int n=0;
    ck(d!=NULL,"opendir");
    while((e=readdir(d)))
        if(!strncmp(e->d_name,".fsrp-",6)||!strncmp(e->d_name,".fsrb-",6)||
           !strcmp(e->d_name,".fstxn.batch")||!strcmp(e->d_name,".fstxn.commit"))n++;
    closedir(d);
    d=opendir(ROOT "/sub");ck(d!=NULL,"opendir sub");
    while((e=readdir(d)))if(!strncmp(e->d_name,".fsrb-",6))n++;
    closedir(d);return n;
}
static void entries(FS_BATCH_REPLACE *e)
{
    for(int k=0;k<3;k++)
        e[k]=(FS_BATCH_REPLACE){NAME[k],OLD[k],strlen(OLD[k]),NEW[k],strlen(NEW[k])};
}
static int crash(FS_READ_ROOT *r,const char *point,int recover_instead,int expect)
{
    FS_BATCH_REPLACE e[3];int st;pid_t pid;entries(e);
    pid=fork();ck(pid>=0,"fork");
    if(pid==0){setenv("FS_CREATE_TEST_CRASH",point,1);
        if(recover_instead)(void)FsBatchRecover(r);else (void)FsBatchReplace(r,e,3);
        _exit(20);}
    ck(waitpid(pid,&st,0)==pid&&WIFEXITED(st)&&WEXITSTATUS(st)==expect,"crash exit");
    return 0;
}
int main(void)
{
    FS_READ_ROOT *r;FS_BATCH_REPLACE e[3];FS_READ_META m;
    const int OLDS[3]={0,0,0},ALLNEW[3]={1,1,1},FIRST[3]={1,0,0};
    ck(system("rm -rf " ROOT)==0,"pre-clean");
    ck(mkdir(ROOT,0700)==0&&mkdir(ROOT "/sub",0700)==0,"root");
    ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
    entries(e);fixtures();
    /* argument and state validation: nothing changes */
    ck(FsBatchReplace(r,e,1)==FS_READ_INVALID,"count 1 invalid");
    ck(FsBatchReplace(NULL,e,3)==FS_READ_INVALID,"null root");
    {FS_BATCH_REPLACE d[2]={e[0],e[0]};
     ck(FsBatchReplace(r,d,2)==FS_READ_INVALID,"duplicate target invalid");}
    {FS_BATCH_REPLACE d[2]={e[0],{"../escape","x",1,"y",1}};
     ck(FsBatchReplace(r,d,2)==FS_READ_INVALID,"escape invalid");}
    {FS_BATCH_REPLACE d[3];entries(d);d[1].expected="old-XX";
     ck(FsBatchReplace(r,d,3)==FS_READ_DENIED,"drift denied");
     bytes(r,OLDS,"drift untouched");ck(control_files()==0,"drift leaves no control file");}
    {FS_BATCH_REPLACE d[2]={e[0],{"missing","x",1,"y",1}};
     ck(FsBatchReplace(r,d,2)!=FS_READ_OK,"missing target refused");
     bytes(r,OLDS,"missing untouched");ck(control_files()==0,"missing leaves no control file");}
    ck(link(ROOT "/b",ROOT "/b-link")==0,"hardlink fixture");
    ck(FsBatchReplace(r,e,3)==FS_READ_DENIED,"hard-linked target denied");
    bytes(r,OLDS,"hardlink untouched");ck(unlink(ROOT "/b-link")==0,"hardlink cleanup");
    ck(control_files()==0,"hardlink leaves no control file");
    /* normal commit */
    ck(FsBatchReplace(r,e,3)==FS_READ_OK,"normal replace");
    bytes(r,ALLNEW,"all new");modes();ck(control_files()==0,"clean after commit");
    ck(FsBatchRecover(r)==FS_READ_OK,"idempotent recover");
    /* crash at every step: commit fully or restore */
    for(int step=60;step<=63;step++){
        char v[8];FS_READ_META mm;snprintf(v,sizeof(v),"%d",step);
        fixtures();
        crash(r,v,0,90+step);
        ck(FsReadStat(r,".fstxn.batch",&mm)==FS_READ_OK,"journal present");
        bytes(r,step==60?OLDS:(step==61?FIRST:ALLNEW),"state at crash");
        ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"create blocked");
        ck(FsReplaceFile(r,"a",step>=61?NEW[0]:OLD[0],strlen(step>=61?NEW[0]:OLD[0]),"z",1)==FS_READ_DENIED,"replace blocked");
        entries(e);
        ck(FsBatchReplace(r,e,3)!=FS_READ_OK,"batch replace blocked");
        ck(FsBatchRecover(r)==FS_READ_OK,"recover");
        bytes(r,step==63?ALLNEW:OLDS,step==63?"committed kept":"restored byte-exact");
        modes();ck(control_files()==0,"control files retired");
        ck(FsReadStat(r,"blocked",&mm)==FS_READ_MISSING,"nothing leaked");
        if(step==63)fixtures();
    }
    /* recovery interrupted after a rollback link exists, then run again */
    fixtures();entries(e);
    crash(r,"62",0,152);
    crash(r,"65",1,155);
    ck(FsBatchRecover(r)==FS_READ_OK,"resumed rollback");
    bytes(r,OLDS,"resumed rollback byte-exact");modes();ck(control_files()==0,"resumed clean");
    /* committed recovery interrupted after the journal is retired */
    crash(r,"63",0,153);
    crash(r,"66",1,156);
    ck(FsReadStat(r,".fstxn.batch",&m)==FS_READ_MISSING&&
       FsReadStat(r,".fstxn.commit",&m)==FS_READ_OK,"marker-only state");
    bytes(r,ALLNEW,"marker-only keeps new");
    ck(FsBatchRecover(r)==FS_READ_DENIED,"marker-only fails closed");
    ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,"marker-only blocks writers");
    ck(unlink(ROOT "/.fstxn.commit")==0,"test-only marker cleanup");
    /* foreign bytes at a target: no partial rollback, foreign file untouched */
    system("rm -f " ROOT "/.fsrp-*");
    fixtures();entries(e);crash(r,"61",0,151);
    ck(unlink(ROOT "/b")==0,"remove old b");put("b","foreign",0600);
    ck(FsBatchRecover(r)==FS_READ_DENIED,"foreign refused");
    {int w[3]={1,0,0};FS_READ_META mm;unsigned char *b;size_t n;
     ck(FsReadFile(r,"a",&b,&n,&mm)==FS_READ_OK&&n==strlen(NEW[0])&&
        !memcmp(b,NEW[0],n),"a not rolled back on conflict");free(b);(void)w;
     ck(FsReadFile(r,"b",&b,&n,&mm)==FS_READ_OK&&n==7&&!memcmp(b,"foreign",7),
        "foreign untouched");free(b);}
    ck(unlink(ROOT "/.fstxn.batch")==0,"test-only journal cleanup");
    ck(system("rm -f " ROOT "/.fsrp-* " ROOT "/.fsrb-*")==0,"test-only stage cleanup");
    /* malformed journal of the replace size is never permission to change files */
    {FILE *f=fopen(ROOT "/.fstxn.batch","wb");char junk[4096]={0};
     ck(f!=NULL,"junk fixture");
     fwrite(junk,1,sizeof(junk),f);fclose(f);}
    ck(FsBatchRecover(r)==FS_READ_DENIED,"malformed journal denied");
    ck(unlink(ROOT "/.fstxn.batch")==0,"junk cleanup");
    /* in-process failure after the journal restores the originals at once */
    if(geteuid()!=0){
        FS_READ_STATUS s;
        fixtures();entries(e);
        ck(chmod(ROOT "/sub",0500)==0,"sub read-only");
        s=FsBatchReplace(r,e,3);
        ck(chmod(ROOT "/sub",0700)==0,"sub writable again");
        ck(s!=FS_READ_OK,"publish failure reported");
        bytes(r,OLDS,"in-process failure restored byte-exact");
        modes();ck(control_files()==0,"in-process failure leaves no journal");
        ck(FsCreateFile(r,"after","x",1,0600)==FS_READ_OK,"writers unblocked");
        ck(unlink(ROOT "/after")==0,"after cleanup");
    }
    FsReadClose(r);
    ck(system("rm -rf " ROOT)==0,"teardown");
    puts("batch replace passed");return 0;
}
#else
int main(void){return 0;}
#endif
