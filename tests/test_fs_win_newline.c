#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_batch.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* Criterion 4, Windows: newlines and odd bytes pass through every writer
   byte for byte (NTFS, one runner). The POSIX counterpart is t_newline in
   tests/test_fs_c4_posix.c.

   What the source says (read, not measured): src/fs_create_win.inc and
   src/fs_batch_win.inc contain no fopen and no text-mode flag (grep for fopen,
   _O_TEXT and "w" finds none); the writers move bytes with handles
   (NtCreateFile, WriteFile, ReadFile), so no C runtime newline translation can
   happen inside them. The newline kind and the binary flag come from
   scan_bytes in src/fs_read.c, shared with POSIX.

   Payloads (name: bytes, kind reported by FsReadFile, binary flag):
   crlf "a\r\nb\r\n" CRLF 0; lf "a\nb\n" LF 0; mixed "a\r\nb\nc\r\n" MIXED 0;
   none "no newline at end" NONE 0; nul "bin\0\r\n\0x\n" binary 1; cr "a\rb\r"
   (lone carriage returns) MIXED 0; ctrlz "a\x1a" "b\r\n" (a 0x1A byte, the DOS
   end-of-file mark that a text-mode read would stop at) CRLF 0.
   Predictions, stated before the first measurement:
   - for each of the seven payloads: create keeps the bytes, FsReadFile reports
     the kind and binary flag above, copy keeps the bytes, replace to the next
     payload gives exactly the next payload's bytes, move keeps them and remove
     with the exact expected image returns OK. All FS_READ_OK. The bytes are
     compared through CreateFileA/ReadFile in this test, not through the Fs
     read API.
   - batch create of all seven payloads in one call returns OK and every file
     holds its payload; batch replace of two files (crlf and nul swapped)
     returns OK with the swapped bytes.
   - a near-miss expected image (LF where the file has CRLF) is refused (status
     other than OK) and the bytes stay.
   45 cells (7 x 6 per-payload checks, batch create, batch replace, near-miss).
   Every wrong expectation is collected and printed with the observed status,
   then the test exits 1. Not claimed: UTF-16 or BOM-aware handling, files
   above FS_READ_MAX, alternate data streams, other volumes, other runners. No
   mutant: the Windows writers contain no newline logic to disable, so a mutant
   would not be a one-line macro. */
#define WS "test_fs_winnl_ws"
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
static int mism,cells;
static int chk(int ok,const char *cell,const char *what)
{cells++;if(!ok){fprintf(stderr,"MISMATCH %s: %s\n",cell,what);mism++;}return ok;}
static int st_ok(FS_READ_STATUS s,const char *cell,const char *what)
{if(s!=FS_READ_OK){fprintf(stderr,"MISMATCH %s: %s, status %d\n",cell,what,(int)s);mism++;return 0;}return 1;}
/* the bytes of WS\rel, read with the Win32 API */
static int raw_is(const char *rel,const char *v,size_t n)
{char path[MAX_PATH],*b;HANDLE h;DWORD got=0;int ok;
 snprintf(path,sizeof(path),"%s\\%s",WS,rel);
 h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 b=(char*)malloc(n+2);if(!b){CloseHandle(h);return 0;}
 ok=ReadFile(h,b,(DWORD)(n+1),&got,NULL)&&got==n&&(n==0||!memcmp(b,v,n));
 free(b);CloseHandle(h);return ok;}
static int absent(const char *rel)
{char path[MAX_PATH];snprintf(path,sizeof(path),"%s\\%s",WS,rel);return GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES;}
typedef struct{const char *name;const char *b;size_t n;FS_READ_NEWLINE nl;int bin;}PAY;
static const PAY P[7]={
 {"crlf","a\r\nb\r\n",6,FS_NEWLINE_CRLF,0},
 {"lf","a\nb\n",4,FS_NEWLINE_LF,0},
 {"mixed","a\r\nb\nc\r\n",8,FS_NEWLINE_MIXED,0},
 {"none","no newline at end",17,FS_NEWLINE_NONE,0},
 {"nul","bin\0\r\n\0x\n",9,FS_NEWLINE_NONE,1},
 {"cr","a\rb\r",4,FS_NEWLINE_MIXED,0},
 {"ctrlz","a\x1a" "b\r\n",5,FS_NEWLINE_CRLF,0}};
int main(void)
{
 FS_READ_ROOT *r;int i;
 rmtree(WS);
 if(_mkdir(WS)!=0){fprintf(stderr,"FAIL mkdir\n");return 1;}
 if(FsReadOpen(WS,&r)!=FS_READ_OK){fprintf(stderr,"FAIL open workspace\n");rmtree(WS);return 1;}
 for(i=0;i<7;i++){
  char f[32],g[32],c[96];int j=(i+1)%7;unsigned char *b=NULL;size_t n=0;FS_READ_META m;FS_READ_STATUS s;
  snprintf(f,sizeof(f),"f_%s",P[i].name);snprintf(g,sizeof(g),"g_%s",P[i].name);
  snprintf(c,sizeof(c),"payload %s create",P[i].name);
  if(st_ok(FsCreateFile(r,f,P[i].b,P[i].n,0666),c,"create")) chk(raw_is(f,P[i].b,P[i].n),c,"bytes differ after create");
  else cells++;
  snprintf(c,sizeof(c),"payload %s read meta",P[i].name);
  s=FsReadFile(r,f,&b,&n,&m);
  if(s!=FS_READ_OK)chk(0,c,"FsReadFile not OK");
  else{if(P[i].bin)chk(m.binary==1,c,"NUL payload not flagged binary");
       else chk(m.newline==P[i].nl&&m.binary==0,c,"newline kind or binary flag differs");free(b);}
  snprintf(c,sizeof(c),"payload %s copy",P[i].name);
  if(st_ok(FsCopyFile(r,f,g,P[i].b,P[i].n),c,"copy")) chk(raw_is(g,P[i].b,P[i].n)&&raw_is(f,P[i].b,P[i].n),c,"bytes differ after copy");
  else cells++;
  snprintf(c,sizeof(c),"payload %s replace",P[i].name);
  if(st_ok(FsReplaceFile(r,g,P[i].b,P[i].n,P[j].b,P[j].n),c,"replace")) chk(raw_is(g,P[j].b,P[j].n),c,"bytes differ after replace");
  else cells++;
  snprintf(c,sizeof(c),"payload %s move",P[i].name);
  if(st_ok(FsMoveFile(r,g,"mv",P[j].b,P[j].n),c,"move")) chk(raw_is("mv",P[j].b,P[j].n)&&absent(g),c,"bytes or names differ after move");
  else cells++;
  snprintf(c,sizeof(c),"payload %s remove",P[i].name);
  if(st_ok(FsRemoveFile(r,"mv",P[j].b,P[j].n),c,"remove")) chk(absent("mv"),c,"name still present after remove");
  else cells++;
 }
 {FS_BATCH_CREATE bc[7];char nm[7][32];
  for(i=0;i<7;i++){snprintf(nm[i],sizeof(nm[i]),"bc_%s",P[i].name);bc[i].target=nm[i];bc[i].bytes=P[i].b;bc[i].len=P[i].n;bc[i].mode=0666;}
  {FS_READ_STATUS s=FsBatchCreate(r,bc,7);char m[80];int all=1;
   for(i=0;i<7;i++)if(!raw_is(nm[i],P[i].b,P[i].n))all=0;
   snprintf(m,sizeof(m),"batch create status %d, all bytes equal %d",(int)s,all);
   chk(s==FS_READ_OK&&all,"batch create of seven payloads",m);}}
 {FS_BATCH_REPLACE br[2]={{"f_crlf",P[0].b,P[0].n,P[4].b,P[4].n},{"f_nul",P[4].b,P[4].n,P[0].b,P[0].n}};
  FS_READ_STATUS s=FsBatchReplace(r,br,2);char m[80];
  snprintf(m,sizeof(m),"status %d, swapped bytes %d",(int)s,raw_is("f_crlf",P[4].b,P[4].n)&&raw_is("f_nul",P[0].b,P[0].n));
  chk(s==FS_READ_OK&&raw_is("f_crlf",P[4].b,P[4].n)&&raw_is("f_nul",P[0].b,P[0].n),"batch replace of crlf and nul",m);}
 {FS_READ_STATUS s=FsReplaceFile(r,"f_lf","a\r\nb\n",5,"x",1);char m[80];
  snprintf(m,sizeof(m),"near-miss expected image: status %d, bytes kept %d",(int)s,raw_is("f_lf",P[1].b,P[1].n));
  chk(s!=FS_READ_OK&&raw_is("f_lf",P[1].b,P[1].n),"near-miss expected image",m);}
 FsReadClose(r);
 rmtree(WS);
 if(cells!=45)fprintf(stderr,"cell count %d, expected 45\n",cells),mism++;
 if(mism){fprintf(stderr,"%d mismatches\n",mism);return 1;}
 printf("fs win newline ok: %d cells\n",cells);
 return 0;
}
#else
int main(void){return 0;}
#endif
