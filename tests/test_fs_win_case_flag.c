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
#include <direct.h>
/* M1 criterion 1, Windows per-directory case flag (m198). test_fs_win_case covers default, case-insensitive
   directories only. Here the directory WS\d is switched to case sensitive with `fsutil file setCaseSensitiveInfo`
   (m197 measured that this works on the runner), so "x.txt" and "X.txt" can both exist, and the library is asked to
   act on "d/X.txt". The question is which entry the library touches, because it resolves names with
   OBJ_CASE_INSENSITIVE. Pairs: x.txt "lower" / X.txt "UPPER" (different bytes, so the expected bytes tell them apart);
   z.txt "SAME!" / Z.txt "SAME!" (equal bytes, so a wrong entry would pass the expected-bytes check: the cell that
   matters); y.txt "ylow!" with no Y.txt (create, copy target and move target onto "d/Y.txt").
   One fresh fixture per cell. Each cell prints op_pair=<status><lower><upper>: status is the FS_READ_STATUS number,
   lower and upper are the state of the lowercase and the uppercase entry afterwards: o original bytes, - missing,
   n other bytes or newly present. A read cell prints the first byte of what it read (U, l, S) instead.
   The line is matched by PASS_REGULAR_EXPRESSION in CMakeLists.txt; the expected values there are my predictions,
   so a wrong prediction goes red and prints the measured line. Only scratch directories under the working
   directory are touched; the flag disappears with the directory. */
#define WS "test_fs_wincaseflag_ws"
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
/* '-' missing, 'o' holds exactly orig, 'n' anything else (orig NULL: any present file is 'n') */
static char state(const char *path,const char *orig)
{char b[64]={0};FILE *f=fopen(path,"rb");size_t n,want=orig?strlen(orig):0;
 if(!f)return '-';n=fread(b,1,sizeof(b)-1,f);fclose(f);
 return orig&&n==want&&!memcmp(b,orig,n)?'o':'n';}
static void fixture(FS_READ_ROOT **r,int pair)
{char full[MAX_PATH],cmd[600];
 rmtree(WS);ck(_mkdir(WS)==0&&_mkdir(WS "\\d")==0,"mkdir");
 ck(GetFullPathNameA(WS "\\d",MAX_PATH,full,NULL)>0,"full path");
 snprintf(cmd,sizeof(cmd),"fsutil file setCaseSensitiveInfo \"%s\" enable >NUL 2>&1",full);
 ck(system(cmd)==0,"set case flag");
 if(pair=='x'){put(WS "\\d\\x.txt","lower");put(WS "\\d\\X.txt","UPPER");}
 else if(pair=='z'){put(WS "\\d\\z.txt","SAME!");put(WS "\\d\\Z.txt","SAME!");}
 else {put(WS "\\d\\y.txt","ylow!");put(WS "\\d\\f","FFFFF");}
 if(pair=='x')ck(state(WS "\\d\\x.txt","lower")=='o'&&state(WS "\\d\\X.txt","UPPER")=='o',"both entries exist");
 if(pair=='z')ck(state(WS "\\d\\z.txt","SAME!")=='o'&&state(WS "\\d\\Z.txt","SAME!")=='o',"both entries exist");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static char out[1024];
static void cell(const char *op,int pair,FS_READ_STATUS s,const char *lo,const char *lo_o,const char *up,const char *up_o)
{snprintf(out+strlen(out),sizeof(out)-strlen(out),"%s%s_%c%c=%d%c%c",out[0]?" ":"",op,pair,pair=='x'?'X':pair=='z'?'Z':'Y',
 (int)s,state(lo,lo_o),state(up,up_o));}
int main(void)
{
 FS_READ_ROOT *r;int pi;
 static const char pairs[2]={'x','z'};
 FS_READ_STATUS s;
 /* read: which entry comes back */
 fixture(&r,'x');
 {unsigned char *b=NULL;size_t n=0;FS_READ_META m;s=FsReadFile(r,"d/X.txt",&b,&n,&m);
  snprintf(out,sizeof(out),"read_xX=%d%c",(int)s,s==FS_READ_OK&&n==5?(b[0]=='U'?'U':b[0]=='l'?'l':'?'):'-');free(b);}
 FsReadClose(r);rmtree(WS);
 for(pi=0;pi<2;pi++){
  int p=pairs[pi];
  const char *lo=p=='x'?WS "\\d\\x.txt":WS "\\d\\z.txt",*up=p=='x'?WS "\\d\\X.txt":WS "\\d\\Z.txt";
  const char *lo_o=p=='x'?"lower":"SAME!",*up_o=p=='x'?"UPPER":"SAME!";
  const char *name=p=='x'?"d/X.txt":"d/Z.txt";
  fixture(&r,p);s=FsReplaceFile(r,name,up_o,5,"NEW!!",5);cell("replace",p,s,lo,lo_o,up,up_o);FsReadClose(r);
  fixture(&r,p);s=FsRemoveFile(r,name,up_o,5);cell("remove",p,s,lo,lo_o,up,up_o);FsReadClose(r);
  fixture(&r,p);s=FsCopyFile(r,name,"d/c",up_o,5);cell("copy_source",p,s,lo,lo_o,up,up_o);FsReadClose(r);
  fixture(&r,p);s=FsMoveFile(r,name,"d/m",up_o,5);cell("move_source",p,s,lo,lo_o,up,up_o);FsReadClose(r);
 }
 fixture(&r,'y');s=FsCreateFile(r,"d/Y.txt","new",3,0666);cell("create",'y',s,WS "\\d\\y.txt","ylow!",WS "\\d\\Y.txt",NULL);FsReadClose(r);
 fixture(&r,'y');s=FsCopyFile(r,"d/f","d/Y.txt","FFFFF",5);cell("copy_target",'y',s,WS "\\d\\y.txt","ylow!",WS "\\d\\Y.txt",NULL);FsReadClose(r);
 fixture(&r,'y');s=FsMoveFile(r,"d/f","d/Y.txt","FFFFF",5);cell("move_target",'y',s,WS "\\d\\y.txt","ylow!",WS "\\d\\Y.txt",NULL);FsReadClose(r);
 rmtree(WS);
 printf("case flag: %s\n",out);
 return 0;}
#else
int main(void){return 0;}
#endif
