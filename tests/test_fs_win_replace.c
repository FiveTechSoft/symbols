#ifdef _WIN32
#include "fs_replace.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
#define ROOT "test_fs_winreplace_scratch"
typedef struct {
 unsigned magic,version,checksum,reserved;
 FILE_ID_INFO root,parent,old_id,new_id;
 unsigned long long old_size,old_digest,new_size,new_digest;
 WCHAR target[4096],old_pin[48],new_stage[48],new_pin[48],rollback[48];
} TEST_WR_RECORD;
static unsigned record_checksum(const TEST_WR_RECORD *r)
{const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;
 for(size_t i=0;i<sizeof(*r);i++){
  if(i>=offsetof(TEST_WR_RECORD,checksum)&&
     i<offsetof(TEST_WR_RECORD,checksum)+sizeof(r->checksum))continue;
  h=(h^p[i])*16777619u;
 }return h;}
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void read_eq(FS_READ_ROOT *r,const char *v)
{unsigned char *b=NULL;size_t n=0;FS_READ_META m;size_t len=strlen(v);
 ck(FsReadFile(r,"inside/target",&b,&n,&m)==FS_READ_OK&&n==len&&
    !memcmp(b,v,len),"target bytes");free(b);}
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(ROOT "\\inside")==0,"mkdir");
 put(ROOT "\\inside\\target","old");}
static void cleanup(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\*",&d);
 ck(f!=INVALID_HANDLE_VALUE,"find");
 do{char p[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,"..")||
      !strcmp(d.cFileName,"inside"))continue;
    snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);
    ck(DeleteFileA(p),"remove control or orphan");
 }while(FindNextFileA(f,&d));FindClose(f);
 ck(DeleteFileA(ROOT "\\inside\\target"),"remove target");
 ck(_rmdir(ROOT "\\inside")==0&&_rmdir(ROOT)==0,"remove root");}
static int run_child(const char *exe,int phase,int recovery)
{char cmd[1024];STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;
 si.cb=sizeof(si);sprintf(cmd,"\"%s\" child %d %d",exe,phase,recovery);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static void child(int phase,int recovery)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",phase);_putenv_s("FS_WIN_REPLACE_CRASH",v);
 s=recovery?FsReplaceRecover(r):FsReplaceFile(r,"inside/target","old",3,"new",3);
 fprintf(stderr,"replace child phase=%d recovery=%d status=%d\n",phase,recovery,(int)s);
 FsReadClose(r);ExitProcess(200);}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;FS_READ_META m;
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 /* F0-F6: unjournaled 0-2 are test-only orphan cleanup; before marker 3-5
    rolls back old; after marker 6 keeps new. */
 for(int phase=0;phase<=6;phase++){
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
   ck(run_child(exe,phase,0)==140+phase,"writer phase kill");
   read_eq(r,phase>=4?"new":"old");
   ck(FsReplaceRecover(r)==FS_READ_OK,"replay");
   ck(FsReplaceRecover(r)==FS_READ_OK,"replay twice");
   read_eq(r,phase==6?"new":"old");
   ck(FsCreateRecover(r)==FS_READ_OK,"other recovery empty");
   FsReadClose(r);cleanup();
 }
 /* R7/R8 rollback from F4; R9/R10 commit cleanup from F6. */
 for(int phase=7;phase<=10;phase++){
   int committed=phase>=9;fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open recovery");
   ck(run_child(exe,committed?6:4,0)==140+(committed?6:4),"setup kill");
   ck(run_child(exe,phase,1)==140+phase,"recovery phase kill");
   ck(FsReplaceRecover(r)==FS_READ_OK,"resume replay");
   ck(FsReplaceRecover(r)==FS_READ_OK,"resume twice");
   read_eq(r,committed?"new":"old");FsReadClose(r);cleanup();
 }
 /* Foreign target after F4 must not be overwritten during rollback. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open foreign");
 ck(run_child(exe,4,0)==144,"foreign setup");
 ck(DeleteFileA(ROOT "\\inside\\target"),"remove published target");
 put(ROOT "\\inside\\target","bad");
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"foreign target denied");
 read_eq(r,"bad");FsReadClose(r);
 ck(DeleteFileA(ROOT "\\inside\\target"),"clear foreign target");
 put(ROOT "\\inside\\target","old");
 /* A legitimate restoration is not inferred by recovery: the test only
    cleans abandoned controls, and this fixture is not part of replay. */
 cleanup();
 /* Altered old pin bytes make rollback unsafe even with the same FileId. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open corrupt pin");
 ck(run_child(exe,4,0)==144,"corrupt pin setup");
 {WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\.fsrp-*",&d);int found=0;
  ck(f!=INVALID_HANDLE_VALUE,"find old pin");
  do{char p[512],b[4]={0};FILE *in;
     snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);
     in=fopen(p,"rb");ck(in!=NULL,"read pin candidate");
     (void)fread(b,1,3,in);ck(fclose(in)==0,"close pin candidate");
     if(!memcmp(b,"old",3)){
       FILE *out=fopen(p,"r+b");ck(out&&fwrite("bad",1,3,out)==3&&fclose(out)==0,
          "alter old pin");found=1;break;
     }
  }while(FindNextFileA(f,&d));FindClose(f);ck(found,"old pin found");}
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"altered pin refused");
 read_eq(r,"new");FsReadClose(r);cleanup();
 /* A malformed or mismatched marker cannot become a commit decision. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open bad marker");
 ck(run_child(exe,4,0)==144,"marker setup");
 put(ROOT "\\.fstxn.pcommit","bad");
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"bad marker denied");
 read_eq(r,"new");FsReadClose(r);cleanup();
 /* A separately checksummed full marker with one changed field conflicts
    with the intent; neither record can authorize a commit or rollback. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open mismatch");
 ck(run_child(exe,4,0)==144,"mismatch setup");
 {TEST_WR_RECORD rec;FILE *in=fopen(ROOT "\\.fstxn.replace","rb");FILE *out;
  ck(in&&fread(&rec,1,sizeof(rec),in)==sizeof(rec)&&fclose(in)==0,
     "read full intent");
  rec.new_digest^=1;rec.checksum=record_checksum(&rec);
  out=fopen(ROOT "\\.fstxn.pcommit","wb");
  ck(out&&fwrite(&rec,1,sizeof(rec),out)==sizeof(rec)&&fclose(out)==0,
     "write mismatched full marker");}
 ck(FsReplaceRecover(r)==FS_READ_DENIED,"valid mismatched records denied");
 read_eq(r,"new");FsReadClose(r);cleanup();
 /* READONLY target: the rename-over is refused, so the replace is PENDING.
    Recovery must remove every pin (a pin of a read-only file needs READONLY
    cleared for the delete), leave no orphan, keep the target 1-link with its
    READONLY attribute, and leave the root usable once the attribute is gone. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open readonly");
 ck(SetFileAttributesA(ROOT "\\inside\\target",FILE_ATTRIBUTE_READONLY),"set readonly");
 ck(FsReplaceFile(r,"inside/target","old",3,"new",3)==FS_READ_PENDING,"read-only target: replace is PENDING");
 ck(FsReplaceRecover(r)==FS_READ_OK,"read-only target: recovery completes");
 ck(FsReplaceRecover(r)==FS_READ_OK,"read-only target: recovery twice");
 {WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\.fsrp-*",&d);
  ck(f==INVALID_HANDLE_VALUE,"no orphan pin in the root");
  f=FindFirstFileA(ROOT "\\inside\\.fsrb-*",&d);
  ck(f==INVALID_HANDLE_VALUE,"no rollback leftover beside the target");}
 {FILE_STANDARD_INFO st;HANDLE h=CreateFileA(ROOT "\\inside\\target",GENERIC_READ,
     FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  ck(h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandleEx(h,FileStandardInfo,&st,sizeof(st))&&
     st.NumberOfLinks==1,"read-only target is 1-link again");CloseHandle(h);}
 ck((GetFileAttributesA(ROOT "\\inside\\target")&FILE_ATTRIBUTE_READONLY)!=0,"READONLY attribute restored");
 read_eq(r,"old");
 ck(SetFileAttributesA(ROOT "\\inside\\target",FILE_ATTRIBUTE_NORMAL),"clear readonly");
 ck(FsReplaceFile(r,"inside/target","old",3,"new",3)==FS_READ_OK,"same replace applies once READONLY is gone");
 read_eq(r,"new");FsReadClose(r);cleanup();
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open simple");
 ck(FsReplaceFile(r,"inside/target","bad",3,"new",3)==FS_READ_DENIED,"stale");
 ck(FsReplaceFile(r,"../escape","old",3,"new",3)==FS_READ_INVALID,"traversal");
 ck(FsReplaceFile(r,"inside/target","old",3,"new",3)==FS_READ_OK,"replace");
 read_eq(r,"new");ck(FsReplaceRecover(r)==FS_READ_OK,"clean recovery");
 ck(FsReplaceFile(r,"inside/target","new",3,"",0)==FS_READ_OK,"empty replacement");
 read_eq(r,"");ck(FsReplaceFile(r,"inside/target","",0,"old",3)==FS_READ_OK,
     "restore from empty");read_eq(r,"old");
 ck(CreateHardLinkA(ROOT "\\inside\\alias",ROOT "\\inside\\target",NULL),"hardlink");
 ck(FsReplaceFile(r,"inside/target","old",3,"new",3)==FS_READ_DENIED,
    "preexisting links refused");
 ck(DeleteFileA(ROOT "\\inside\\alias"),"clear alias");
 ck(FsReadStat(r,"inside/target",&m)==FS_READ_OK,"target remains");
 FsReadClose(r);cleanup();puts("Windows replace crash matrix passed");return 0;
}
#else
int main(void){return 0;}
#endif
