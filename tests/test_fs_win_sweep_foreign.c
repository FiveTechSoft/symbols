#ifdef _WIN32
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 2 and 1, Windows (m213): what the orphan sweep does to a FOREIGN file whose name has the exact shape of a
   control name. The sweep in src/fs_batch_win.inc (wb_sweep_orphans) runs from FsBatchRecover, the batch calls and
   FsReplaceRecover when no journal or marker of any protocol exists. It touches root regular files named prefix + 32 lowercase
   hex for .fst- .fsp- .fsrp- .fsrs-, decides per name, and deletes .fst- .fsp- and .fsrs- whatever their link count and
   .fsrp- only when it has two or more links. I read that code; nobody measured it against a file the library did not write.
   Each cell: a fresh workspace, one foreign file "<prefix>0123456789abcdef0123456789abcdef" with the bytes "FOREIGN", then
   one recovery call that must return OK. State afterwards: o present with the same bytes, - missing, n other bytes.
   Prefixes: the four swept shapes and the four other reserved prefixes (.fsj- .fsrb- .fsrm- .fsmv-), which the sweep does not
   match. "twolink": a foreign ".fsrp-" file that also has a second name "other.txt" (CreateHardLink): the sweep may drop the
   name only; the state is of the .fsrp- name then of other.txt. Predictions, written before the first run: the three
   deleted shapes are deleted, which means a foreign file of that exact name loses its bytes (a measured limit of the
   reserved namespace, not a bug the test can fix); .fsrp- with one link, and the four other prefixes, stay; twolink is -o
   (the extra name goes, the data stays under other.txt). The line is matched by PASS_REGULAR_EXPRESSION; a wrong prediction
   goes red and prints the measured line. Only a scratch directory under the working directory is touched. */
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
static FS_READ_STATUS recover(FS_READ_ROOT *r,int mode){return mode==0?FsBatchRecover(r):FsReplaceRecover(r);}
static char cell(int mode,const char *prefix)
{FS_READ_ROOT *r;char path[256];char st;
 rmtree(WS);ck(_mkdir(WS)==0,"mkdir");
 snprintf(path,sizeof(path),WS "\\%s" HEX,prefix);put(path,"FOREIGN");
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open");
 ck(recover(r,mode)==FS_READ_OK,"recover returns OK");
 st=state(path);FsReadClose(r);return st;}
static void twolink(int mode,char *a,char *b)
{FS_READ_ROOT *r;char path[256];
 rmtree(WS);ck(_mkdir(WS)==0,"mkdir");
 snprintf(path,sizeof(path),WS "\\.fsrp-" HEX);put(path,"FOREIGN");
 ck(CreateHardLinkA(WS "\\other.txt",path,NULL),"hard link");
 ck(FsReadOpen(WS,&r)==FS_READ_OK,"open twolink");
 ck(recover(r,mode)==FS_READ_OK,"recover twolink returns OK");
 *a=state(path);*b=state(WS "\\other.txt");FsReadClose(r);}
int main(void)
{
 static const char *pre[8]={".fst-",".fsp-",".fsrp-",".fsrs-",".fsj-",".fsrb-",".fsrm-",".fsmv-"};
 static const char *name[2]={"batch","replace"};
 char out[1024]="";int m,i;
 for(m=0;m<2;m++){char a,b,tmp[96];
  strcat(out,name[m]);strcat(out,":");
  for(i=0;i<8;i++){snprintf(tmp,sizeof(tmp)," %s=%c",pre[i],cell(m,pre[i]));strcat(out,tmp);}
  twolink(m,&a,&b);snprintf(tmp,sizeof(tmp)," twolink=%c%c%s",a,b,m==0?"; ":"");strcat(out,tmp);}
 printf("sweep foreign cells: %s\n",out);fflush(stdout);
 rmtree(WS);
 return 0;}
#else
int main(void){return 0;}
#endif
