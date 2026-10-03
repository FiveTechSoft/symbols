#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <errno.h>
/* Criterion 4, Windows: permission and attribute cases against the writers
   (NTFS, one runner). POSIX mode bits have no NTFS equivalent and are not
   claimed. Built twice: the default target covers the READONLY attribute, the
   ACL_ONLY target covers an explicit deny entry and exits 77 (ctest Skipped)
   if the deny entry has no effect for this account, so a skip is visible.

   Default target, expected result before the first measurement:
   - READONLY file as the source or target of a mutation. Single replace of a
     read-only target is already pinned by test_fs_win_replace (refused, PENDING,
     recovery clean). Here: remove and move-source of a READONLY file. The
     delete goes through SetFileInformationByHandle(FileDispositionInfo), which
     NTFS refuses for a READONLY file. PREDICTION: remove and move-source are
     not OK, the file keeps its bytes and its attribute, and after the
     operation's own recovery call the workspace root holds no journal, stage
     or pin name and the next writer works. Whether the refused call itself
     already leaves the root clean (as opposed to PENDING until recovery) is
     NOT asserted. If recovery leaves a name behind, or the file is changed, or
     the next writer is blocked, that is a finding: STOP, report, no repair.
   - copy of a READONLY source only reads it: PREDICTED OK, source untouched,
     and the target is an ordinary new file.
   - the READONLY attribute on a DIRECTORY is a marker NTFS does not enforce:
     PREDICTED create inside it is OK.
   ACL_ONLY target:
   - an explicit deny of write-data and append on a directory for Everyone:
     create, copy target, move target and batch create into it are refused (not
     OK) and nothing appears; guard: a plain CreateFileA in it fails first.
   - an explicit deny of delete on a file: remove is refused, bytes intact.
   Replace and move-source over a file with a deny-write entry: only the
   consistency is asserted (OK implies the new state, otherwise the old one,
   and no stray names), because I cannot predict whether renaming over a file
   that denies only write-data is allowed.
   Not claimed: ownership by another account, inherited ACLs, a directory the
   process cannot list, non-NTFS volumes, other runners. */
#ifdef ACL_ONLY
#define WS "test_fs_winperm_acl_ws"
#else
#define WS "test_fs_winperm_ws"
#endif
/* Remove the workspace tree with Win32 calls only (the cmd based removal of
   the previous version left the tree in place with exit 2, run 37133270380).
   Every failure prints the path and GetLastError. Deny entries are cleared
   first with icacls (needed only by the ACL cells, its result is not used). */
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
{char full[MAX_PATH],cmd[2*MAX_PATH+160];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH||GetFileAttributesA(full)==INVALID_FILE_ATTRIBUTES)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C icacls \"%s\" /T /Q /remove:d *S-1-1-0 >NUL 2>&1",full);
 system(cmd);
 rm_rec(full);}
static void dump(const char *dir)
{char pat[MAX_PATH];WIN32_FIND_DATAA d;HANDLE f;snprintf(pat,sizeof(pat),"%s\\*",dir);
 f=FindFirstFileA(pat,&d);
 if(f==INVALID_HANDLE_VALUE){fprintf(stderr,"  [%s] not listable (%lu)\n",dir,GetLastError());return;}
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,".."))fprintf(stderr,"  [%s] %s attr=0x%lx size=%lu\n",dir,d.cFileName,(unsigned long)d.dwFileAttributes,(unsigned long)d.nFileSizeLow);}while(FindNextFileA(f,&d));
 FindClose(f);}
static void fail(const char *what){DWORD e=GetLastError();
#ifdef FS_PIN_MUTANT
 printf("MC4 killed by: %s\n",what);rmtree(WS);exit(0);
#endif
fprintf(stderr,"FAIL %s (GetLastError %lu)\n",what,(unsigned long)e);dump(WS);dump(WS "\\Dir");rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void rec(const char *what,int status)
{if(status!=FS_READ_OK){char m[160];snprintf(m,sizeof(m),"%s returned status %d",what,status);fail(m);}}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static int bytes_are(const char *path,const char *v)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n=strlen(v);int ok;
 if(!f)return 0;ok=fread(b,1,sizeof(b)-1,f)==n&&!memcmp(b,v,n);fclose(f);return ok;}
static int exists(const char *p){return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;}
static int entries(const char *pattern)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pattern,&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&strcmp(d.cFileName,".fstxn.lock"))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static void sys(const char *fmt,const char *a)
{char abs[MAX_PATH],cmd[2*MAX_PATH+160];DWORD n=GetFullPathNameA(a,MAX_PATH,abs,NULL);
 ck(n>0&&n<MAX_PATH,"abs path");snprintf(cmd,sizeof(cmd),fmt,abs);ck(system(cmd)==0,cmd);}
static int cells;
#define OKC(expr,what) do{ck((expr)==FS_READ_OK,what);cells++;}while(0)
#define REFUSED(expr,what) do{ck((expr)!=FS_READ_OK,what);cells++;}while(0)
/* the root holds no journal, stage or pin name: only Dir remains */
static void root_clean(const char *what)
{char m[128];snprintf(m,sizeof(m),"%s: workspace root has only Dir",what);ck(entries(WS "\\*")==1,m);}
static void fixture(FS_READ_ROOT **r)
{int a,b;rmtree(WS);
 if(exists(WS))fprintf(stderr,"fixture: %s still exists after rmtree\n",WS);
 a=_mkdir(WS);if(a!=0){char m[96];snprintf(m,sizeof(m),"mkdir root (errno %d)",errno);fail(m);}
 b=_mkdir(WS "\\Dir");if(b!=0){char m[96];snprintf(m,sizeof(m),"mkdir Dir (errno %d)",errno);fail(m);}
 put(WS "\\Dir\\Ro","RO!!");put(WS "\\Dir\\Src","SRC");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
int main(void)
{
 FS_READ_ROOT *r;
#ifndef ACL_ONLY
 fixture(&r);
 OKC(FsCreateFile(r,"Dir/C1","c",1,0666),"control create");
 OKC(FsRemoveFile(r,"Dir/C1","c",1),"control remove");
 ck(SetFileAttributesA(WS "\\Dir\\Ro",FILE_ATTRIBUTE_READONLY),"make Ro read-only");
 /* remove */
 REFUSED(FsRemoveFile(r,"Dir/Ro","RO!!",4),"remove read-only");
 rec("remove recovery",(int)FsRemoveRecover(r));
 ck(bytes_are(WS "\\Dir\\Ro","RO!!")&&(GetFileAttributesA(WS "\\Dir\\Ro")&FILE_ATTRIBUTE_READONLY),"remove read-only: bytes and attribute kept");
 ck(entries(WS "\\Dir\\*")==2,"remove read-only: Dir entries");root_clean("remove read-only");
 OKC(FsCreateFile(r,"Dir/After1","a",1,0666),"next writer after read-only remove");
 /* move source */
 REFUSED(FsMoveFile(r,"Dir/Ro","Dir/Moved","RO!!",4),"move read-only source");
 rec("move recovery",(int)FsMoveRecover(r));
 ck(bytes_are(WS "\\Dir\\Ro","RO!!")&&!exists(WS "\\Dir\\Moved")&&(GetFileAttributesA(WS "\\Dir\\Ro")&FILE_ATTRIBUTE_READONLY),"move read-only source: unchanged");
 ck(entries(WS "\\Dir\\*")==3,"move read-only source: Dir entries");root_clean("move read-only source");
 OKC(FsCreateFile(r,"Dir/After2","b",1,0666),"next writer after read-only move");
 /* copy source */
 OKC(FsCopyFile(r,"Dir/Ro","Dir/Copy","RO!!",4),"copy read-only source");
 ck(bytes_are(WS "\\Dir\\Copy","RO!!")&&bytes_are(WS "\\Dir\\Ro","RO!!")&&(GetFileAttributesA(WS "\\Dir\\Ro")&FILE_ATTRIBUTE_READONLY),"copy read-only source: state");
 FsReadClose(r);
 /* READONLY attribute on a directory */
 fixture(&r);
 ck(SetFileAttributesA(WS "\\Dir",FILE_ATTRIBUTE_READONLY),"make Dir read-only");
 OKC(FsCreateFile(r,"Dir/InRo","x",1,0666),"create in READONLY directory");
 ck(bytes_are(WS "\\Dir\\InRo","x"),"create in READONLY directory: bytes");
 FsReadClose(r);
 ck(cells==8,"cell count");
 rmtree(WS);
 printf("fs win perm ok: %d cells\n",cells);
#ifdef FS_PIN_MUTANT
 printf("MC4 SURVIVED: the READONLY remove cells did not notice the mutant\n");return 1;
#endif
 return 0;
#else
 fixture(&r);
 OKC(FsCreateFile(r,"Dir/C1","c",1,0666),"control create");
 OKC(FsRemoveFile(r,"Dir/C1","c",1),"control remove");
 /* deny write-data and append (WD, AD) on Dir for Everyone */
 sys("icacls \"%s\" /deny *S-1-1-0:(WD,AD) >NUL 2>&1",WS "\\Dir");
 {HANDLE h=CreateFileA(WS "\\Dir\\Probe",GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);FsReadClose(r);rmtree(WS);
   printf("SKIP: the deny entry has no effect for this account\n");return 77;}}
 REFUSED(FsCreateFile(r,"Dir/New","x",1,0666),"create in denied directory");
 REFUSED(FsCopyFile(r,"Dir/Src","Dir/Copy","SRC",3),"copy into denied directory");
 REFUSED(FsMoveFile(r,"Dir/Src","Dir/Moved","SRC",3),"move into denied directory");
 {FS_BATCH_CREATE b[2]={{"Dir/B1","x",1,0666},{"Dir/B2","y",1,0666}};
  REFUSED(FsBatchCreate(r,b,2),"batch create into denied directory");}
 ck(FsCreateRecover(r)==FS_READ_OK&&FsBatchRecover(r)==FS_READ_OK&&FsMoveRecover(r)==FS_READ_OK,"recoveries");
 ck(!exists(WS "\\Dir\\New")&&!exists(WS "\\Dir\\Copy")&&!exists(WS "\\Dir\\Moved")&&!exists(WS "\\Dir\\B1")&&!exists(WS "\\Dir\\B2"),"nothing appeared in the denied directory");
 ck(bytes_are(WS "\\Dir\\Src","SRC")&&bytes_are(WS "\\Dir\\Ro","RO!!")&&entries(WS "\\Dir\\*")==2,"denied directory: contents unchanged");
 root_clean("denied directory");
 FsReadClose(r);
 /* a file that denies delete */
 rmtree(WS);fixture(&r);
 sys("icacls \"%s\" /deny *S-1-1-0:(D) >NUL 2>&1",WS "\\Dir\\Ro");
 {FILE *f=fopen(WS "\\Dir\\Ro","rb");
  if(!f)fprintf(stderr,"diag: BEFORE the remove, fopen Ro failed, errno %d, GetLastError %lu\n",errno,(unsigned long)GetLastError());
  else{fprintf(stderr,"diag: BEFORE the remove, Ro is readable\n");fclose(f);}}
 fprintf(stderr,"diag: ACL before the remove:\n");sys("icacls \"%s\" >&2",WS "\\Dir\\Ro");
 REFUSED(FsRemoveFile(r,"Dir/Ro","RO!!",4),"remove file that denies delete");
 rec("remove recovery",(int)FsRemoveRecover(r));
 fprintf(stderr,"diag: ACL after the recovery:\n");sys("icacls \"%s\" >&2",WS "\\Dir\\Ro");
 {FILE *f=fopen(WS "\\Dir\\Ro","rb");char b[16]={0};size_t n=0;
  if(!f)fprintf(stderr,"diag: fopen Ro failed, errno %d, GetLastError %lu\n",errno,(unsigned long)GetLastError());
  else{n=fread(b,1,sizeof(b)-1,f);fclose(f);fprintf(stderr,"diag: read %lu bytes from Ro: \"%s\"\n",(unsigned long)n,b);}
  fprintf(stderr,"diag: Dir entries %d\n",entries(WS "\\Dir\\*"));}
 ck(bytes_are(WS "\\Dir\\Ro","RO!!")&&entries(WS "\\Dir\\*")==2,"remove denied file: unchanged");root_clean("remove denied file");
 FsReadClose(r);
 /* a file that denies write-data: consistency only */
 rmtree(WS);fixture(&r);
 sys("icacls \"%s\" /deny *S-1-1-0:(WD,AD) >NUL 2>&1",WS "\\Dir\\Ro");
 {FS_READ_STATUS s=FsReplaceFile(r,"Dir/Ro","RO!!",4,"NEW!",4);
  rec("replace recovery",(int)FsReplaceRecover(r));
  ck(s==FS_READ_OK?bytes_are(WS "\\Dir\\Ro","NEW!"):bytes_are(WS "\\Dir\\Ro","RO!!"),"replace over deny-write file: state agrees with status");
  ck(entries(WS "\\Dir\\*")==2,"replace over deny-write file: Dir entries");root_clean("replace deny-write");cells++;}
 FsReadClose(r);
 ck(cells==8,"cell count");
 rmtree(WS);
 printf("fs win perm acl ok: %d cells\n",cells);
 return 0;
#endif
}
#else
int main(void){return 0;}
#endif
