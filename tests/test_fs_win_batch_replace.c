#ifdef _WIN32
#include "fs_batch.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winbatch_scratch"
/* W4: FsBatchReplace on NTFS. Success, refusals, a target changed between the
   final check and the publish, a pending replace journal, crash smoke at
   writer points 24-28, and one mutant. The full crash matrix and the foreign
   open handle are W5. Crashes are real process exits (ExitProcess 80+point). */
void FsWinBatchTestSetBeforePublish(void (*f)(unsigned k));
FS_READ_STATUS FsWinBatchTestOpenStatus(const FS_READ_ROOT *r,const char *leaf);
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(ROOT "\\sub")==0,"mkdir");}
/* Count the names directly in dir, excluding . and .. and the persistent
   .fstxn.lock that the writers leave in the root. */
static int count_skip(const char *pattern,const char *skip)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pattern,&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&
       strcmp(d.cFileName,".fstxn.lock")&&
       !(skip&&!strncmp(d.cFileName,skip,strlen(skip))))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static int count(const char *pattern){return count_skip(pattern,NULL);}
static void is_file(const char *p,const char *v,int readonly)
{char b[64]={0};FILE *in=fopen(p,"rb");DWORD a;size_t n=strlen(v);
 ck(in!=NULL,"open created file");ck(fread(b,1,sizeof(b)-1,in)==n,"created size");
 ck(fclose(in)==0&&!memcmp(b,v,n),"created bytes");
 a=GetFileAttributesA(p);ck(a!=INVALID_FILE_ATTRIBUTES,"attrs");
 ck(((a&FILE_ATTRIBUTE_READONLY)!=0)==readonly,"read-only attribute");}
static void rm(const char *p)
{DWORD a=GetFileAttributesA(p);
 if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_READONLY))SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);
 ck(DeleteFileA(p),"remove file");}
static void cleanup(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\*",&d);
 ck(f!=INVALID_HANDLE_VALUE,"find");
 do{char p[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,"..")||
      !strcmp(d.cFileName,"sub"))continue;
    snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);rm(p);
 }while(FindNextFileA(f,&d));FindClose(f);
 f=FindFirstFileA(ROOT "\\sub\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{char p[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
    snprintf(p,sizeof(p),ROOT "\\sub\\%s",d.cFileName);rm(p);
 }while(FindNextFileA(f,&d));FindClose(f);}
 ck(_rmdir(ROOT "\\sub")==0&&_rmdir(ROOT)==0,"remove root");}
static int run_proc(const char *exe,const char *args)
{char cmd[1024];STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;
 si.cb=sizeof(si);sprintf(cmd,"\"%s\" %s",exe,args);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static void nuke(void)
{WIN32_FIND_DATAA d;HANDLE f;char p[512];
 f=FindFirstFileA(ROOT "\\sub\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")){
   snprintf(p,sizeof(p),ROOT "\\sub\\%s",d.cFileName);SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}}while(FindNextFileA(f,&d));FindClose(f);}
 f=FindFirstFileA(ROOT "\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")){
   snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}}while(FindNextFileA(f,&d));FindClose(f);}
 _rmdir(ROOT "\\sub");_rmdir(ROOT);}

static int run_child(const char *exe,int point,int recovery)
{char a[64];sprintf(a,"rchild %d %d",point,recovery);return run_proc(exe,a);}
static const FS_BATCH_REPLACE trio[3]={
 {"r1","one-old",7,"ONE-NEW-LONGER",14},
 {"sub/r2","two-old!",8,"",0},
 {"r3","three",5,"3",1}};
static void child(int point,int recovery)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",point);_putenv_s("FS_WIN_BATCH_CRASH",v);
 s=recovery?FsBatchRecover(r):FsBatchReplace(r,trio,3);
 fprintf(stderr,"replace child point=%d recovery=%d status=%d\n",point,recovery,(int)s);
 FsReadClose(r);ExitProcess(200);}
static void put_old(void)
{put(ROOT "\\r1","one-old");put(ROOT "\\sub\\r2","two-old!");put(ROOT "\\r3","three");}
static void is_old(void)
{is_file(ROOT "\\r1","one-old",0);is_file(ROOT "\\sub\\r2","two-old!",0);is_file(ROOT "\\r3","three",0);
 ck(count(ROOT "\\*")==3,"root holds only r1 r3 sub");ck(count(ROOT "\\sub\\*")==1,"sub holds only r2");}
static void is_new(void)
{is_file(ROOT "\\r1","ONE-NEW-LONGER",0);is_file(ROOT "\\sub\\r2","",0);is_file(ROOT "\\r3","3",0);
 ck(count(ROOT "\\*")==3,"root holds only r1 r3 sub after replace");ck(count(ROOT "\\sub\\*")==1,"sub holds only r2 after replace");}
/* Another writer swaps sub\\r2 for a different inode just before its publish. */
static void tamper(unsigned k)
{if(k==1){ck(DeleteFileA(ROOT "\\sub\\r2"),"tamper delete");put(ROOT "\\sub\\r2","two-NEW!");}}
static void sc_crash(const char *exe,int pt)
{FS_READ_ROOT *r;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open crash");
 ck(run_child(exe,pt,0)==80+pt,"replace writer killed at point");
 if(pt<28){FS_BATCH_CREATE zz[2]={{"zz1","x",1,0644},{"zz2","x",1,0644}};
  FS_BATCH_REPLACE again[3]={trio[0],trio[1],trio[2]};
  ck(FsBatchCreate(r,zz,2)==FS_READ_DENIED,"pending replace blocks create");
  ck(FsBatchReplace(r,again,3)==FS_READ_DENIED,"pending replace blocks replace");}
 ck(FsBatchRecover(r)==FS_READ_OK,"recover");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
 if(pt>=27)is_new();else is_old();
 FsReadClose(r);cleanup();}
/* The same scenarios a mutant must fail: a hard-linked target is refused. */
static void sc_hardlink(void)
{FS_READ_ROOT *r;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open hardlink");
 ck(CreateHardLinkA(ROOT "\\alias",ROOT "\\r3",NULL),"hard link");
 ck(FsBatchReplace(r,trio,3)==FS_READ_DENIED,"hard-linked target denied");
 is_file(ROOT "\\alias","three",0);is_file(ROOT "\\r3","three",0);
 is_file(ROOT "\\r1","one-old",0);is_file(ROOT "\\sub\\r2","two-old!",0);
 ck(count(ROOT "\\*")==4,"hard link left no residue");
 FsReadClose(r);cleanup();}
static void mutant_child(int m)
{char v[8];sprintf(v,"%d",m);_putenv_s("FS_WIN_BATCH_MUTANT",v);
 sc_hardlink();ExitProcess(0);}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;
 if(argc==4&&!strcmp(argv[1],"rchild")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 if(argc==3&&!strcmp(argv[1],"rmutant")){mutant_child(atoi(argv[2]));return 0;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 /* Success, three files in two directories, one replaced by an empty file. */
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
 ck(FsBatchReplace(r,trio,3)==FS_READ_OK,"batch replace");is_new();
 ck(FsBatchRecover(r)==FS_READ_OK,"recover on a clean tree");is_new();
 FsReadClose(r);cleanup();
 /* Two files in one directory. */
 fixture();put(ROOT "\\x1","a");put(ROOT "\\x2","b");ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open two");
 {FS_BATCH_REPLACE two[2]={{"x1","a",1,"AA",2},{"x2","b",1,"BB",2}};
  ck(FsBatchReplace(r,two,2)==FS_READ_OK,"two files");
  is_file(ROOT "\\x1","AA",0);is_file(ROOT "\\x2","BB",0);ck(count(ROOT "\\*")==3,"two files and sub only");}
 FsReadClose(r);cleanup();
 /* Refusals leave every file as it was. */
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open refusals");
 {FS_BATCH_REPLACE b[3]={trio[0],trio[1],{"r3","THREE",5,"3",1}};
  ck(FsBatchReplace(r,b,3)==FS_READ_DENIED,"same size, different bytes");is_old();}
 {FS_BATCH_REPLACE b[3]={trio[0],trio[1],{"r3","thre",4,"3",1}};
  ck(FsBatchReplace(r,b,3)==FS_READ_DENIED,"wrong expected size");is_old();}
 {FS_BATCH_REPLACE b[2]={trio[0],{"r1","one-old",7,"x",1}};
  ck(FsBatchReplace(r,b,2)==FS_READ_INVALID,"same file twice");is_old();}
 {FS_BATCH_REPLACE b[2]={trio[0],{"R1","one-old",7,"x",1}};
  ck(FsBatchReplace(r,b,2)==FS_READ_INVALID,"case-insensitive duplicate");is_old();}
 {FS_BATCH_REPLACE b[2]={trio[0],{"nofile","x",1,"y",1}};
  ck(FsBatchReplace(r,b,2)!=FS_READ_OK,"missing target");is_old();}
 {FS_BATCH_REPLACE b[2]={trio[0],{"sub","x",1,"y",1}};
  ck(FsBatchReplace(r,b,2)!=FS_READ_OK,"directory target");is_old();}
 ck(SetFileAttributesA(ROOT "\\r3",FILE_ATTRIBUTE_READONLY),"make read-only");
 ck(FsBatchReplace(r,trio,3)==FS_READ_DENIED,"read-only target denied");
 SetFileAttributesA(ROOT "\\r3",FILE_ATTRIBUTE_NORMAL);is_old();
 FsReadClose(r);cleanup();
 sc_hardlink();
 /* A target is swapped for a foreign inode after the journal and after item 0
    was published. Recovery cannot claim a file that is neither the old nor
    the new inode, so nothing is rolled back: PENDING, journal kept, the
    foreign file untouched. Fail closed, not a restored tree. */
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open tamper");
 FsWinBatchTestSetBeforePublish(tamper);
 ck(FsBatchReplace(r,trio,3)==FS_READ_PENDING,"swapped target is PENDING");
 FsWinBatchTestSetBeforePublish(NULL);
 is_file(ROOT "\\sub\\r2","two-NEW!",0);is_file(ROOT "\\r3","three",0);
 ck(GetFileAttributesA(ROOT "\\.fstxn.batch")!=INVALID_FILE_ATTRIBUTES,"journal kept");
 ck(FsBatchRecover(r)==FS_READ_DENIED,"recovery refuses a foreign inode");
 is_file(ROOT "\\sub\\r2","two-NEW!",0);
 FsReadClose(r);nuke();
 /* Crash smoke: journal complete, first target replaced, all replaced without
    marker, marker, committed cleanup done. */
 for(int i=0;i<5;i++){int pts[5]={24,25,26,27,28};sc_crash(exe,pts[i]);}
 /* Mutant: the single-link check removed must be caught. */
 {char a[32];int code;sprintf(a,"rmutant 6");code=run_proc(exe,a);nuke();
  if(code!=1){fprintf(stderr,"mutant 6 SURVIVED (exit %d)\n",code);exit(1);}}
 printf("test_fs_win_batch_replace ok\n");
 return 0;
}
#else
int main(void){return 0;}
#endif
