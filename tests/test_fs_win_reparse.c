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
/* Criterion 1, Windows: deterministic junction matrix (NTFS, one runner).

   A junction "jl" inside the workspace points at a directory OUTSIDE it that
   holds one file, "victim". For each writer, the junction is used as a parent
   component, as the leaf, and as the workspace root, and the outcome must be a
   refusal (status other than FS_READ_OK) with the outside directory, the real
   workspace directory and the junction itself unchanged.

   Expected result per cell, stated before the first measurement:
   - junction as parent, any writer (replace, remove, move source, move
     target, copy source, copy target, create, batch create, batch replace):
     refused, FS_READ_DENIED, because every parent component is resolved by
     win_open, which opens a reparse point itself and rejects it.
   - junction as the leaf (replace, remove, move source, copy source, create
     over it): refused; the exact status is not predicted, only "not OK",
     because the leaf is opened by wc_open with FILE_NON_DIRECTORY_FILE.
   - junction as the workspace root: FsReadOpen refuses (win_meta rejects a
     reparse point), so no writer can be handed such a root.
   - controls: the same nine operations on the real directory "inside" succeed,
     so the refusals above are not a harness that rejects everything.
   If any cell is accepted or writes outside, this test fails and the finding
   is reported before any change to src. */
#ifdef FS_REPARSE_MUTANT
/* MC1 build: the same test, linked against an fs_read.c whose win_meta no
   longer rejects a reparse point (see src/fs_read.c). It passes (exit 0) only
   if the matrix catches the mutant: a failed check after the controls counts
   as a kill, a failed control is a real failure, and reaching the end means
   the mutant survived (exit 1). Separate fixture names so ctest may run both
   builds in parallel. */
#define WS "test_fs_winreparse_mc1_ws"
#define OUTD "test_fs_winreparse_mc1_out"
#define RL "test_fs_winreparse_mc1_rootlink"
#else
#define WS "test_fs_winreparse_ws"
#define OUTD "test_fs_winreparse_out"
#define RL "test_fs_winreparse_rootlink"
#endif
static int matrix_started;
/* Junctions are removed as links first so the recursive delete never follows
   them; no ck() in here, it runs from the failure path too. */
static void cleanup(void)
{system("cmd /D /C if exist " RL " rmdir " RL " >NUL 2>&1");
 system("cmd /D /C if exist " WS "\\jl rmdir " WS "\\jl >NUL 2>&1");
 system("cmd /D /C if exist " WS " rmdir /S /Q " WS " >NUL 2>&1");
 system("cmd /D /C if exist " OUTD " rmdir /S /Q " OUTD " >NUL 2>&1");}
static void ck(int ok,const char *what)
{if(ok)return;
#ifdef FS_REPARSE_MUTANT
 if(matrix_started){printf("MC1 killed by: %s\n",what);cleanup();exit(0);}
#endif
 fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());exit(1);}
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
static void junction(const char *link,const char *target)
{char abs[MAX_PATH],cmd[2*MAX_PATH+128];DWORD n=GetFullPathNameA(target,MAX_PATH,abs,NULL);
 ck(n>0&&n<MAX_PATH,"junction target");
 ck(snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"%s\" \"%s\" >NUL",link,abs)>0,"junction cmd");
 ck(system(cmd)==0,"junction fixture");}
static void unlink_dir(const char *p)
{DWORD a=GetFileAttributesA(p);if(a!=INVALID_FILE_ATTRIBUTES)ck(RemoveDirectoryA(p),"remove directory");}
/* Everything that must not move when a refused operation ends. */
static void untouched(const char *what)
{char m[160];
 snprintf(m,sizeof(m),"%s: victim bytes",what);same_bytes(OUTD "\\victim","VICTIM");
 snprintf(m,sizeof(m),"%s: outside entries",what);ck(count(OUTD "\\*")==1,m);
 snprintf(m,sizeof(m),"%s: real file bytes",what);same_bytes(WS "\\inside\\f","REAL");
 snprintf(m,sizeof(m),"%s: real dir entries",what);ck(count(WS "\\inside\\*")==1,m);
 snprintf(m,sizeof(m),"%s: workspace entries",what);ck(count(WS "\\*")==2,m);
 snprintf(m,sizeof(m),"%s: junction still a link",what);
 {DWORD a=GetFileAttributesA(WS "\\jl");ck(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT),m);}}
static int refused;
#define NO(expr,what) do{FS_READ_STATUS s_=(expr);ck(s_!=FS_READ_OK,what " accepted");untouched(what);refused++;}while(0)
#define DENIED(expr,what) do{FS_READ_STATUS s_=(expr);ck(s_==FS_READ_DENIED,what " not DENIED");untouched(what);refused++;}while(0)
int main(void)
{
 FS_READ_ROOT *r,*rl=NULL;FS_READ_STATUS s;
 cleanup();
 ck(_mkdir(WS)==0&&_mkdir(WS "\\inside")==0&&_mkdir(OUTD)==0,"mkdir");
 put(OUTD "\\victim","VICTIM");put(WS "\\inside\\f","REAL");
 junction(WS "\\jl",OUTD);junction(RL,WS);
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open workspace");
 /* Controls on the real directory: the writers work in this fixture. */
 ck(FsCreateFile(r,"inside/c1","c1",2,0666)==FS_READ_OK,"control create");
 ck(FsCopyFile(r,"inside/f","inside/c2","REAL",4)==FS_READ_OK,"control copy");
 ck(FsReplaceFile(r,"inside/c2","REAL",4,"NEW!",4)==FS_READ_OK,"control replace");
 ck(FsMoveFile(r,"inside/c2","inside/c3","NEW!",4)==FS_READ_OK,"control move");
 ck(FsRemoveFile(r,"inside/c3","NEW!",4)==FS_READ_OK,"control remove c3");
 ck(FsRemoveFile(r,"inside/c1","c1",2)==FS_READ_OK,"control remove c1");
 {FS_BATCH_CREATE b[2]={{"inside/b1","b1",2,0666},{"inside/b2","b2",2,0666}};
  ck(FsBatchCreate(r,b,2)==FS_READ_OK,"control batch create");}
 {FS_BATCH_REPLACE b[2]={{"inside/b1","b1",2,"B1",2},{"inside/b2","b2",2,"B2",2}};
  ck(FsBatchReplace(r,b,2)==FS_READ_OK,"control batch replace");}
 ck(FsRemoveFile(r,"inside/b1","B1",2)==FS_READ_OK&&FsRemoveFile(r,"inside/b2","B2",2)==FS_READ_OK,"control cleanup");
 ck(FsBatchRecover(r)==FS_READ_OK,"control recover");
 untouched("after controls");
 matrix_started=1;
 /* Junction as a parent: expected DENIED in every writer. */
 DENIED(FsReplaceFile(r,"jl/victim","VICTIM",6,"HACKED",6),"replace via junction parent");
 DENIED(FsRemoveFile(r,"jl/victim","VICTIM",6),"remove via junction parent");
 DENIED(FsMoveFile(r,"jl/victim","inside/m","VICTIM",6),"move source via junction parent");
 DENIED(FsMoveFile(r,"inside/f","jl/m","REAL",4),"move target via junction parent");
 DENIED(FsCopyFile(r,"jl/victim","inside/c","VICTIM",6),"copy source via junction parent");
 DENIED(FsCopyFile(r,"inside/f","jl/c","REAL",4),"copy target via junction parent");
 DENIED(FsCreateFile(r,"jl/new","x",1,0666),"create via junction parent");
 {FS_BATCH_CREATE b[2]={{"inside/ok1","x",1,0666},{"jl/new","x",1,0666}};
  DENIED(FsBatchCreate(r,b,2),"batch create via junction parent");}
 {FS_BATCH_REPLACE b[2]={{"inside/f","REAL",4,"NEW!",4},{"jl/victim","VICTIM",6,"HACKED",6}};
  DENIED(FsBatchReplace(r,b,2),"batch replace via junction parent");}
 /* Junction as the leaf: refused, status not predicted. */
 NO(FsReplaceFile(r,"jl","VICTIM",6,"HACKED",6),"replace junction leaf");
 NO(FsRemoveFile(r,"jl","VICTIM",6),"remove junction leaf");
 NO(FsMoveFile(r,"jl","inside/m","VICTIM",6),"move junction leaf source");
 NO(FsMoveFile(r,"inside/f","jl","REAL",4),"move onto junction leaf");
 NO(FsCopyFile(r,"jl","inside/c","VICTIM",6),"copy junction leaf source");
 NO(FsCopyFile(r,"inside/f","jl","REAL",4),"copy onto junction leaf");
 NO(FsCreateFile(r,"jl","x",1,0666),"create over junction leaf");
 FsReadClose(r);
 /* Junction as the workspace root: no writer is ever handed such a root. */
 s=FsReadOpen(RL,&rl);ck(s!=FS_READ_OK&&!rl,"junction root accepted");
 if(rl)FsReadClose(rl);
 ck(count(WS "\\*")==2,"junction root: workspace entries");
 unlink_dir(RL);unlink_dir(WS "\\jl");
 cleanup();
 ck(refused==16,"cell count");
#ifdef FS_REPARSE_MUTANT
 printf("MC1 SURVIVED: the matrix did not notice win_meta accepting reparse points\n");return 1;
#else
 printf("fs win reparse ok: %d refused cells\n",refused);
 return 0;
#endif
}
#else
int main(void){return 0;}
#endif
