#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* Criterion 4, Windows: a file held by a foreign handle (NTFS, one runner).

   What the source says (read, not measured), src/fs_create_win.inc: replace,
   remove and move open the existing file with GENERIC_READ|DELETE (lines 1005
   and 1127), copy opens its source with GENERIC_READ, and every open is made
   with WC_SHARE (read, write, delete). A foreign handle therefore blocks a
   writer only through the share mode it was opened with. Every writer takes
   the workspace lock first (wc_lock, line 465): it opens ".fstxn.lock" and
   calls LockFileEx exclusive WITHOUT LOCKFILE_FAIL_IMMEDIATELY, so a foreign
   byte-range lock on that file makes the writer wait, not fail.

   Fixture: a workspace holding F ("OLD!"). A foreign handle opens F with
   GENERIC_READ and one share mode, then each of replace (to "NEW!"), remove,
   move (to M) and copy (to C) is called.
   RETRACTION. The first version (run 37167486214, base 091843d, red on
   build-test-msvc and asan-msvc with 10 mismatches in 23 cells) predicted OK
   for replace, remove and move over a foreign handle that shares delete (share
   read+delete and read+write+delete). That was wrong: all six returned
   FS_READ_PENDING (6) and left their journal; I had flagged these as the least
   certain cells. The same run showed a fault of my test expectation: it
   required the replace, remove and move recoveries ALL to return OK, but a
   recovery for another operation's journal returned FS_READ_DENIED (3) (remove
   cells: replace recovery 3; move cells: replace and remove recovery 3), only
   the matching recovery returned OK. The other 16 cells matched (all refusals,
   all copies, the three lock file cells).
   Predictions for this version, stated before the measurement (OK means
   FS_READ_OK, "no" a status other than OK that is not PENDING, PENDING is
   FS_READ_PENDING exactly; the code of a refusal is not asserted):
   - share none:            replace no, remove no, move no, copy no
   - share read:            replace no, remove no, move no, copy OK
   - share read+write:      replace no, remove no, move no, copy OK
   - share read+delete:     replace PENDING, remove PENDING, move PENDING, copy OK
   - share read+write+delete: replace PENDING, remove PENDING, move PENDING, copy OK
   After the foreign handle is closed:
   - the recoveries of the OTHER two operations are called first, and they must
     leave the workspace listing (names and sizes, journals included) exactly
     as it was: a DENIED recovery leaves the journal intact (their statuses are
     not asserted);
   - the MATCHING recovery returns OK;
   - a "no" cell is then in the old state (F "OLD!" and nothing else); a
     PENDING cell is in the FINISHED state of its operation (replace: F "NEW!";
     remove: nothing; move: M "OLD!"), because I predict recovery rolls forward
     once the file is no longer held. This end-state prediction is a guess, not
     read from the source; a wrong one is named in the output. OK requires the
     finished state, copy cells end with F and C "OLD!".
   Lock file cells (the foreign handle is opened with the workspace lock file
   as the writer expects it, an empty file named .fstxn.lock):
   - a foreign exclusive LockFileEx on byte 0, held 1500 ms: a create in a
     worker thread is still waiting after 1500 ms (it does not fail), and after
     the lock is released it returns OK and the file exists;
   - a foreign handle on the lock file opened with share none: a create is
     refused (status other than OK) within 5 seconds and nothing is created.
   All mismatches are collected and printed, then the test exits 1, so one run
   shows every wrong prediction. Not claimed: foreign handles with write
   access, memory-mapped files, a handle held by another account, the exact
   status code of any refusal, other volumes, other runners. No mutant: the
   behaviour is the Windows share-mode arithmetic of the kernel, not one macro
   in src. */
#define WS "test_fs_winlock_ws"
static int rm_rec(const char *dir)
{char pat[MAX_PATH+4],p[MAX_PATH];WIN32_FIND_DATAA d;HANDLE f;int ok=1;
 snprintf(pat,sizeof(pat),"%s\\*",dir);
 f=FindFirstFileA(pat,&d);
 if(f!=INVALID_HANDLE_VALUE){
  do{
   if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
   snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);
   if((d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){
    if(!rm_rec(p))ok=0;
   }else{
    SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);
    if(!DeleteFileA(p)){fprintf(stderr,"rmtree: DeleteFile %s failed (%lu)\n",p,(unsigned long)GetLastError());ok=0;}
   }
  }while(FindNextFileA(f,&d));
  FindClose(f);}
 SetFileAttributesA(dir,FILE_ATTRIBUTE_NORMAL);
 if(!RemoveDirectoryA(dir)){fprintf(stderr,"rmtree: RemoveDirectory %s failed (%lu)\n",dir,(unsigned long)GetLastError());ok=0;}
 return ok;}
static void rmtree(const char *rel)
{char full[MAX_PATH];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH||GetFileAttributesA(full)==INVALID_FILE_ATTRIBUTES)return;
 rm_rec(full);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,(unsigned long)GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static int bytes_are(const char *path,const char *v)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n=strlen(v);int ok;
 if(!f)return 0;ok=fread(b,1,sizeof(b)-1,f)==n&&!memcmp(b,v,n);fclose(f);return ok;}
static int exists(const char *p){return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;}
/* entries of the workspace without the workspace lock */
static int entries(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(WS "\\*",&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return -1;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&strcmp(d.cFileName,".fstxn.lock"))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static int mism,cells;
static void note(const char *cell,const char *what){fprintf(stderr,"MISMATCH %s: %s\n",cell,what);mism++;}
static void fixture(FS_READ_ROOT **r)
{rmtree(WS);ck(_mkdir(WS)==0,"mkdir");put(WS "\\F","OLD!");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static const char *OPN[4]={"replace","remove","move","copy"};
static FS_READ_STATUS run_op(FS_READ_ROOT *r,int op)
{switch(op){
 case 0:return FsReplaceFile(r,"F","OLD!",4,"NEW!",4);
 case 1:return FsRemoveFile(r,"F","OLD!",4);
 case 2:return FsMoveFile(r,"F","M","OLD!",4);
 default:return FsCopyFile(r,"F","C","OLD!",4);}}
static int is_old(void){return bytes_are(WS "\\F","OLD!")&&entries()==1;}
static int is_done(int op)
{switch(op){
 case 0:return bytes_are(WS "\\F","NEW!")&&entries()==1;
 case 1:return !exists(WS "\\F")&&entries()==0;
 case 2:return !exists(WS "\\F")&&bytes_are(WS "\\M","OLD!")&&entries()==1;
 default:return bytes_are(WS "\\F","OLD!")&&bytes_are(WS "\\C","OLD!")&&entries()==2;}}
/* names and sizes of every entry of the workspace, the lock file excluded */
static void snap(char *out,size_t cap)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(WS "\\*",&d);size_t k=0;
 out[0]=0;if(f==INVALID_HANDLE_VALUE)return;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&strcmp(d.cFileName,".fstxn.lock")){
     int w=snprintf(out+k,cap-k,"%s:%lu;",d.cFileName,(unsigned long)d.nFileSizeLow);
     if(w<0||(size_t)w>=cap-k)break;k+=(size_t)w;}}while(FindNextFileA(f,&d));
 FindClose(f);}
static FS_READ_STATUS recover_op(FS_READ_ROOT *r,int op)
{switch(op){case 0:return FsReplaceRecover(r);case 1:return FsRemoveRecover(r);default:return FsMoveRecover(r);}}
/* expect: 0 refused (not OK, not PENDING), 1 OK, 2 PENDING */
static void cell(const char *mode,DWORD share,int op,int expect)
{FS_READ_ROOT *r;HANDLE fh;FS_READ_STATUS st,mr;char name[96],m[240],before[600],after[600];int o;
 snprintf(name,sizeof(name),"share %s, %s",mode,OPN[op]);
 fixture(&r);
 fh=CreateFileA(WS "\\F",GENERIC_READ,share,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 ck(fh!=INVALID_HANDLE_VALUE,"foreign open");
 st=run_op(r,op);
 if(expect==1?st!=FS_READ_OK:expect==2?st!=FS_READ_PENDING:(st==FS_READ_OK||st==FS_READ_PENDING)){
  snprintf(m,sizeof(m),"predicted %s, status %d",expect==1?"OK":expect==2?"PENDING":"refused",(int)st);note(name,m);}
 CloseHandle(fh);
 /* the recoveries of the other operations first: they must change nothing */
 for(o=0;o<3;o++){
  if(o==op)continue;
  snap(before,sizeof(before));
  m[0]=0;{FS_READ_STATUS x=recover_op(r,o);(void)x;}
  snap(after,sizeof(after));
  if(strcmp(before,after)){
   snprintf(m,sizeof(m),"the %s recovery changed the workspace: [%s] -> [%s]",OPN[o],before,after);note(name,m);}}
 if(op<3){
  mr=recover_op(r,op);
  if(mr!=FS_READ_OK){snprintf(m,sizeof(m),"the matching %s recovery returned %d",OPN[op],(int)mr);note(name,m);}}
 if(expect==2){
  if(!is_done(op)){snap(after,sizeof(after));
   snprintf(m,sizeof(m),"PENDING cell: the end state is not the finished state (old state %d), listing [%s]",is_old(),after);note(name,m);}}
 else if(expect==0){
  if(!is_old()){snap(after,sizeof(after));snprintf(m,sizeof(m),"refused cell: the state is not the old state, listing [%s]",after);note(name,m);}}
 else if(!is_done(op)){snap(after,sizeof(after));snprintf(m,sizeof(m),"status OK but the finished state is not there, listing [%s]",after);note(name,m);}
 FsReadClose(r);cells++;}
typedef struct{FS_READ_ROOT *r;volatile LONG done;FS_READ_STATUS st;}JOB;
static DWORD WINAPI worker(LPVOID p)
{JOB *j=(JOB*)p;j->st=FsCreateFile(j->r,"N","X",1,0666);InterlockedExchange(&j->done,1);return 0;}
int main(void)
{
 static const struct{const char *name;DWORD share;int ok[4];}M[5]={
  {"none",0,{0,0,0,0}},
  {"read",FILE_SHARE_READ,{0,0,0,1}},
  {"read+write",FILE_SHARE_READ|FILE_SHARE_WRITE,{0,0,0,1}},
  {"read+delete",FILE_SHARE_READ|FILE_SHARE_DELETE,{2,2,2,1}},
  {"read+write+delete",FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,{2,2,2,1}}};
 int i,op;
 for(i=0;i<5;i++)for(op=0;op<4;op++)cell(M[i].name,M[i].share,op,M[i].ok[op]);
 /* foreign exclusive byte-range lock on the workspace lock file */
 {FS_READ_ROOT *r;HANDLE lk,th;JOB j;OVERLAPPED o;DWORD w;
  fixture(&r);
  lk=CreateFileA(WS "\\.fstxn.lock",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  ck(lk!=INVALID_HANDLE_VALUE,"lock file open");
  memset(&o,0,sizeof(o));
  ck(LockFileEx(lk,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,&o)!=0,"foreign LockFileEx");
  memset(&j,0,sizeof(j));j.r=r;
  th=CreateThread(NULL,0,worker,&j,0,NULL);ck(th!=NULL,"thread");
  Sleep(1500);
  if(j.done){char m[80];snprintf(m,sizeof(m),"the writer returned while the lock was held, status %d",(int)j.st);note("foreign lock held, create",m);}
  cells++;
  UnlockFileEx(lk,0,1,0,&o);CloseHandle(lk);
  w=WaitForSingleObject(th,20000);
  if(w!=WAIT_OBJECT_0){note("foreign lock released, create","the writer did not return within 20 s after the release");fprintf(stderr,"FAIL stuck writer\n");exit(1);}
  CloseHandle(th);
  if(j.st!=FS_READ_OK||!bytes_are(WS "\\N","X")||entries()!=2)
   {char m[120];snprintf(m,sizeof(m),"status %d, N exists %d, entries %d",(int)j.st,exists(WS "\\N"),entries());note("foreign lock released, create",m);}
  cells++;
  FsReadClose(r);}
 /* foreign handle on the lock file with share none */
 {FS_READ_ROOT *r;HANDLE lk,th;JOB j;DWORD w;
  fixture(&r);
  lk=CreateFileA(WS "\\.fstxn.lock",GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  ck(lk!=INVALID_HANDLE_VALUE,"lock file open, share none");
  memset(&j,0,sizeof(j));j.r=r;
  th=CreateThread(NULL,0,worker,&j,0,NULL);ck(th!=NULL,"thread");
  w=WaitForSingleObject(th,5000);
  if(w!=WAIT_OBJECT_0){note("lock file share none, create","the writer was still waiting after 5 s");CloseHandle(lk);lk=INVALID_HANDLE_VALUE;
   if(WaitForSingleObject(th,20000)!=WAIT_OBJECT_0){fprintf(stderr,"FAIL stuck writer\n");exit(1);}}
  else if(j.st==FS_READ_OK){note("lock file share none, create","predicted refused, status OK");}
  else if(exists(WS "\\N")||entries()!=1){note("lock file share none, create","refused but the state changed");}
  CloseHandle(th);
  if(lk!=INVALID_HANDLE_VALUE)CloseHandle(lk);
  cells++;
  FsReadClose(r);}
 rmtree(WS);
 if(cells!=23){fprintf(stderr,"cell count %d\n",cells);return 1;}
 if(mism){fprintf(stderr,"%d mismatches in %d cells\n",mism,cells);return 1;}
 printf("fs win locked ok: %d cells\n",cells);
 return 0;
}
#else
int main(void){return 0;}
#endif
