#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 1, Windows 8.3 aliases: a probe, not a claim. test_fs_win_aliases_short is Skipped on the runner
   because the volume makes no short name for .fstxn.lock. This test measures, on the volume that holds the
   workspace, (1) whether a new file with a long name gets a short name now, (2) only when step 1 found no short name, whether the runner account can
   switch short name creation on with `fsutil 8dot3name set X: 0` and a new file then gets one, and (3) it puts
   the setting back (set X: 1) if step 1 found no short name, on every path after the enable, including a failed create. It touches the volume setting of the CI runner
   only, never a source file. The printed line is matched by PASS_REGULAR_EXPRESSION in CMakeLists.txt; the
   expected values there are my predictions, so a wrong prediction goes red and prints the measured line. */
#define DIR_ "test_fs_win_8dot3_probe_dir"
static int short_name(const char *rel,char *leaf_out,size_t n)
{char full[MAX_PATH],sp[MAX_PATH],*a,*b;DWORD x=GetFullPathNameA(rel,MAX_PATH,full,NULL),y;
 if(!x||x>=MAX_PATH)return -1;
 y=GetShortPathNameA(full,sp,MAX_PATH);if(!y||y>=MAX_PATH)return -1;
 a=strrchr(full,'\\');b=strrchr(sp,'\\');a=a?a+1:full;b=b?b+1:sp;
 snprintf(leaf_out,n,"%s",b);return _stricmp(a,b)!=0;}
static int make(const char *rel)
{FILE *f=fopen(rel,"wb");if(!f)return 0;fputs("x",f);return fclose(f)==0;}
int main(void)
{char full[MAX_PATH],leaf1[64]="",leaf2[64]="",cmd[128];int s1,s2,rc=-1,restore=-1;char drive;
 system("cmd /D /C if exist " DIR_ " rmdir /S /Q " DIR_ " >NUL 2>&1");
 if(_mkdir(DIR_)!=0){fprintf(stderr,"FAIL mkdir\n");return 1;}
 if(!GetFullPathNameA(DIR_,MAX_PATH,full,NULL)){fprintf(stderr,"FAIL path\n");return 1;}
 drive=full[0];
 if(!make(DIR_ "\\probe_long_file_name_first.txt")){fprintf(stderr,"FAIL create 1\n");return 1;}
 s1=short_name(DIR_ "\\probe_long_file_name_first.txt",leaf1,sizeof(leaf1));
 if(s1==0){snprintf(cmd,sizeof(cmd),"fsutil 8dot3name set %c: 0 >NUL 2>&1",drive);rc=system(cmd);}
 /* No early return from here to the restore below: the setting must be put back on every path. */
 s2=make(DIR_ "\\probe_long_file_name_second.txt")?short_name(DIR_ "\\probe_long_file_name_second.txt",leaf2,sizeof(leaf2)):-1;
 if(s1==0){snprintf(cmd,sizeof(cmd),"fsutil 8dot3name set %c: 1 >NUL 2>&1",drive);restore=system(cmd);}
 system("cmd /D /C rmdir /S /Q " DIR_ " >NUL 2>&1");
 printf("8dot3 probe: drive %c before short=%s enable rc=%d after short=%s restore rc=%d\n",drive,
        s1<0?"error":s1?"yes":"no",rc,s2<0?"error":s2?"yes":"no",restore);
 return 0;}
#else
int main(void){return 0;}
#endif
