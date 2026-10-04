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
/* M1 criterion 5, Windows: a path whose parent component is an existing FILE
   must be refused by every writer with the workspace unchanged.

   Origin: run 37171472851 (test_fs_fuzz_ops seeds 1 and 12648430, create on
   safe/sub and safe/safe) and run 37172080904 (the diagnostic): FsCreateFile
   on "safe/sub", with "safe" a regular file, returned PENDING and left
   .fsp-*, .fst-* and .fstxn.intent in the workspace root. Correction of an
   earlier prediction: the header of the diagnostic test said the fuzz red was
   probably state from earlier operations of the fuzz run (harness fault).
   That was wrong; it is a production behaviour measured on a fresh fixture.
   The diagnostic also ran its six cells on one fixture, so after the first
   cell the pending intent made the later cells meaningless. Here every cell
   runs on its OWN fresh fixture.

   Read before measuring: win_open (src/fs_read.c) opens intermediate
   components with FILE_DIRECTORY_FILE and the last component without, and
   wc_parent (src/fs_create_win.inc) returns the handle of the last component
   of the parent path without checking that it is a directory. All writers
   resolve their parents through wc_parent (create, copy, move, remove, replace,
   batch create, batch replace: the call sites are in fs_create_win.inc and
   fs_batch_win.inc). A "safe/new/leaf" path has the file as an intermediate
   component, which win_open refuses.

   Cells (workspace: file safe "SAFE", dir sub, file sub/inner "INNER";
   outside: file target), each asserting status other than OK and an identical
   listing of workspace (kind, attributes, size, first 16 bytes, .fstxn.lock
   excluded) and of the outside directory:
   1 create safe/sub; 2 create safe/safe; 3 create safe/new/leaf; 4 copy
   sub/inner to safe/sub; 5 move sub/inner to safe/sub; 6 replace safe/sub;
   7 remove safe/sub; 8 batch create [newfile, safe/sub]; 9 batch replace
   [safe, safe/sub].
   Predictions, stated before the first measurement, for the code WITHOUT the
   fix (the first dispatch of this test):
   - cells 1 and 2 return PENDING and leave .fsp-, .fst- and .fstxn.intent
     (cell 1 is measured; cell 2 has the same code path);
   - cell 3 returns DENIED with the listing unchanged (the refusal comes from
     win_open, before any stage);
   - cells 4 and 5 (copy and move target under a file) leave artifacts like
     cell 1, with moderate confidence, because the target parent goes through
     the same wc_parent and the same stage-then-publish steps;
   - cell 8 (batch create) leaves artifacts, with lower confidence;
   - cells 6, 7 and 9 (the file-parent path is the SOURCE, which must exist)
     are refused cleanly with the listing unchanged.
   So the predicted red cells without the fix are 1, 2, 4, 5 and 8 and the
   predicted clean cells are 3, 6, 7 and 9. After the fix (second dispatch) all
   nine cells must pass. Every cell prints its status and CHANGED/same on
   stderr; wrong expectations are collected, the first differing line is
   printed, and the test exits 1. It does not test recovery of an intent left
   by the old behaviour: NOT DONE.
   With FS_PARENT_MUTANT defined (a build of src with the directory check
   removed) the test is inverted: it exits 0 when it sees at least one
   mismatch (the mutant is killed) and 1 when it sees none. */
#define WS "test_fs_win_fp_ws"
#define OUTD "test_fs_win_fp_out"
static void put(const char *p,const char *v)
{HANDLE h=CreateFileA(p,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD w;
 if(h==INVALID_HANDLE_VALUE||!WriteFile(h,v,(DWORD)strlen(v),&w,NULL)){fprintf(stderr,"FAIL fixture %s\n",p);exit(1);}CloseHandle(h);}
static void rm_rec(const char *dir)
{char pat[MAX_PATH+4],p[MAX_PATH];WIN32_FIND_DATAA d;HANDLE f;
 snprintf(pat,sizeof(pat),"%s\\*",dir);f=FindFirstFileA(pat,&d);
 if(f!=INVALID_HANDLE_VALUE){do{
   if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
   snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);
   if((d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))rm_rec(p);
   else{SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}
  }while(FindNextFileA(f,&d));FindClose(f);}
 SetFileAttributesA(dir,FILE_ATTRIBUTE_NORMAL);RemoveDirectoryA(dir);}
static void list(char *o,size_t cap,const char *dir,const char *rel)
{char pat[MAX_PATH+4],p[MAX_PATH],r[MAX_PATH],line[400];WIN32_FIND_DATAA d;HANDLE f;
 snprintf(pat,sizeof(pat),"%s\\*",dir);f=FindFirstFileA(pat,&d);
 if(f==INVALID_HANDLE_VALUE)return;
 do{
  if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,"..")||!strcmp(d.cFileName,".fstxn.lock"))continue;
  snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);snprintf(r,sizeof(r),"%s/%s",rel,d.cFileName);
  if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
   snprintf(line,sizeof(line),"D %s attr=%lx\n",r,(unsigned long)d.dwFileAttributes);strncat(o,line,cap-strlen(o)-1);
   if(!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))list(o,cap,p,r);
  }else{
   char hex[40]="";unsigned char b[16];DWORD got=0;
   HANDLE h=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
   if(h!=INVALID_HANDLE_VALUE){if(ReadFile(h,b,sizeof(b),&got,NULL))for(DWORD k=0;k<got;k++)snprintf(hex+2*k,4,"%02x",b[k]);CloseHandle(h);}
   snprintf(line,sizeof(line),"F %s attr=%lx size=%lu head=%s\n",r,(unsigned long)d.dwFileAttributes,(unsigned long)d.nFileSizeLow,hex);
   strncat(o,line,cap-strlen(o)-1);
  }
 }while(FindNextFileA(f,&d));
 FindClose(f);}
static void snapshot(char *o,size_t cap)
{o[0]=0;strncat(o,"WS\n",cap-1);list(o,cap,WS,"w");strncat(o,"OUT\n",cap-strlen(o)-1);list(o,cap,OUTD,"o");}
static int mism;
static void first_diff(const char *a,const char *b)
{int ln=1;
 while(*a&&*b){const char *ae=strchr(a,'\n'),*be=strchr(b,'\n');
  size_t al=ae?(size_t)(ae-a):strlen(a),bl=be?(size_t)(be-b):strlen(b);
  if(al!=bl||memcmp(a,b,al))break;a+=al+(ae!=NULL);b+=bl+(be!=NULL);ln++;}
 {const char *ae=strchr(a,'\n'),*be=strchr(b,'\n');
  int al=ae?(int)(ae-a):(int)strlen(a),bl=be?(int)(be-b):(int)strlen(b);
  fprintf(stderr,"  first differing line %d: before=[%.*s] after=[%.*s]\n",ln,al,a,bl,b);}}
static const char *const NAME[9]={"create safe/sub","create safe/safe","create safe/new/leaf",
 "copy sub/inner to safe/sub","move sub/inner to safe/sub","replace safe/sub",
 "remove safe/sub","batch create [newfile, safe/sub]","batch replace [safe, safe/sub]"};
static FS_READ_STATUS run(FS_READ_ROOT *r,int c)
{
 switch(c){
 case 0:return FsCreateFile(r,"safe/sub","x",1,0600);
 case 1:return FsCreateFile(r,"safe/safe","x",1,0600);
 case 2:return FsCreateFile(r,"safe/new/leaf","x",1,0600);
 case 3:return FsCopyFile(r,"sub/inner","safe/sub","INNER",5);
 case 4:return FsMoveFile(r,"sub/inner","safe/sub","INNER",5);
 case 5:return FsReplaceFile(r,"safe/sub","x",1,"y",1);
 case 6:return FsRemoveFile(r,"safe/sub","x",1);
 case 7:{FS_BATCH_CREATE b[2]={{"newfile","n",1,0600},{"safe/sub","x",1,0600}};return FsBatchCreate(r,b,2);}
 default:{FS_BATCH_REPLACE b[2]={{"safe","SAFE",4,"SAFF",4},{"safe/sub","x",1,"y",1}};return FsBatchReplace(r,b,2);}
 }
}
int main(void)
{
 int c;static char a[8192],b[8192];
 for(c=0;c<9;c++){
  FS_READ_ROOT *r;FS_READ_STATUS s;
  rm_rec(WS);rm_rec(OUTD);
  if(_mkdir(WS)||_mkdir(WS "/sub")||_mkdir(OUTD)){fprintf(stderr,"FAIL dirs\n");return 1;}
  put(WS "/safe","SAFE");put(WS "/sub/inner","INNER");put(OUTD "/target","outside");
  if(FsReadOpen(WS,&r)!=FS_READ_OK){fprintf(stderr,"FAIL open\n");return 1;}
  snapshot(a,sizeof(a));
  s=run(r,c);
  snapshot(b,sizeof(b));
  fprintf(stderr,"cell %d [%s]: status %d, listing %s\n",c+1,NAME[c],(int)s,strcmp(a,b)?"CHANGED":"same");
  if(s==FS_READ_OK){fprintf(stderr,"MISMATCH cell %d: accepted\n",c+1);mism++;}
  if(strcmp(a,b)){first_diff(a,b);fprintf(stderr,"--- after\n%s---\n",b);mism++;}
  FsReadClose(r);
 }
 rm_rec(WS);rm_rec(OUTD);
#ifdef FS_PARENT_MUTANT
 if(mism){printf("FS_PARENT_MUTANT killed: %d mismatches\n",mism);return 0;}
 printf("FS_PARENT_MUTANT SURVIVED: no cell noticed the missing directory check\n");return 1;
#else
 if(mism){fprintf(stderr,"%d mismatches\n",mism);return 1;}
 printf("fs win fileparent ok: 9 cells\n");return 0;
#endif
}
#else
int main(void){return 0;}
#endif
