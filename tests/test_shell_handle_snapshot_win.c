/* External observation of a private process, never a production hook. */
#define _WIN32_WINNT 0x0603
#include <windows.h>
#include <processsnapshot.h>
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
    return WaitForSingleObject(resume,5000)==WAIT_OBJECT_0;
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
    int launched=0,ok=0,n;
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
        if(GetTickCount()-start>=30000)goto cleanup;
        wait=WaitForSingleObject(ready,5000);
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
        if(!SetEvent(resume))goto cleanup;
    }
    wait=WaitForSingleObject(pi.hProcess,5000);
    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(pi.hProcess,&code) || code!=0 || s->ok!=1 || GetTickCount()-start>=30000)goto cleanup;
    for(stage=0;stage<5;stage++) {
        long long expected=pipes?((stage==1 || stage==3)?3:(stage?1:0)):0;
        if((long long)counts[stage]-(long long)counts[0]!=expected)goto cleanup;
    }
    if(pipes && snapshots && !validate_tables(arm,tables,returned))goto cleanup;
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
    if(argc!=1)return 2;
    n=GetModuleFileNameA(NULL,self,sizeof(self));if(!n || n>=sizeof(self))return 2;
    if(!run_arm(self,"count_only_pipe",1,0))return 1;
    if(!run_arm(self,"snapshot_only_no_pipe",0,1))return 1;
    if(!run_arm(self,"external_snapshot_pipe",1,1))return 1;
    puts("WINDOWS_HANDLE_SNAPSHOT arms=3; candidate_not_closed=1; complete=1");return 0;
}
