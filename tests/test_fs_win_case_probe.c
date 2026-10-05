#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
/* M1 criterion 1, Windows per-directory case flag: a probe, not a claim. test_fs_win_case says the flag is out of
   scope. This test measures on the volume that holds the workspace: (1) in a default directory, can a file "x.txt"
   and a second file "X.txt" both exist (expected: no, the second create finds the first); (2) can the runner switch
   one empty directory to case sensitive with `fsutil file setCaseSensitiveInfo DIR enable`; (3) does `fsutil file
   queryCaseSensitiveInfo DIR` say it is enabled; (4) in that directory can both names exist. It only touches a
   scratch directory under the build working directory and removes it. The printed line is matched by
   PASS_REGULAR_EXPRESSION in CMakeLists.txt; the expected values there are my predictions, so a wrong prediction
   goes red and prints the measured line. */
#define D1 "test_fs_win_case_probe_default"
#define D2 "test_fs_win_case_probe_flag"
static int make(const char *p)
{HANDLE h=CreateFileA(p,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;CloseHandle(h);return 1;}
static void rmtree(const char *d)
{char cmd[256];snprintf(cmd,sizeof(cmd),"cmd /D /C if exist %s rmdir /S /Q %s >NUL 2>&1",d,d);system(cmd);}
int main(void)
{char cmd[512],full[MAX_PATH];int d1a,d1b,rc,q,f2a=0,f2b=0;
 rmtree(D1);rmtree(D2);
 if(_mkdir(D1)!=0||_mkdir(D2)!=0){fprintf(stderr,"FAIL mkdir\n");return 1;}
 if(!GetFullPathNameA(D2,MAX_PATH,full,NULL)){fprintf(stderr,"FAIL path\n");return 1;}
 d1a=make(D1 "\\x.txt");d1b=make(D1 "\\X.txt");
 snprintf(cmd,sizeof(cmd),"fsutil file setCaseSensitiveInfo \"%s\" enable >NUL 2>&1",full);rc=system(cmd);
 snprintf(cmd,sizeof(cmd),"fsutil file queryCaseSensitiveInfo \"%s\" 2>&1 | findstr /C:\"is enabled\" >NUL",full);q=system(cmd);
 f2a=make(D2 "\\x.txt");f2b=make(D2 "\\X.txt");
 rmtree(D1);rmtree(D2);
 printf("case probe: default both=%s set rc=%d query enabled=%s flagged both=%s\n",
        d1a&&d1b?"yes":"no",rc,q==0?"yes":"no",f2a&&f2b?"yes":"no");
 return 0;}
#else
int main(void){return 0;}
#endif
