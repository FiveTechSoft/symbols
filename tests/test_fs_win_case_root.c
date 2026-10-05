#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <direct.h>
/* M1 criterion 1, Windows per-directory case flag on the WORKSPACE ROOT (m200). test_fs_win_case_flag (m198) flagged a
   subdirectory only. The root holds the lock file ".fstxn.lock" and the journals, and the reserved names are matched
   case-insensitively by the name check. Questions: (1) a foreign file ".FSTXN.LOCK" (3 bytes) in a flagged root: does
   the library still take its own empty ".fstxn.lock", leave the foreign file alone, and do a create; (2) can the library
   create or remove ".FSTXN.LOCK" (a reserved name in another case); (3) replace and remove of "A.txt" next to "a.txt"
   (equal bytes) in the flagged root, which also runs the journal path there. One fresh fixture per cell. The flag is set
   on the empty scratch root with `fsutil file setCaseSensitiveInfo` before the library opens it. Cell format: name=<status>
   then one state letter per entry: o original bytes, - missing, n other bytes or newly present. The line is matched by
   PASS_REGULAR_EXPRESSION in CMakeLists.txt; the expected values there are my predictions, so a wrong prediction goes red
   and prints the measured line. Only a scratch directory under the working directory is touched. */
#define WS "test_fs_wincaseroot_ws"
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static char state(const char *path,const char *orig)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n,want=orig?strlen(orig):0;
 if(!f)return '-';n=fread(b,1,sizeof(b)-1,f);fclose(f);
 return orig&&n==want&&!memcmp(b,orig,n)?'o':'n';}
static void fixture(FS_READ_ROOT **r,int foreign,int pair)
{char full[MAX_PATH],cmd[600];
 rmtree(WS);ck(_mkdir(WS)==0,"mkdir");
 ck(GetFullPathNameA(WS,MAX_PATH,full,NULL)>0,"full path");
 snprintf(cmd,sizeof(cmd),"fsutil file setCaseSensitiveInfo \"%s\" enable >NUL 2>&1",full);
 ck(system(cmd)==0,"set case flag on root");
 if(foreign)put(WS "\\.FSTXN.LOCK","BAD");
 if(pair){put(WS "\\a.txt","SAME!");put(WS "\\A.txt","SAME!");
  ck(state(WS "\\a.txt","SAME!")=='o'&&state(WS "\\A.txt","SAME!")=='o',"both entries exist");}
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static char out[1024];
static void add(const char *fmt,...)
{va_list ap;size_t n=strlen(out);va_start(ap,fmt);vsnprintf(out+n,sizeof(out)-n,fmt,ap);va_end(ap);}
int main(void)
{
 FS_READ_ROOT *r;FS_READ_STATUS s;
 fixture(&r,1,0);s=FsCreateFile(r,"a.txt","x",1,0666);
 add("foreign_lock=%d%c%c%c",(int)s,state(WS "\\.fstxn.lock",""),state(WS "\\.FSTXN.LOCK","BAD"),state(WS "\\a.txt","x"));
 FsReadClose(r);
 fixture(&r,0,0);s=FsCreateFile(r,".FSTXN.LOCK","x",1,0666);
 add(" reserved_create=%d%c",(int)s,state(WS "\\.FSTXN.LOCK",NULL));FsReadClose(r);
 fixture(&r,1,0);s=FsRemoveFile(r,".FSTXN.LOCK","BAD",3);
 add(" reserved_remove=%d%c",(int)s,state(WS "\\.FSTXN.LOCK","BAD"));FsReadClose(r);
 fixture(&r,0,1);s=FsReplaceFile(r,"A.txt","SAME!",5,"NEW!!",5);
 add(" root_replace_aA=%d%c%c",(int)s,state(WS "\\a.txt","SAME!"),state(WS "\\A.txt","SAME!"));FsReadClose(r);
 fixture(&r,0,1);s=FsRemoveFile(r,"A.txt","SAME!",5);
 add(" root_remove_aA=%d%c%c",(int)s,state(WS "\\a.txt","SAME!"),state(WS "\\A.txt","SAME!"));FsReadClose(r);
 rmtree(WS);
 printf("root flag: %s\n",out);
 return 0;}
#else
int main(void){return 0;}
#endif
