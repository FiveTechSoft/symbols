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
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+160];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" (icacls \"%s\" /T /Q /remove:d *S-1-1-0 >NUL 2>&1 & attrib -R \"%s\\*\" /S /D >NUL 2>&1 & rmdir /S /Q \"\\\\?\\%s\") >NUL 2>&1",full,full,full,full);
 system(cmd);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
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
{rmtree(WS);ck(_mkdir(WS)==0&&_mkdir(WS "\\Dir")==0,"mkdir");
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
 ck(FsRemoveRecover(r)==FS_READ_OK,"remove recovery");
 ck(bytes_are(WS "\\Dir\\Ro","RO!!")&&(GetFileAttributesA(WS "\\Dir\\Ro")&FILE_ATTRIBUTE_READONLY),"remove read-only: bytes and attribute kept");
 ck(entries(WS "\\Dir\\*")==2,"remove read-only: Dir entries");root_clean("remove read-only");
 OKC(FsCreateFile(r,"Dir/After1","a",1,0666),"next writer after read-only remove");
 /* move source */
 REFUSED(FsMoveFile(r,"Dir/Ro","Dir/Moved","RO!!",4),"move read-only source");
 ck(FsMoveRecover(r)==FS_READ_OK,"move recovery");
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
 REFUSED(FsRemoveFile(r,"Dir/Ro","RO!!",4),"remove file that denies delete");
 ck(FsRemoveRecover(r)==FS_READ_OK,"remove recovery");
 ck(bytes_are(WS "\\Dir\\Ro","RO!!")&&entries(WS "\\Dir\\*")==2,"remove denied file: unchanged");root_clean("remove denied file");
 FsReadClose(r);
 /* a file that denies write-data: consistency only */
 rmtree(WS);fixture(&r);
 sys("icacls \"%s\" /deny *S-1-1-0:(WD,AD) >NUL 2>&1",WS "\\Dir\\Ro");
 {FS_READ_STATUS s=FsReplaceFile(r,"Dir/Ro","RO!!",4,"NEW!",4);
  ck(FsReplaceRecover(r)==FS_READ_OK,"replace recovery");
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
