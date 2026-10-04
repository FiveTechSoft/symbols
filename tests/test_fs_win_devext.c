#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 5, Windows: eight reserved-looking names that the device-name
   check in src/fs_create_win.inc (wc_device_name) does not list: CONIN$,
   CONOUT$ and the superscript forms COM1-3 and LPT1-3 (U+00B9, U+00B2,
   U+00B3, written as UTF-8 byte pairs). They are the "not covered" names named
   in docs/core-m0-m1-exit-audit.md (section "Finding: Windows writers accepted
   Win32 device names").

   Read before measuring: wc_device_name returns 0 unless the part before the
   first dot, with trailing spaces trimmed, is 3 characters (CON PRN AUX NUL) or
   4 characters (COM or LPT followed by an ASCII digit 1-9). CONIN$ has 6
   characters and the superscript forms have a non-ASCII fourth character, so
   all eight names fall through. No other check in wc_target_status rejects
   them (no backslash, colon, control character, trailing dot or space).
   Prediction, stated before the first measurement: FsCreateFile returns
   FS_READ_OK for all eight names, the file exists inside the workspace with
   the written byte, FsRemoveFile with the exact expected image returns OK and
   the name is gone, and a sentinel file outside the workspace is unchanged.
   The writers open names relative to a directory handle (NT path rules), so I
   expect a plain file and no device access. A refusal (INVALID) of any name
   is a wrong prediction and is printed as such; it is not a confinement
   violation. The property that must hold in either case is confinement: the
   sentinel is unchanged and nothing is created outside the workspace. This
   test pins the observed behaviour of the current code; it does not say the
   names should be accepted. Not covered: the same names as non-leaf
   components, other writer entry points, names with an extension. */
#define WS "test_fs_win_devext_ws"
#define SENT "test_fs_win_devext_sentinel.txt"
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
static void miss(const char *name,const char *what,int st)
{fprintf(stderr,"MISMATCH %s: %s (status %d)\n",name,what,st);mism++;}
static int sentinel_ok(void)
{char b[16];DWORD got=0;HANDLE h=CreateFileA(SENT,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 int ok;if(h==INVALID_HANDLE_VALUE)return 0;
 ok=ReadFile(h,b,sizeof(b),&got,NULL)&&got==8&&!memcmp(b,"SENTINEL",8);CloseHandle(h);return ok;}
static void put_sentinel(void)
{HANDLE h=CreateFileA(SENT,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD w;
 if(h==INVALID_HANDLE_VALUE||!WriteFile(h,"SENTINEL",8,&w,NULL)||w!=8){fprintf(stderr,"FAIL sentinel\n");exit(1);}CloseHandle(h);}
static const char *const N[8]={"CONIN$","CONOUT$","COM\xC2\xB9","COM\xC2\xB2","COM\xC2\xB3","LPT\xC2\xB9","LPT\xC2\xB2","LPT\xC2\xB3"};
int main(void)
{
 FS_READ_ROOT *r;int i;
 rmtree(WS);DeleteFileA(SENT);put_sentinel();
 if(_mkdir(WS)!=0){fprintf(stderr,"FAIL mkdir\n");return 1;}
 if(FsReadOpen(WS,&r)!=FS_READ_OK){fprintf(stderr,"FAIL open workspace\n");rmtree(WS);return 1;}
 for(i=0;i<8;i++){
  unsigned char *b=NULL;size_t n=0;FS_READ_META m;
  FS_READ_STATUS s=FsCreateFile(r,N[i],"X",1,0666);
  cells++;
  if(s!=FS_READ_OK){miss(N[i],"create was not accepted",(int)s);}
  else{
   FS_READ_STATUS g=FsReadFile(r,N[i],&b,&n,&m);
   if(g!=FS_READ_OK||n!=1||b[0]!='X')miss(N[i],"accepted name does not read back inside the workspace",(int)g);
   free(b);
   s=FsRemoveFile(r,N[i],"X",1);
   if(s!=FS_READ_OK)miss(N[i],"remove with the exact image was not OK",(int)s);
   else if(FsReadStat(r,N[i],&m)==FS_READ_OK)miss(N[i],"name still present after remove",0);
  }
  cells++;
  if(!sentinel_ok()){miss(N[i],"sentinel outside the workspace changed",0);}
 }
 FsReadClose(r);
 rmtree(WS);DeleteFileA(SENT);
 if(mism){fprintf(stderr,"%d mismatches\n",mism);return 1;}
 printf("fs win device names ext ok: %d cells\n",cells);
 return 0;
}
#else
int main(void){return 0;}
#endif
