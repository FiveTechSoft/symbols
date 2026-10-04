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
#include <wchar.h>
/* Criterion 4, Windows: long paths against the writers (NTFS, one runner).

   What the source says (read, not measured): on Windows the writers do not use
   MAX_PATH. Every component is opened by handle with NtCreateFile (one
   component per call). The limits are in src/fs_create_win.inc:
   - wc_target_status (lines 100-115, called by every writer): every path
     segment, directories included, must be 1..240 wide characters, else
     INVALID. wc_open (line 184) and the leaf split at line 488 repeat the
     240 cap for the leaf. No source comment gives the reason for 240; the
     rejection of over-long segments is listed as intended in
     docs/core-m0-m1-exit-audit.md (the device-name section).
   - wcslen(path) >= WC_MAX_PATH (#define at line 17, 4096) -> INVALID in
     create, copy, move, replace and batch validation (lines 117, 857, 938,
     954, 987 and src/fs_batch_win.inc line 117). WC_MAX_PATH is also the size
     of the path field of the journal records. No comment gives a reason
     either; the code is the only statement of the limit.
   FS_INTENT_MAX_PATH (1024) is in the POSIX code and is not part of the
   Windows branch.

   RETRACTION. The first version of this test (run 37165509182, red on
   build-test-msvc and asan-msvc at "leaf 255 create") predicted that a leaf of
   254 and 255 characters works and that 250-character directories work. That
   prediction was wrong: it used the NTFS limit (255) and I had not read the
   240 cap in wc_target_status. It was a fault of the test expectation, not of
   the production code. The 823-character block of that version passed (7
   cells) and is kept unchanged.

   Predictions for this version, stated before the first measurement:
   - A relative path of 823 characters (3 directories of 240, leaf of 100),
     whose absolute path is far above 260, works for create, replace, copy,
     move, remove, batch create and batch replace: OK, with the bytes read back
     through wide "\\?\" APIs. (Measured OK in run 37165509182.)
   - A leaf of 240 characters is created (OK). A leaf of 241 is refused (status
     other than OK) and nothing is created. A directory component of 241
     characters, made by the test, is refused as a parent (status other than
     OK) and nothing is created in it.
   - A relative path of exactly 4095 characters (16 directories of 240, leaf of
     239) works for create, replace, move and remove: OK.
   - A relative path of exactly 4096 characters (the same directories, leaf of
     240, which is itself a legal leaf) is refused (status other than OK, a
     predicted INVALID but only "not OK" is asserted) by create, copy target,
     move target and batch create, and nothing is created. Against an existing
     file at that length, replace, remove, move source and copy source are
     refused and the file keeps its bytes.
   The lengths are asserted in the test. Not claimed: the status code of the
   refusals, which layer refuses, paths above 32767, long names through the
   8.3 alias, other volumes, other runners. The directories of the fixture are
   created by the test itself with wide "\\?\" calls; the narrow helpers of
   the other tests cannot reach them. No mutant: the limits are several
   comparisons spread over the file, not one macro in src. */
#define WS "test_fs_winlp_ws"
static WCHAR g_root[700];
static WCHAR *wpath(const char *rel)
{size_t n=wcslen(g_root),m=strlen(rel),i;WCHAR *w=(WCHAR*)malloc((n+m+2)*sizeof(WCHAR));
 if(!w)exit(2);
 wcscpy(w,g_root);
 if(m){w[n]=L'\\';for(i=0;i<m;i++)w[n+1+i]=rel[i]=='/'?L'\\':(WCHAR)(unsigned char)rel[i];w[n+1+m]=0;}
 return w;}
static void rmrf_w(const WCHAR *dir)
{size_t n=wcslen(dir);WCHAR *pat=(WCHAR*)malloc((n+3)*sizeof(WCHAR));WIN32_FIND_DATAW d;HANDLE f;
 if(!pat)return;
 wcscpy(pat,dir);wcscat(pat,L"\\*");
 f=FindFirstFileW(pat,&d);free(pat);
 if(f==INVALID_HANDLE_VALUE)return;
 do{if(wcscmp(d.cFileName,L".")&&wcscmp(d.cFileName,L"..")){
     size_t m=wcslen(d.cFileName);WCHAR *c=(WCHAR*)malloc((n+m+2)*sizeof(WCHAR));
     if(!c)continue;
     wcscpy(c,dir);wcscat(c,L"\\");wcscat(c,d.cFileName);
     if((d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){
         rmrf_w(c);RemoveDirectoryW(c);}
     else{SetFileAttributesW(c,FILE_ATTRIBUTE_NORMAL);DeleteFileW(c);}
     free(c);}}while(FindNextFileW(f,&d));
 FindClose(f);}
static void cleanup(void){rmrf_w(g_root);RemoveDirectoryW(g_root);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());cleanup();exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put_w(const char *rel,const char *v)
{WCHAR *w=wpath(rel);HANDLE h=CreateFileW(w,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 DWORD n=0;free(w);
 ck(h!=INVALID_HANDLE_VALUE,"put_w open");
 ck(WriteFile(h,v,(DWORD)strlen(v),&n,NULL)&&n==strlen(v),"put_w write");CloseHandle(h);}
static int bytes_w(const char *rel,const char *v)
{WCHAR *w=wpath(rel);HANDLE h=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 char b[64]={0};DWORD n=0;int ok;free(w);
 if(h==INVALID_HANDLE_VALUE)return 0;
 ok=ReadFile(h,b,sizeof(b)-1,&n,NULL)&&n==strlen(v)&&!memcmp(b,v,n);CloseHandle(h);return ok;}
static int exists_w(const char *rel)
{WCHAR *w=wpath(rel);DWORD a=GetFileAttributesW(w);free(w);return a!=INVALID_FILE_ATTRIBUTES;}
/* entries of a directory, without the workspace lock; "" is the workspace */
static int count_w(const char *rel)
{WCHAR *w=wpath(rel);size_t n=wcslen(w);WCHAR *pat=(WCHAR*)malloc((n+3)*sizeof(WCHAR));
 WIN32_FIND_DATAW d;HANDLE f;int c=0;
 if(!pat)exit(2);
 wcscpy(pat,w);wcscat(pat,L"\\*");free(w);
 f=FindFirstFileW(pat,&d);free(pat);
 if(f==INVALID_HANDLE_VALUE)return -1;
 do{if(wcscmp(d.cFileName,L".")&&wcscmp(d.cFileName,L"..")&&wcscmp(d.cFileName,L".fstxn.lock"))c++;}while(FindNextFileW(f,&d));
 FindClose(f);return c;}
/* "aaa.../bbb.../.../leaf": dirs directories of dirlen characters ('a'+i), then a leaf */
static void make_dir(char *out,int dirs,int dirlen)
{int i,k=0;for(i=0;i<dirs;i++){memset(out+k,'a'+i,(size_t)dirlen);k+=dirlen;if(i+1<dirs)out[k++]='/';}out[k]=0;}
static void make_rel(char *out,int dirs,int dirlen,int leaflen,char ch)
{size_t k;make_dir(out,dirs,dirlen);k=strlen(out);if(dirs)out[k++]='/';memset(out+k,ch,(size_t)leaflen);out[k+(size_t)leaflen]=0;}
static void mkchain(const char *dir)
{char pre[5200];size_t i,n=strlen(dir);WCHAR *w;
 for(i=0;i<=n;i++)if(dir[i]=='/'||dir[i]==0){memcpy(pre,dir,i);pre[i]=0;w=wpath(pre);
     ck(CreateDirectoryW(w,NULL)!=0||GetLastError()==ERROR_ALREADY_EXISTS,"mkchain");free(w);}}
static int cells;
#define STAY_OK(expr,what) do{ck((expr)==FS_READ_OK,what);cells++;}while(0)
#define STAY_REFUSED(expr,what) do{ck((expr)!=FS_READ_OK,what);cells++;}while(0)
int main(void)
{
 FS_READ_ROOT *r;static char rel[5200],relc[5200],relm[5200],dir[5200],rel4095[5200],rel4096[5200];
 WCHAR full[600];DWORD n;
 n=GetFullPathNameW(L"" WS,600,full,NULL);
 ck(n>0&&n<580,"full path");
 wcscpy(g_root,L"\\\\?\\");wcscat(g_root,full);
 cleanup();ck(CreateDirectoryW(g_root,NULL)!=0,"create workspace");
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open workspace");
 /* 823 characters: 3 directories of 240 and a leaf of 100 */
 make_dir(dir,3,240);mkchain(dir);
 make_rel(rel,3,240,100,'f');make_rel(relc,3,240,100,'c');make_rel(relm,3,240,100,'m');
 ck(strlen(rel)==823,"823 length");
 ck(wcslen(g_root)+1+strlen(rel)>260,"absolute path above 260");
 STAY_OK(FsCreateFile(r,rel,"ONE",3,0666),"823 create");
 ck(bytes_w(rel,"ONE")&&count_w(dir)==1,"823 create: state");
 STAY_OK(FsReplaceFile(r,rel,"ONE",3,"TWO",3),"823 replace");
 ck(bytes_w(rel,"TWO")&&count_w(dir)==1,"823 replace: state");
 STAY_OK(FsCopyFile(r,rel,relc,"TWO",3),"823 copy");
 ck(bytes_w(rel,"TWO")&&bytes_w(relc,"TWO")&&count_w(dir)==2,"823 copy: state");
 STAY_OK(FsMoveFile(r,relc,relm,"TWO",3),"823 move");
 ck(!exists_w(relc)&&bytes_w(relm,"TWO")&&count_w(dir)==2,"823 move: state");
 STAY_OK(FsRemoveFile(r,rel,"TWO",3),"823 remove");
 ck(!exists_w(rel)&&bytes_w(relm,"TWO")&&count_w(dir)==1,"823 remove: state");
 ck(FsRemoveFile(r,relm,"TWO",3)==FS_READ_OK&&count_w(dir)==0,"823 cleanup");
 {FS_BATCH_CREATE b[2]={{rel,"B1",2,0666},{relm,"B2",2,0666}};
  STAY_OK(FsBatchCreate(r,b,2),"823 batch create");}
 ck(bytes_w(rel,"B1")&&bytes_w(relm,"B2")&&count_w(dir)==2,"823 batch create: state");
 {FS_BATCH_REPLACE b[2]={{rel,"B1",2,"C1",2},{relm,"B2",2,"C2",2}};
  STAY_OK(FsBatchReplace(r,b,2),"823 batch replace");}
 ck(bytes_w(rel,"C1")&&bytes_w(relm,"C2")&&count_w(dir)==2,"823 batch replace: state");
 ck(FsRemoveFile(r,rel,"C1",2)==FS_READ_OK&&FsRemoveFile(r,relm,"C2",2)==FS_READ_OK&&count_w(dir)==0,"823 batch cleanup");
 /* leaf of 240 and 241 characters, directory of 241 characters */
 {char leaf[400],bd[400];
  {WCHAR *w=wpath("D");ck(CreateDirectoryW(w,NULL)!=0,"mkdir D");free(w);}
  strcpy(leaf,"D/");memset(leaf+2,'x',240);leaf[242]=0;
  ck(strlen(leaf)==242,"leaf 240 length");
  STAY_OK(FsCreateFile(r,leaf,"L",1,0666),"leaf 240 create");
  ck(bytes_w(leaf,"L")&&count_w("D")==1,"leaf 240: state");
  ck(FsRemoveFile(r,leaf,"L",1)==FS_READ_OK&&count_w("D")==0,"leaf 240 cleanup");
  strcpy(leaf,"D/");memset(leaf+2,'x',241);leaf[243]=0;
  ck(strlen(leaf)==243,"leaf 241 length");
  STAY_REFUSED(FsCreateFile(r,leaf,"L",1,0666),"leaf 241 create");
  ck(!exists_w(leaf)&&count_w("D")==0&&count_w("")==2,"leaf 241: nothing created");
  memset(bd,'y',241);bd[241]=0;
  {WCHAR *w=wpath(bd);ck(CreateDirectoryW(w,NULL)!=0,"mkdir 241");free(w);}
  strcpy(leaf,bd);strcat(leaf,"/f");
  STAY_REFUSED(FsCreateFile(r,leaf,"L",1,0666),"directory 241 create under it");
  ck(count_w(bd)==0&&count_w("")==3,"directory 241: nothing created");}
 /* 4095 characters: 16 directories of 240 and a leaf of 239 */
 make_dir(dir,16,240);mkchain(dir);
 make_rel(rel4095,16,240,239,'p');make_rel(relm,16,240,239,'q');make_rel(rel4096,16,240,240,'p');
 ck(strlen(rel4095)==4095&&strlen(rel4096)==4096,"4095 and 4096 lengths");
 STAY_OK(FsCreateFile(r,rel4095,"ONE",3,0666),"4095 create");
 ck(bytes_w(rel4095,"ONE")&&count_w(dir)==1,"4095 create: state");
 STAY_OK(FsReplaceFile(r,rel4095,"ONE",3,"TWO",3),"4095 replace");
 ck(bytes_w(rel4095,"TWO")&&count_w(dir)==1,"4095 replace: state");
 STAY_OK(FsMoveFile(r,rel4095,relm,"TWO",3),"4095 move");
 ck(!exists_w(rel4095)&&bytes_w(relm,"TWO")&&count_w(dir)==1,"4095 move: state");
 STAY_OK(FsRemoveFile(r,relm,"TWO",3),"4095 remove");
 ck(count_w(dir)==0,"4095 remove: state");
 /* 4096 characters: refused, nothing created */
 put_w("D/src","SRC");
 STAY_REFUSED(FsCreateFile(r,rel4096,"X",1,0666),"4096 create");
 ck(!exists_w(rel4096)&&count_w(dir)==0,"4096 create: nothing created");
 STAY_REFUSED(FsCopyFile(r,"D/src",rel4096,"SRC",3),"4096 copy target");
 ck(!exists_w(rel4096)&&count_w(dir)==0&&bytes_w("D/src","SRC"),"4096 copy target: state");
 STAY_REFUSED(FsMoveFile(r,"D/src",rel4096,"SRC",3),"4096 move target");
 ck(!exists_w(rel4096)&&count_w(dir)==0&&bytes_w("D/src","SRC"),"4096 move target: state");
 {FS_BATCH_CREATE b[2]={{"D/New1","x",1,0666},{rel4096,"x",1,0666}};
  STAY_REFUSED(FsBatchCreate(r,b,2),"4096 batch create");}
 ck(!exists_w("D/New1")&&!exists_w(rel4096)&&count_w(dir)==0&&count_w("D")==1,"4096 batch create: nothing created");
 /* an existing file at 4096 characters, made by the test */
 put_w(rel4096,"KEEP");
 STAY_REFUSED(FsReplaceFile(r,rel4096,"KEEP",4,"NEW!",4),"4096 replace");
 ck(bytes_w(rel4096,"KEEP")&&count_w(dir)==1,"4096 replace: state");
 STAY_REFUSED(FsRemoveFile(r,rel4096,"KEEP",4),"4096 remove");
 ck(bytes_w(rel4096,"KEEP")&&count_w(dir)==1,"4096 remove: state");
 STAY_REFUSED(FsMoveFile(r,rel4096,"D/moved","KEEP",4),"4096 move source");
 ck(bytes_w(rel4096,"KEEP")&&!exists_w("D/moved")&&count_w(dir)==1,"4096 move source: state");
 STAY_REFUSED(FsCopyFile(r,rel4096,"D/copied","KEEP",4),"4096 copy source");
 ck(bytes_w(rel4096,"KEEP")&&!exists_w("D/copied")&&count_w(dir)==1,"4096 copy source: state");
 FsReadClose(r);
 ck(cells==22,"cell count");
 cleanup();
 printf("fs win longpath ok: %d cells\n",cells);
 return 0;
}
#else
int main(void){return 0;}
#endif
