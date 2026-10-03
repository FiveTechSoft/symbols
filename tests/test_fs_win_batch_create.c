#ifdef _WIN32
#include "fs_batch.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#define ROOT "test_fs_winbatch_scratch"
/* W2: FsBatchCreate and FsBatchRecover on NTFS. Crash points 5-7 are a smoke
   check; the full crash matrix, mutants and recovery-crash cases are W3. */
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(ROOT "\\sub")==0,"mkdir");}
/* Count the names directly in dir (excluding . and ..). */
static int count(const char *pattern)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pattern,&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,".."))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static void is_file(const char *p,const char *v,int readonly)
{char b[64]={0};FILE *in=fopen(p,"rb");DWORD a;size_t n=strlen(v);
 ck(in!=NULL,"open created file");ck(fread(b,1,sizeof(b)-1,in)==n,"created size");
 ck(fclose(in)==0&&!memcmp(b,v,n),"created bytes");
 a=GetFileAttributesA(p);ck(a!=INVALID_FILE_ATTRIBUTES,"attrs");
 ck(((a&FILE_ATTRIBUTE_READONLY)!=0)==readonly,"read-only attribute");}
static void rm(const char *p)
{DWORD a=GetFileAttributesA(p);
 if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_READONLY))SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);
 ck(DeleteFileA(p),"remove file");}
static void cleanup(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(ROOT "\\*",&d);
 ck(f!=INVALID_HANDLE_VALUE,"find");
 do{char p[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,"..")||
      !strcmp(d.cFileName,"sub"))continue;
    snprintf(p,sizeof(p),ROOT "\\%s",d.cFileName);rm(p);
 }while(FindNextFileA(f,&d));FindClose(f);
 f=FindFirstFileA(ROOT "\\sub\\*",&d);
 if(f!=INVALID_HANDLE_VALUE){do{char p[512];if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
    snprintf(p,sizeof(p),ROOT "\\sub\\%s",d.cFileName);rm(p);
 }while(FindNextFileA(f,&d));FindClose(f);}
 ck(_rmdir(ROOT "\\sub")==0&&_rmdir(ROOT)==0,"remove root");}
static int run_child(const char *exe,int point)
{char cmd[1024];STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD code=0;
 si.cb=sizeof(si);sprintf(cmd,"\"%s\" child %d",exe,point);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static const FS_BATCH_CREATE three[3]={
 {"a.txt","AAA",3,0444},{"sub/b.txt","BBBB",4,0644},{"c.txt","C",1,0444}};
static void child(int point)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",point);_putenv_s("FS_WIN_BATCH_CRASH",v);
 s=FsBatchCreate(r,three,3);
 fprintf(stderr,"batch child point=%d status=%d\n",point,(int)s);
 FsReadClose(r);ExitProcess(200);}
static void created_all(void)
{is_file(ROOT "\\a.txt","AAA",1);is_file(ROOT "\\sub\\b.txt","BBBB",0);
 is_file(ROOT "\\c.txt","C",1);
 ck(count(ROOT "\\*")==4,"root holds only a.txt c.txt sub and nothing else");
 ck(count(ROOT "\\sub\\*")==1,"sub holds only b.txt");}
static void nothing(void)
{ck(count(ROOT "\\*")==1,"root holds only sub");ck(count(ROOT "\\sub\\*")==0,"sub empty");}
int main(int argc,char **argv)
{char exe[768];DWORD got;FS_READ_ROOT *r;
 if(argc==3&&!strcmp(argv[1],"child")){child(atoi(argv[2]));return 200;}
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 /* Success across directories, read-only mode honoured, nothing left over. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
 ck(FsBatchCreate(r,three,3)==FS_READ_OK,"batch create");
 created_all();
 ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"existing targets denied");
 created_all();
 ck(FsBatchRecover(r)==FS_READ_OK,"recover on a clean tree");created_all();
 FsReadClose(r);cleanup();
 /* Two files in one directory. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open two");
 {FS_BATCH_CREATE two[2]={{"x1","one",3,0644},{"x2","two",3,0644}};
  ck(FsBatchCreate(r,two,2)==FS_READ_OK,"two files");
  is_file(ROOT "\\x1","one",0);is_file(ROOT "\\x2","two",0);
  ck(count(ROOT "\\*")==3,"two files and sub only");}
 FsReadClose(r);cleanup();
 /* Rejections leave the tree untouched. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open invalid");
 {FS_BATCH_CREATE bad[2]={{"ok1","x",1,0644},{"../escape","x",1,0644}};
  ck(FsBatchCreate(r,bad,2)==FS_READ_INVALID,"traversal");nothing();}
 {FS_BATCH_CREATE dup[2]={{"d","x",1,0644},{"D","x",1,0644}};
  ck(FsBatchCreate(r,dup,2)==FS_READ_INVALID,"case-insensitive duplicate");nothing();}
 {FS_BATCH_CREATE one[1]={{"solo","x",1,0644}};
  ck(FsBatchCreate(r,one,1)==FS_READ_INVALID,"one entry");nothing();}
 {FS_BATCH_CREATE miss[2]={{"ok2","x",1,0644},{"nodir/f","x",1,0644}};
  ck(FsBatchCreate(r,miss,2)!=FS_READ_OK,"missing parent");nothing();}
 {FS_BATCH_CREATE res[2]={{"ok3","x",1,0644},{".fstxn.batch","x",1,0644}};
  ck(FsBatchCreate(r,res,2)==FS_READ_INVALID,"reserved name");nothing();}
 put(ROOT "\\sub\\there","t");
 {FS_BATCH_CREATE ex[2]={{"fresh","x",1,0644},{"sub/there","x",1,0644}};
  ck(FsBatchCreate(r,ex,2)==FS_READ_DENIED,"existing target");
  ck(count(ROOT "\\*")==1&&count(ROOT "\\sub\\*")==1,"existing target left no residue");
  is_file(ROOT "\\sub\\there","t",0);}
 FsReadClose(r);cleanup();
 /* Publish of the second item refused after the first was published: the
    in-process rollback restores the tree. Needs a missing-until-then target. */
 fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open injected");
 ck(_putenv_s("FS_WIN_BATCH_FAIL_PUBLISH","1")==0,"set injection");
 ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"publish failure reported");
 ck(_putenv_s("FS_WIN_BATCH_FAIL_PUBLISH","99")==0,"clear injection");
 nothing();
 ck(FsBatchRecover(r)==FS_READ_OK,"recover after rollback");nothing();
 ck(FsBatchCreate(r,three,3)==FS_READ_OK,"batch works after rollback");created_all();
 FsReadClose(r);cleanup();
 /* Crash smoke. 3: journal complete, nothing published. 5: all published,
    no marker (rolls back). 6: marker (rolls forward). 7: all done. */
 for(int k=0;k<4;k++){
   int pt=k==0?3:k==1?5:k==2?6:7;int forward=pt>=6;
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open crash");
   ck(run_child(exe,pt)==80+pt,"writer killed at point");
   if(pt<7)ck(FsBatchCreate(r,three,3)==FS_READ_DENIED,"pending batch blocks a new one");
   ck(FsBatchRecover(r)==FS_READ_OK,"recover");
   ck(FsBatchRecover(r)==FS_READ_OK,"recover twice");
   if(forward)created_all();else nothing();
   ck(FsCreateRecover(r)==FS_READ_OK,"single create recovery sees nothing");
   FsReadClose(r);cleanup();
 }
 printf("test_fs_win_batch_create ok\n");
 return 0;
}
#else
int main(void){return 0;}
#endif
