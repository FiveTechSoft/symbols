#ifdef _WIN32
#include "fs_replace.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 1, Windows 8.3 short names, the other reserved prefixes (m206). m203 found that the 8.3 alias of a foreign
   ".fstxn.foreign" file was accepted and m204 refuses it by reading the final name of the opened handle in wc_open. The
   reserved check in src/fs_create_win.inc (wc_reserved) covers nine prefixes: .fstxn .fst- .fsp- .fsj- .fsrp- .fsrs- .fsrb-
   .fsrm- .fsmv-. m204 tested only the first. Here, for each prefix, an empty foreign file "<prefix>foreign" in the root is
   replaced through its long name and through its 8.3 name ("" expected, "NEW!" new), one fresh fixture per cell. The foreign
   names never equal a real control name, so the library does not read them as journals. Cell format per prefix:
   prefix=<long status><state><alias status><state>; status is the FS_READ_STATUS number (3 DENIED, 0 OK), state is of the file
   afterwards: e empty and present, - missing, n other bytes. One more cell, "dir": a directory ".fstxn.dir" holding
   "inner.txt" (bytes "IN") replaced through its long path and through the 8.3 name of the directory ("FSTXN~1.DIR/inner.txt");
   the reserved check names every path segment, but the parent directory is opened by win_open, not wc_open, so m204 does not
   cover it (stated as not covered in the docs). Cell dir=<long status><state><alias status><state>, state of inner.txt: o
   original "IN", n other bytes, - missing. The line is matched by PASS_REGULAR_EXPRESSION in CMakeLists.txt; the values there are my
   predictions, so a wrong one goes red and prints the measured line. The 8.3 setting of the volume is switched on for the test
   and restored by atexit, as in test_fs_win_aliases_short. Only a scratch directory under the working directory is touched. */
#define WS "test_fs_winresalias_ws"
static void rmtree(const char *rel)
{char full[MAX_PATH],cmd[2*MAX_PATH+64];DWORD n=GetFullPathNameA(rel,MAX_PATH,full,NULL);
 if(!n||n>=MAX_PATH)return;
 snprintf(cmd,sizeof(cmd),"cmd /D /C if exist \"%s\" rmdir /S /Q \"\\\\?\\%s\" >NUL 2>&1",full,full);
 system(cmd);}
static char drive_off[64];static int restore_needed;
static void restore_8dot3(void){if(restore_needed){restore_needed=0;system(drive_off);}}
static void fail(const char *what){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());rmtree(WS);exit(1);}
static void ck(int ok,const char *what){if(!ok)fail(what);}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static char state(const char *path,const char *orig)
{char b[16]={0};FILE *f=fopen(path,"rb");size_t n;
 if(!f)return '-';n=fread(b,1,sizeof(b)-1,f);fclose(f);
 if(orig)return n==strlen(orig)&&!memcmp(b,orig,n)?'o':'n';
 return n==0?'e':'n';}
static void shortleaf(const char *path,char *leaf,size_t cap)
{char full[MAX_PATH],shortp[MAX_PATH],*l;DWORD n=GetFullPathNameA(path,MAX_PATH,full,NULL);
 ck(n>0&&n<MAX_PATH,"full path");n=GetShortPathNameA(full,shortp,MAX_PATH);ck(n>0&&n<MAX_PATH,"short path");
 l=strrchr(shortp,'\\');l=l?l+1:shortp;
 ck(_stricmp(l,strrchr(full,'\\')+1)!=0,"no short name with 8.3 on");snprintf(leaf,cap,"%s",l);}
static void fixture(FS_READ_ROOT **r)
{rmtree(WS);ck(_mkdir(WS)==0,"mkdir");}
int main(void)
{
 static const char *pre[9]={".fstxn.",".fst-",".fsp-",".fsj-",".fsrp-",".fsrs-",".fsrb-",".fsrm-",".fsmv-"};
 char out[1024]="",full[MAX_PATH],shortp[MAX_PATH];DWORD n;int had,i,a;
 {char probe[MAX_PATH];FILE *f;char drive;
  n=GetFullPathNameA(".",MAX_PATH,full,NULL);ck(n>0&&n<MAX_PATH,"cwd path");drive=full[0];
  snprintf(drive_off,sizeof(drive_off),"fsutil 8dot3name set %c: 1 >NUL 2>&1",drive);
  rmtree(WS);ck(_mkdir(WS)==0,"mkdir probe");
  f=fopen(WS "\\probe_long_file_name_first.txt","wb");ck(f&&fclose(f)==0,"probe file");
  n=GetFullPathNameA(WS "\\probe_long_file_name_first.txt",MAX_PATH,probe,NULL);ck(n>0&&n<MAX_PATH,"probe path");
  n=GetShortPathNameA(probe,shortp,MAX_PATH);
  had=n&&n<MAX_PATH&&_stricmp(strrchr(shortp,'\\')+1,strrchr(probe,'\\')+1)!=0;
  rmtree(WS);
  if(!had){char cmd[64];atexit(restore_8dot3);restore_needed=1;
   snprintf(cmd,sizeof(cmd),"fsutil 8dot3name set %c: 0 >NUL 2>&1",drive);ck(system(cmd)==0,"enable 8dot3");}}
 for(i=0;i<9;i++){
  char lname[64],path[MAX_PATH],al[96],cellbuf[48];FS_READ_STATUS s[2];char st[2];
  snprintf(lname,sizeof(lname),"%sforeign",pre[i]);snprintf(path,sizeof(path),WS "\\%s",lname);
  for(a=0;a<2;a++){FS_READ_ROOT *r;const char *name=lname;
   fixture(&r);put(path,"");
   if(a){shortleaf(path,al,sizeof(al));name=al;}
   ck(FsReadOpen(WS,&r)==FS_READ_OK,"open workspace");
   s[a]=FsReplaceFile(r,name,"",0,"NEW!",4);st[a]=state(path,NULL);FsReadClose(r);rmtree(WS);}
  snprintf(cellbuf,sizeof(cellbuf),"%s=%d%c%d%c",pre[i],(int)s[0],st[0],(int)s[1],st[1]);
  snprintf(out+strlen(out),sizeof(out)-strlen(out),"%s%s",out[0]?" ":"",cellbuf);}
 {FS_READ_STATUS s[2];char st[2];int b;
  for(b=0;b<2;b++){FS_READ_ROOT *r;char al[96],name[160];
   fixture(&r);ck(_mkdir(WS "\\.fstxn.dir")==0,"mkdir reserved dir");put(WS "\\.fstxn.dir\\inner.txt","IN");
   if(b){shortleaf(WS "\\.fstxn.dir",al,sizeof(al));snprintf(name,sizeof(name),"%s/inner.txt",al);}
   else snprintf(name,sizeof(name),".fstxn.dir/inner.txt");
   ck(FsReadOpen(WS,&r)==FS_READ_OK,"open workspace");
   s[b]=FsReplaceFile(r,name,"IN",2,"NEW!",4);st[b]=state(WS "\\.fstxn.dir\\inner.txt","IN");FsReadClose(r);rmtree(WS);}
  snprintf(out+strlen(out),sizeof(out)-strlen(out)," dir=%d%c%d%c",(int)s[0],st[0],(int)s[1],st[1]);}
 rmtree(WS);
 printf("reserved alias cells: %s\n",out);
 return 0;}
#else
int main(void){return 0;}
#endif
