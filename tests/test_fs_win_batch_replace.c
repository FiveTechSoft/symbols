#ifdef _WIN32
#include "fs_batch.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winbatch_scratch"
/* W4/W5: FsBatchReplace on NTFS. Success, refusals, a swapped target, the full
   crash matrix (writer 21-28, recovery 29-36), a foreign process holding a
   handle on a target (0xc0000022 on the rename-over => PENDING with rollback),
   a foreign file under a pin name, and mutants 6-9 that the same scenarios
   must kill. Crashes are real process exits (ExitProcess 80+point). */
void FsWinBatchTestSetBeforePublish(void (*f)(unsigned k));
FS_READ_STATUS FsWinBatchTestOpenStatus(const FS_READ_ROOT *r,const char *leaf);
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
/* A failed cell exits without cleanup: start every fixture from nothing so a
   leftover scratch directory cannot fail the next cell with "mkdir". */
static void nuke(void);
static void fixture(void)
{nuke();ck(_mkdir(ROOT)==0&&_mkdir(ROOT "\\sub")==0,"mkdir");}
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
static void hchild(int k,const char *sync)
{FS_READ_ROOT *r;char v[16];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"holder child open");
 sprintf(v,"%d",k);_putenv_s("FS_WIN_BATCH_HOLD",v);_putenv_s("FS_WIN_BATCH_SYNC",sync);
 s=FsBatchReplace(r,trio,3);FsReadClose(r);ExitProcess((UINT)(100+(int)s));}
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
/* Writer killed at 21-23 (before any journal): orphan names only. A leftover
   old pin makes r1 a two-link file. Recovery now sweeps those names (mutant 10
   skips the sweep), so the tree is clean and the next batch on r1 works. */
static void sc_early(const char *exe,int pt)
{FS_READ_ROOT *r;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open early");
 ck(run_child(exe,pt,0)==80+pt,"early writer killed");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover");ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
 is_old();
 ck(FsBatchReplace(r,trio,3)==FS_READ_OK,"batch on the target works after the sweep");
 is_new();
 FsReadClose(r);cleanup();}
/* Writer killed at spt, recovery killed at rpt, then full recovery. */
static void sc_reccrash(const char *exe,int spt,int rpt)
{FS_READ_ROOT *r;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open recovery crash");
 ck(run_child(exe,spt,0)==80+spt,"setup writer killed");
 ck(run_child(exe,rpt,1)==80+rpt,"recovery killed at point");
 if(spt==26&&rpt==30){ /* reverse order: the last item is restored first */
  is_file(ROOT "\\r3","three",0);is_file(ROOT "\\r1","ONE-NEW-LONGER",0);is_file(ROOT "\\sub\\r2","",0);}
 if(spt==25&&rpt==30){is_file(ROOT "\\r1","one-old",0);is_file(ROOT "\\r3","three",0);}
 ck(FsBatchRecover(r)==FS_READ_OK,"recover after recovery crash");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
 if(spt>=27)is_new();else is_old();
 FsReadClose(r);cleanup();}
/* Another process holds the target open without delete sharing, opened after
   our handle on it was closed: the rename-over is refused. */
static void sc_foreign_handle(const char *exe,int k)
{FS_READ_ROOT *r;char sync[64]="test_fs_winbatch_sync",f1[96],f2[96],args[160],cmd[1024],*tg[3]={ROOT "\\r1",ROOT "\\sub\\r2",ROOT "\\r3"};
 STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;HANDLE foreign,g;int i;
 snprintf(f1,sizeof(f1),"%s.ready",sync);snprintf(f2,sizeof(f2),"%s.go",sync);
 DeleteFileA(f1);DeleteFileA(f2);
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open foreign handle");
 sprintf(args,"hchild %d %s",k,sync);sprintf(cmd,"\"%s\" %s",exe,args);si.cb=sizeof(si);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn holder child");
 for(i=0;i<1000&&GetFileAttributesA(f1)==INVALID_FILE_ATTRIBUTES;i++)Sleep(20);
 ck(GetFileAttributesA(f1)!=INVALID_FILE_ATTRIBUTES,"writer reached the hold point");
 foreign=CreateFileA(tg[k],GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 ck(foreign!=INVALID_HANDLE_VALUE,"foreign open of the target");
 g=CreateFileA(f2,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);ck(g!=INVALID_HANDLE_VALUE,"go file");CloseHandle(g);
 ck(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"writer finished");
 ck(GetExitCodeProcess(pi.hProcess,&code),"writer exit");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 ck(code==(DWORD)(100+(int)FS_READ_PENDING),"foreign handle on the rename-over is PENDING");
 is_old(); /* items before k were rolled back, journal and pins gone */
 ck(GetFileAttributesA(ROOT "\\.fstxn.batch")==INVALID_FILE_ATTRIBUTES,"journal gone");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover is a no-op");is_old();
 CloseHandle(foreign);
 ck(FsBatchReplace(r,trio,3)==FS_READ_OK,"same batch accepted once the foreign handle is closed");is_new();
 DeleteFileA(f1);DeleteFileA(f2);
 FsReadClose(r);cleanup();}
/* A foreign handle opened BEFORE the batch (no delete sharing) is refused by
   the up-front check: nothing was linked or created. */
static void sc_foreign_early(void)
{FS_READ_ROOT *r;HANDLE foreign;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open foreign early");
 foreign=CreateFileA(ROOT "\\r3",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 ck(foreign!=INVALID_HANDLE_VALUE,"foreign open");
 ck(FsBatchReplace(r,trio,3)==FS_READ_DENIED,"foreign handle held before the batch is DENIED");
 is_old();CloseHandle(foreign);
 FsReadClose(r);cleanup();}
/* A foreign file with the right bytes under the old pin name must not be
   accepted: recovery compares the FileId, refuses, and changes nothing. */
static void sc_foreign_pin(const char *exe)
{FS_READ_ROOT *r;WIN32_FIND_DATAA d;HANDLE f;char pin[512]="",b[16]={0};FILE *in;
 fixture();put_old();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open foreign pin");
 ck(run_child(exe,26,0)==106,"writer killed at 26");
 f=FindFirstFileA(ROOT "\\.fsrp-*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{char p[512];snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);
   in=fopen(p,"rb");memset(b,0,sizeof(b));if(in){size_t n=fread(b,1,15,in);fclose(in);if(n==7&&!memcmp(b,"one-old",7))strcpy(pin,p);}
  }while(FindNextFileA(f,&d));FindClose(f);}
 ck(pin[0],"found the old pin of r1");
 put(ROOT "\\foreign.tmp","one-old");
 ck(MoveFileExA(ROOT "\\foreign.tmp",pin,MOVEFILE_REPLACE_EXISTING),"swap in a foreign pin");
 ck(FsBatchRecover(r)==FS_READ_DENIED,"foreign pin denied");
 is_file(ROOT "\\r1","ONE-NEW-LONGER",0);is_file(pin,"one-old",0);
 ck(count_skip(ROOT "\\*",".fsrb-")==count(ROOT "\\*"),"no rollback name left behind");
 FsReadClose(r);nuke();}
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
static void mutant_child(const char *exe,int m)
{char v[8];sprintf(v,"%d",m);_putenv_s("FS_WIN_BATCH_MUTANT",v);
 if(m==6)sc_hardlink();            /* single-link check removed */
 else if(m==7)sc_crash(exe,26);    /* replace roll-forward without a marker */
 else if(m==8)sc_reccrash(exe,26,30);/* rollback in ascending order */
 else if(m==10)sc_early(exe,21);   /* orphan sweep skipped */
 else sc_foreign_pin(exe);         /* pin accepted by name, no FileId */
 ExitProcess(0);}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;
 if(argc==4&&!strcmp(argv[1],"rchild")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 if(argc==4&&!strcmp(argv[1],"hchild")){hchild(atoi(argv[2]),argv[3]);return 200;}
 if(argc==3&&!strcmp(argv[1],"rmutant")){got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");mutant_child(exe,atoi(argv[2]));return 0;}
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
 /* Writer crash matrix 21-28. */
 for(int pt=21;pt<=23;pt++)sc_early(exe,pt);
 for(int pt=24;pt<=28;pt++)sc_crash(exe,pt);
 /* Recovery crashes: rollback 29-33 from writer points 25 and 26, roll-forward
    34-36 from 27. */
 for(int sp=25;sp<=26;sp++)for(int rp=29;rp<=33;rp++)sc_reccrash(exe,sp,rp);
 for(int rp=34;rp<=36;rp++)sc_reccrash(exe,27,rp);
 /* Foreign process: handle opened before the batch, and after our own handle
    on target k was closed (items before k must be rolled back). */
 sc_foreign_early();
 for(int k=0;k<3;k++)sc_foreign_handle(exe,k);
 sc_foreign_pin(exe);
 /* Mutants: each must fail the scenario that targets it. */
 for(int m=6;m<=10;m++){char a[32];int code;sprintf(a,"rmutant %d",m);code=run_proc(exe,a);nuke();
  if(code!=1){fprintf(stderr,"mutant %d SURVIVED (exit %d)\n",m,code);exit(1);}}
 printf("test_fs_win_batch_replace ok\n");
 return 0;
}
#else
int main(void){return 0;}
#endif
