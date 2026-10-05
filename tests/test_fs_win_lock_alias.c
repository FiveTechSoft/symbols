#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_read.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 1, Windows 8.3 short names, isolating why the alias of ".fstxn.lock" is refused (m202 follow-up).
   test_fs_win_aliases_short measured: the short name of the lock file is refused with DENIED by copy source, move source,
   replace, remove and batch replace (and by the four ops that name an existing target), while an ordinary file's alias and an
   empty ordinary file are accepted. Two explanations stay open: (H1) the byte-range lock the library holds on the lock file
   makes the byte check fail (ReadFile under LockFileEx); (H2) something else about the lock file or its name. Three groups
   of the five ops that pass the expected bytes "" (copy source, move source, replace, remove, batch replace), each on a
   fresh fixture, each target an empty file:
   held_long   an ordinary empty file inside/held_long_name_file.txt reached by its long name while the test itself holds
               LockFileEx on byte 0 of it with another handle (H1 on an ordinary file: no name or control role involved);
   foreign_long a foreign empty file ".fstxn.foreign" in the root reached by its long name (control: the name check);
   foreign_alias the same foreign file reached by its 8.3 name (a reserved-prefix file that nobody locks). m203 measured
               this group accepted by all five ops (a name-only check bypassed); m204 adds the final-name check in wc_open
               and the group is predicted refused like the long name.
   Each cell prints op=<status><state>: status is the FS_READ_STATUS number, state is of the target afterwards: e empty and
   present, - missing, n other bytes. The line is matched by PASS_REGULAR_EXPRESSION in CMakeLists.txt; the values there are
   my predictions, so a wrong one goes red and prints the measured line. The 8.3 setting of the volume is switched on for the
   test and restored by atexit, as in test_fs_win_aliases_short. Only scratch directories under the working directory are touched. */
#define WS "test_fs_winlockalias_ws"
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
static char state(const char *path)
{char b[16]={0};FILE *f=fopen(path,"rb");size_t n;
 if(!f)return '-';n=fread(b,1,sizeof(b)-1,f);fclose(f);return n==0?'e':'n';}
static void fixture(FS_READ_ROOT **r)
{rmtree(WS);ck(_mkdir(WS)==0&&_mkdir(WS "\\inside")==0,"mkdir");
 put(WS "\\inside\\f","REAL");ck(FsReadOpen(WS,r)==FS_READ_OK,"open workspace");}
static const char *op[5]={"copy_source","move_source","replace","remove","batch_replace"};
static void group(const char *label,int g,char *out)
{char tbl[300]="",full[MAX_PATH],shortp[MAX_PATH],name[96],path[MAX_PATH+16];int i;DWORD n;
 for(i=0;i<5;i++){
  FS_READ_ROOT *r;FS_READ_STATUS res;HANDLE held=INVALID_HANDLE_VALUE;OVERLAPPED o;
  fixture(&r);
  if(g==0){put(WS "\\inside\\held_long_name_file.txt","");
   snprintf(name,sizeof(name),"inside/held_long_name_file.txt");snprintf(path,sizeof(path),"%s",WS "\\inside\\held_long_name_file.txt");
   held=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
   ck(held!=INVALID_HANDLE_VALUE,"open held");memset(&o,0,sizeof(o));
   ck(LockFileEx(held,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&o),"hold byte lock");}
  else{put(WS "\\.fstxn.foreign","");snprintf(path,sizeof(path),"%s",WS "\\.fstxn.foreign");
   if(g==1)snprintf(name,sizeof(name),".fstxn.foreign");
   else{n=GetFullPathNameA(path,MAX_PATH,full,NULL);ck(n>0&&n<MAX_PATH,"foreign path");
    n=GetShortPathNameA(full,shortp,MAX_PATH);ck(n>0&&n<MAX_PATH,"short path");
    {char *leaf=strrchr(shortp,'\\');leaf=leaf?leaf+1:shortp;
     ck(_stricmp(leaf,".fstxn.foreign")!=0,"foreign file has no short name with 8.3 on");
     snprintf(name,sizeof(name),"%s",leaf);}}}
  switch(i){
  case 0:res=FsCopyFile(r,name,"inside/c","",0);break;
  case 1:res=FsMoveFile(r,name,"inside/m","",0);break;
  case 2:res=FsReplaceFile(r,name,"",0,"NEW!",4);break;
  case 3:res=FsRemoveFile(r,name,"",0);break;
  default:{FS_BATCH_REPLACE b[2]={{"inside/f","REAL",4,"NEW!",4},{name,"",0,"NEW!",4}};res=FsBatchReplace(r,b,2);}
  }
  if(held!=INVALID_HANDLE_VALUE){UnlockFileEx(held,0,1,0,&o);CloseHandle(held);}
  snprintf(tbl+strlen(tbl),sizeof(tbl)-strlen(tbl),"%s%s=%d%c",i?" ":"",op[i],(int)res,state(path));
  FsReadClose(r);rmtree(WS);}
 snprintf(out+strlen(out),1024-strlen(out),"%s%s: %s",out[0]?" ; ":"",label,tbl);}
int main(void)
{
 char out[1024]="",full[MAX_PATH],shortp[MAX_PATH];DWORD n;int had;
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
 group("held_long",0,out);group("foreign_long",1,out);group("foreign_alias",2,out);
 printf("lock alias cells: %s\n",out);
 return 0;}
#else
int main(void){return 0;}
#endif
