/* External observation of a private process, never a production hook. */
#define _WIN32_WINNT 0x0603
#include <windows.h>
#include <processsnapshot.h>
#include <winternl.h>
#include <wchar.h>
#include "shell_resource_type.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ENTRY_MAX 2048
#define TEXT_MAX 1024

typedef struct {
    DWORD stage, count, error;
    ULONG_PTR read_handle, write_handle;
    LONG ok;
} SHARED;
typedef struct {
    ULONG_PTR value;
    DWORD flags, type;
    WORD type_bytes, name_bytes;
    WCHAR type_name[TEXT_MAX], object_name[TEXT_MAX];
} ENTRY;
typedef struct { unsigned count; ENTRY entries[ENTRY_MAX]; } TABLE;

static int checkpoint(SHARED *s, HANDLE ready, HANDLE resume, unsigned stage, HANDLE r, HANDLE w)
{
    s->stage=stage; s->read_handle=(ULONG_PTR)r; s->write_handle=(ULONG_PTR)w;
    if (!GetProcessHandleCount(GetCurrentProcess(), &s->count)) {s->error=GetLastError(); return 0;}
    if (!SetEvent(ready)) return 0;
    return WaitForSingleObject(resume,20000)==WAIT_OBJECT_0;
}
static int child(HANDLE mapping, HANDLE ready, HANDLE resume, int pipes)
{
    SHARED *s=(SHARED *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SHARED));
    SECURITY_ATTRIBUTES sa;
    HANDLE r=NULL,w=NULL;
    int ok=0;
    if(!s) return 2;
    memset(&sa,0,sizeof(sa)); sa.nLength=sizeof(sa); sa.bInheritHandle=TRUE;
    s->ok=0; s->error=0;
    if(!checkpoint(s,ready,resume,0,NULL,NULL)) goto done;
    if(pipes && !CreatePipe(&r,&w,&sa,0)) {s->error=GetLastError();goto done;}
    if(!checkpoint(s,ready,resume,1,r,w)) goto done;
    if(pipes) {
        if(!CloseHandle(r)) {s->error=GetLastError();goto done;} r=NULL;
        if(!CloseHandle(w)) {s->error=GetLastError();goto done;} w=NULL;
    }
    if(!checkpoint(s,ready,resume,2,NULL,NULL)) goto done;
    if(pipes && !CreatePipe(&r,&w,&sa,0)) {s->error=GetLastError();goto done;}
    if(!checkpoint(s,ready,resume,3,r,w)) goto done;
    if(pipes) {
        if(!CloseHandle(r)) {s->error=GetLastError();goto done;} r=NULL;
        if(!CloseHandle(w)) {s->error=GetLastError();goto done;} w=NULL;
    }
    if(!checkpoint(s,ready,resume,4,NULL,NULL)) goto done;
    s->ok=1; ok=1;
done:
    if(r) CloseHandle(r);
    if(w) CloseHandle(w);
    if(!UnmapViewOfFile(s))ok=0;
    if(!CloseHandle(mapping))ok=0;
    if(!CloseHandle(ready))ok=0;
    if(!CloseHandle(resume))ok=0;
    return ok?0:1;
}
static int compare_entry(const void *a,const void *b)
{
    ULONG_PTR x=((const ENTRY *)a)->value,y=((const ENTRY *)b)->value;
    return x<y?-1:(x>y?1:0);
}
static const ENTRY *lookup(const TABLE *t,ULONG_PTR h)
{
    unsigned i;
    for(i=0;i<t->count;i++) if(t->entries[i].value==h) return &t->entries[i];
    return NULL;
}
static int same_table(const TABLE *a,const TABLE *b)
{
    unsigned i;
    if(a->count!=b->count)return 0;
    for(i=0;i<a->count;i++) {
        const ENTRY *x=&a->entries[i],*y=&b->entries[i];
        if(x->value!=y->value || x->type_bytes!=y->type_bytes || x->name_bytes!=y->name_bytes ||
           x->type!=y->type || x->flags!=y->flags ||
           memcmp(x->type_name,y->type_name,x->type_bytes) || memcmp(x->object_name,y->object_name,x->name_bytes)) return 0;
    }
    return 1;
}
static int capture(HANDLE process,TABLE *table,DWORD target_count,const char *arm,unsigned stage)
{
    HPSS snapshot=NULL;
    HPSSWALK marker=NULL;
    PSS_HANDLE_ENTRY raw;
    DWORD err,free_error,after_count=0;
    int ok=0;
    PSS_CAPTURE_FLAGS flags=(PSS_CAPTURE_FLAGS)(PSS_CAPTURE_HANDLES | PSS_CAPTURE_HANDLE_NAME_INFORMATION | PSS_CAPTURE_HANDLE_BASIC_INFORMATION);
    table->count=0;
    err=PssCaptureSnapshot(process,flags,0,&snapshot);
    if(err!=ERROR_SUCCESS) {printf("PSS_ERROR arm=%s stage=%u capture=%lu\n",arm,stage,(unsigned long)err);return 0;}
    err=PssWalkMarkerCreate(NULL,&marker);
    if(err!=ERROR_SUCCESS) goto done;
    for(;;) {
        ENTRY *e;
        memset(&raw,0,sizeof(raw));
        err=PssWalkSnapshot(snapshot,PSS_WALK_HANDLES,marker,&raw,sizeof(raw));
        if(err==ERROR_NO_MORE_ITEMS) {ok=1;break;}
        if(err!=ERROR_SUCCESS || table->count>=ENTRY_MAX) break;
        if(raw.TypeNameLength%sizeof(WCHAR) || raw.ObjectNameLength%sizeof(WCHAR) ||
           raw.TypeNameLength>=sizeof(table->entries[0].type_name) ||
           raw.ObjectNameLength>=sizeof(table->entries[0].object_name) ||
           (raw.TypeNameLength && !raw.TypeName) || (raw.ObjectNameLength && !raw.ObjectName)) {err=ERROR_INVALID_DATA;break;}
        e=&table->entries[table->count++]; memset(e,0,sizeof(*e));
        e->value=(ULONG_PTR)raw.Handle; e->flags=(DWORD)raw.Flags; e->type=(DWORD)raw.ObjectType;
        e->type_bytes=raw.TypeNameLength; e->name_bytes=raw.ObjectNameLength;
        if(e->type_bytes)memcpy(e->type_name,raw.TypeName,e->type_bytes);
        if(e->name_bytes)memcpy(e->object_name,raw.ObjectName,e->name_bytes);
    }
done:
    if(marker) {free_error=PssWalkMarkerFree(marker);if(free_error!=ERROR_SUCCESS){ok=0;printf("PSS_ERROR marker_free=%lu\n",(unsigned long)free_error);}}
    free_error=PssFreeSnapshot(GetCurrentProcess(),snapshot);
    if(free_error!=ERROR_SUCCESS) {ok=0;printf("PSS_ERROR snapshot_free=%lu\n",(unsigned long)free_error);}
    qsort(table->entries,table->count,sizeof(ENTRY),compare_entry);
    if(!GetProcessHandleCount(process,&after_count) || after_count!=target_count) {
        ok=0;printf("PSS_OBSERVER arm=%s stage=%u before=%lu after_free=%lu complete=0\n",arm,stage,(unsigned long)target_count,(unsigned long)after_count);
    }
    if(table->count!=target_count)ok=0;
    printf("PSS_CAPTURE arm=%s stage=%u child_count=%lu entries=%u walk_status=%lu complete=%d\n",
           arm,stage,(unsigned long)target_count,table->count,(unsigned long)err,ok);
    return ok;
}
static int print_entry(const char *arm,unsigned stage,const char *kind,const ENTRY *e)
{
    char type[TEXT_MAX*3],name[TEXT_MAX*3];
    int tn=0,nn=0;
    if(e->type_bytes)tn=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,e->type_name,e->type_bytes/sizeof(WCHAR),type,sizeof(type)-1,NULL,NULL);
    if(e->name_bytes)nn=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,e->object_name,e->name_bytes/sizeof(WCHAR),name,sizeof(name)-1,NULL,NULL);
    type[tn>0?tn:0]=0;name[nn>0?nn:0]=0;
    printf("PSS_OBJECT arm=%s stage=%u change=%s value=0x%llx flags=%lu object_type=%lu type_bytes=%u type=%s name_bytes=%u name_status=%s name=%s\n",
           arm,stage,kind,(unsigned long long)e->value,(unsigned long)e->flags,(unsigned long)e->type,
           e->type_bytes,tn>0?type:"UNAVAILABLE",e->name_bytes,
           (e->flags&PSS_HANDLE_HAVE_NAME)?(e->name_bytes?(nn>0?"present":"encoding_error"):"empty"):"unavailable",nn>0?name:"");
    return (!e->type_bytes || tn>0) && (!e->name_bytes || nn>0);
}
static int print_delta(const char *arm,unsigned stage,const TABLE *a,const TABLE *b)
{
    unsigned i;
    int ok=1;
    for(i=0;i<b->count;i++)if(!lookup(a,b->entries[i].value) && !print_entry(arm,stage,"added",&b->entries[i]))ok=0;
    for(i=0;i<a->count;i++)if(!lookup(b,a->entries[i].value) && !print_entry(arm,stage,"removed",&a->entries[i]))ok=0;
    return ok;
}
static int same_identity(const ENTRY *a,const ENTRY *b)
{
    return a && b && a->value==b->value && a->type==b->type &&
        a->type_bytes==b->type_bytes && a->name_bytes==b->name_bytes && a->flags==b->flags &&
        !memcmp(a->type_name,b->type_name,a->type_bytes) && !memcmp(a->object_name,b->object_name,a->name_bytes);
}
static int validate_tables(const char *arm,TABLE *tables,ULONG_PTR returned[2][2])
{
    unsigned i,j,new_n=0,persistent=0;
    const ENTRY *candidate=NULL;
    for(i=0;i<tables[1].count;i++) if(!lookup(&tables[0],tables[1].entries[i].value))new_n++;
    for(i=0;i<tables[2].count;i++)if(!lookup(&tables[0],tables[2].entries[i].value)) {persistent++;candidate=&tables[2].entries[i];}
    if(new_n!=3 || persistent!=1 || !candidate || !candidate->type_bytes || !(candidate->flags&PSS_HANDLE_HAVE_TYPE))goto fail;
    for(j=0;j<2;j++) {
        unsigned open_stage=j?3:1,closed_stage=j?4:2,prior_stage=j?2:0;
        if(!returned[j][0] || !returned[j][1] || returned[j][0]==returned[j][1])goto fail;
        for(i=0;i<2;i++) {
            ULONG_PTR h=returned[j][i];
            if(!lookup(&tables[open_stage],h) || lookup(&tables[prior_stage],h) || lookup(&tables[closed_stage],h))goto fail;
        }
    }
    if(!resource_type_file(candidate->type_name,candidate->type_bytes,candidate->flags))goto fail;
    /* Every baseline identity and the new candidate must survive all later tables. */
    for(j=1;j<5;j++) {
        for(i=0;i<tables[0].count;i++)
            if(!same_identity(&tables[0].entries[i],lookup(&tables[j],tables[0].entries[i].value)))goto fail;
        if(!same_identity(candidate,lookup(&tables[j],candidate->value)))goto fail;
    }
    if(!print_entry(arm,4,"persistent_candidate",candidate))goto fail;
    printf("PSS_IDENTIFICATION arm=%s new=3 persistent=1 candidate_never_closed=1 complete=1\n",arm);
    return 1;
fail:
    printf("PSS_IDENTIFICATION arm=%s new=%u persistent=%u complete=0\n",arm,new_n,persistent);return 0;
}
#define QUERY_BUFFER_BYTES 65536
#define QUERY_WCHARS (QUERY_BUFFER_BYTES/sizeof(WCHAR))
#define QUERY_MS 3000

typedef struct {
    ULONG_PTR duplicate;
    DWORD mode, done, complete, close_ok, pending;
    DWORD file_type, error, name_chars, duplicate_error, timed_out;
    DWORD api_attempted, return_recorded, return_value, harness_error;
    LONG native_status, iosb_status;
    ULONG_PTR iosb_bytes;
    WCHAR name[QUERY_WCHARS];
} QUERY_RESULT;
typedef LONG (NTAPI *NT_QUERY_FILE)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,ULONG);

static int query_child(HANDLE mapping,HANDLE done)
{
    QUERY_RESULT *r=(QUERY_RESULT *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(QUERY_RESULT));
    HANDLE h;
    int ok=1;
    if(!r)return 2;
    h=(HANDLE)r->duplicate;
    if(r->mode==3) {Sleep(INFINITE);return 2;}
    if(r->mode==0) {
        r->api_attempted=1;SetLastError(0);r->file_type=GetFileType(h);r->error=GetLastError();
        r->return_value=r->file_type;r->return_recorded=1;
        r->complete=(r->file_type!=FILE_TYPE_UNKNOWN || r->error==NO_ERROR);
    } else if(r->mode==1) {
        DWORD n;
        r->api_attempted=1;SetLastError(0);
        n=GetFinalPathNameByHandleW(h,r->name,QUERY_WCHARS,FILE_NAME_OPENED|VOLUME_NAME_NT);
        r->error=n?0:GetLastError();r->return_value=n;r->return_recorded=1;
        if(n && n<QUERY_WCHARS) {r->name_chars=n;r->complete=1;}
        else if(n>=QUERY_WCHARS)r->error=ERROR_INSUFFICIENT_BUFFER;
    } else if(r->mode==2) {
        HMODULE module=GetModuleHandleW(L"ntdll.dll");
        NT_QUERY_FILE query=module?(NT_QUERY_FILE)GetProcAddress(module,"NtQueryInformationFile"):NULL;
        BYTE *buffer=(BYTE *)calloc(1,QUERY_BUFFER_BYTES);
        IO_STATUS_BLOCK iosb;
        memset(&iosb,0,sizeof(iosb));
        if(!query || !buffer)r->error=query?ERROR_NOT_ENOUGH_MEMORY:ERROR_PROC_NOT_FOUND;
        else {
            r->api_attempted=1;
            r->native_status=query(h,&iosb,buffer,QUERY_BUFFER_BYTES,9);r->return_recorded=1;
            r->pending=(r->native_status==0x103);
            /* Keep IOSB/buffer live until process teardown if completion is pending. */
            if(r->pending) {
                r->close_ok=2;r->done=1;
                if(!SetEvent(done))ExitProcess(2);
                ExitProcess(0);
            }
            if(!r->pending) {
                DWORD bytes;
                r->iosb_status=iosb.Status;r->iosb_bytes=iosb.Information;
                if(r->native_status==0 && r->iosb_status==0 && iosb.Information>=sizeof(DWORD) && iosb.Information<=QUERY_BUFFER_BYTES) {
                    memcpy(&bytes,buffer,sizeof(bytes));
                    if(bytes%sizeof(WCHAR)==0 && bytes>0 && bytes<=QUERY_BUFFER_BYTES-sizeof(DWORD) &&
                       bytes<=iosb.Information-sizeof(DWORD) && bytes/sizeof(WCHAR)<QUERY_WCHARS) {
                        memcpy(r->name,buffer+sizeof(DWORD),bytes);r->name_chars=bytes/sizeof(WCHAR);r->complete=1;
                    } else r->error=ERROR_INVALID_DATA;
                }
            }
        }
        if(!r->pending)free(buffer);
    } else {r->error=ERROR_INVALID_PARAMETER;}
    if(h && h!=INVALID_HANDLE_VALUE && !r->pending)r->close_ok=CloseHandle(h)?1:0;
    else r->close_ok=r->pending?2:1; /* 2: released by verified helper process teardown. */
    r->done=1;
    if(!SetEvent(done))ok=0;
    if(!UnmapViewOfFile(r))ok=0;
    if(!CloseHandle(mapping))ok=0;
    if(!CloseHandle(done))ok=0;
    return ok?0:1;
}
static DWORD remaining(DWORD start)
{
    DWORD elapsed=GetTickCount()-start;
    return elapsed<QUERY_MS?QUERY_MS-elapsed:0;
}
static int run_query(const char *self,HANDLE source,HANDLE original,DWORD mode,int expect_timeout,int invalid,QUERY_RESULT *out)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOEXA si;
    PROCESS_INFORMATION pi;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    HANDLE mapping=NULL,done=NULL,job=NULL,handles[2],duplicate=NULL;
    QUERY_RESULT *shared=NULL;
    SIZE_T bytes=0;
    DWORD start=GetTickCount(),wait,code;
    char command[MAX_PATH+128];
    int launched=0,attributes=0,ok=0,n;
    memset(out,0,sizeof(*out));out->mode=mode;
    memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
    memset(&si,0,sizeof(si));si.StartupInfo.cb=sizeof(si);memset(&pi,0,sizeof(pi));
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(QUERY_RESULT),NULL);
    done=CreateEventA(&sa,TRUE,FALSE,NULL);job=CreateJobObjectA(NULL,NULL);
    if(!mapping || !done || !job)goto cleanup;
    shared=(QUERY_RESULT *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(QUERY_RESULT));if(!shared)goto cleanup;
    memset(shared,0,sizeof(*shared));shared->mode=mode;
    memset(&limit,0,sizeof(limit));limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))goto cleanup;
    InitializeProcThreadAttributeList(NULL,1,0,&bytes);
    si.lpAttributeList=(LPPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);if(!si.lpAttributeList)goto cleanup;
    if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&bytes))goto cleanup;
    attributes=1;handles[0]=mapping;handles[1]=done;
    if(!UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),NULL,NULL))goto cleanup;
    n=snprintf(command,sizeof(command),"\"%s\" --query %llu %llu",self,
        (unsigned long long)(ULONG_PTR)mapping,(unsigned long long)(ULONG_PTR)done);
    if(n<0 || (size_t)n>=sizeof(command))goto cleanup;
    if(!CreateProcessA(self,command,NULL,NULL,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED|CREATE_NO_WINDOW,NULL,NULL,&si.StartupInfo,&pi))goto cleanup;
    launched=1;
    if(!AssignProcessToJobObject(job,pi.hProcess))goto cleanup;
    if(invalid)shared->duplicate=(ULONG_PTR)INVALID_HANDLE_VALUE;
    else if(!DuplicateHandle(source,original,pi.hProcess,&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS)) {
        out->duplicate_error=GetLastError();out->close_ok=1;
        if(!TerminateProcess(pi.hProcess,2) || WaitForSingleObject(pi.hProcess,QUERY_MS)!=WAIT_OBJECT_0)goto cleanup;
        ok=1;goto cleanup;
    } else shared->duplicate=(ULONG_PTR)duplicate;
    if(!remaining(start) || ResumeThread(pi.hThread)==(DWORD)-1)goto cleanup;
    wait=WaitForSingleObject(done,remaining(start));
    if(wait==WAIT_TIMEOUT) {
        out->timed_out=1;out->close_ok=2;
        if(!TerminateProcess(pi.hProcess,2) || WaitForSingleObject(pi.hProcess,QUERY_MS)!=WAIT_OBJECT_0)goto cleanup;
        memcpy(out,shared,sizeof(*out));out->timed_out=1;out->complete=0;out->close_ok=2;
        ok=1;goto cleanup;
    }
    if(wait!=WAIT_OBJECT_0)goto cleanup;
    wait=WaitForSingleObject(pi.hProcess,remaining(start));
    if(wait==WAIT_TIMEOUT) {
        out->timed_out=1;out->close_ok=2;
        if(!TerminateProcess(pi.hProcess,2) || WaitForSingleObject(pi.hProcess,QUERY_MS)!=WAIT_OBJECT_0)goto cleanup;
        memcpy(out,shared,sizeof(*out));out->timed_out=1;out->complete=0;out->close_ok=2;
        ok=1;goto cleanup;
    }
    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(pi.hProcess,&code) || code!=0 || !shared->done)goto cleanup;
    memcpy(out,shared,sizeof(*out));
    if(!out->close_ok || expect_timeout)goto cleanup;
    ok=1;
cleanup:
    if(!ok)out->harness_error=GetLastError();
    if(launched) {
        if(!ok && WaitForSingleObject(pi.hProcess,0)!=WAIT_OBJECT_0) {
            if(!TerminateProcess(pi.hProcess,2) || WaitForSingleObject(pi.hProcess,QUERY_MS)!=WAIT_OBJECT_0)ok=0;
        }
        if(!CloseHandle(pi.hThread))ok=0;
        if(!CloseHandle(pi.hProcess))ok=0;
    }
    if(attributes)DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);
    if(shared && !UnmapViewOfFile(shared))ok=0;
    if(mapping && !CloseHandle(mapping))ok=0;
    if(done && !CloseHandle(done))ok=0;
    if(job && !CloseHandle(job))ok=0;
    printf("RESOURCE_HELPER mode=%lu timeout=%lu duplicate_error=%lu cleanup=%d\n",(unsigned long)mode,(unsigned long)out->timed_out,(unsigned long)out->duplicate_error,ok);
    return ok;
}
static int report_query(const char *context,unsigned mode,const QUERY_RESULT *r)
{
    char text[QUERY_BUFFER_BYTES*2];
    int n=0,i;
    if(r->name_chars) {
        if(r->name_chars>=QUERY_WCHARS)return 0;
        n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,r->name,(int)r->name_chars,text,sizeof(text),NULL,NULL);
        if(n<=0)return 0;
    }
    printf("RESOURCE_QUERY context=%s mode=%u attempted=%lu returned=%lu raw_return=%lu harness_error=%lu complete=%lu type=%lu error=%lu ntstatus=0x%08lx iosb_status=0x%08lx iosb_bytes=%llu pending=%lu timeout=%lu duplicate_error=%lu close=%lu name_chars=%lu name=\"",
        context,mode,(unsigned long)r->api_attempted,(unsigned long)r->return_recorded,(unsigned long)r->return_value,(unsigned long)r->harness_error,(unsigned long)r->complete,(unsigned long)r->file_type,(unsigned long)r->error,
        (unsigned long)(ULONG)r->native_status,(unsigned long)(ULONG)r->iosb_status,(unsigned long long)r->iosb_bytes,
        (unsigned long)r->pending,(unsigned long)r->timed_out,(unsigned long)r->duplicate_error,(unsigned long)r->close_ok,(unsigned long)r->name_chars);
    for(i=0;i<n;i++) {unsigned char c=(unsigned char)text[i];if(c<32 || c==127)printf("\\x%02x",c);else {if(c=='"' || c=='\\')putchar('\\');putchar(c);}}
    puts("\"");return 1;
}
static int controls(const char *self)
{
    WCHAR path[MAX_PATH],file[MAX_PATH],expected[QUERY_WCHARS];
    HANDLE disk=INVALID_HANDLE_VALUE,read=NULL,write=NULL;
    QUERY_RESULT *r=(QUERY_RESULT *)calloc(1,sizeof(QUERY_RESULT));
    DWORD n;unsigned mode;int ok=0,created=0;
    if(!r)goto done;
    n=GetTempPathW(MAX_PATH,path);if(!n || n>=MAX_PATH || !GetTempFileNameW(path,L"sfr",0,file))goto done;
    created=1;
    disk=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(disk==INVALID_HANDLE_VALUE)goto done;
    n=GetFinalPathNameByHandleW(disk,expected,QUERY_WCHARS,FILE_NAME_OPENED|VOLUME_NAME_NT);
    if(!n || n>=QUERY_WCHARS || wcsncmp(expected,L"\\Device\\",8))goto done;
    for(mode=0;mode<3;mode++) {
        if(!run_query(self,GetCurrentProcess(),disk,mode,0,0,r) || !report_query("control_disk",mode,r) || !r->complete)goto done;
        if(mode==0 && r->file_type!=FILE_TYPE_DISK)goto done;
        if(mode==1 && (r->name_chars!=n || memcmp(r->name,expected,n*sizeof(WCHAR))))goto done;
        if(mode==2) {
            const WCHAR *base=wcsrchr(file,L'\\');size_t len;
            if(!base)goto done;base++;len=wcslen(base);
            if(r->name_chars<len || memcmp(r->name+r->name_chars-len,base,len*sizeof(WCHAR)))goto done;
        }
    }
    if(!CreatePipe(&read,&write,NULL,0))goto done;
    for(mode=0;mode<3;mode++) {
        if(!run_query(self,GetCurrentProcess(),read,mode,0,0,r) || !report_query("control_pipe",mode,r))goto done;
        if(mode==0 && (!r->complete || r->file_type!=FILE_TYPE_PIPE))goto done;
    }
    if(!run_query(self,GetCurrentProcess(),NULL,0,0,1,r) || !report_query("control_invalid",0,r) || r->complete || r->error!=ERROR_INVALID_HANDLE)goto done;
    if(!run_query(self,GetCurrentProcess(),disk,3,1,0,r) || !r->timed_out || r->complete)goto done;
    ok=1;
done:
    if(disk!=INVALID_HANDLE_VALUE && !CloseHandle(disk))ok=0;
    if(read && !CloseHandle(read))ok=0;
    if(write && !CloseHandle(write))ok=0;
    if(created && !DeleteFileW(file))ok=0;
    free(r);printf("RESOURCE_CONTROLS complete=%d\n",ok);return ok;
}
static int candidate_queries(const char *self,HANDLE target,const TABLE *baseline,DWORD count,ULONG_PTR handle)
{
    TABLE *before=(TABLE *)calloc(1,sizeof(TABLE)),*after=(TABLE *)calloc(1,sizeof(TABLE));
    QUERY_RESULT *r=(QUERY_RESULT *)calloc(1,sizeof(QUERY_RESULT));
    unsigned mode;int ok=1,resource=0;
    if(!before || !after || !r) {ok=0;goto done;}
    for(mode=0;mode<3;mode++) {
        if(!capture(target,before,count,"resource_before",mode) || !same_table(baseline,before)) {ok=0;break;}
        if(!run_query(self,target,(HANDLE)handle,mode,0,0,r))ok=0;
        if(!report_query("candidate",mode,r))ok=0;
        if(r->timed_out || r->pending || r->duplicate_error)ok=0;
        if(mode==1 && r->complete && r->name_chars>8 && !wcsncmp(r->name,L"\\Device\\",8))resource=1;
        if(!capture(target,after,count,"resource_after",mode) || !same_table(baseline,after)) {ok=0;break;}
    }
done:
    free(before);free(after);free(r);
    printf("RESOURCE_RESULT resource=%s owner=unproven target_stable=%d candidate_not_closed=1\n",resource?"full_path_observed":"blocked",ok);
    return ok && resource;
}

static int run_arm(const char *self,const char *arm,int pipes,int snapshots)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    HANDLE mapping=NULL,ready=NULL,resume=NULL,job=NULL;
    SHARED *s=NULL;
    TABLE *tables=NULL,*duplicate=NULL;
    DWORD counts[5]={0},stage,wait,code=2,start;
    ULONG_PTR returned[2][2]={{0}};
    char command[MAX_PATH+160];
    int launched=0,ok=0,n,resource_ok=1;
    memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
    memset(&pi,0,sizeof(pi));memset(&si,0,sizeof(si));si.cb=sizeof(si);
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(SHARED),NULL);
    ready=CreateEventA(&sa,FALSE,FALSE,NULL); resume=CreateEventA(&sa,FALSE,FALSE,NULL);
    job=CreateJobObjectA(NULL,NULL);
    if(!mapping || !ready || !resume || !job)goto cleanup;
    memset(&limit,0,sizeof(limit));limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))goto cleanup;
    s=(SHARED *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SHARED));if(!s)goto cleanup;
    memset(s,0,sizeof(*s));
    if(snapshots) {tables=(TABLE *)calloc(5,sizeof(TABLE));duplicate=(TABLE *)calloc(1,sizeof(TABLE));if(!tables || !duplicate)goto cleanup;}
    n=snprintf(command,sizeof(command),"\"%s\" --child %llu %llu %llu %d",self,
               (unsigned long long)(ULONG_PTR)mapping,(unsigned long long)(ULONG_PTR)ready,(unsigned long long)(ULONG_PTR)resume,pipes);
    if(n<0 || (size_t)n>=sizeof(command))goto cleanup;
    start=GetTickCount();
    if(!CreateProcessA(self,command,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,NULL,&si,&pi))goto cleanup;
    launched=1;
    if(!AssignProcessToJobObject(job,pi.hProcess) || ResumeThread(pi.hThread)==(DWORD)-1)goto cleanup;
    for(stage=0;stage<5;stage++) {
        if(GetTickCount()-start>=60000)goto cleanup;
        wait=WaitForSingleObject(ready,20000);
        if(wait!=WAIT_OBJECT_0 || s->stage!=stage)goto cleanup;
        counts[stage]=s->count;
        if(stage==1 || stage==3) {returned[stage==1?0:1][0]=s->read_handle;returned[stage==1?0:1][1]=s->write_handle;}
        printf("PSS_CHECKPOINT arm=%s pid=%lu stage=%lu count=%lu read=0x%llx write=0x%llx\n",arm,(unsigned long)pi.dwProcessId,(unsigned long)stage,(unsigned long)s->count,(unsigned long long)s->read_handle,(unsigned long long)s->write_handle);
        if(snapshots) {
            if(!capture(pi.hProcess,&tables[stage],counts[stage],arm,stage))goto cleanup;
            if(stage==0) {
                if(!capture(pi.hProcess,duplicate,counts[stage],arm,stage) || !same_table(&tables[0],duplicate)) {
                    puts("PSS_OBSERVER pre_snapshot_mismatch");goto cleanup;
                }
            } else if(!print_delta(arm,stage,&tables[stage-1],&tables[stage]))goto cleanup;
            if(!pipes && !same_table(&tables[0],&tables[stage])) {puts("PSS_OBSERVER no_pipe_table_changed");goto cleanup;}
        }
        if(stage==4 && pipes && snapshots) {
            unsigned k;ULONG_PTR candidate=0;
            for(k=0;k<5;k++) {
                long long expected=(k==1 || k==3)?3:(k?1:0);
                if((long long)counts[k]-(long long)counts[0]!=expected)goto cleanup;
            }
            if(!validate_tables(arm,tables,returned))goto cleanup;
            for(k=0;k<tables[4].count;k++)if(!lookup(&tables[0],tables[4].entries[k].value))candidate=tables[4].entries[k].value;
            resource_ok=candidate_queries(self,pi.hProcess,&tables[4],counts[4],candidate);
        }
        if(!SetEvent(resume))goto cleanup;
    }
    wait=WaitForSingleObject(pi.hProcess,5000);
    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(pi.hProcess,&code) || code!=0 || s->ok!=1 || GetTickCount()-start>=60000)goto cleanup;
    for(stage=0;stage<5;stage++) {
        long long expected=pipes?((stage==1 || stage==3)?3:(stage?1:0)):0;
        if((long long)counts[stage]-(long long)counts[0]!=expected)goto cleanup;
    }
    if(!resource_ok)goto cleanup;
    printf("PSS_ARM arm=%s complete=1 deltas=%lld,%lld,%lld,%lld,%lld\n",arm,
           (long long)counts[0]-counts[0],(long long)counts[1]-counts[0],(long long)counts[2]-counts[0],(long long)counts[3]-counts[0],(long long)counts[4]-counts[0]);
    ok=1;
cleanup:
    if(!ok)printf("PSS_ARM arm=%s complete=0 child_error=%lu last_error=%lu\n",arm,s?(unsigned long)s->error:0,(unsigned long)GetLastError());
    if(launched) {if(!ok) {TerminateProcess(pi.hProcess,2);WaitForSingleObject(pi.hProcess,5000);}if(!CloseHandle(pi.hThread))ok=0;if(!CloseHandle(pi.hProcess))ok=0;}
    if(job && !CloseHandle(job))ok=0;
    if(s && !UnmapViewOfFile(s))ok=0;
    if(mapping && !CloseHandle(mapping))ok=0;
    if(ready && !CloseHandle(ready))ok=0;
    if(resume && !CloseHandle(resume))ok=0;
    free(tables);free(duplicate);fflush(stdout);
    return ok;
}
int main(int argc,char **argv)
{
    char self[MAX_PATH];DWORD n;
    if(argc==6 && !strcmp(argv[1],"--child")) {
        return child((HANDLE)(ULONG_PTR)_strtoui64(argv[2],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[3],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[4],NULL,10),atoi(argv[5]));
    }
    if(argc==4 && !strcmp(argv[1],"--query"))return query_child((HANDLE)(ULONG_PTR)_strtoui64(argv[2],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[3],NULL,10));
    if(argc!=1)return 2;
    n=GetModuleFileNameA(NULL,self,sizeof(self));if(!n || n>=sizeof(self))return 2;
    if(!controls(self))return 1;
    if(!run_arm(self,"count_only_pipe",1,0))return 1;
    if(!run_arm(self,"snapshot_only_no_pipe",0,1))return 1;
    if(!run_arm(self,"external_snapshot_pipe",1,1))return 1;
    puts("WINDOWS_FILE_RESOURCE arms=3; candidate_not_closed=1; owner_unproven=1; complete=1");return 0;
}
