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
/* Criterion 4, Windows: case-insensitive names against the writers (NTFS, one
   runner). The per-directory case-sensitivity flag of NTFS is out of scope:
   every directory here has the default, case-insensitive behaviour.

   Fixture: a directory "Dir" holding "File.txt" (REAL) and "Src" (SRC).
   A name that differs from the stored one only by case is an alias of the
   stored entry. Expected result per cell, stated before the first measurement:
   - Creating an alias of an existing name (create, copy target, move target,
     batch create) is refused (status other than OK) and changes nothing: both
     entries keep their bytes and the directory holds exactly the same names.
     Create already shows DENIED in test_fs_create_windows; the others are
     predicted refusals, only "not OK" is asserted.
   - Addressing an existing entry through its alias (replace, remove, copy
     source, move source, batch replace) acts on that one entry and returns OK,
     because every component is resolved by win_open with OBJ_CASE_INSENSITIVE
     and the operations work on handles. This is a prediction: if one of them
     is refused it is still fail-closed, but it is a result the test reports as
     a failure so it can be read, not repaired here. A replace through the
     alias with a wrong expected image is refused and changes nothing.
   - A directory component given in another case ("DIR/Fresh") resolves to the
     existing directory: no second directory appears.
   - Create keeps the case it was given ("Plain.txt" is stored as written).
   Not asserted: which case the entry shows after a replace or a move through
   an alias, and any status other than OK for the refused cells.
   Out of scope and not claimed: the per-directory case-sensitivity flag, other
   volumes, a manifest or transaction plan with aliases, Unicode case folding
   beyond ASCII, other runners. */
#define WS "test_fs_wincase_ws"
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static int bytes_are(const char *path,const char *v)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n=strlen(v);int ok;
 if(!f)return 0;ok=fread(b,1,sizeof(b)-1,f)==n&&!memcmp(b,v,n);fclose(f);return ok;}
static int exists(const char *p){return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;}
/* entries of a directory, without the workspace lock; *exact is set when an
   entry is spelled exactly like "name" */
static int entries(const char *pattern,const char *name,int *exact)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pattern,&d);int n=0;
 if(exact)*exact=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&strcmp(d.cFileName,".fstxn.lock")){
     n++;if(exact&&name&&!strcmp(d.cFileName,name))*exact=1;}}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static void fixture(FS_READ_ROOT **r)
{rmtree(WS);ck(_mkdir(WS)==0&&_mkdir(WS "\\Dir")==0,"mkdir");
 put(WS "\\Dir\\File.txt","REAL");put(WS "\\Dir\\Src","SRC");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static int cells;
#define STAY_OK(expr,what) do{ck((expr)==FS_READ_OK,what);cells++;}while(0)
#define STAY_REFUSED(expr,what) do{ck((expr)!=FS_READ_OK,what);cells++;}while(0)
/* nothing changed: both entries and the exact names */
static void untouched(const char *what)
{int exact;char m[128];
 snprintf(m,sizeof(m),"%s: File.txt bytes",what);ck(bytes_are(WS "\\Dir\\File.txt","REAL"),m);
 snprintf(m,sizeof(m),"%s: Src bytes",what);ck(bytes_are(WS "\\Dir\\Src","SRC"),m);
 snprintf(m,sizeof(m),"%s: Dir entries",what);ck(entries(WS "\\Dir\\*","File.txt",&exact)==2&&exact,m);
 snprintf(m,sizeof(m),"%s: workspace entries",what);ck(entries(WS "\\*","Dir",&exact)==1&&exact,m);}
int main(void)
{
 FS_READ_ROOT *r;int exact;
 /* controls: legal names work, and create keeps the case it was given */
 fixture(&r);
 STAY_OK(FsCreateFile(r,"Dir/Plain.txt","P",1,0666),"control create");
 ck(entries(WS "\\Dir\\*","Plain.txt",&exact)==3&&exact,"create keeps the given case");
 STAY_OK(FsRemoveFile(r,"Dir/Plain.txt","P",1),"control remove");
 untouched("after controls");FsReadClose(r);
 /* creating an alias of an existing name */
 fixture(&r);
 STAY_REFUSED(FsCreateFile(r,"dir/FILE.TXT","x",1,0666),"create alias");untouched("create alias");
 STAY_REFUSED(FsCopyFile(r,"Dir/Src","dir/FILE.TXT","SRC",3),"copy target alias");untouched("copy target alias");
 STAY_REFUSED(FsMoveFile(r,"Dir/Src","DIR/file.txt","SRC",3),"move target alias");untouched("move target alias");
 {FS_BATCH_CREATE b[2]={{"Dir/New1","x",1,0666},{"dir/FILE.TXT","x",1,0666}};
  STAY_REFUSED(FsBatchCreate(r,b,2),"batch create alias");}
 ck(!exists(WS "\\Dir\\New1"),"batch create alias: nothing created");untouched("batch create alias");
 STAY_REFUSED(FsReplaceFile(r,"dir/FILE.TXT","XXXX",4,"NEW!",4),"replace alias, wrong expected");untouched("replace alias, wrong expected");
 FsReadClose(r);
 /* addressing an existing entry through its alias */
 fixture(&r);
 STAY_OK(FsReplaceFile(r,"dir/FILE.TXT","REAL",4,"NEW!",4),"replace through alias");
 ck(bytes_are(WS "\\Dir\\File.txt","NEW!"),"replace through alias: bytes");
 ck(entries(WS "\\Dir\\*",NULL,NULL)==2&&entries(WS "\\*",NULL,NULL)==1,"replace through alias: no extra entry");
 FsReadClose(r);
 fixture(&r);
 STAY_OK(FsRemoveFile(r,"DIR/FILE.TXT","REAL",4),"remove through alias");
 ck(!exists(WS "\\Dir\\File.txt")&&bytes_are(WS "\\Dir\\Src","SRC")&&entries(WS "\\Dir\\*",NULL,NULL)==1,"remove through alias: state");
 FsReadClose(r);
 fixture(&r);
 STAY_OK(FsCopyFile(r,"dir/FILE.TXT","Dir/Copy.txt","REAL",4),"copy source through alias");
 ck(bytes_are(WS "\\Dir\\Copy.txt","REAL")&&bytes_are(WS "\\Dir\\File.txt","REAL")&&entries(WS "\\Dir\\*",NULL,NULL)==3,"copy source through alias: state");
 FsReadClose(r);
 fixture(&r);
 STAY_OK(FsMoveFile(r,"DIR/FILE.TXT","Dir/Moved","REAL",4),"move source through alias");
 ck(bytes_are(WS "\\Dir\\Moved","REAL")&&!exists(WS "\\Dir\\File.txt")&&entries(WS "\\Dir\\*",NULL,NULL)==2,"move source through alias: state");
 FsReadClose(r);
 fixture(&r);
 {FS_BATCH_REPLACE b[2]={{"dir/FILE.TXT","REAL",4,"NEW!",4},{"Dir/Src","SRC",3,"SRC2",4}};
  STAY_OK(FsBatchReplace(r,b,2),"batch replace through alias");}
 ck(bytes_are(WS "\\Dir\\File.txt","NEW!")&&bytes_are(WS "\\Dir\\Src","SRC2")&&entries(WS "\\Dir\\*",NULL,NULL)==2,"batch replace through alias: state");
 FsReadClose(r);
 /* a directory component in another case resolves to the existing directory */
 fixture(&r);
 STAY_OK(FsCreateFile(r,"DIR/Fresh","F",1,0666),"create under directory alias");
 ck(bytes_are(WS "\\Dir\\Fresh","F")&&entries(WS "\\*","Dir",&exact)==1&&exact,"create under directory alias: one directory, original spelling");
 ck(entries(WS "\\Dir\\*",NULL,NULL)==3,"create under directory alias: entries");
 FsReadClose(r);
 ck(cells==13,"cell count");
 rmtree(WS);
 printf("fs win case ok: %d cells\n",cells);
 return 0;
}
#else
int main(void){return 0;}
#endif
