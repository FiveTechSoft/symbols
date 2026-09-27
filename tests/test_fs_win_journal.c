#ifdef _WIN32
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winjournal_scratch"
static void check(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static int remove_file(const char *path)
{DWORD a=GetFileAttributesA(path);if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_READONLY))SetFileAttributesA(path,FILE_ATTRIBUTE_NORMAL);return DeleteFileA(path)!=0;}
static void child(int phase,int recovering)
{
 FS_READ_ROOT *r;char value[20];
 check(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(value,"%d",phase);_putenv_s("FS_WIN_JOURNAL_CRASH",value);
 {FS_READ_STATUS status=recovering?FsCreateRecover(r):
       FsCreateFile(r,"inside/new","payload",7,0666);
  fprintf(stderr,"journal child phase=%d recovery=%d FS_READ_STATUS=%d\n",
          phase,recovering,(int)status);}
 FsReadClose(r);ExitProcess(200);
}
static int run_child(const char *exe,int point,int recovery)
{
 char cmd[1024];STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;
 si.cb=sizeof(si);
 sprintf(cmd,"\"%s\" child %d %d",exe,point,recovery);
 check(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn child");
 check(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"wait child");
 check(GetExitCodeProcess(pi.hProcess,&code),"child exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 return (int)code;
}
static void read_bytes(FS_READ_ROOT *r,const char *name,const char *text,size_t n)
{
 FS_READ_META m;unsigned char *b=NULL;size_t got;
 check(FsReadFile(r,name,&b,&got,&m)==FS_READ_OK&&got==n&&!memcmp(b,text,n),"bytes");free(b);
}
static FILE_ID_INFO file_id(const char *name)
{
 FILE_ID_INFO id;HANDLE h=CreateFileA(name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
 check(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandleEx(h,FileIdInfo,&id,sizeof(id)),"file ID");CloseHandle(h);return id;
}
static void probe_directory(void)
{
 HANDLE h=CreateFileA(ROOT,FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,
   FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
   FILE_FLAG_BACKUP_SEMANTICS,NULL);
 BOOL ok;DWORD e;check(h!=INVALID_HANDLE_VALUE,"directory probe open");
 SetLastError(0);ok=FlushFileBuffers(h);e=GetLastError();
 fprintf(stderr,"NTFS parent-directory FlushFileBuffers diagnostic: return=%d GetLastError=%lu (no power-loss claim)\n",(int)ok,e);
 CloseHandle(h);
}
int main(int argc,char **argv)
{
 char exe[768];DWORD exelen;FS_READ_ROOT *r;FS_READ_META m;FILE_ID_INFO foreign,post;
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 exelen=GetModuleFileNameA(NULL,exe,sizeof(exe));check(exelen&&exelen<sizeof(exe),"exe");
 /* Each row gets an independent workspace. Rows 1-2 intentionally leave
    untracked stage/pin orphans and are cleaned by this test, not recovery. */
 for(int row=1;row<=7;row++){
   check(_mkdir(ROOT)==0,"mkdir root");
   check(_mkdir(ROOT "\\inside")==0,"mkdir inside");
   {FILE *f=fopen(ROOT "\\outside","wb");check(f&&fwrite("foreign",1,7,f)==7&&fclose(f)==0,"foreign fixture");}
   foreign=file_id(ROOT "\\outside");
   check(FsReadOpen(ROOT,&r)==FS_READ_OK,"open root");
   if(row==1)probe_directory();
   check(run_child(exe,row,0)==80+row,"crash point reached");
   check(FsCreateRecover(r)==FS_READ_OK,"first recovery");
   check(FsCreateRecover(r)==FS_READ_OK,"second recovery");
   check(FsReadStat(r,"inside/new",&m)==(row>=6?FS_READ_OK:FS_READ_MISSING),"target state");
   if(row>=6){read_bytes(r,"inside/new","payload",7);post=file_id(ROOT "\\inside\\new");
     check(post.VolumeSerialNumber==foreign.VolumeSerialNumber&&
       memcmp(&post.FileId,&foreign.FileId,sizeof(post.FileId))!=0,"new ID distinct");}
   read_bytes(r,"outside","foreign",7);
   post=file_id(ROOT "\\outside");check(!memcmp(&foreign,&post,sizeof(foreign)),"foreign ID untouched");
   FsReadClose(r);
   if(row>=6)check(remove_file(ROOT "\\inside\\new"),"clean new");
   /* Test-only cleanup: untracked random pre-intent artifacts. */
   {WIN32_FIND_DATAA data;HANDLE find=FindFirstFileA(ROOT "\\*",&data);
    check(find!=INVALID_HANDLE_VALUE,"list root");
    do{if(data.cFileName[0]=='.'&&!strcmp(data.cFileName,"."))continue;
       if(data.cFileName[0]=='.'&&!strcmp(data.cFileName,".."))continue;
       if(data.cFileName[0]=='.'){
          char path[400];snprintf(path,sizeof(path),ROOT "\\%s",data.cFileName);
          check(remove_file(path),"clean control orphan");
       }
    }while(FindNextFileA(find,&data));FindClose(find);}
   check(remove_file(ROOT "\\outside"),"clean foreign");
   check(_rmdir(ROOT "\\inside")==0,"clean inside");check(_rmdir(ROOT)==0,"clean root");
 }
 /* More precise process-kill points within rollback and marker cleanup. */
 for(int point=8;point<=12;point++){
   int committed=point==10||point==11;
   check(_mkdir(ROOT)==0&&_mkdir(ROOT "\\inside")==0,"recovery root");
   check(FsReadOpen(ROOT,&r)==FS_READ_OK,"open recovery root");
   check(run_child(exe,committed?6:4,0)==(committed?86:84),"initial crash");
   check(run_child(exe,point,1)==80+point,"recovery crash reached");
   check(FsCreateRecover(r)==FS_READ_OK&&FsCreateRecover(r)==FS_READ_OK,"idempotent after recovery kill");
   check(FsReadStat(r,"inside/new",&m)==(committed?FS_READ_OK:FS_READ_MISSING),"recovery target");
   if(committed)read_bytes(r,"inside/new","payload",7);
   FsReadClose(r);
   if(committed)check(remove_file(ROOT "\\inside\\new"),"remove committed target");
   check(remove_file(ROOT "\\.fstxn.lock"),"remove lock");
   check(_rmdir(ROOT "\\inside")==0&&_rmdir(ROOT)==0,"remove root");
 }
 puts("Windows process-crash journal matrix passed");return 0;
}
#else
int main(void){return 0;}
#endif
