#ifdef NDEBUG
#undef NDEBUG
#endif
#include "attempt_capture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MK(p) _mkdir(p)
#define PID ((unsigned long)GetCurrentProcessId())
#define ENV(k,v) _putenv_s(k,v)
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define MK(p) mkdir(p,0700)
#define PID ((unsigned long)getpid())
#define ENV(k,v) setenv(k,v,1)
#endif
static void put(const char *p,const char *s){FILE *f=fopen(p,"wb");assert(f);assert(fwrite(s,1,strlen(s),f)==strlen(s));assert(!fclose(f));}
static int child(const char *exe,int phase,const char *ws,const char *run)
{
#ifdef _WIN32
 STARTUPINFOA si={0};PROCESS_INFORMATION pi={0};DWORD status=0;char cmd[1600];
 si.cb=sizeof(si);snprintf(cmd,sizeof(cmd),"\"%s\" child %d \"%s\" %s",exe,phase,ws,run);
 assert(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi));
 assert(WaitForSingleObject(pi.hProcess,30000)==WAIT_OBJECT_0);
 assert(GetExitCodeProcess(pi.hProcess,&status));CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 return (int)status;
#else
 pid_t pid=fork();assert(pid>=0);
 if(pid==0){char value[16];snprintf(value,sizeof(value),"%d",phase);ENV("SYMBOLS_ATTEMPT_CRASH",value);
  ATTEMPT_CAPTURE c;assert(AttemptCaptureBegin(&c,ws,run,1));assert(AttemptCaptureEnd(&c,ws,"abstained"));_exit(100);}
 int status=0;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status));(void)exe;return WEXITSTATUS(status);
#endif
}
int main(int argc,char **argv)
{
 const char *root="/tmp/symbols-attempt-capture-test";
 char path[1200],ws[1024],cap[1024],runpath[1200],ownroot[1024];
#ifdef _WIN32
 char temp[MAX_PATH];DWORD got=GetTempPathA(sizeof(temp),temp);
 assert(got>0&&got<sizeof(temp));
 static char base[1024];snprintf(base,sizeof(base),"%ssymbols-attempt-capture-test",temp);
 root=base;
#endif
 if(argc==5&&!strcmp(argv[1],"child")){
   char v[16];snprintf(v,sizeof(v),"%d",atoi(argv[2]));ENV("SYMBOLS_ATTEMPT_CRASH",v);
   ATTEMPT_CAPTURE c;assert(AttemptCaptureBegin(&c,argv[3],argv[4],1));
   assert(AttemptCaptureEnd(&c,argv[3],"abstained"));return 100;
 }
 snprintf(ownroot,sizeof(ownroot),"%s-%lu",root,PID);root=ownroot;
 snprintf(ws,sizeof(ws),"%s/ws",root);snprintf(cap,sizeof(cap),"%s/capture",root);
 assert(MK(root)==0);assert(MK(ws)==0);assert(MK(cap)==0);ENV("SYMBOLS_ATTEMPT_CAPTURE",cap);
 snprintf(path,sizeof(path),"%s/main.c",ws);put(path,"int x=1;\n");
 ATTEMPT_CAPTURE a;
 assert(AttemptCaptureBegin(&a,ws,"normal",1));put(path,"int x=2;\n");
 assert(AttemptCaptureEnd(&a,ws,"verified"));assert(AttemptCaptureValidate(cap,"normal",1));
 assert(AttemptCaptureBegin(&a,ws,"normal",2));put(path,"int x=3;\n");
 assert(AttemptCaptureEnd(&a,ws,"verified"));assert(AttemptCaptureValidate(cap,"normal",2));
 assert(!AttemptCaptureBegin(&a,ws,"normal",2));
 snprintf(path,sizeof(path),"%s/normal/002/after/main.c",cap);put(path,"int x=4;\n");
 assert(!AttemptCaptureValidate(cap,"normal",2));
 snprintf(path,sizeof(path),"%s/normal/002/after/main.c",cap);put(path,"int x=3;\n");
 assert(AttemptCaptureValidate(cap,"normal",2));
 snprintf(path,sizeof(path),"%s/normal/002/manifest.v2",cap);
 FILE *f=fopen(path,"r+b");assert(f);assert(fseek(f,-3,SEEK_END)==0);assert(fwrite("fff",1,3,f)==3);assert(!fclose(f));
 assert(!AttemptCaptureValidate(cap,"normal",2));
 /* A valid but changed manifest hash cannot assert a different tree. */
 snprintf(path,sizeof(path),"%s/normal/001/manifest.v2",cap);
 f=fopen(path,"r+b");assert(f);assert(fseek(f,0,SEEK_SET)==0);
 char buf[512];size_t got=fread(buf,1,sizeof(buf),f);assert(got<sizeof(buf));
 char *digest=strstr(buf,"before\t");assert(digest);digest+=7;
 digest[0]=digest[0]=='a'?'b':'a';assert(fseek(f,0,SEEK_SET)==0);
 assert(fwrite(buf,1,got,f)==got);assert(!fclose(f));
 assert(!AttemptCaptureValidate(cap,"normal",1));
 /* A pair before publication cannot masquerade as a complete pair. */
 assert(child(argv[0],1,ws,"crash-before")==191);
 assert(!AttemptCaptureValidate(cap,"crash-before",1));
 assert(child(argv[0],2,ws,"crash-after")==192);
 assert(AttemptCaptureValidate(cap,"crash-after",1));
 assert(AttemptCaptureBegin(&a,ws,"crash-after",2));
 assert(AttemptCaptureEnd(&a,ws,"abstained"));
 assert(AttemptCaptureBegin(&a,ws,"crash-after",3));
 assert(AttemptCaptureEnd(&a,ws,"abstained"));
 assert(AttemptCaptureValidate(cap,"crash-after",3));
 /* An untracked extra entry makes the declared run length unavailable. */
 snprintf(path,sizeof(path),"%s/crash-after/extra",cap);put(path,"x");
 assert(!AttemptCaptureValidate(cap,"crash-after",3));assert(remove(path)==0);
 snprintf(runpath,sizeof(runpath),"%s/normal/002/manifest.v2",cap);
 /* restoring a corrupted marker is not a signal to trust a stale pair */
 assert(!AttemptCaptureValidate(cap,"normal",2));
 /* Failed capture does not mutate source and cannot be replayed. */
 snprintf(runpath,sizeof(runpath),"%s/too-big.c",ws);f=fopen(runpath,"wb");assert(f);
 for(int i=0;i<270000;i++)assert(fputc('a',f)!=EOF);
 assert(!fclose(f));
 assert(!AttemptCaptureBegin(&a,ws,"oversize",1));
 assert(!AttemptCaptureValidate(cap,"oversize",1));
 assert(remove(runpath)==0);
 snprintf(runpath,sizeof(runpath),"%s/alias.c",ws);
#ifdef _WIN32
 snprintf(path,sizeof(path),"%s/main.c",ws);assert(CreateHardLinkA(runpath,path,NULL));
 assert(!AttemptCaptureBegin(&a,ws,"hardlink",1));assert(DeleteFileA(runpath));
#else
 snprintf(path,sizeof(path),"%s/main.c",ws);assert(link(path,runpath)==0);
 assert(!AttemptCaptureBegin(&a,ws,"hardlink",1));assert(unlink(runpath)==0);
 assert(symlink("main.c",runpath)==0);
 assert(!AttemptCaptureBegin(&a,ws,"symlink",1));assert(unlink(runpath)==0);
#endif
 puts("attempt capture integrity and crash checks passed");return 0;
}
