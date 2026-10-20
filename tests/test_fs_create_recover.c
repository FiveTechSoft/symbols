#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include "fs_remove.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
static void ck(int ok,const char *m){if(!ok){fprintf(stderr,"FAIL %s\n",m);exit(1);}}
static void clean_root(void)
{
    /* Test-only teardown via a held scratch directory. The pre-intent
       orphan is swept by recovery since slice G; teardown asserts that. */
    char full[256];struct dirent *e;DIR *d=opendir("test_fs_recover_scratch");
    ck(d!=NULL,"teardown open");
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        snprintf(full,sizeof(full),"test_fs_recover_scratch/%s",e->d_name);
        ck(unlink(full)==0,"teardown unlink");
    }
    ck(closedir(d)==0&&rmdir("test_fs_recover_scratch")==0,"teardown root");
}
static int count_prefix(const char *dirpath,const char *prefix,int plen)
{
    int n=0;struct dirent *e;DIR *d=opendir(dirpath);ck(d!=NULL,"count open");
    while((e=readdir(d)))if(!strncmp(e->d_name,prefix,plen))n++;
    ck(closedir(d)==0,"count close");return n;
}
static int exists_plain(const char *p){struct stat st;return stat(p,&st)==0;}
static void plant(const char *name,const char *v)
{char full[256];FILE *f;snprintf(full,sizeof(full),"test_fs_recover_scratch/%s",name);
 f=fopen(full,"wb");ck(f!=NULL&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"plant");}
static void read_target(FS_READ_ROOT *r,const char *name,int exists)
{
    unsigned char *b;size_t n;FS_READ_META m;
    if(!exists){ck(FsReadStat(r,name,&m)==FS_READ_MISSING,"target absent");return;}
    ck(FsReadFile(r,name,&b,&n,&m)==FS_READ_OK && n==7 &&
       !memcmp(b,"payload",7),"target bytes");free(b);
}
int main(void)
{
    FS_READ_ROOT *r;int status;pid_t child;char path[40];FS_READ_META m;
    ck(mkdir("test_fs_recover_scratch",0700)==0,"root");
    ck(FsReadOpen("test_fs_recover_scratch",&r)==FS_READ_OK,"open");
    for(int step=1;step<=4;step++){
        snprintf(path,sizeof(path),"target-%d",step);
        child=fork();ck(child>=0,"fork");
        if(child==0){char value[4];snprintf(value,sizeof(value),"%d",step);
            setenv("FS_CREATE_TEST_CRASH",value,1);
            (void)FsCreateFile(r,path,"payload",7,0600);_exit(20);
        }
        ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&
           WEXITSTATUS(status)==90+step,"crash point");
        if(step==1){read_target(r,path,0);
            ck(FsCreateRecover(r)==FS_READ_OK,"no intent");
            ck(count_prefix("test_fs_recover_scratch",".fst-",5)==0,
               "stage orphan swept");
        }else{
            ck(FsReadStat(r,".fstxn.intent",&m)==FS_READ_OK,"intent exists");
            ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
               "pending blocks new create");
            read_target(r,path,step>=3);
            ck(FsCreateRecover(r)==FS_READ_OK,"replay");
            read_target(r,path,step>=3);
            ck(FsReadStat(r,".fstxn.intent",&m)==FS_READ_MISSING,
               "intent cleared");
        }

    }
    /* A pre-existing different target must be left untouched on replay. */
    child=fork();ck(child>=0,"foreign fork");
    if(child==0){setenv("FS_CREATE_TEST_CRASH","2",1);
        (void)FsCreateFile(r,"foreign","payload",7,0600);_exit(20);}
    ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&
       WEXITSTATUS(status)==92,"foreign crash");
    {FILE *f=fopen("test_fs_recover_scratch/foreign","wb");
     ck(f!=NULL&&fwrite("foreign",1,7,f)==7&&fclose(f)==0,"foreign fixture");}
    ck(FsCreateRecover(r)==FS_READ_DENIED,"foreign target refuses recovery");
    {unsigned char *b;size_t n;FS_READ_META meta;
     ck(FsReadFile(r,"foreign",&b,&n,&meta)==FS_READ_OK&&n==7&&
        !memcmp(b,"foreign",7),"foreign target intact");free(b);}
    ck(unlink("test_fs_recover_scratch/foreign")==0,"remove foreign fixture");
    ck(FsCreateRecover(r)==FS_READ_OK,"recover after foreign removed");
    /* Replacing the staged inode must not make recovery delete that file. */
    child=fork();ck(child>=0,"stage fork");
    if(child==0){setenv("FS_CREATE_TEST_CRASH","2",1);
        (void)FsCreateFile(r,"stage-guard","payload",7,0600);_exit(20);}
    ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&
       WEXITSTATUS(status)==92,"stage crash");
    {char stage_path[256]={0};unsigned char record[1096];FILE *intent_file;
     intent_file=fopen("test_fs_recover_scratch/.fstxn.intent","rb");
     ck(intent_file!=NULL&&fread(record,1,sizeof(record),intent_file)==sizeof(record)&&
        fclose(intent_file)==0,"read test intent");
     ck(!strncmp((char*)record+1048,".fst-",5),"intent stage name");
     snprintf(stage_path,sizeof(stage_path),"test_fs_recover_scratch/%s",
              (char*)record+1048);
     {int held=open(stage_path,O_RDONLY|O_NOFOLLOW);ck(held>=0,"hold old inode");
      ck(unlink(stage_path)==0,"remove staged inode");
      {FILE *f=fopen(stage_path,"wb");
       ck(f!=NULL&&fwrite("foreign",1,7,f)==7&&fclose(f)==0,"foreign stage");}
      ck(FsCreateRecover(r)==FS_READ_DENIED,"foreign stage refused");
      close(held);
     }
     {FILE *f=fopen(stage_path,"rb");char value[8]={0};
      ck(f!=NULL&&fread(value,1,7,f)==7&&!memcmp(value,"foreign",7)&&
         fclose(f)==0,"foreign stage intact");}
     /* The fixture has no safe replay now; cleanup is test-only. */
     ck(unlink("test_fs_recover_scratch/.fstxn.intent")==0,"clear fixture intent");
    }
    /* A malformed record must neither authorize cleanup nor new creation. */
    {FILE *f=fopen("test_fs_recover_scratch/.fstxn.intent","wb");
     ck(f!=NULL&&fwrite("bad",1,3,f)==3&&fclose(f)==0,"malformed intent");}
    ck(FsCreateRecover(r)==FS_READ_DENIED,"malformed record refused");
    ck(FsCreateFile(r,"blocked","x",1,0600)==FS_READ_DENIED,
       "malformed pending blocks create");
    ck(unlink("test_fs_recover_scratch/.fstxn.intent")==0,"test cleanup malformed");
    ck(FsCreateFile(r,".fst-manual","x",1,0600)==FS_READ_DENIED,
       "stage namespace reserved");
    ck(mkdir("test_fs_recover_scratch/nested",0700)==0,"nested dir");
    child=fork();ck(child>=0,"nested fork");
    if(child==0){setenv("FS_CREATE_TEST_CRASH","4",1);
        (void)FsCreateFile(r,"nested/child","payload",7,0600);_exit(20);}
    ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&
       WEXITSTATUS(status)==94,"nested crash");
    ck(FsCreateRecover(r)==FS_READ_OK,"nested recovery");
    read_target(r,"nested/child",1);
    ck(unlink("test_fs_recover_scratch/nested/child")==0&&
       rmdir("test_fs_recover_scratch/nested")==0,"nested cleanup");
    ck(FsCreateRecover(r)==FS_READ_OK,"idempotent recovery");
    ck(FsCreateFile(r,"fresh","payload",7,0600)==FS_READ_OK,
       "create after replay");
    read_target(r,"fresh",1);
    /* Copy runs the same create protocol: a pre-intent crash leaves the
       same .fst- orphan and recovery sweeps it. */
    ck(FsCreateFile(r,"copy-source","payload",7,0600)==FS_READ_OK,"copy source");
    child=fork();ck(child>=0,"copy fork");
    if(child==0){setenv("FS_CREATE_TEST_CRASH","1",1);
        (void)FsCopyFile(r,"copy-source","copy-target","payload",7);_exit(20);}
    ck(waitpid(child,&status,0)==child&&WIFEXITED(status)&&
       WEXITSTATUS(status)==91,"copy crash point");
    ck(FsCreateRecover(r)==FS_READ_OK,"copy orphan recovery");
    ck(count_prefix("test_fs_recover_scratch",".fst-",5)==0,
       "copy stage orphan swept");
    ck(FsRemoveFile(r,"copy-source","payload",7)==FS_READ_OK,"copy source cleanup");
    /* Slice G sweep controls: foreign exact-shape and near-shape names. */
    plant(".fst-0123456789abcdef0123456789abcdef","foreign");   /* new-stage shape: swept */
    plant(".fsrp-0123456789abcdef0123456789abcdef","foreign");  /* unshared old-pin shape: kept */
    plant(".fsrm-0123456789abcdef0123456789abcdef","foreign");  /* kept */
    plant(".fsmv-0123456789abcdef0123456789abcdef","foreign");  /* kept */
    plant(".fst-0123456789abcdef0123456789abcde","x");          /* 31 hex: kept */
    plant(".fst-0123456789ABCDEF0123456789ABCDEF","x");         /* uppercase: kept */
    plant(".fstx-0123456789abcdef0123456789abcdef","x");        /* wrong prefix: kept */
    ck(mkdir("test_fs_recover_scratch/.fst-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",0700)==0,
       "plant directory of exact shape");
    ck(FsCreateRecover(r)==FS_READ_OK,"control recovery");
    ck(!exists_plain("test_fs_recover_scratch/.fst-0123456789abcdef0123456789abcdef"),
       "new-stage shape swept");
    ck(exists_plain("test_fs_recover_scratch/.fsrp-0123456789abcdef0123456789abcdef"),
       "unshared .fsrp- kept (last-link rule)");
    ck(exists_plain("test_fs_recover_scratch/.fsrm-0123456789abcdef0123456789abcdef"),
       "unshared .fsrm- kept");
    ck(exists_plain("test_fs_recover_scratch/.fsmv-0123456789abcdef0123456789abcdef"),
       "unshared .fsmv- kept");
    ck(exists_plain("test_fs_recover_scratch/.fst-0123456789abcdef0123456789abcde"),
       "31-hex name kept");
    ck(exists_plain("test_fs_recover_scratch/.fst-0123456789ABCDEF0123456789ABCDEF"),
       "uppercase name kept");
    ck(exists_plain("test_fs_recover_scratch/.fstx-0123456789abcdef0123456789abcdef"),
       "wrong-prefix name kept");
    {struct stat st;ck(stat("test_fs_recover_scratch/.fst-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",&st)==0&&
       S_ISDIR(st.st_mode),"directory of exact shape kept");}
    /* With a journal of any protocol present the sweep must not run. */
    plant(".fst-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","foreign");
    plant(".fstxn.batch","bad");
    ck(FsCreateRecover(r)==FS_READ_DENIED,"foreign batch journal blocks");
    ck(exists_plain("test_fs_recover_scratch/.fst-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
       "orphan kept while a journal exists (sweep did not run)");
    ck(unlink("test_fs_recover_scratch/.fstxn.batch")==0,"clear batch fixture");
    ck(FsCreateRecover(r)==FS_READ_OK,"recovery after journal cleared");
    ck(!exists_plain("test_fs_recover_scratch/.fst-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
       "orphan swept once journals are clear");
    /* Control fixture cleanup (teardown only unlinks files). */
    ck(unlink("test_fs_recover_scratch/.fsrp-0123456789abcdef0123456789abcdef")==0&&
       unlink("test_fs_recover_scratch/.fsrm-0123456789abcdef0123456789abcdef")==0&&
       unlink("test_fs_recover_scratch/.fsmv-0123456789abcdef0123456789abcdef")==0&&
       unlink("test_fs_recover_scratch/.fst-0123456789abcdef0123456789abcde")==0&&
       unlink("test_fs_recover_scratch/.fst-0123456789ABCDEF0123456789ABCDEF")==0&&
       unlink("test_fs_recover_scratch/.fstx-0123456789abcdef0123456789abcdef")==0&&
       rmdir("test_fs_recover_scratch/.fst-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")==0,
       "control fixture cleanup");
    FsReadClose(r);
    clean_root();
    puts("create recovery crash points passed");return 0;
}
#else
int main(void){return 0;}
#endif
