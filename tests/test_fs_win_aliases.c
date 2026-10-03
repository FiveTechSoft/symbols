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
/* Criterion 1, Windows: path-syntax aliases against every writer (NTFS, one
   runner). Every string below is something a Windows caller can use to name a
   file in a way a plain "relative/utf8" validator may not expect. Each is
   handed to nine writer entry points (create, copy target, copy source, move
   target, move source, replace, remove, batch create, batch replace) and the
   result must be a refusal with the workspace, a real file inside it and a
   victim file OUTSIDE it unchanged.

   Expected result per string, stated before the first measurement:
   - Strings containing a colon or a backslash (device and extended-length
     prefixes \\?\ and \\.\, drive-relative C:rel, alternate data streams,
     UNC and drive-absolute paths): refused by the validator before any I/O.
     Not a measurement, a prediction of fs_read.c valid_relative and
     fs_create_win.inc wc_target_status.
   - Strings made only of the characters * ? < > | " and the quote: MEASURED,
     not assumed. wc_target_status does not reject them; the only thing that
     can is NtCreateFile. My prediction is that NTFS refuses them with
     STATUS_OBJECT_NAME_INVALID, so every cell is refused. If any writer
     accepts one, a file with that name exists, this test fails on the tree
     check, and that is a finding, not something to repair here.
   - Dot and space forms ("...", "f.", "f ", " ", "."): refused (wc_target_status).
   - Absolute paths to the outside victim in plain, \\?\ and \\.\ form: refused.
   Reading the cells: the creating entry points (create, copy target, move
   target, batch create) are the ones where a bad name could make a file appear,
   so those are the informative ones. For the source-side entry points (copy
   source, move source, replace, remove, batch replace) the named file never
   exists, so a refusal there can be a plain "missing"; they are kept because
   an accepted one would still be a defect. In the 8.3 cell the file does exist
   (the lock, empty) and the expected bytes are the empty string, so a writer
   that follows the alias would succeed and be caught.
   MC2 and MC3 build this same file with a mutant in src (valid_relative lets
   a colon through; wc_target_status stops rejecting a trailing dot or space),
   exit 0 only if the matrix catches it. Predictions, before measuring: MC3 is
   killed by the creating cells on "f.", "f ", "...", " " and "inside/...", which
   would create a file. MC2 is killed only by the two read cells on
   "inside/f::$DATA" (an existing file's default stream, which would resolve to
   the file); the writers have a second colon check in wc_target_status, so the
   writer cells cannot tell the layers apart and are not expected to kill it.
   Controls on legal paths succeed, so these refusals are not a harness that
   rejects everything. ALIAS_SHORT builds run only the 8.3 cell: the short
   name of the persistent .fstxn.lock control file, if the volume generates
   one; when it does not, the test exits 77 (ctest "Skipped"), so "not shown"
   is visible in the run. */
#if defined(ALIAS_SHORT)
#define WS "test_fs_winshort_ws"
#define OUTD "test_fs_winshort_out"
#elif defined(FS_ALIAS_MUTANT) && FS_ALIAS_MUTANT==2
#define WS "test_fs_winalias_mc2_ws"
#define OUTD "test_fs_winalias_mc2_out"
#elif defined(FS_ALIAS_MUTANT) && FS_ALIAS_MUTANT==3
#define WS "test_fs_winalias_mc3_ws"
#define OUTD "test_fs_winalias_mc3_out"
#else
#define WS "test_fs_winalias_ws"
#define OUTD "test_fs_winalias_out"
#endif
static int matrix_started;
/* Names such as "..." and "f." can be created through the NT API by a mutant
   and cannot be deleted by a plain path, so the delete uses the extended
   prefix. No ck() in here: it also runs from the failure path. */
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static void cleanup(void){rmtree(WS);rmtree(OUTD);}
/* In a mutant build a failed check after the controls is a kill (exit 0); a
   failed control stays a real failure; reaching the end is SURVIVED. */
static void fail(const char *what)
{
#if defined(FS_ALIAS_MUTANT)
 if(matrix_started){printf("MC%d killed by: %s\n",(int)FS_ALIAS_MUTANT,what);cleanup();exit(0);}
#endif
 fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static void same_bytes(const char *path,const char *v)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n=strlen(v);
 ck(f!=NULL,"open file");ck(fread(b,1,sizeof(b)-1,f)==n&&!memcmp(b,v,n),"bytes unchanged");fclose(f);}
static int count(const char *pattern)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pattern,&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&
       strcmp(d.cFileName,".fstxn.lock"))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static char label[300];
static void untouched(void)
{char m[360];
 snprintf(m,sizeof(m),"%s: victim bytes",label);same_bytes(OUTD "\\victim","VICTIM");
 snprintf(m,sizeof(m),"%s: outside entries",label);ck(count(OUTD "\\*")==1,m);
 snprintf(m,sizeof(m),"%s: real file bytes",label);same_bytes(WS "\\inside\\f","REAL");
 snprintf(m,sizeof(m),"%s: inside entries",label);ck(count(WS "\\inside\\*")==1,m);
 snprintf(m,sizeof(m),"%s: workspace entries",label);ck(count(WS "\\*")==1,m);}
static int cells;
#define REFUSE(expr,op) do{FS_READ_STATUS s_;snprintf(label,sizeof(label),"%s [%s]",op,name);\
 s_=(expr);if(s_==FS_READ_OK){char w_[360];snprintf(w_,sizeof(w_),"ACCEPTED %s",label);fail(w_);}untouched();cells++;}while(0)
static void nine(FS_READ_ROOT *r,const char *name,const char *exp,size_t el)
{
#ifndef ALIAS_SHORT
 /* Readers go only through win_open and valid_relative, with no second
    validator behind them, so these two cells are where a loosened
    valid_relative shows. Not run for the 8.3 lock cell: the lock may be read
    through its own name too. */
 {FS_READ_META m;unsigned char *b=NULL;size_t bn=0;
  REFUSE(FsReadStat(r,name,&m),"read stat");
  REFUSE(FsReadFile(r,name,&b,&bn,&m),"read file");free(b);}
#endif
 REFUSE(FsCreateFile(r,name,"x",1,0666),"create");
 REFUSE(FsCopyFile(r,"inside/f",name,"REAL",4),"copy target");
 REFUSE(FsCopyFile(r,name,"inside/c",exp,el),"copy source");
 REFUSE(FsMoveFile(r,"inside/f",name,"REAL",4),"move target");
 REFUSE(FsMoveFile(r,name,"inside/m",exp,el),"move source");
 REFUSE(FsReplaceFile(r,name,exp,el,"NEW!",4),"replace");
 REFUSE(FsRemoveFile(r,name,exp,el),"remove");
 {FS_BATCH_CREATE b[2]={{"inside/ok1","x",1,0666},{name,"x",1,0666}};
  REFUSE(FsBatchCreate(r,b,2),"batch create");}
 {FS_BATCH_REPLACE b[2]={{"inside/f","REAL",4,"NEW!",4},{name,exp,el,"NEW!",4}};
  REFUSE(FsBatchReplace(r,b,2),"batch replace");}
}
static void fixture(FS_READ_ROOT **r)
{cleanup();ck(_mkdir(WS)==0&&_mkdir(WS "\\inside")==0&&_mkdir(OUTD)==0,"mkdir");
 put(OUTD "\\victim","VICTIM");put(WS "\\inside\\f","REAL");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");
 ck(FsCreateFile(*r,"inside/c1","c1",2,0666)==FS_READ_OK,"control create");
 ck(FsCopyFile(*r,"inside/f","inside/c2","REAL",4)==FS_READ_OK,"control copy");
 ck(FsReplaceFile(*r,"inside/c2","REAL",4,"NEW!",4)==FS_READ_OK,"control replace");
 ck(FsMoveFile(*r,"inside/c2","inside/c3","NEW!",4)==FS_READ_OK,"control move");
 ck(FsRemoveFile(*r,"inside/c3","NEW!",4)==FS_READ_OK,"control remove c3");
 ck(FsRemoveFile(*r,"inside/c1","c1",2)==FS_READ_OK,"control remove c1");
 {FS_BATCH_CREATE b[2]={{"inside/b1","b1",2,0666},{"inside/b2","b2",2,0666}};
  ck(FsBatchCreate(*r,b,2)==FS_READ_OK,"control batch create");}
 {FS_BATCH_REPLACE b[2]={{"inside/b1","b1",2,"B1",2},{"inside/b2","b2",2,"B2",2}};
  ck(FsBatchReplace(*r,b,2)==FS_READ_OK,"control batch replace");}
 ck(FsRemoveFile(*r,"inside/b1","B1",2)==FS_READ_OK&&FsRemoveFile(*r,"inside/b2","B2",2)==FS_READ_OK,"control cleanup");
 ck(FsBatchRecover(*r)==FS_READ_OK,"control recover");
 label[0]=0;snprintf(label,sizeof(label),"after controls");untouched();matrix_started=1;}
int main(void)
{
 FS_READ_ROOT *r;
#ifdef ALIAS_SHORT
 char full[MAX_PATH],shortp[MAX_PATH],*leaf;DWORD n;
 fixture(&r);
 n=GetFullPathNameA(WS "\\.fstxn.lock",MAX_PATH,full,NULL);
 ck(n>0&&n<MAX_PATH,"lock path");
 n=GetShortPathNameA(full,shortp,MAX_PATH);
 leaf=strrchr(shortp,'\\');leaf=leaf?leaf+1:shortp;
 if(!n||n>=MAX_PATH||!_stricmp(leaf,".fstxn.lock")){
  FsReadClose(r);cleanup();
  printf("SKIP: this volume has no 8.3 short name for .fstxn.lock (%s)\n",n?leaf:"none");return 77;}
 {char name[64];snprintf(name,sizeof(name),"%s",leaf);nine(r,name,"",0);}
 ck(cells==9,"cell count");
 /* the control file itself must still be a usable, empty lock */
 {HANDLE h=CreateFileA(WS "\\.fstxn.lock",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
  ck(h!=INVALID_HANDLE_VALUE&&GetFileSize(h,NULL)==0,"lock file intact");CloseHandle(h);}
 FsReadClose(r);cleanup();
 printf("fs win short ok: 8.3 alias %s refused in %d cells\n",leaf,cells);
 return 0;
#else
 char absv[MAX_PATH],plain[MAX_PATH+16],ext[MAX_PATH+16],dev[MAX_PATH+16];DWORD n;
 static const char *fixed[]={
  "C:rel","C:","inside\\f","inside/f:ads","f:ads","f::$DATA","inside/f:ads:$DATA","inside/f::$DATA","inside/f:ads",
  "\\\\?\\C:\\x","\\\\?\\UNC\\localhost\\C$\\x","\\\\.\\C:\\x","\\\\.\\PhysicalDrive0","\\\\localhost\\C$\\x",
  "a*b","a?b","a<b","a>b","a|b","a\"b","*","?","<",">","|","\"","*.*","inside/*","inside/?","inside/a<b","inside/a|b",
  "...","f.","f "," ",".","inside/..."};
 size_t i;
 fixture(&r);
 n=GetFullPathNameA(OUTD "\\victim",MAX_PATH,absv,NULL);ck(n>0&&n<MAX_PATH,"victim abs");
 snprintf(plain,sizeof(plain),"%s",absv);
 snprintf(ext,sizeof(ext),"\\\\?\\%s",absv);
 snprintf(dev,sizeof(dev),"\\\\.\\%s",absv);
 for(i=0;i<sizeof(fixed)/sizeof(fixed[0]);i++)nine(r,fixed[i],"REAL",4);
 nine(r,plain,"REAL",4);nine(r,ext,"REAL",4);nine(r,dev,"REAL",4);
 ck(cells==11*(int)(sizeof(fixed)/sizeof(fixed[0])+3),"cell count");
 FsReadClose(r);cleanup();
#ifdef FS_ALIAS_MUTANT
 printf("MC%d SURVIVED: the alias matrix did not notice the mutant\n",(int)FS_ALIAS_MUTANT);return 1;
#else
 printf("fs win aliases ok: %d refused cells\n",cells);
 return 0;
#endif
#endif
}
#else
int main(void){return 0;}
#endif
