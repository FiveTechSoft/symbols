#ifdef _WIN32
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* Diagnostic for the red of run 37171472851 (test_fs_fuzz_ops_seed_1_i400 and
   _seed_12648430_i400 on Windows: a create whose path has the existing FILE
   "safe" as a parent component was rejected but the tree snapshot changed).
   Measures exactly that call, on the same kind of fixture, and prints a
   listing (name, attributes, size, first 16 bytes in hex) of the workspace and
   of the outside directory before and after, on stderr, whatever the result.
   Cells: create "safe/sub" and "safe/safe" (the two failing paths; the leaf
   names "sub" and "safe" already exist in the workspace root), "safe/other"
   and "safe/target" (leaf names that do not exist in the root), "safe/new/leaf"
   and "sub/inner/x" (a file as a parent deeper in the path). The workspace
   lock file .fstxn.lock is left out of the listing, as in the fuzz snapshot.
   Prediction, stated before measuring (low confidence, a hypothesis): the
   six calls return a status other than OK, and on a fresh fixture all six
   listings are identical before and after, because 400-iteration runs of
   other seeds, with many "safe/<name>" creates, passed and only the two
   paths whose leaf name exists in the root failed. If that holds, the cause
   is state left by an earlier operation of the same fuzz iteration or run that
   the baseline does not contain (fixture or harness), not a single create. If
   "safe/sub" and "safe/safe" change a listing here and the four others do not,
   the cause is in production code and depends on the leaf name existing in
   the root. The first differing line is printed. No repair is attempted. */
#define WS "test_fs_win_cfp_ws"
#define OUT "test_fs_win_cfp_out"
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
{o[0]=0;strncat(o,"WS\n",cap-1);list(o,cap,WS,"w");strncat(o,"OUT\n",cap-strlen(o)-1);list(o,cap,OUT,"o");}
int main(void)
{
 static const char *P[6]={"safe/sub","safe/safe","safe/other","safe/target","safe/new/leaf","sub/inner/x"};
 FS_READ_ROOT *r;int i,bad=0;static char a[8192],b[8192];
 rm_rec(WS);rm_rec(OUT);
 if(_mkdir(WS)||_mkdir(WS "/sub")||_mkdir(OUT)){fprintf(stderr,"FAIL dirs\n");return 1;}
 put(WS "/safe","SAFE");put(WS "/sub/inner","INNER");put(OUT "/target","outside");
 if(FsReadOpen(WS,&r)!=FS_READ_OK){fprintf(stderr,"FAIL open\n");return 1;}
 for(i=0;i<6;i++){
  FS_READ_STATUS s;
  snapshot(a,sizeof(a));
  s=FsCreateFile(r,P[i],"x",1,0600);
  snapshot(b,sizeof(b));
  fprintf(stderr,"create [%s]: status %d, listing %s\n",P[i],(int)s,strcmp(a,b)?"CHANGED":"same");
  if(s==FS_READ_OK){fprintf(stderr,"MISMATCH create [%s] accepted\n",P[i]);bad++;}
  if(strcmp(a,b)){
   const char *x=a,*y=b;int ln=1;
   while(*x&&*y){const char *xe=strchr(x,'\n'),*ye=strchr(y,'\n');size_t xl=xe?(size_t)(xe-x):strlen(x),yl=ye?(size_t)(ye-y):strlen(y);
    if(xl!=yl||memcmp(x,y,xl))break;x+=xl+(xe!=NULL);y+=yl+(ye!=NULL);ln++;}
   fprintf(stderr,"first differing line %d: before=[%.*s] after=[%.*s]\n",ln,(int)(strchr(x,'\n')?strchr(x,'\n')-x:(int)strlen(x)),x,(int)(strchr(y,'\n')?strchr(y,'\n')-y:(int)strlen(y)),y);
  }
  if(strcmp(a,b)){fprintf(stderr,"--- before\n%s--- after\n%s---\n",a,b);bad++;}
  else if(i==0)fprintf(stderr,"--- listing (same before and after)\n%s---\n",a);
 }
 FsReadClose(r);rm_rec(WS);rm_rec(OUT);
 if(bad){fprintf(stderr,"%d mismatches\n",bad);return 1;}
 printf("fs win createfileparent ok: 6 cells\n");return 0;
}
#else
int main(void){return 0;}
#endif
