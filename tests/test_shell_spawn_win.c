/* Contract v2: measure the actual unmodified AgentShellExec failure process.
   The diagnostic source supplies private PSS/query helpers only. Its own target
   and frozen gold are unchanged. v1 remains a byte-identical historical source. */
#define main resource_reference_main
#include "test_shell_file_resource_win.c"
#undef main
#define child v1_child
#define main v1_reference_main
#include "fixtures/shell_spawn_contract_v1/test_shell_spawn_win.c"
#undef main
#undef child

#define V2_CALLS 27
#define V2_WAIT_MS 15000
#define V2_TOTAL_MS 120000

typedef struct {
    DWORD call, count, error;
    LONG done, exec_ok;
    unsigned mask;
    SHELL_EXEC_RESULT result;
} SPAWN_SHARED;

static int spawn_checkpoint(SPAWN_SHARED *s,HANDLE ready,HANDLE resume,unsigned call)
{
    s->call=call;
    if(!GetProcessHandleCount(GetCurrentProcess(),&s->count)) {s->error=GetLastError();return 0;}
    if(!SetEvent(ready))return 0;
    return WaitForSingleObject(resume,V2_WAIT_MS)==WAIT_OBJECT_0;
}
static int spawn_v2_child(HANDLE mapping,HANDLE ready,HANDLE resume,int kind)
{
    SPAWN_SHARED *s=(SPAWN_SHARED *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SPAWN_SHARED));
    unsigned i;int ok=0;
    if(!s)return 2;
    s->done=0;s->error=0;
    if(kind) {
        STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD error,old;
        char command[]="cmd.exe /c echo witness>witness.txt";
        old=SetErrorMode(SEM_FAILCRITICALERRORS);SetErrorMode(old|SEM_FAILCRITICALERRORS);
        memset(&si,0,sizeof(si));si.cb=sizeof(si);memset(&pi,0,sizeof(pi));
        if(CreateProcessA(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,".",&si,&pi)) {
            TerminateProcess(pi.hProcess,2);WaitForSingleObject(pi.hProcess,5000);
            CloseHandle(pi.hThread);CloseHandle(pi.hProcess);goto done;
        }
        error=GetLastError();
        printf("SPAWN_PROBE kind=%d ok=0 error=%lu witness=%d\n",kind,(unsigned long)error,GetFileAttributesA("witness.txt")!=INVALID_FILE_ATTRIBUTES);fflush(stdout);
        if((error!=193 && error!=216) || GetFileAttributesA("witness.txt")!=INVALID_FILE_ATTRIBUTES)goto done;
    }
    /* No output, allocation, PSS or name queries in this measured process. */
    if(!spawn_checkpoint(s,ready,resume,0))goto done;
    for(i=1;i<=V2_CALLS;i++) {
        if(kind) {
            memset(&s->result,0xa5,sizeof(s->result));
            s->exec_ok=AgentShellExec("echo witness>witness.txt",".",5000,&s->result);
            s->mask=failure_mask(s->exec_ok,&s->result);
        }
        if(!spawn_checkpoint(s,ready,resume,i))goto done;
        if(kind && s->mask)goto done;
    }
    s->done=1;ok=1;
done:
    if(!UnmapViewOfFile(s))ok=0;
    if(!CloseHandle(mapping))ok=0;
    if(!CloseHandle(ready))ok=0;
    if(!CloseHandle(resume))ok=0;
    return ok?0:1;
}
static int spawn_table(const TABLE *baseline,const TABLE *now,ENTRY *candidate,unsigned call)
{
    unsigned i,extra=0;const ENTRY *added=NULL;
    for(i=0;i<baseline->count;i++)
        if(!same_identity(&baseline->entries[i],lookup(now,baseline->entries[i].value)))return 0;
    for(i=0;i<now->count;i++)if(!lookup(baseline,now->entries[i].value)){extra++;added=&now->entries[i];}
    if(extra!=1 || now->count!=baseline->count+1 || !added ||
       !resource_type_file(added->type_name,added->type_bytes,added->flags))return 0;
    if(call==1)*candidate=*added;
    else if(!same_identity(candidate,added))return 0;
    printf("SPAWN_TABLE call=%u baseline=%u entries=%u baseline_stable=1 extras=1 candidate=0x%llx type=File type_bytes=%u flags=%lu candidate_stable=1\n",
        call,baseline->count,now->count,(unsigned long long)candidate->value,candidate->type_bytes,(unsigned long)candidate->flags);
    return 1;
}
static int spawn_resource(const char *self,HANDLE target,const TABLE *expected,DWORD count,ULONG_PTR value,unsigned call)
{
    static const WCHAR root[]=L"\\Device\\NamedPipe\\";
    TABLE *before=(TABLE *)calloc(1,sizeof(TABLE)),*after=(TABLE *)calloc(1,sizeof(TABLE));
    QUERY_RESULT *q=(QUERY_RESULT *)calloc(1,sizeof(QUERY_RESULT));
    char context[32];unsigned mode;int ok=0;
    if(!before || !after || !q)goto done;
    snprintf(context,sizeof(context),"spawn_call_%u",call);
    for(mode=0;mode<2;mode++) {
        if(!capture(target,before,count,"spawn_query_before",call*10+mode) || !same_table(expected,before))goto done;
        if(!run_query(self,target,(HANDLE)value,mode,0,0,q) || !report_query(context,mode,q))goto done;
        if(!q->api_attempted || !q->return_recorded || !q->complete || q->error || q->harness_error ||
           q->pending || q->timed_out || q->duplicate_error || q->close_ok!=1)goto done;
        if(mode==0 && q->file_type!=FILE_TYPE_PIPE)goto done;
        if(mode==1 && (q->name_chars!=18 || q->return_value!=18 || memcmp(q->name,root,18*sizeof(WCHAR))))goto done;
        if(!capture(target,after,count,"spawn_query_after",call*10+mode) || !same_table(expected,after))goto done;
    }
    ok=1;
done:
    printf("SPAWN_RESOURCE call=%u candidate=0x%llx exact_nt_root=%d file_type=3 target_stable=%d candidate_not_closed=1\n",call,(unsigned long long)value,ok,ok);
    free(before);free(after);free(q);return ok;
}
static int spawn_arm(const char *self,const char *root,const char *arm,int kind)
{
    SECURITY_ATTRIBUTES sa;STARTUPINFOA si;PROCESS_INFORMATION pi;JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    HANDLE mapping=NULL,ready=NULL,resume=NULL,job=NULL,invalid=INVALID_HANDLE_VALUE;
    SPAWN_SHARED *s=NULL;TABLE *baseline=NULL,*now=NULL,*duplicate=NULL;ENTRY candidate;
    DWORD counts[V2_CALLS+1]={0},stage,wait,code=2,start=GetTickCount(),written;
    char dir[MAX_PATH],exe[MAX_PATH],shell[MAX_PATH],command[MAX_PATH+200],witness[MAX_PATH];
    int ok=0,launched=0,made_dir=0,copied=0,made_image=0,n;
    memset(&pi,0,sizeof(pi));memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
    memset(&candidate,0,sizeof(candidate));
    if(snprintf(dir,sizeof(dir),"%s\\%s",root,arm)>=sizeof(dir) ||
       snprintf(exe,sizeof(exe),"%s\\runner.exe",dir)>=sizeof(exe) ||
       snprintf(shell,sizeof(shell),"%s\\cmd.exe",dir)>=sizeof(shell) ||
       snprintf(witness,sizeof(witness),"%s\\witness.txt",dir)>=sizeof(witness))goto cleanup;
    if(!CreateDirectoryA(dir,NULL))goto cleanup;made_dir=1;
    if(!CopyFileA(self,exe,TRUE))goto cleanup;copied=1;
    if(kind) {
        char image[64];memset(image,'X',sizeof(image));
        invalid=CreateFileA(shell,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(invalid==INVALID_HANDLE_VALUE)goto cleanup;made_image=1;
        if(!WriteFile(invalid,image,sizeof(image),&written,NULL) || written!=sizeof(image))goto cleanup;
        if(!CloseHandle(invalid))goto cleanup;invalid=INVALID_HANDLE_VALUE;
    }
    mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(SPAWN_SHARED),NULL);
    ready=CreateEventA(&sa,FALSE,FALSE,NULL);resume=CreateEventA(&sa,FALSE,FALSE,NULL);job=CreateJobObjectA(NULL,NULL);
    if(!mapping || !ready || !resume || !job)goto cleanup;
    s=(SPAWN_SHARED *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SPAWN_SHARED));if(!s)goto cleanup;memset(s,0,sizeof(*s));
    memset(&limit,0,sizeof(limit));limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limit,sizeof(limit)))goto cleanup;
    if(kind!=1) {baseline=(TABLE *)calloc(1,sizeof(TABLE));now=(TABLE *)calloc(1,sizeof(TABLE));duplicate=(TABLE *)calloc(1,sizeof(TABLE));if(!baseline||!now||!duplicate)goto cleanup;}
    memset(&si,0,sizeof(si));si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    n=snprintf(command,sizeof(command),"\"%s\" --v2-child %llu %llu %llu %d",exe,(unsigned long long)(ULONG_PTR)mapping,(unsigned long long)(ULONG_PTR)ready,(unsigned long long)(ULONG_PTR)resume,kind);
    if(n<0 || (size_t)n>=sizeof(command))goto cleanup;
    if(!CreateProcessA(exe,command,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,dir,&si,&pi))goto cleanup;launched=1;
    if(!AssignProcessToJobObject(job,pi.hProcess) || ResumeThread(pi.hThread)==(DWORD)-1)goto cleanup;
    for(stage=0;stage<=V2_CALLS;stage++) {
        if(GetTickCount()-start>=V2_TOTAL_MS)goto cleanup;
        wait=WaitForSingleObject(ready,V2_WAIT_MS);
        if(wait!=WAIT_OBJECT_0 || s->call!=stage || s->error)goto cleanup;
        counts[stage]=s->count;
        printf("SPAWN_CHECKPOINT arm=%s call=%lu count=%lu\n",arm,(unsigned long)stage,(unsigned long)counts[stage]);
        if(stage && kind) {
            const SHELL_EXEC_RESULT *r=&s->result;
            printf("SPAWN_RESULT arm=%s call=%lu mask=%u ok=%ld exit=%d launch=%d timeout=%d stored=%llu/%llu total=%llu/%llu truncated=%d/%d first=%u/%u witness=%d\n",arm,(unsigned long)stage,s->mask,(long)s->exec_ok,r->exit_code,r->execution_failed,r->timed_out,(unsigned long long)r->stdout_len,(unsigned long long)r->stderr_len,(unsigned long long)r->stdout_total_len,(unsigned long long)r->stderr_total_len,r->stdout_truncated,r->stderr_truncated,(unsigned char)r->stdout_buf[0],(unsigned char)r->stderr_buf[0],GetFileAttributesA(witness)!=INVALID_FILE_ATTRIBUTES);
            if(s->mask || s->exec_ok!=0 || (long long)counts[stage]-(long long)counts[0]!=1)goto cleanup;
        } else if(stage && counts[stage]!=counts[0])goto cleanup;
        if(kind!=1) {
            TABLE *table=stage?now:baseline;
            if(!capture(pi.hProcess,table,counts[stage],arm,stage))goto cleanup;
            if(stage==0) {
                if(!capture(pi.hProcess,duplicate,counts[stage],arm,stage) || !same_table(baseline,duplicate))goto cleanup;
            } else if(kind==0) {
                if(!same_table(baseline,now))goto cleanup;
            } else {
                if(!spawn_table(baseline,now,&candidate,stage))goto cleanup;
                if((stage==1 || stage==V2_CALLS) && !spawn_resource(self,pi.hProcess,now,counts[stage],candidate.value,stage))goto cleanup;
            }
        }
        if(!SetEvent(resume))goto cleanup;
    }
    wait=WaitForSingleObject(pi.hProcess,V2_WAIT_MS);
    if(wait!=WAIT_OBJECT_0 || !GetExitCodeProcess(pi.hProcess,&code) || code!=0 || s->done!=1 || GetTickCount()-start>=V2_TOTAL_MS)goto cleanup;
    ok=1;
cleanup:
    if(launched) {
        if(!ok && WaitForSingleObject(pi.hProcess,0)!=WAIT_OBJECT_0) {
            if(!TerminateProcess(pi.hProcess,2) || WaitForSingleObject(pi.hProcess,5000)!=WAIT_OBJECT_0)ok=0;
        }
        if(!CloseHandle(pi.hThread))ok=0;if(!CloseHandle(pi.hProcess))ok=0;
    }
    if(invalid!=INVALID_HANDLE_VALUE && !CloseHandle(invalid))ok=0;
    if(s && !UnmapViewOfFile(s))ok=0;
    if(mapping && !CloseHandle(mapping))ok=0;if(ready && !CloseHandle(ready))ok=0;if(resume && !CloseHandle(resume))ok=0;if(job && !CloseHandle(job))ok=0;
    if(made_image && !DeleteFileA(shell))ok=0;if(copied && !DeleteFileA(exe))ok=0;if(made_dir && !RemoveDirectoryA(dir))ok=0;
    free(baseline);free(now);free(duplicate);
    printf("SPAWN_ARM arm=%s complete=%d calls=%d baseline=%lu final=%lu child_exit=%lu cleanup=%d\n",arm,ok,V2_CALLS,(unsigned long)counts[0],(unsigned long)counts[V2_CALLS],(unsigned long)code,ok);fflush(stdout);
    return ok;
}
int main(int argc,char **argv)
{
    char self[MAX_PATH],temp[MAX_PATH],root[MAX_PATH];DWORD n;int ok;
    if(argc==6 && !strcmp(argv[1],"--v2-child"))return spawn_v2_child((HANDLE)(ULONG_PTR)_strtoui64(argv[2],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[3],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[4],NULL,10),atoi(argv[5]));
    if(argc==4 && !strcmp(argv[1],"--query"))return query_child((HANDLE)(ULONG_PTR)_strtoui64(argv[2],NULL,10),(HANDLE)(ULONG_PTR)_strtoui64(argv[3],NULL,10));
    if(argc==3 && !strcmp(argv[1],"--child"))return v1_child(argv[2]); /* frozen positive control only */
    if(argc!=1)return 2;
    n=GetModuleFileNameA(NULL,self,sizeof(self));if(!n || n>=sizeof(self))return 2;
    n=GetTempPathA(sizeof(temp),temp);if(!n || n>=sizeof(temp))return 2;
    if(snprintf(root,sizeof(root),"%ssymbols-spawn-v2-%lu-%lu",temp,(unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount())>=sizeof(root))return 2;
    if(!CreateDirectoryA(root,NULL))return 2;
    ok=scenario(self,root,"positive");
    if(ok)ok=controls(self);
    if(ok)ok=spawn_arm(self,root,"observer_no_call",0);
    if(ok)ok=spawn_arm(self,root,"count_only_invalid",1);
    if(ok)ok=spawn_arm(self,root,"observed_invalid",2);
    if(!RemoveDirectoryA(root))ok=0;
    if(!ok)return 1;
    puts("WINDOWS_SPAWN_V2 calls=27; persistent=1; exact_namedpipe_root=1; owner_unproven=1; candidate_not_closed=1; complete=1");return 0;
}
