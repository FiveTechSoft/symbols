#ifdef _WIN32
/* W1: Windows batch record formats and batch-level validation. Pure checks, no
   filesystem. The hooks exist only in FS_CREATE_TEST_CRASH builds; the public
   FsBatch* functions are still UNSUPPORTED stubs on Windows. */
#include "fs_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FS_READ_STATUS FsWinBatchTestValidateCreate(const FS_BATCH_CREATE *e,size_t n);
FS_READ_STATUS FsWinBatchTestValidateReplace(const FS_BATCH_REPLACE *e,size_t n);
size_t FsWinBatchTestRecordSize(int kind);
int FsWinBatchTestBuild(int kind,unsigned count,void *out);
int FsWinBatchTestValid(int kind,const void *rec);
int FsWinBatchTestMutate(int kind,void *rec,int m);
static int failures;
static void ck(int ok,const char *label,int a)
{if(!ok){fprintf(stderr,"FAIL %s %d\n",label,a);failures++;}}
static void records(void)
{
    for(int kind=0;kind<2;kind++){
        size_t size=FsWinBatchTestRecordSize(kind);
        void *rec=malloc(size),*work=malloc(size);
        ck(rec&&work&&size>0,"alloc",kind);if(!rec||!work)exit(1);
        for(unsigned n=2;n<=8;n++){
            ck(FsWinBatchTestBuild(kind,n,rec)&&FsWinBatchTestValid(kind,rec),"valid record",(int)(kind*10+n));
        }
        for(int m=1;m<=18;m++){
            int applied;
            ck(FsWinBatchTestBuild(kind,3,work),"build",kind);
            applied=FsWinBatchTestMutate(kind,work,m);
            if(!applied){ck(kind==0&&m==17,"mutation applies",kind*100+m);continue;}
            ck(!FsWinBatchTestValid(kind,work),"mutated record refused",kind*100+m);
        }
        free(rec);free(work);
    }
}
static FS_BATCH_CREATE C(const char *t){FS_BATCH_CREATE c={t,"x",1,0644};return c;}
static FS_BATCH_REPLACE R(const char *t){FS_BATCH_REPLACE r={t,"old",3,"new",3};return r;}
static void create_requests(void)
{
    FS_BATCH_CREATE ok[2]={C("a/one"),C("a/two")};
    FS_BATCH_CREATE many[9],bad[2];char names[9][8];
    ck(FsWinBatchTestValidateCreate(ok,2)==FS_READ_OK,"create valid",0);
    ck(FsWinBatchTestValidateCreate(ok,1)==FS_READ_INVALID,"create one",0);
    ck(FsWinBatchTestValidateCreate(NULL,2)==FS_READ_INVALID,"create null",0);
    for(int k=0;k<9;k++){sprintf(names[k],"f%d",k);many[k]=C(names[k]);}
    ck(FsWinBatchTestValidateCreate(many,8)==FS_READ_OK,"create eight",0);
    ck(FsWinBatchTestValidateCreate(many,9)==FS_READ_INVALID,"create nine",0);
    {const char *pairs[][2]={{"A.txt","a.txt"},{"a.txt","a.txt"},{"a","A/b"},{"dir/x","DIR/X"},
       {"a/b","a"},{"ok",".fstxn.lock"},{"ok",".fst-0123456789abcdef0123456789abcdef"},
       {"ok","d/con.txt"},{"ok","dir/NUL"},{"ok","name."},{"ok","name "},{"ok","a\\b"},{"ok","a:b"},
       {"ok",NULL},{"ok","../up"},{"ok","/abs"}};
     for(unsigned k=0;k<sizeof(pairs)/sizeof(pairs[0]);k++){
        bad[0]=C(pairs[k][0]);bad[1]=C(pairs[k][1]?pairs[k][1]:"x");bad[1].target=pairs[k][1];
        ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_INVALID,"create refused pair",(int)k);
     }}
    bad[0]=C("p");bad[1]=C("q");bad[1].mode=01000;
    ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_INVALID,"create mode",0);
    bad[1]=C("q");bad[1].bytes=NULL;bad[1].len=1;
    ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_INVALID,"create null bytes",0);
    bad[1]=C("q");bad[1].len=1024u*1024u+1u;
    ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_INVALID,"create too big",0);
    bad[1]=C("q");bad[1].bytes=NULL;bad[1].len=0;
    ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_OK,"create empty file",0);
    {char *longp=(char*)malloc(5000);memset(longp,'a',4999);longp[4999]=0;
     bad[1]=C(longp);ck(FsWinBatchTestValidateCreate(bad,2)==FS_READ_INVALID,"create long path",0);free(longp);}
}
static void replace_requests(void)
{
    FS_BATCH_REPLACE ok[2]={R("a/one"),R("a/two")},bad[2];
    ck(FsWinBatchTestValidateReplace(ok,2)==FS_READ_OK,"replace valid",0);
    ck(FsWinBatchTestValidateReplace(ok,1)==FS_READ_INVALID,"replace one",0);
    ck(FsWinBatchTestValidateReplace(ok,9)==FS_READ_INVALID,"replace nine",0);
    bad[0]=R("A");bad[1]=R("a");
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace case duplicate",0);
    bad[0]=R("a");bad[1]=R("a/b");
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace prefix conflict",0);
    bad[0]=R("p");bad[1]=R(".fstxn.batch");
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace control name",0);
    bad[1]=R("q");bad[1].expected=NULL;
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace null expected",0);
    bad[1]=R("q");bad[1].replacement=NULL;bad[1].replacement_len=1;
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace null replacement",0);
    bad[1]=R("q");bad[1].replacement_len=1024u*1024u+1u;
    ck(FsWinBatchTestValidateReplace(bad,2)==FS_READ_INVALID,"replace too big",0);
}
int main(void)
{
    records();create_requests();replace_requests();
    if(failures){fprintf(stderr,"%d failure(s)\n",failures);return 1;}
    puts("Windows batch validation passed");return 0;
}
#else
int main(void){return 0;}
#endif
