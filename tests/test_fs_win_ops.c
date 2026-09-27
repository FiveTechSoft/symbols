#ifdef _WIN32
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winops_scratch"
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void read_eq(FS_READ_ROOT *r,const char *p,const char *v)
{unsigned char *b=NULL;size_t n=0;FS_READ_META m;size_t len=strlen(v);
 ck(FsReadFile(r,p,&b,&n,&m)==FS_READ_OK&&n==len&&
    !memcmp(b,v,len),"read bytes");free(b);}
static int run_child(const char *exe,int kind,int phase,int recovery)
{char command[1024];STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;
 si.cb=sizeof(si);sprintf(command,"\"%s\" child %d %d %d",exe,kind,phase,recovery);
 ck(CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static void child(int kind,int phase,int recovery)
{FS_READ_ROOT *r;char number[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(number,"%d",kind*10+phase);_putenv_s("FS_WIN_OP_CRASH",number);
 s=recovery?(kind==1?FsRemoveRecover(r):FsMoveRecover(r)):
   kind==1?FsRemoveFile(r,"inside/source","payload",7):
           FsMoveFile(r,"inside/source","inside/target","payload",7);
 fprintf(stderr,"child kind=%d phase=%d recovery=%d status=%d\n",kind,phase,recovery,(int)s);
 FsReadClose(r);ExitProcess(200);}
static void cleanup(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\*",&d);
 ck(f!=INVALID_HANDLE_VALUE,"find");
 do{char path[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,"..")||
      !strcmp(d.cFileName,"inside"))continue;
    snprintf(path,sizeof(path),ROOT "\\%s",d.cFileName);
    ck(DeleteFileA(path),"remove control or orphan");
 }while(FindNextFileA(f,&d));FindClose(f);
 ck(_rmdir(ROOT "\\inside")==0&&_rmdir(ROOT)==0,"remove root");}
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(ROOT "\\inside")==0,"mkdir");
 put(ROOT "\\inside\\source","payload");}
static void state(FS_READ_ROOT *r,int kind,int committed)
{FS_READ_META m;
 ck(FsReadStat(r,"inside/source",&m)==(committed?FS_READ_MISSING:FS_READ_OK),"source state");
 if(!committed)read_eq(r,"inside/source","payload");
 ck(FsReadStat(r,"inside/target",&m)==(kind==2&&committed?FS_READ_OK:FS_READ_MISSING),"target state");
 if(kind==2&&committed)read_eq(r,"inside/target","payload");}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;FS_READ_META m;
 if(argc==5&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]));return 200;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 for(int kind=1;kind<=2;kind++)for(int phase=0;phase<=4;phase++){
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
   ck(run_child(exe,kind,phase,0)==100+kind*10+phase,"write kill");
   ck((kind==1?FsRemoveRecover(r):FsMoveRecover(r))==FS_READ_OK,"replay");
   ck((kind==1?FsRemoveRecover(r):FsMoveRecover(r))==FS_READ_OK,"idempotent");
   state(r,kind,phase==4);
   ck(FsCreateRecover(r)==FS_READ_OK,"other empty recovery");
   FsReadClose(r);
   if(phase!=4)ck(DeleteFileA(ROOT "\\inside\\source"),"clean source");
   if(kind==2&&phase==4)ck(DeleteFileA(ROOT "\\inside\\target"),"clean target");
   cleanup();
 }
 for(int kind=1;kind<=2;kind++)for(int phase=5;phase<=9;phase++){
   int committed=phase==5||phase==9;
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open recovery");
   ck(run_child(exe,kind,committed?4:3,0)==100+kind*10+(committed?4:3),"setup kill");
   ck(run_child(exe,kind,phase,1)==100+kind*10+phase,"recovery kill");
   ck((kind==1?FsRemoveRecover(r):FsMoveRecover(r))==FS_READ_OK,"recovery resume");
   ck((kind==1?FsRemoveRecover(r):FsMoveRecover(r))==FS_READ_OK,"recovery twice");
   state(r,kind,committed);FsReadClose(r);
   if(!committed)ck(DeleteFileA(ROOT "\\inside\\source"),"clean source");
   if(kind==2&&committed)ck(DeleteFileA(ROOT "\\inside\\target"),"clean target");
   cleanup();
 }
 for(int kind=1;kind<=2;kind++){
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open pending");
   ck(run_child(exe,kind,1,0)==100+kind*10+1,"pending intent setup");
   ck(FsCreateFile(r,"blocked","x",1,0666)==FS_READ_DENIED,"create cross block");
   ck(FsCopyFile(r,"inside/source","blocked","payload",7)==FS_READ_DENIED,
      "copy cross block");
   ck((kind==1?FsMoveFile(r,"inside/source","inside/target","payload",7):
     FsRemoveFile(r,"inside/source","payload",7))==FS_READ_DENIED,
      "other operation cross block");
   ck((kind==1?FsMoveRecover(r):FsRemoveRecover(r))==FS_READ_DENIED,
      "other recovery cross block");
   ck((kind==1?FsRemoveRecover(r):FsMoveRecover(r))==FS_READ_OK,"clear pending");
   FsReadClose(r);ck(DeleteFileA(ROOT "\\inside\\source"),"clean source");cleanup();
 }
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open simple");
 ck(FsRemoveFile(r,"inside/source","wrong",5)==FS_READ_DENIED,"stale remove");
 ck(FsMoveFile(r,"inside/source","inside/target","wrong",5)==FS_READ_DENIED,"stale move");
 ck(FsRemoveFile(r,"../escape","payload",7)==FS_READ_INVALID,"remove traversal");
 ck(FsMoveFile(r,"inside/source",".FSTXN.MOVE","payload",7)==FS_READ_DENIED,"move reserved");
 ck(FsMoveFile(r,"inside/source","inside/source","payload",7)==FS_READ_DENIED,"same path");
 ck(CreateHardLinkA(ROOT "\\inside\\alias",ROOT "\\inside\\source",NULL),
    "preexisting hardlink fixture");
 ck(FsRemoveFile(r,"inside/source","payload",7)==FS_READ_DENIED,
    "remove refuses preexisting links");
 ck(FsMoveFile(r,"inside/source","inside/target","payload",7)==FS_READ_DENIED,
    "move refuses preexisting links");
 ck(DeleteFileA(ROOT "\\inside\\alias"),"remove hardlink fixture");
 put(ROOT "\\inside\\target","foreign");
 ck(FsMoveFile(r,"inside/source","inside/target","payload",7)==FS_READ_DENIED,
    "move no replace");
 read_eq(r,"inside/target","foreign");
 ck(DeleteFileA(ROOT "\\inside\\target"),"clear foreign target");
 ck(FsReadStat(r,"inside/target",&m)==FS_READ_MISSING,"target absent");
 ck(FsMoveFile(r,"inside/source","inside/target","payload",7)==FS_READ_OK,"normal move");
 ck(FsMoveRecover(r)==FS_READ_OK,"clean move recovery");state(r,2,1);
 FsReadClose(r);ck(DeleteFileA(ROOT "\\inside\\target"),"clean target");cleanup();
 puts("Windows remove/move crash matrix passed");return 0;
}
#else
int main(void){return 0;}
#endif
