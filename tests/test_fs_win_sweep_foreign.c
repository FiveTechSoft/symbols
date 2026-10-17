#ifdef _WIN32
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 2 and 1, Windows (m213): what the orphan sweep does to a FOREIGN file whose name has the exact shape of a
   control name. The sweep in src/fs_batch_win.inc (wb_sweep_orphans) runs from every single-file recovery
   (FsCreateRecover, FsRemoveRecover, FsMoveRecover, FsReplaceRecover since slice G), FsBatchRecover and the batch
   calls when no journal or marker of any protocol exists. It touches root regular files named prefix + 32 lowercase
   hex for .fst- .fsp- .fsrp- .fsrs- .fsrm- .fsmv-, decides per name, and deletes .fst- .fsp- and .fsrs- whatever
   their link count and .fsrp- .fsrm- .fsmv- only when they have two or more links. I read that code; nobody measured it against a file the library did not write.
   Each cell: a fresh workspace, one foreign file "<prefix>0123456789abcdef0123456789abcdef" with the bytes "FOREIGN", then
   one recovery call that must return OK. State afterwards: o present with the same bytes, - missing, n other bytes.
   Prefixes: the six swept shapes and the two other reserved prefixes (.fsj- .fsrb-), which the sweep does not
   match. "twolink"/"rm2"/"mv2": a foreign ".fsrp-"/".fsrm-"/".fsmv-" file that also has a second name "other.txt"
   (CreateHardLink): the sweep may drop the swept name only; the state is of the swept name then of other.txt.
   Predictions, written before the first run: the three new-inode shapes are deleted, which means a foreign file of
   that exact name loses its bytes (a measured limit of the reserved namespace, not a bug the test can fix); one-link
   old-pin shapes and the two other prefixes stay; the twolink cells are -o (the extra name goes, the data stays under
   other.txt). Under FS_WIN_BATCH_MUTANT=13 (test only) the sweep keeps .fsrm- and .fsmv- pins, so rm2 and mv2 read oo;
   the normal run therefore kills mutant 13. The final line is matched exactly by windows_six_gate.py; a wrong
   prediction goes red and prints the measured line. Only a scratch directory under the working directory is touched. */
#define WS "test_fs_winsweepforeign_ws"
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static char state(const char *path)
{char b[16]={0};FILE *f=fopen(path,"rb");size_t n;
 if(!f)return '-';n=fread(b,1,sizeof(b)-1,f);fclose(f);
 return n==7&&!memcmp(b,"FOREIGN",7)?'o':'n';}
#define HEX "0123456789abcdef0123456789abcdef"
static FS_READ_STATUS recover(FS_READ_ROOT *r,int mode){
 switch(mode){
 case 0:return FsBatchRecover(r);
 case 1:return FsReplaceRecover(r);
 case 2:return FsCreateRecover(r);
 case 3:return FsRemoveRecover(r);
 default:return FsMoveRecover(r);}}
static char cell(int mode,const char *prefix)
{FS_READ_ROOT *r;char path[256];char st;
 rmtree(WS);ck(_mkdir(WS)==0,"mkdir");
 snprintf(path,sizeof(path),WS "\\%s" HEX,prefix);put(path,"FOREIGN");
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open");
 ck(recover(r,mode)==FS_READ_OK,"recover returns OK");
 st=state(path);FsReadClose(r);return st;}
static void twolink(const char *prefix,int mode,char *a,char *b)
{FS_READ_ROOT *r;char path[256];
 rmtree(WS);ck(_mkdir(WS)==0,"mkdir");
 snprintf(path,sizeof(path),WS "\\%s" HEX,prefix);put(path,"FOREIGN");
 ck(CreateHardLinkA(WS "\\other.txt",path,NULL),"hard link");
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open twolink");
 ck(recover(r,mode)==FS_READ_OK,"recover twolink returns OK");
 *a=state(path);*b=state(WS "\\other.txt");FsReadClose(r);}
int main(int argc,char**argv)
{
 /* Slice H2 (2026-10-17): the mutant arrives as argv[1] and is activated in-process (test only). Two
    out-of-process deliveries of FS_WIN_BATCH_MUTANT (a CTest ENVIRONMENT property, then the gate
    wrapper's own env=) both failed to reach this binary through the python wrapper on the Windows
    runner while the same property reaches directly-run binaries (mc10/mc11); an argument cannot get
    lost between processes. */
 if(argc>1)_putenv_s("FS_WIN_BATCH_MUTANT",argv[1]);
 /* Slice H3 (2026-10-17), TEMPORARY diagnostic (remove when mc13 is resolved): the merged gate stream
    allows extra lines before the unique final line, so this prints whether the argument arrived and
    whether the in-process env activation stuck. */
 fprintf(stderr,"sweep foreign diag argc=%d mutant_env=%s\n",argc,getenv("FS_WIN_BATCH_MUTANT")?getenv("FS_WIN_BATCH_MUTANT"):"(null)");
 static const char *pre[8]={".fst-",".fsp-",".fsrp-",".fsrs-",".fsj-",".fsrb-",".fsrm-",".fsmv-"};
 static const char *name[5]={"batch","replace","create","remove","move"};
 char out[2048]="";int m,i;
 for(m=0;m<5;m++){char a,b,c,d,e,f,tmp[128];
  strcat(out,name[m]);strcat(out,":");
  for(i=0;i<8;i++){snprintf(tmp,sizeof(tmp)," %s=%c",pre[i],cell(m,pre[i]));strcat(out,tmp);}
  twolink(".fsrp-",m,&a,&b);twolink(".fsrm-",m,&c,&d);twolink(".fsmv-",m,&e,&f);
  snprintf(tmp,sizeof(tmp)," twolink=%c%c rm2=%c%c mv2=%c%c%s",a,b,c,d,e,f,m==4?"":"; ");strcat(out,tmp);}
 printf("sweep foreign cells: %s\n",out);fflush(stdout);
 rmtree(WS);
 return 0;}
#else
int main(void){return 0;}
#endif
