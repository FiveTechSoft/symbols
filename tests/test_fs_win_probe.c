#ifdef _WIN32
/* W0 measurement probe. It changes no production behaviour and asserts no
   NTFS outcome: it prints what the runner's filesystem does, with the exact
   Win32 / NTSTATUS values, so the Windows batch design can rely on numbers.
   It fails only if its own fixture cannot be built or cleaned up. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define DIR_A L"test_fs_winprobe_scratch"
#define SHARE_ALL (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
#define NT_RENAME 10u
#define NT_LINK 11u
typedef NTSTATUS (NTAPI *NT_SET)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,ULONG);
typedef struct { BOOLEAN replace; HANDLE root; ULONG length; WCHAR name[1]; } NAME_INFO;
static NT_SET nt_set;
static HANDLE dir;
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"PROBE SETUP FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void path(WCHAR *out,const WCHAR *leaf){swprintf(out,300,DIR_A L"\\%ls",leaf);}
static void del(const WCHAR *leaf)
{WCHAR p[300];path(p,leaf);SetFileAttributesW(p,FILE_ATTRIBUTE_NORMAL);DeleteFileW(p);}
static HANDLE mk(const WCHAR *leaf,const char *bytes,DWORD access,DWORD share)
{WCHAR p[300];HANDLE h;DWORD w;path(p,leaf);
 h=CreateFileW(p,access|GENERIC_WRITE,share,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 ck(h!=INVALID_HANDLE_VALUE,"mk");
 if(bytes&&*bytes)ck(WriteFile(h,bytes,(DWORD)strlen(bytes),&w,NULL),"write");
 return h;}
static HANDLE open_name(const WCHAR *leaf,DWORD access,DWORD share,DWORD *err)
{WCHAR p[300];HANDLE h;path(p,leaf);
 h=CreateFileW(p,access,share,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 *err=h==INVALID_HANDLE_VALUE?GetLastError():0;return h;}
static NTSTATUS setname(HANDLE src,const WCHAR *leaf,ULONG type,int replace)
{IO_STATUS_BLOCK io;size_t n=wcslen(leaf);size_t size=offsetof(NAME_INFO,name)+(n+1)*sizeof(WCHAR);
 NAME_INFO *v=(NAME_INFO*)calloc(1,size);NTSTATUS st;ck(v!=NULL,"alloc");
 v->replace=replace?TRUE:FALSE;v->root=dir;v->length=(ULONG)(n*sizeof(WCHAR));
 memcpy(v->name,leaf,(n+1)*sizeof(WCHAR));
 st=nt_set(src,&io,v,(ULONG)size,type);free(v);return st;}
static int exists(const WCHAR *leaf)
{WCHAR p[300];path(p,leaf);return GetFileAttributesW(p)!=INVALID_FILE_ATTRIBUTES;}
static void cleanup(void)
{static const WCHAR *n[]={L"t",L"s",L"p",L"x",L"y",L"r0",L"r1",L"r2",L"r3",L"r4",L"r5",L"r6",L"r7",
  L"n0",L"n1",L"n2",L"n3",L"n4",L"n5",L"n6",L"n7"};
 for(size_t i=0;i<sizeof(n)/sizeof(n[0]);i++)del(n[i]);}
int main(void)
{HMODULE nt=GetModuleHandleW(L"ntdll.dll");HANDLE a,b,c;DWORD e;NTSTATUS st;
 LARGE_INTEGER t0,t1,fq;
 nt_set=(NT_SET)(void*)GetProcAddress(nt,"NtSetInformationFile");ck(nt_set!=NULL,"ntdll");
 CreateDirectoryW(DIR_A,NULL);cleanup();
 dir=CreateFileW(DIR_A,GENERIC_READ,SHARE_ALL,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
 ck(dir!=INVALID_HANDLE_VALUE,"dir");
 puts("W0 PROBE (measurement only; no assertion on the values below)");
 /* P1: rename-over while a foreign handle is open WITHOUT FILE_SHARE_DELETE */
 a=mk(L"t","old",GENERIC_READ,SHARE_ALL);CloseHandle(a);
 b=mk(L"s","new",GENERIC_READ|GENERIC_WRITE|DELETE,SHARE_ALL);
 c=open_name(L"t",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&e);
 printf("P1 foreign open without DELETE share: open_err=%lu\n",e);
 if(c!=INVALID_HANDLE_VALUE){
  st=setname(b,L"t",NT_RENAME,1);
  printf("P1 rename-over while foreign handle open: NTSTATUS=0x%08lx target_exists=%d s_exists=%d\n",
   (unsigned long)st,exists(L"t"),exists(L"s"));
  CloseHandle(c);}
 CloseHandle(b);cleanup();
 /* P2: same, foreign handle WITH FILE_SHARE_DELETE */
 a=mk(L"t","old",GENERIC_READ|DELETE,SHARE_ALL);CloseHandle(a);
 b=mk(L"s","new",GENERIC_READ|GENERIC_WRITE|DELETE,SHARE_ALL);
 c=open_name(L"t",GENERIC_READ,SHARE_ALL,&e);
 st=setname(b,L"t",NT_RENAME,1);
 printf("P2 foreign open with DELETE share: rename-over NTSTATUS=0x%08lx target_exists=%d s_exists=%d\n",
  (unsigned long)st,exists(L"t"),exists(L"s"));
 if(c!=INVALID_HANDLE_VALUE)CloseHandle(c);CloseHandle(b);cleanup();
 /* P3: delete-pending visibility */
 a=mk(L"x","data",GENERIC_READ|DELETE,SHARE_ALL);
 c=open_name(L"x",GENERIC_READ,SHARE_ALL,&e);
 {FILE_DISPOSITION_INFO d;d.DeleteFile=TRUE;
  printf("P3 SetFileInformationByHandle(delete) ok=%d\n",(int)(SetFileInformationByHandle(a,FileDispositionInfo,&d,sizeof(d))!=FALSE));}
 CloseHandle(a);
 b=open_name(L"x",GENERIC_READ,SHARE_ALL,&e);
 printf("P3 delete pending, holder open: reopen_err=%lu attrs_exists=%d\n",e,exists(L"x"));
 if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);
 {WCHAR p[300];HANDLE n;path(p,L"x");
  n=CreateFileW(p,GENERIC_WRITE,SHARE_ALL,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  printf("P3 create same name while delete pending: err=%lu\n",n==INVALID_HANDLE_VALUE?GetLastError():0);
  if(n!=INVALID_HANDLE_VALUE)CloseHandle(n);}
 CloseHandle(c);
 printf("P3 after holder closed: exists=%d\n",exists(L"x"));cleanup();
 /* P4: read-only target */
 a=mk(L"t","old",GENERIC_READ,SHARE_ALL);CloseHandle(a);
 {WCHAR p[300];path(p,L"t");SetFileAttributesW(p,FILE_ATTRIBUTE_READONLY);}
 b=mk(L"s","new",GENERIC_READ|GENERIC_WRITE|DELETE,SHARE_ALL);
 st=setname(b,L"t",NT_RENAME,1);
 printf("P4 rename-over read-only target: NTSTATUS=0x%08lx\n",(unsigned long)st);
 CloseHandle(b);cleanup();
 a=mk(L"t","old",GENERIC_READ,SHARE_ALL);CloseHandle(a);
 {WCHAR p[300];path(p,L"t");SetFileAttributesW(p,FILE_ATTRIBUTE_READONLY);}
 {WCHAR p[300];HANDLE h;FILE_DISPOSITION_INFO d;d.DeleteFile=TRUE;path(p,L"t");
  h=CreateFileW(p,DELETE,SHARE_ALL,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  printf("P4 open read-only with DELETE access: err=%lu\n",h==INVALID_HANDLE_VALUE?GetLastError():0);
  if(h!=INVALID_HANDLE_VALUE){
   printf("P4 delete read-only by handle: ok=%d err=%lu\n",
    (int)(SetFileInformationByHandle(h,FileDispositionInfo,&d,sizeof(d))!=FALSE),GetLastError());
   CloseHandle(h);}}
 cleanup();
 /* P5: hard-link pin on a read-only file and on a file with a foreign handle */
 a=mk(L"t","old",GENERIC_READ|DELETE,SHARE_ALL);
 {WCHAR p[300];path(p,L"t");SetFileAttributesW(p,FILE_ATTRIBUTE_READONLY);}
 st=setname(a,L"p",NT_LINK,0);
 printf("P5 link pin of read-only file: NTSTATUS=0x%08lx p_exists=%d\n",(unsigned long)st,exists(L"p"));
 CloseHandle(a);cleanup();
 /* P6: rename-no-replace onto an existing name */
 a=mk(L"t","old",GENERIC_READ,SHARE_ALL);CloseHandle(a);
 b=mk(L"s","new",GENERIC_READ|GENERIC_WRITE|DELETE,SHARE_ALL);
 st=setname(b,L"t",NT_RENAME,0);
 printf("P6 rename no-replace onto existing: NTSTATUS=0x%08lx\n",(unsigned long)st);
 CloseHandle(b);cleanup();
 /* P7: cost of 8 rename-over operations (information only) */
 {WCHAR nm[8][4],rn[8][4];HANDLE h[8];int i;QueryPerformanceFrequency(&fq);
  for(i=0;i<8;i++){swprintf(nm[i],4,L"n%d",i);swprintf(rn[i],4,L"r%d",i);
   a=mk(nm[i],"old",GENERIC_READ,SHARE_ALL);CloseHandle(a);
   h[i]=mk(rn[i],"new",GENERIC_READ|GENERIC_WRITE|DELETE,SHARE_ALL);}
  QueryPerformanceCounter(&t0);st=0;
  for(i=0;i<8;i++){NTSTATUS s2=setname(h[i],nm[i],NT_RENAME,1);if(s2)st=s2;}
  QueryPerformanceCounter(&t1);
  printf("P7 8 rename-over: last_nonzero_NTSTATUS=0x%08lx elapsed_us=%lld\n",(unsigned long)st,
   (long long)((t1.QuadPart-t0.QuadPart)*1000000/fq.QuadPart));
  for(i=0;i<8;i++)CloseHandle(h[i]);}
 cleanup();CloseHandle(dir);
 ck(RemoveDirectoryW(DIR_A),"remove scratch");
 puts("W0 PROBE DONE");return 0;}
#else
int main(void){return 0;}
#endif
