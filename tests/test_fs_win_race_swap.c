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
/* Criterion 1, Windows (m209): a parent swapped between a real directory and a junction to an OUTSIDE directory while
   writers MOVE, COPY and BATCH-REPLACE through it. test_fs_win_race.c cell E does the same swap for create, replace,
   remove and batch create; the roadmap said the rename side (move and copy, whose source and target both name the
   parent) and the batch replace had no Windows counterpart. The outside directory holds victim "VICTIM", r1 "A1" and
   r2 "A2". Each writer thread, per iteration:
     a) FsMoveFile D/victim -> mv_<id> (expected VICTIM): the real D never holds "victim", so an OK means the junction was followed;
     b) FsCopyFile D/victim -> cp_<id>: same;
     c) a root file s1_<id> moved INTO D/m_<id>, then removed there;
     d) a root file s2_<id> copied INTO D/c_<id>, then removed there;
     e) create D/r1 "A1" and D/r2 "A2", FsBatchReplace both to "B1" and "B2" (a followed junction would change the outside r1 and r2,
        whose bytes equal the expected ones), then remove both.
   Invariants, whatever the interleaving: the outside directory keeps exactly its three entries with their bytes; a and b are
   never OK; no mv_ or cp_ file ever appears in the root. Guards against a vacuous pass: the junction state is seen at least 5
   times and at least one writer call succeeds. Counts are printed before the guards. This is an invariant over a time budget
   (SWAP_BUDGET_MS), never a proof that no interleaving exists; one runner; cooperating writers only. A write outside, or an OK
   of a or b, is a finding. Expected before measuring (written before the first run): the invariant holds; the guards pass. */
#define ROOT "test_fs_winraceswap_ws"
#define OUTD "test_fs_winraceswap_out"
#define SWAP_BUDGET_MS 5000
#define REAL_HOLD_MS 30
static volatile LONG g_stop,g_swaps,g_junctions,g_calls,g_ok,g_bad;
static volatile LONG g_opn[7],g_opok[7];
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
static void flip_to_junction(void)
{char abs[MAX_PATH],cmd[2*MAX_PATH+128];DWORD n=GetFullPathNameA(OUTD,MAX_PATH,abs,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"" ROOT "\\D\" \"%s\" >NUL 2>&1",abs);
 system(cmd);}
static void clear_dir(const char *dir)
{WIN32_FIND_DATAA d;char pat[MAX_PATH],p[MAX_PATH];HANDLE h;
 /* Never delete through a junction: only the swapper changes D, so this check cannot race with a flip. */
 {DWORD a=GetFileAttributesA(dir);if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))return;}
 snprintf(pat,sizeof(pat),"%s\\*",dir);h=FindFirstFileA(pat,&d);
 if(h==INVALID_HANDLE_VALUE)return;
 do{if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){
   snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);DeleteFileA(p);}}while(FindNextFileA(h,&d));
 FindClose(h);}
static DWORD WINAPI swapper(LPVOID p)
{(void)p;
 while(!g_stop){
  clear_dir(ROOT "\\D");
  if(RemoveDirectoryA(ROOT "\\D")){
   flip_to_junction();
   {DWORD a=GetFileAttributesA(ROOT "\\D");
    if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))InterlockedIncrement(&g_junctions);}
   Sleep(15);
   if(RemoveDirectoryA(ROOT "\\D")){_mkdir(ROOT "\\D");}
   Sleep(REAL_HOLD_MS);
   InterlockedIncrement(&g_swaps);
  }else Sleep(5);
 }
 if(!exists(ROOT "\\D"))_mkdir(ROOT "\\D");
 return 0;}
static void count(int op,FS_READ_STATUS s)
{InterlockedIncrement(&g_calls);InterlockedIncrement(&g_opn[op]);
 if(s==FS_READ_OK){InterlockedIncrement(&g_ok);InterlockedIncrement(&g_opok[op]);}}
static DWORD WINAPI writer(LPVOID p)
{LONG id=(LONG)(INT_PTR)p;FS_READ_ROOT *r;char mv[32],cp[32],s1[32],s2[32],dm[32],dc[32],pm[64],pc[64],ps1[64],ps2[64];
 FS_READ_STATUS s;
 if(FsReadOpen(ROOT,&r)!=FS_READ_OK)return 1;
 snprintf(mv,sizeof(mv),"mv_%ld",id);snprintf(cp,sizeof(cp),"cp_%ld",id);
 snprintf(s1,sizeof(s1),"s1_%ld",id);snprintf(s2,sizeof(s2),"s2_%ld",id);
 snprintf(dm,sizeof(dm),"D/m_%ld",id);snprintf(dc,sizeof(dc),"D/c_%ld",id);
 snprintf(pm,sizeof(pm),ROOT "\\%s",mv);snprintf(pc,sizeof(pc),ROOT "\\%s",cp);
 snprintf(ps1,sizeof(ps1),ROOT "\\%s",s1);snprintf(ps2,sizeof(ps2),ROOT "\\%s",s2);
 while(!g_stop){
  FS_BATCH_REPLACE b[2]={{"D/r1","A1",2,"B1",2},{"D/r2","A2",2,"B2",2}};
  s=FsMoveFile(r,"D/victim",mv,"VICTIM",6);count(0,s);if(s==FS_READ_OK||exists(pm))InterlockedIncrement(&g_bad);
  s=FsCopyFile(r,"D/victim",cp,"VICTIM",6);count(1,s);if(s==FS_READ_OK||exists(pc))InterlockedIncrement(&g_bad);
  put(ps1,"S1");s=FsMoveFile(r,s1,dm,"S1",2);count(2,s);
  if(s==FS_READ_OK){s=FsRemoveFile(r,dm,"S1",2);count(3,s);}
  DeleteFileA(ps1);
  put(ps2,"S2");s=FsCopyFile(r,s2,dc,"S2",2);count(4,s);
  if(s==FS_READ_OK){s=FsRemoveFile(r,dc,"S2",2);count(3,s);}
  DeleteFileA(ps2);
  if(id==0){
   s=FsCreateFile(r,"D/r1","A1",2,0666);
   if(s==FS_READ_OK){FsCreateFile(r,"D/r2","A2",2,0666);
    s=FsBatchReplace(r,b,2);count(5,s);
    if(FsRemoveFile(r,"D/r1","B1",2)!=FS_READ_OK)FsRemoveFile(r,"D/r1","A1",2);
    if(FsRemoveFile(r,"D/r2","B2",2)!=FS_READ_OK)FsRemoveFile(r,"D/r2","A2",2);}
  }
  Sleep(1);
 }
 FsReadClose(r);return 0;}
static void outside_intact(const char *when)
{WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(OUTD "\\*",&d);int n=0;char m[96];
 ck(h!=INVALID_HANDLE_VALUE,"list outside");
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,".."))n++;}while(FindNextFileA(h,&d));
 FindClose(h);
 snprintf(m,sizeof(m),"%s: outside entry count",when);ck(n==3,m);
 snprintf(m,sizeof(m),"%s: outside victim bytes",when);ck(has_bytes(OUTD "\\victim","VICTIM"),m);
 snprintf(m,sizeof(m),"%s: outside r1 bytes",when);ck(has_bytes(OUTD "\\r1","A1"),m);
 snprintf(m,sizeof(m),"%s: outside r2 bytes",when);ck(has_bytes(OUTD "\\r2","A2"),m);}
int main(void)
{HANDLE th[3];DWORD t0;int i;
 rmtree(ROOT);rmtree(OUTD);
 ck(_mkdir(ROOT)==0&&_mkdir(OUTD)==0&&_mkdir(ROOT "\\D")==0,"mkdir");
 put(OUTD "\\victim","VICTIM");put(OUTD "\\r1","A1");put(OUTD "\\r2","A2");
 t0=GetTickCount();
 th[0]=CreateThread(NULL,0,swapper,NULL,0,NULL);
 th[1]=CreateThread(NULL,0,writer,(LPVOID)(INT_PTR)0,0,NULL);
 th[2]=CreateThread(NULL,0,writer,(LPVOID)(INT_PTR)1,0,NULL);
 ck(th[0]&&th[1]&&th[2],"threads");
 while(GetTickCount()-t0<SWAP_BUDGET_MS){outside_intact("during swap");Sleep(50);}
 InterlockedExchange(&g_stop,1);
 WaitForMultipleObjects(3,th,TRUE,60000);
 for(i=0;i<3;i++)CloseHandle(th[i]);
 outside_intact("after swap");
 printf("race swap diag: calls %ld ok %ld swaps %ld junctions %ld bad %ld; ok per op move_out %ld copy_out %ld move_in %ld remove %ld copy_in %ld batch_replace %ld\n",
        (long)g_calls,(long)g_ok,(long)g_swaps,(long)g_junctions,(long)g_bad,(long)g_opok[0],(long)g_opok[1],
        (long)g_opok[2],(long)g_opok[3],(long)g_opok[4],(long)g_opok[5]);
 fflush(stdout);
 ck(g_bad==0,"move or copy out of D/victim succeeded or left a file in the root");
 ck(g_junctions>=5,"junction state verified at least 5 times");
 ck(g_ok>0,"at least one writer call succeeded");
 printf("fs win race swap ok: parent swap invariant held over %ld writer calls (%ld OK), %ld swaps, %ld verified junction phases\n",
        (long)g_calls,(long)g_ok,(long)g_swaps,(long)g_junctions);
 rmtree(ROOT);rmtree(OUTD);
 return 0;}
#else
int main(void){return 0;}
#endif
