#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Criterion 1, Windows: races between cooperating writers beyond the create
   and copy-vs-move cases in test_fs_race.c. Every check is an INVARIANT that
   must hold whatever the interleaving; none of them asserts which contender
   wins. The loser statuses are hypotheses (probably DENIED, stale bytes or a
   missing source) and are only required to be "not OK".

   Expected before measuring, all on windows-latest NTFS, one runner:
   A replace: 6 processes replace the same file, same expected bytes, different
     replacements. Exactly one OK (the writers serialize on the workspace lock
     and only the first sees the expected bytes); the file holds the winner's
     replacement; no stage, pin or journal name left; FsReplaceRecover is OK.
   B remove: 6 processes remove the same file. At most one OK; if one is OK the
     file is gone, if none is OK the file is still there with its bytes. I
     expect exactly one, but that outcome is not asserted.
   C move vs remove of one source: never both OK; the bytes exist in at most
     one place; the state agrees with whichever is OK.
   D create vs batch create on one target: never both OK; the target holds
     exactly the winner's bytes; a failed batch leaves its other entry absent
     (atomic), a successful one has both.
   E parent swap, the check-then-use shape: a directory D is flipped in a loop
     between a real directory and a junction to an OUTSIDE directory while
     writer threads create, replace, remove and batch-create through D. Whatever
     interleaving happens, the outside directory never gains, loses or changes a
     file. The junction state must be verified present at least 5 times (guard
     against a swapper that never swaps) and at least one writer call must
     succeed (guard against writers that always fail). This is an invariant held
     over K iterations in a time budget, never a proof that no interleaving
     exists; K and the swap count are printed. If it turns flaky in CI the
     budget is cut and the cut is declared. A write outside is a finding. */
#define ROOT "test_fs_winrace_ws"
#define OUTD "test_fs_winrace_out"
#define ROUNDS 8
#define PROCS 6
#define STATUS_BASE 10
#define CODE(s) ((DWORD)(STATUS_BASE+(int)(s)))
#define SWAP_BUDGET_MS 5000
static void ck(int x,const char *m){if(!x){fprintf(stderr,"FAIL %s (%lu)\n",m,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static int has_bytes(const char *p,const char *v)
{char b[64]={0};FILE *f=fopen(p,"rb");size_t n=strlen(v);int ok;
 if(!f)return 0;ok=fread(b,1,sizeof(b)-1,f)==n&&!memcmp(b,v,n);fclose(f);return ok;}
static int exists(const char *p){return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;}
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
/* Child: op 0 replace(k), 1 remove, 2 move t->u, 3 create n, 4 batch create. */
static void child(int op,int k)
{FS_READ_ROOT *r;FS_READ_STATUS s;char rep[4];
 if(FsReadOpen(ROOT,&r)!=FS_READ_OK)ExitProcess(90);
 snprintf(rep,sizeof(rep),"N%d",k);
 switch(op){
  case 0:s=FsReplaceFile(r,"t","ORIG",4,rep,2);break;
  case 1:s=FsRemoveFile(r,"t","ORIG",4);break;
  case 2:s=FsMoveFile(r,"t","u","ORIG",4);break;
  case 3:s=FsCreateFile(r,"n","CR",2,0666);break;
  default:{FS_BATCH_CREATE b[2]={{"b0","B0",2,0666},{"n","BN",2,0666}};s=FsBatchCreate(r,b,2);}
 }
 FsReadClose(r);ExitProcess((UINT)(STATUS_BASE+(int)s));}
static void spawn(const char *exe,int op,int k,HANDLE *out)
{char cmd[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
 snprintf(cmd,sizeof(cmd),"\"%s\" child %d %d",exe,op,k);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn child");
 CloseHandle(pi.hThread);*out=pi.hProcess;}
/* A child exit code outside STATUS_BASE..STATUS_BASE+19 is not a writer status
   (90 is "could not open the root"), so it is a failure, never a loser. */
static void collect(HANDLE *procs,int n,DWORD *codes)
{ck(WaitForMultipleObjects((DWORD)n,procs,TRUE,60000)==WAIT_OBJECT_0,"wait children");
 for(int k=0;k<n;k++){ck(GetExitCodeProcess(procs[k],&codes[k]),"exit code");CloseHandle(procs[k]);
  ck(codes[k]>=STATUS_BASE&&codes[k]<STATUS_BASE+20,"child exit code is a writer status");}}
/* Fail on any name in the workspace that is not one of the fixture names. */
static void only_fixture_names(const char *what)
{WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(ROOT "\\*",&d);char m[160];
 ck(h!=INVALID_HANDLE_VALUE,"list");
 do{const char *n=d.cFileName;
  if(!strcmp(n,".")||!strcmp(n,"..")||!strcmp(n,".fstxn.lock")||!strcmp(n,"t")||
     !strcmp(n,"u")||!strcmp(n,"n")||!strcmp(n,"b0")||!strcmp(n,"D"))continue;
  snprintf(m,sizeof(m),"%s: leftover name %s",what,n);ck(0,m);
 }while(FindNextFileA(h,&d));FindClose(h);}
static void recovers(FS_READ_ROOT *r,const char *what)
{char m[96];snprintf(m,sizeof(m),"%s: recovery clean",what);
 ck(FsReplaceRecover(r)==FS_READ_OK&&FsRemoveRecover(r)==FS_READ_OK&&
    FsMoveRecover(r)==FS_READ_OK&&FsCreateRecover(r)==FS_READ_OK&&
    FsBatchRecover(r)==FS_READ_OK,m);}
static void reset(void)
{DeleteFileA(ROOT "\\t");DeleteFileA(ROOT "\\u");DeleteFileA(ROOT "\\n");DeleteFileA(ROOT "\\b0");
 put(ROOT "\\t","ORIG");}
/* ---- E: parent swap ---- */
static volatile LONG g_stop,g_swaps,g_junctions,g_ok,g_calls;
static void flip_to_junction(void)
{char abs[MAX_PATH],cmd[2*MAX_PATH+128];DWORD n=GetFullPathNameA(OUTD,MAX_PATH,abs,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"" ROOT "\\D\" \"%s\" >NUL 2>&1",abs);
 system(cmd);}
static void clear_dir(const char *dir)
{WIN32_FIND_DATAA d;char pat[MAX_PATH],p[MAX_PATH];HANDLE h;
 /* Never delete through a junction: only the swapper changes D, so this
    check cannot race with a flip. */
 {DWORD a=GetFileAttributesA(dir);if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))return;}
 snprintf(pat,sizeof(pat),"%s\\*",dir);h=FindFirstFileA(pat,&d);
 if(h==INVALID_HANDLE_VALUE)return;
 do{if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){
   snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);DeleteFileA(p);}}while(FindNextFileA(h,&d));
 FindClose(h);}
static DWORD WINAPI swapper(LPVOID p)
{(void)p;
 while(!g_stop){
  /* real directory -> junction */
  clear_dir(ROOT "\\D");
  if(RemoveDirectoryA(ROOT "\\D")){
   flip_to_junction();
   {DWORD a=GetFileAttributesA(ROOT "\\D");
    if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))InterlockedIncrement(&g_junctions);}
   Sleep(15);
   /* junction -> real directory */
   if(RemoveDirectoryA(ROOT "\\D"))_mkdir(ROOT "\\D");
   InterlockedIncrement(&g_swaps);
  }else Sleep(5);
 }
 /* leave a real directory behind */
 if(!exists(ROOT "\\D"))_mkdir(ROOT "\\D");
 return 0;}
static DWORD WINAPI writer(LPVOID p)
{FS_READ_ROOT *r=(FS_READ_ROOT*)p;
 while(!g_stop){
  FS_READ_STATUS s[6];
  FS_BATCH_CREATE b[2]={{"D/b1","x",1,0666},{"D/b2","y",1,0666}};
  s[0]=FsCreateFile(r,"D/w","v1",2,0666);
  s[1]=FsReplaceFile(r,"D/w","v1",2,"v2",2);
  s[2]=FsRemoveFile(r,"D/w","v2",2);
  s[3]=FsBatchCreate(r,b,2);
  s[4]=FsRemoveFile(r,"D/b1","x",1);
  s[5]=FsRemoveFile(r,"D/b2","y",1);
  for(int i=0;i<6;i++){InterlockedIncrement(&g_calls);if(s[i]==FS_READ_OK)InterlockedIncrement(&g_ok);}
  Sleep(1);
 }
 return 0;}
static void outside_intact(const char *when)
{WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(OUTD "\\*",&d);int n=0;char m[96];
 ck(h!=INVALID_HANDLE_VALUE,"list outside");
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,".."))n++;}while(FindNextFileA(h,&d));
 FindClose(h);
 snprintf(m,sizeof(m),"%s: outside entry count",when);ck(n==1,m);
 snprintf(m,sizeof(m),"%s: outside victim bytes",when);ck(has_bytes(OUTD "\\victim","VICTIM"),m);}
int main(int argc,char **argv)
{FS_READ_ROOT *r;char exe[768];DWORD got;HANDLE procs[PROCS];DWORD codes[PROCS];
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]));return 90;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe path");
 rmtree(ROOT);rmtree(OUTD);
 ck(_mkdir(ROOT)==0&&_mkdir(OUTD)==0,"mkdir");put(OUTD "\\victim","VICTIM");
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
 /* guard: a plain replace works in this fixture */
 put(ROOT "\\t","ORIG");ck(FsReplaceFile(r,"t","ORIG",4,"GUAR",4)==FS_READ_OK,"guard replace");
 for(int round=0;round<ROUNDS;round++){
  int ok=0,win=-1;char m[64];
  /* A */
  reset();for(int k=0;k<PROCS;k++)spawn(exe,0,k,&procs[k]);collect(procs,PROCS,codes);
  for(int k=0;k<PROCS;k++){if(codes[k]==CODE(FS_READ_OK)){ok++;win=k;}
}
  ck(ok==1,"A: exactly one replace winner");
  {char want[4];snprintf(want,sizeof(want),"N%d",win);ck(has_bytes(ROOT "\\t",want),"A: file holds the winner's bytes");}
  only_fixture_names("A");recovers(r,"A");
  /* B */
  reset();ok=0;for(int k=0;k<PROCS;k++)spawn(exe,1,k,&procs[k]);collect(procs,PROCS,codes);
  for(int k=0;k<PROCS;k++)if(codes[k]==CODE(FS_READ_OK))ok++;
  ck(ok<=1,"B: at most one remove OK");
  snprintf(m,sizeof(m),"B: state agrees with %d OK",ok);
  ck(ok==1?!exists(ROOT "\\t"):has_bytes(ROOT "\\t","ORIG"),m);
  only_fixture_names("B");recovers(r,"B");
  /* C */
  reset();spawn(exe,2,0,&procs[0]);spawn(exe,1,0,&procs[1]);collect(procs,2,codes);
  {int mv=codes[0]==CODE(FS_READ_OK),rm=codes[1]==CODE(FS_READ_OK);
   int at_t=has_bytes(ROOT "\\t","ORIG"),at_u=has_bytes(ROOT "\\u","ORIG");
   ck(!(mv&&rm),"C: move and remove both OK");
   ck(!(at_t&&at_u),"C: bytes in two places");
   if(mv)ck(at_u&&!exists(ROOT "\\t"),"C: move OK but state disagrees");
   if(rm)ck(!at_t&&!at_u,"C: remove OK but state disagrees");
   if(!mv&&!rm)ck(at_t&&!at_u,"C: neither OK but source changed");}
  only_fixture_names("C");recovers(r,"C");
  /* D */
  reset();spawn(exe,3,0,&procs[0]);spawn(exe,4,0,&procs[1]);collect(procs,2,codes);
  {int cr=codes[0]==CODE(FS_READ_OK),bt=codes[1]==CODE(FS_READ_OK);
   ck(!(cr&&bt),"D: create and batch create both OK");
   if(cr)ck(has_bytes(ROOT "\\n","CR")&&!exists(ROOT "\\b0"),"D: create won, state disagrees");
   if(bt)ck(has_bytes(ROOT "\\n","BN")&&has_bytes(ROOT "\\b0","B0"),"D: batch won, state disagrees");
   if(!cr&&!bt)ck(!exists(ROOT "\\n")&&!exists(ROOT "\\b0"),"D: neither OK but files appeared");}
  only_fixture_names("D");recovers(r,"D");
 }
 /* E */
 {HANDLE th[3];DWORD t0=GetTickCount();
  ck(_mkdir(ROOT "\\D")==0,"mkdir D");
  th[0]=CreateThread(NULL,0,swapper,NULL,0,NULL);
  th[1]=CreateThread(NULL,0,writer,r,0,NULL);
  th[2]=CreateThread(NULL,0,writer,r,0,NULL);
  ck(th[0]&&th[1]&&th[2],"threads");
  while(GetTickCount()-t0<SWAP_BUDGET_MS){outside_intact("during swap");Sleep(50);}
  InterlockedExchange(&g_stop,1);
  WaitForMultipleObjects(3,th,TRUE,60000);
  for(int i=0;i<3;i++)CloseHandle(th[i]);
  outside_intact("after swap");
  ck(g_junctions>=5,"E: junction state verified at least 5 times");
  ck(g_ok>0,"E: at least one writer call succeeded");
  printf("fs win race ok: parent swap invariant held over %ld writer calls (%ld OK), %ld swaps, %ld verified junction phases\n",
         (long)g_calls,(long)g_ok,(long)g_swaps,(long)g_junctions);}
 FsReadClose(r);rmtree(ROOT);rmtree(OUTD);
 return 0;
}
#else
int main(void){return 0;}
#endif
