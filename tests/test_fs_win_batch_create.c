#ifdef _WIN32
#include "fs_batch.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winbatch_scratch"
/* W2/W3: FsBatchCreate and FsBatchRecover on NTFS. Writer crash points 1-7,
   recovery crash points 8-13 (rollback 8-11, roll-forward 12-13), a foreign
   stage under a journaled name, the STATUS_DELETE_PENDING mapping, and four
   mutants that the same scenarios must kill. Crashes are real process exits
   (ExitProcess 80+point) in child processes. */
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
static int run_child(const char *exe,int point,int recovery)
{char a[64];sprintf(a,"child %d %d",point,recovery);return run_proc(exe,a);}
static const FS_BATCH_CREATE three[3]={
 {"a.txt","AAA",3,0444},{"sub/b.txt","BBBB",4,0644},{"c.txt","C",1,0444}};
static void child(int point,int recovery)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",point);_putenv_s("FS_WIN_BATCH_CRASH",v);
 s=recovery?FsBatchRecover(r):FsBatchCreate(r,three,3);
 fprintf(stderr,"batch child point=%d recovery=%d status=%d\n",point,recovery,(int)s);
 FsReadClose(r);ExitProcess(200);}
static void created_all(void)
{is_file(ROOT "\\a.txt","AAA",1);is_file(ROOT "\\sub\\b.txt","BBBB",0);
 is_file(ROOT "\\c.txt","C",1);
 ck(count(ROOT "\\*")==3,"root holds only a.txt c.txt sub and nothing else");
 ck(count(ROOT "\\sub\\*")==1,"sub holds only b.txt");}
static void nothing(void)
{ck(count(ROOT "\\*")==1,"root holds only sub");ck(count(ROOT "\\sub\\*")==0,"sub empty");}
static void nuke(void)
{WIN32_FIND_DATAA d;HANDLE f;char p[512];
 f=FindFirstFileA(ROOT "\\sub\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")){
   snprintf(p,sizeof(p),ROOT "\\sub\\%s",d.cFileName);SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}}while(FindNextFileA(f,&d));FindClose(f);}
 f=FindFirstFileA(ROOT "\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")){
   snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}}while(FindNextFileA(f,&d));FindClose(f);}
 _rmdir(ROOT "\\sub");_rmdir(ROOT);}
/* Scenarios. Each runs on a fresh fixture, uses ck for every check, and is
   also the body a mutant must fail. */
static void sc_matrix(const char *exe,int pt)
{FS_READ_ROOT *r;int forward=pt>=6;
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open matrix");
 ck(run_child(exe,pt,0)==80+pt,"writer killed at point");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
 if(forward)created_all();else nothing();
 ck(FsCreateRecover(r)==FS_READ_OK,"single create recovery sees nothing");
 FsReadClose(r);cleanup();}
/* Writer killed at setup_pt, then the recovery itself killed at rec_pt, then
   a full recovery. Pins may be orphaned once the journal is gone (11). */
static void sc_reccrash(const char *exe,int setup_pt,int rec_pt)
{FS_READ_ROOT *r;
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open recovery crash");
 ck(run_child(exe,setup_pt,0)==80+setup_pt,"setup writer killed");
 ck(run_child(exe,rec_pt,1)==80+rec_pt,"recovery killed at point");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover after recovery crash");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
 if(setup_pt>=6)created_all();else nothing();
 FsReadClose(r);cleanup();}
/* A foreign file under a journaled stage name must stop recovery, untouched. */
static void sc_foreign_stage(const char *exe)
{FS_READ_ROOT *r;WIN32_FIND_DATAA d;HANDLE f;char p[512]="";
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open foreign stage");
 ck(run_child(exe,3,0)==83,"writer killed at 3");
 f=FindFirstFileA(ROOT "\\.fst-*",&d);ck(f!=INVALID_HANDLE_VALUE,"find stage");
 snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);FindClose(f);
 SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);ck(DeleteFileA(p),"remove stage name");
 put(p,"foreign");
 ck(FsBatchRecover(r)==FS_READ_DENIED,"foreign stage denied");
 {char b[16]={0};FILE *in=fopen(p,"rb");ck(in&&fread(b,1,7,in)==7&&!memcmp(b,"foreign",7),"foreign file intact");fclose(in);}
 ck(GetFileAttributesA(ROOT "\\.fstxn.batch")!=INVALID_FILE_ATTRIBUTES,"journal kept");
 ck(GetFileAttributesA(ROOT "\\a.txt")==INVALID_FILE_ATTRIBUTES,"nothing published");
 FsReadClose(r);cleanup();}
/* Publish of item 1 fails after item 0 was published: the in-process rollback
   must leave a tree that accepts the same batch again immediately. With a
   stage handle still open on the published inode (mutant 4) the rollback can
   only mark a.txt delete-pending, and the retry finds that name still taken
   (0xc0000056, PENDING) instead of free. */
static void sc_inject_retry(void)
{FS_READ_ROOT *r;
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open inject retry");
 ck(_putenv_s("FS_WIN_BATCH_FAIL_PUBLISH","1")==0,"set injection");
 ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"publish failure reported");
 ck(_putenv_s("FS_WIN_BATCH_FAIL_PUBLISH","99")==0,"clear injection");
 nothing();
 ck(FsBatchRecover(r)==FS_READ_OK,"recover after rollback");nothing();
 ck(FsBatchCreate(r,three,3)==FS_READ_OK,"same batch accepted right after the rollback");
 created_all();
 FsReadClose(r);cleanup();}
/* Mutants are enabled by FS_WIN_BATCH_MUTANT in a child; the child fails
   (exit 1 through ck) when its checks catch the break, and exits 0 when they
   do not. The parent requires the failure. */
static void mutant_child(const char *exe,int m)
{char v[8];sprintf(v,"%d",m);_putenv_s("FS_WIN_BATCH_MUTANT",v);
 if(m==1)sc_matrix(exe,5);          /* journal only treated as committed */
 else if(m==2)sc_foreign_stage(exe);/* stage deleted before identity check */
 else if(m==3)sc_reccrash(exe,6,13);/* marker retired before journal */
 else if(m==10)sc_matrix(exe,1);   /* orphan sweep skipped */
 else sc_inject_retry();            /* rollback with stage handles open */
 ExitProcess(0);}
static void delete_pending(void)
{FS_READ_ROOT *r;HANDLE h;FILE_DISPOSITION_INFO di;
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open delete pending");
 put(ROOT "\\dp","x");
 ck(FsWinBatchTestOpenStatus(r,"dp")==FS_READ_OK,"existing name opens");
 ck(FsWinBatchTestOpenStatus(r,"nope")==FS_READ_MISSING,"missing name is MISSING");
 h=CreateFileA(ROOT "\\dp",GENERIC_READ|DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
               NULL,OPEN_EXISTING,0,NULL);
 ck(h!=INVALID_HANDLE_VALUE,"holder");
 di.DeleteFile=TRUE;ck(SetFileInformationByHandle(h,FileDispositionInfo,&di,sizeof(di)),"delete by handle");
 ck(FsWinBatchTestOpenStatus(r,"dp")==FS_READ_PENDING,"delete-pending name is PENDING");
 CloseHandle(h);
 ck(FsWinBatchTestOpenStatus(r,"dp")==FS_READ_MISSING,"gone once the holder closes");
 FsReadClose(r);cleanup();}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 if(argc==3&&!strcmp(argv[1],"mutant")){got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");mutant_child(exe,atoi(argv[2]));return 0;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 /* Success across directories, read-only mode honoured, nothing left over. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
 ck(FsBatchCreate(r,three,3)==FS_READ_OK,"batch create");
 created_all();
 ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"existing targets denied");
 created_all();
 ck(FsBatchRecover(r)==FS_READ_OK,"recover on a clean tree");created_all();
 FsReadClose(r);cleanup();
 /* Two files in one directory. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open two");
 {FS_BATCH_CREATE two[2]={{"x1","one",3,0644},{"x2","two",3,0644}};
  ck(FsBatchCreate(r,two,2)==FS_READ_OK,"two files");
  is_file(ROOT "\\x1","one",0);is_file(ROOT "\\x2","two",0);
  ck(count(ROOT "\\*")==3,"two files and sub only");}
 FsReadClose(r);cleanup();
 /* Rejections leave the tree untouched. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open invalid");
 {FS_BATCH_CREATE bad[2]={{"ok1","x",1,0644},{"../escape","x",1,0644}};
  ck(FsBatchCreate(r,bad,2)==FS_READ_INVALID,"traversal");nothing();}
 {FS_BATCH_CREATE dup[2]={{"d","x",1,0644},{"D","x",1,0644}};
  ck(FsBatchCreate(r,dup,2)==FS_READ_INVALID,"case-insensitive duplicate");nothing();}
 {FS_BATCH_CREATE one[1]={{"solo","x",1,0644}};
  ck(FsBatchCreate(r,one,1)==FS_READ_INVALID,"one entry");nothing();}
 {FS_BATCH_CREATE miss[2]={{"ok2","x",1,0644},{"nodir/f","x",1,0644}};
  ck(FsBatchCreate(r,miss,2)!=FS_READ_OK,"missing parent");nothing();}
 {FS_BATCH_CREATE res[2]={{"ok3","x",1,0644},{".fstxn.batch","x",1,0644}};
  ck(FsBatchCreate(r,res,2)==FS_READ_INVALID,"reserved name");nothing();}
 put(ROOT "\\sub\\there","t");
 {FS_BATCH_CREATE ex[2]={{"fresh","x",1,0644},{"sub/there","x",1,0644}};
  ck(FsBatchCreate(r,ex,2)==FS_READ_DENIED,"existing target");
  ck(count(ROOT "\\*")==1&&count(ROOT "\\sub\\*")==1,"existing target left no residue");
  is_file(ROOT "\\sub\\there","t",0);}
 FsReadClose(r);cleanup();
 /* Publish of the second item refused after the first was published: the
    in-process rollback restores the tree. Needs a missing-until-then target. */
 sc_inject_retry();
 /* Writer crash matrix, points 1-7. */
 for(int pt=1;pt<=7;pt++)sc_matrix(exe,pt);
 /* A pending batch blocks a new one until recovery. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open blocked");
 ck(run_child(exe,5,0)==85,"writer killed at 5");
 ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"pending batch blocks a new one");
 ck(FsBatchRecover(r)==FS_READ_OK,"recover blocked");nothing();
 ck(FsBatchCreate(r,three,3)==FS_READ_OK,"create after recovery");created_all();
 FsReadClose(r);cleanup();
 /* Recovery crash matrix: rollback 8-11 from a writer killed at 5, roll-forward
    12-13 from a writer killed at 6. */
 for(int rp=8;rp<=11;rp++)sc_reccrash(exe,5,rp);
 for(int rp=12;rp<=13;rp++)sc_reccrash(exe,6,rp);
 sc_foreign_stage(exe);
 delete_pending();
 /* Mutants: each must fail the scenario that targets it. */
 for(int mi=0;mi<5;mi++){
   int m=mi<4?mi+1:10;char a[32];int code;sprintf(a,"mutant %d",m);
   code=run_proc(exe,a);nuke();
   if(code!=1){fprintf(stderr,"mutant %d SURVIVED (exit %d)\n",m,code);exit(1);}
 }
 printf("test_fs_win_batch_create ok\n");
 return 0;
}
#else
int main(void){return 0;}
#endif
