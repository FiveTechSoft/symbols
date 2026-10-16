/* Private translation unit: real Win32 calls, byte-identical shell source. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "shell_handle_boundary_win.h"

typedef struct {
    const char *api;
    DWORD before, after, error;
    ULONG_PTR h1, h2;
    int success;
} BOUNDARY_RECORD;
typedef struct { DWORD error, count; } BOUNDARY_START;
static BOUNDARY_RECORD records[1024];
static HANDLE owned[8], borrowed;
static unsigned closes[8], used, owned_n;
static int enabled, invalid;

static BOUNDARY_START begin_boundary(void)
{
    BOUNDARY_START s;
    s.error = GetLastError(); s.count = 0;
    if (enabled && !GetProcessHandleCount(GetCurrentProcess(), &s.count)) invalid = 1;
    SetLastError(s.error);
    return s;
}
static void end_boundary(BOUNDARY_START s, const char *api, int success, HANDLE h1, HANDLE h2)
{
    DWORD error = GetLastError(), count = 0;
    if (enabled) {
        if (!GetProcessHandleCount(GetCurrentProcess(), &count)) invalid = 1;
        if (used >= 1024) invalid = 1;
        else {
            BOUNDARY_RECORD *r = &records[used++];
            r->api=api; r->before=s.count; r->after=count; r->error=error;
            r->success=success; r->h1=(ULONG_PTR)h1; r->h2=(ULONG_PTR)h2;
        }
    }
    SetLastError(error);
}
static void add_owned(HANDLE h)
{
    DWORD error = GetLastError();
    if (enabled) {
        if (!h || h == INVALID_HANDLE_VALUE || owned_n >= 8) invalid = 1;
        else {owned[owned_n]=h; closes[owned_n++]=0;}
    }
    SetLastError(error);
}
static DWORD traced_GetFileAttributesA(LPCSTR path)
{
    BOUNDARY_START s=begin_boundary(); DWORD ret=GetFileAttributesA(path);
    end_boundary(s,"GetFileAttributesA",ret!=INVALID_FILE_ATTRIBUTES,NULL,NULL); return ret;
}
static BOOL traced_QueryPerformanceFrequency(LARGE_INTEGER *v)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=QueryPerformanceFrequency(v);
    end_boundary(s,"QueryPerformanceFrequency",ret,NULL,NULL); return ret;
}
static BOOL traced_QueryPerformanceCounter(LARGE_INTEGER *v)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=QueryPerformanceCounter(v);
    end_boundary(s,"QueryPerformanceCounter",ret,NULL,NULL); return ret;
}
static BOOL traced_CreatePipe(PHANDLE r, PHANDLE w, LPSECURITY_ATTRIBUTES a, DWORD size)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=CreatePipe(r,w,a,size);
    end_boundary(s,"CreatePipe",ret,ret?*r:NULL,ret?*w:NULL);
    if (ret) {add_owned(*r); add_owned(*w);} return ret;
}
static BOOL traced_SetHandleInformation(HANDLE h, DWORD mask, DWORD flags)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=SetHandleInformation(h,mask,flags);
    end_boundary(s,"SetHandleInformation",ret,h,NULL); return ret;
}
static HANDLE traced_CreateJobObjectA(LPSECURITY_ATTRIBUTES a, LPCSTR name)
{
    BOUNDARY_START s=begin_boundary(); HANDLE ret=CreateJobObjectA(a,name);
    end_boundary(s,"CreateJobObjectA",ret!=NULL,ret,NULL); if(ret) add_owned(ret); return ret;
}
static BOOL traced_SetInformationJobObject(HANDLE h, JOBOBJECTINFOCLASS cls, LPVOID v, DWORD len)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=SetInformationJobObject(h,cls,v,len);
    end_boundary(s,"SetInformationJobObject",ret,h,NULL); return ret;
}
static HANDLE traced_GetStdHandle(DWORD which)
{
    BOUNDARY_START s=begin_boundary(); HANDLE ret=GetStdHandle(which);
    end_boundary(s,"GetStdHandle",ret!=INVALID_HANDLE_VALUE,ret,NULL);
    if(enabled) borrowed=ret; return ret;
}
static BOOL traced_CreateProcessA(LPCSTR app, LPSTR cmd, LPSECURITY_ATTRIBUTES pa,
                                LPSECURITY_ATTRIBUTES ta, BOOL inherit, DWORD flags,
                                LPVOID env, LPCSTR dir, LPSTARTUPINFOA si, LPPROCESS_INFORMATION pi)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=CreateProcessA(app,cmd,pa,ta,inherit,flags,env,dir,si,pi);
    end_boundary(s,"CreateProcessA",ret,ret?pi->hProcess:NULL,ret?pi->hThread:NULL); return ret;
}
static BOOL traced_CloseHandle(HANDLE h)
{
    BOUNDARY_START s=begin_boundary(); BOOL ret=CloseHandle(h);
    unsigned i; int found=0;
    end_boundary(s,"CloseHandle",ret,h,NULL);
    if(enabled) {
        for(i=0;i<owned_n;i++) if(owned[i]==h) {found=1; if(ret) closes[i]++; else invalid=1;}
        if(!found || h==borrowed) invalid=1;
    }
    return ret;
}
void ShellBoundaryReset(void)
{
    DWORD error=GetLastError();
    used=owned_n=0; borrowed=NULL; invalid=0; enabled=1;
    SetLastError(error);
}
int ShellBoundaryReport(int call, DWORD outer_before, DWORD outer_after)
{
    unsigned i; long long sum=0; int ok;
    enabled=0;
    for(i=0;i<used;i++) sum+=(long long)records[i].after-(long long)records[i].before;
    if(owned_n!=5 || sum!=(long long)outer_after-(long long)outer_before) invalid=1;
    for(i=0;i<owned_n;i++) if(closes[i]!=1) invalid=1;
    ok=!invalid;
    for(i=0;i<used;i++) {
        BOUNDARY_RECORD *r=&records[i];
        printf("SHELL_BOUNDARY call=%d index=%u api=%s before=%lu after=%lu delta=%lld success=%d last_error=%lu h1=0x%llx h2=0x%llx\n",
               call,i,r->api,(unsigned long)r->before,(unsigned long)r->after,
               (long long)r->after-(long long)r->before,r->success,(unsigned long)r->error,
               (unsigned long long)r->h1,(unsigned long long)r->h2);
    }
    for(i=0;i<owned_n;i++) printf("SHELL_LEDGER call=%d owned=0x%llx successful_closes=%u\n",call,(unsigned long long)(ULONG_PTR)owned[i],closes[i]);
    printf("SHELL_LEDGER call=%d borrowed_stdin=0x%llx owned_count=%u sum_delta=%lld outer_delta=%lld complete=%d\n",
           call,(unsigned long long)(ULONG_PTR)borrowed,owned_n,sum,
           (long long)outer_after-(long long)outer_before,ok); fflush(stdout);
    return ok;
}

#define AgentShellResultInit PrivateAgentShellResultInit
#define AgentShellDetectBackend PrivateAgentShellDetectBackend
#define AgentShellBackendName PrivateAgentShellBackendName
#define AgentShellWrapCommand PrivateAgentShellWrapCommand
#define AgentShellExecExplicit PrivateAgentShellExecExplicit
#define AgentShellExec PrivateAgentShellExec
#define AgentShellExecGuarded PrivateAgentShellExecGuarded
#define AgentShellExecCompiler PrivateAgentShellExecCompiler
#define AgentShellExecCompilerGuarded PrivateAgentShellExecCompilerGuarded
#define GetFileAttributesA traced_GetFileAttributesA
#define QueryPerformanceFrequency traced_QueryPerformanceFrequency
#define QueryPerformanceCounter traced_QueryPerformanceCounter
#define CreatePipe traced_CreatePipe
#define SetHandleInformation traced_SetHandleInformation
#define CreateJobObjectA traced_CreateJobObjectA
#define SetInformationJobObject traced_SetInformationJobObject
#define GetStdHandle traced_GetStdHandle
#define CreateProcessA traced_CreateProcessA
#define CloseHandle traced_CloseHandle
#include "../src/agent_shell.c"
