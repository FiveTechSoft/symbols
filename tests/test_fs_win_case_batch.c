#ifdef _WIN32
#include "fs_batch.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 1, Windows per-directory case flag, batch operations (m201). test_fs_win_case_flag (m198) measured
   single operations in a case-sensitive directory: the library acts on the exact-case entry. The batch code compares
   target names ignoring case on purpose ("errs toward refusing"), so two questions: (1) a batch naming "d/x.txt" and
   "d/X.txt", two real entries in the flagged directory, is it refused whole, and are both entries left alone;
   (2) a batch naming one cased entry plus an unrelated file: does it act on the exact-case entry only, as the single
   operations do. Cells: replace of x.txt+X.txt (different bytes); replace of X.txt+k.txt next to x.txt; the same with
   equal bytes Z.txt/z.txt (the cell a wrong entry could hide in); create of n.txt+N.txt (neither exists); create of
   Y.txt+k.txt next to an existing y.txt. One fresh fixture per cell. Each cell prints name=<status><lower><upper><k>:
   status is the FS_READ_STATUS number, then the state of the lowercase entry, the uppercase entry and k.txt afterwards:
   o original bytes, - missing, n other bytes or newly present. The line is matched by PASS_REGULAR_EXPRESSION in
   CMakeLists.txt; the expected values there are my predictions, so a wrong prediction goes red and prints the
   measured line. Only a scratch directory under the working directory is touched. */
#define WS "test_fs_wincasebatch_ws"
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
static void fixture(FS_READ_ROOT **r,int pair)
{char full[MAX_PATH],cmd[600];
 rmtree(WS);ck(_mkdir(WS)==0&&_mkdir(WS "\\d")==0,"mkdir");
 ck(GetFullPathNameA(WS "\\d",MAX_PATH,full,NULL)>0,"full path");
 snprintf(cmd,sizeof(cmd),"fsutil file setCaseSensitiveInfo \"%s\" enable >NUL 2>&1",full);
 ck(system(cmd)==0,"set case flag");
 if(pair=='x'){put(WS "\\d\\x.txt","lower");put(WS "\\d\\X.txt","UPPER");put(WS "\\d\\k.txt","KKKKK");}
 else if(pair=='z'){put(WS "\\d\\z.txt","SAME!");put(WS "\\d\\Z.txt","SAME!");put(WS "\\d\\k.txt","KKKKK");}
 else if(pair=='y')put(WS "\\d\\y.txt","ylow!");
 if(pair=='x')ck(state(WS "\\d\\x.txt","lower")=='o'&&state(WS "\\d\\X.txt","UPPER")=='o',"both entries exist");
 if(pair=='z')ck(state(WS "\\d\\z.txt","SAME!")=='o'&&state(WS "\\d\\Z.txt","SAME!")=='o',"both entries exist");
 ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static char out[1024];
static void cell(const char *name,FS_READ_STATUS s,const char *lo,const char *lo_o,const char *up,const char *up_o,const char *k_o)
{snprintf(out+strlen(out),sizeof(out)-strlen(out),"%s%s=%d%c%c%c",out[0]?" ":"",name,(int)s,
 state(lo,lo_o),state(up,up_o),state(WS "\\d\\k.txt",k_o));}
int main(void)
{
 FS_READ_ROOT *r;FS_READ_STATUS s;
 out[0]=0;
 {FS_BATCH_REPLACE e[2]={{"d/x.txt","lower",5,"NEWLO",5},{"d/X.txt","UPPER",5,"NEWUP",5}};
  fixture(&r,'x');s=FsBatchReplace(r,e,2);
  cell("rep_dup_xX",s,WS "\\d\\x.txt","lower",WS "\\d\\X.txt","UPPER","KKKKK");FsReadClose(r);}
 {FS_BATCH_REPLACE e[2]={{"d/X.txt","UPPER",5,"NEWUP",5},{"d/k.txt","KKKKK",5,"NEWKK",5}};
  fixture(&r,'x');s=FsBatchReplace(r,e,2);
  cell("rep_X_xX",s,WS "\\d\\x.txt","lower",WS "\\d\\X.txt","UPPER","KKKKK");FsReadClose(r);}
 {FS_BATCH_REPLACE e[2]={{"d/Z.txt","SAME!",5,"NEWZZ",5},{"d/k.txt","KKKKK",5,"NEWKK",5}};
  fixture(&r,'z');s=FsBatchReplace(r,e,2);
  cell("rep_Z_zZ",s,WS "\\d\\z.txt","SAME!",WS "\\d\\Z.txt","SAME!","KKKKK");FsReadClose(r);}
 {FS_BATCH_CREATE e[2]={{"d/n.txt","aaa",3,0666},{"d/N.txt","bbb",3,0666}};
  fixture(&r,'n');s=FsBatchCreate(r,e,2);
  cell("cre_dup_nN",s,WS "\\d\\n.txt",NULL,WS "\\d\\N.txt",NULL,NULL);FsReadClose(r);}
 {FS_BATCH_CREATE e[2]={{"d/Y.txt","new",3,0666},{"d/k.txt","kkk",3,0666}};
  fixture(&r,'y');s=FsBatchCreate(r,e,2);
  cell("cre_Y_yY",s,WS "\\d\\y.txt","ylow!",WS "\\d\\Y.txt",NULL,NULL);FsReadClose(r);}
 rmtree(WS);
 printf("batch flag: %s\n",out);
 return 0;}
#else
int main(void){return 0;}
#endif
