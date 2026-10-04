/* NTFS process-termination journal for create-only writes. Included inside the
   Windows section of fs_read.c so the held-root and component opener are shared. */
#include <stddef.h>
#include <stdio.h>
#define WC_INTENT L".fstxn.intent"
#define WC_COMMIT L".fstxn.ccommit"
#define WC_LOCK L".fstxn.lock"
#define WO_REMOVE L".fstxn.remove"
#define WO_RCOMMIT L".fstxn.rcommit"
#define WO_MOVE L".fstxn.move"
#define WO_MCOMMIT L".fstxn.mcommit"
#define WR_INTENT L".fstxn.replace"
#define WR_COMMIT L".fstxn.pcommit"
#define WR_MAGIC 0x57525031u
#define WO_MAGIC 0x574F5031u
#define WC_MAGIC 0x57435231u
#define WC_MAX_PATH 4096
#define WC_SHARE (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
#ifndef FILE_OPEN_IF
#define FILE_OPEN_IF 0x00000003
#endif
#define WC_OPTIONS (FILE_OPEN_REPARSE_POINT|FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE)
#define WC_FILE_ID(h,p) GetFileInformationByHandleEx((h),FileIdInfo,(p),sizeof(*(p)))
/* The public user-mode winternl.h omits these FILE_INFORMATION_CLASS enum
   members. WDK's stable ABI values are rename=10 and link=11 (72 is LinkEx). */
#define WC_NT_RENAME 10u
#define WC_NT_LINK 11u
typedef NTSTATUS (NTAPI *WC_NT_SET)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,ULONG);
typedef struct { BOOLEAN replace; HANDLE root; ULONG length; WCHAR name[1]; } WC_NAME_INFO;
typedef struct {
    unsigned magic, version, checksum, reserved;
    FILE_ID_INFO root, parent, file;
    unsigned long long size, digest;
    unsigned mode;
    WCHAR target[WC_MAX_PATH];
    WCHAR stage[48], pin[48];
} WC_RECORD;
/* Diagnostic-only tracing: compiled only into the native crash-test binaries.
   Production code and its return values are untouched by these probes. */
#ifdef FS_CREATE_TEST_CRASH
#define WC_TRACE(...) fprintf(stderr,__VA_ARGS__)
#else
#define WC_TRACE(...) ((void)0)
#endif
typedef struct {
    unsigned magic,version,checksum,kind;
    FILE_ID_INFO root,source_parent,target_parent,file;
    unsigned long long size,digest;
    WCHAR source[WC_MAX_PATH],target[WC_MAX_PATH],pin[48];
} WO_RECORD;
typedef struct {
    unsigned magic,version,checksum,reserved;
    FILE_ID_INFO root,parent,old_id,new_id;
    unsigned long long old_size,old_digest,new_size,new_digest;
    WCHAR target[WC_MAX_PATH],old_pin[48],new_stage[48],new_pin[48],rollback[48];
} WR_RECORD;
static int wc_terminated(const WCHAR *s,size_t cap)
{for(size_t i=0;i<cap;i++)if(!s[i])return 1;return 0;}
static int wc_random_name(const WCHAR *s,const WCHAR *prefix)
{
    if(wcslen(s)!=wcslen(prefix)+32||wcsncmp(s,prefix,wcslen(prefix)))return 0;
    for(const WCHAR *p=s+wcslen(prefix);*p;p++)
        if(!((*p>=L'0'&&*p<=L'9')||(*p>=L'a'&&*p<=L'f')))return 0;
    return 1;
}
static int wc_reserved(const WCHAR *s)
{
    const WCHAR *leaf=wcsrchr(s,L'/');leaf=leaf?leaf+1:s;
    return !_wcsnicmp(leaf,L".fstxn",6)||!_wcsnicmp(leaf,L".fst-",5)||
       !_wcsnicmp(leaf,L".fsp-",5)||!_wcsnicmp(leaf,L".fsj-",5)||
       !_wcsnicmp(leaf,L".fsrp-",6)||!_wcsnicmp(leaf,L".fsrs-",6)||!_wcsnicmp(leaf,L".fsrb-",6)||
       !_wcsnicmp(leaf,L".fsrm-",6)||!_wcsnicmp(leaf,L".fsmv-",6);
}
/* Win32 device names (CON PRN AUX NUL COM1-9 LPT1-9), with any extension and
   any trailing spaces before the extension, are aliases of devices in the Win32
   namespace. They are refused for every component, as the manifest already
   does (valid_component in fs_manifest.c). */
static int wc_device_name(const WCHAR *s)
{
    WCHAR b[6];size_t n=0,i;
    while(s[n]&&s[n]!=L'.')n++;
    while(n&&s[n-1]==L' ')n--;
    if(n!=3&&n!=4)return 0;
    for(i=0;i<n;i++)b[i]=(s[i]>=L'a'&&s[i]<=L'z')?(WCHAR)(s[i]-32):s[i];
    b[n]=0;
    if(n==3)return !wcscmp(b,L"CON")||!wcscmp(b,L"PRN")||
                   !wcscmp(b,L"AUX")||!wcscmp(b,L"NUL");
    return (!wcsncmp(b,L"COM",3)||!wcsncmp(b,L"LPT",3))&&b[3]>=L'1'&&b[3]<=L'9';
}
/* Validate the whole path before classifying reserved control leaves. A malformed
   suffix must stay INVALID even if an earlier component is reserved. */
/* Test-only mutant (never defined in production targets): MC3 drops the
   trailing dot and space rejection. See tests/test_fs_win_aliases.c. */
#if defined(FS_ALIAS_MUTANT) && FS_ALIAS_MUTANT==3
#define WC_TRAIL 0
#else
#define WC_TRAIL (segment[n-1]==L'.'||segment[n-1]==L' ')
#endif
static FS_READ_STATUS wc_target_status(const WCHAR *s)
{
    WCHAR segment[241];size_t n=0;int reserved=0;
    if(!*s)return FS_READ_INVALID;
    for(const WCHAR *p=s;;p++){
        if(*p==L'/'||!*p){
            if(!n||n>240)return FS_READ_INVALID;
            segment[n]=0;
            if(WC_TRAIL||
               !wcscmp(segment,L".")||!wcscmp(segment,L".."))return FS_READ_INVALID;
            if(wc_device_name(segment))return FS_READ_INVALID;
            if(wc_reserved(segment))reserved=1;
            n=0;if(!*p)break;
        }else{
            if(*p==L'\\'||*p==L':'||*p<32||n>=240)return FS_READ_INVALID;
            segment[n++]=*p;
        }
    }
    return reserved?FS_READ_DENIED:FS_READ_OK;
}
static unsigned wc_checksum(const WC_RECORD *r)
{
    const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;
    for(size_t i=0;i<sizeof(*r);i++){
        if(i>=offsetof(WC_RECORD,checksum)&&i<offsetof(WC_RECORD,checksum)+sizeof(r->checksum))continue;
        h=(h^p[i])*16777619u;
    }return h;
}
static unsigned long long wc_hash(const unsigned char *p,size_t n)
{
    unsigned long long h=14695981039346656037ULL;
    for(size_t i=0;i<n;i++)h=(h^p[i])*1099511628211ULL;
    return h;
}
static unsigned wo_checksum(const WO_RECORD *r)
{
    const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;
    for(size_t i=0;i<sizeof(*r);i++){
        if(i>=offsetof(WO_RECORD,checksum)&&i<offsetof(WO_RECORD,checksum)+sizeof(r->checksum))continue;
        h=(h^p[i])*16777619u;
    }return h;
}
static unsigned wr_checksum(const WR_RECORD *r)
{
    const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;
    for(size_t i=0;i<sizeof(*r);i++){
        if(i>=offsetof(WR_RECORD,checksum)&&i<offsetof(WR_RECORD,checksum)+sizeof(r->checksum))continue;
        h=(h^p[i])*16777619u;
    }return h;
}
static int wc_same(const FILE_ID_INFO *a,const FILE_ID_INFO *b)
{return a->VolumeSerialNumber==b->VolumeSerialNumber&&memcmp(&a->FileId,&b->FileId,sizeof(a->FileId))==0;}
static int wc_id(HANDLE h,FILE_ID_INFO *out)
{
    BOOL ok=WC_FILE_ID(h,out);DWORD error=ok?0:GetLastError();
    WC_TRACE("wc_id FileIdInfo ok=%d error=%lu serial=%llu\n",(int)ok,
             (unsigned long)error,ok?(unsigned long long)out->VolumeSerialNumber:0ULL);
    (void)error;
    return ok&&out->VolumeSerialNumber!=0;
}
static int wc_ntfs(HANDLE root)
{
    WCHAR type[32];
#ifdef FS_CREATE_TEST_CRASH
    if(getenv("FS_WIN_TEST_SIMULATE_NON_NTFS")){
        WC_TRACE("wc_ntfs simulated non-NTFS rejection\n");return 0;
    } /* simulation, not a volume test */
#endif
    BOOL ok=GetVolumeInformationByHandleW(root,NULL,0,NULL,NULL,NULL,type,32);
    DWORD error=ok?0:GetLastError();
    WC_TRACE("wc_ntfs GetVolumeInformationByHandleW ok=%d error=%lu fs=%ls\n",
             (int)ok,(unsigned long)error,ok?type:L"(unavailable)");
    (void)error;
    return ok&&wcscmp(type,L"NTFS")==0;
}
static FS_NT_CREATE wc_create_fn(void)
{HMODULE dll=GetModuleHandleW(L"ntdll.dll");return dll?(FS_NT_CREATE)(void*)GetProcAddress(dll,"NtCreateFile"):NULL;}
static WC_NT_SET wc_set_fn(void)
{HMODULE dll=GetModuleHandleW(L"ntdll.dll");return dll?(WC_NT_SET)(void*)GetProcAddress(dll,"NtSetInformationFile"):NULL;}
static FS_READ_STATUS wc_open(HANDLE dir,const WCHAR *leaf,ACCESS_MASK access,ULONG disposition,HANDLE *out)
{
    FS_NT_CREATE f=wc_create_fn();UNICODE_STRING name;OBJECT_ATTRIBUTES a;IO_STATUS_BLOCK io;
    NTSTATUS nt;size_t len=wcslen(leaf);
    if(!f){WC_TRACE("wc_open NtCreateFile unavailable name=%ls error=%lu\n",
                    leaf,(unsigned long)GetLastError());return FS_READ_UNSUPPORTED;}
    if(!len||len>240||wcschr(leaf,L'/')||wcschr(leaf,L'\\'))return FS_READ_INVALID;
    name.Buffer=(PWSTR)leaf;name.Length=(USHORT)(len*sizeof(WCHAR));name.MaximumLength=name.Length;
    memset(&a,0,sizeof(a));a.Length=sizeof(a);a.RootDirectory=dir;
    a.ObjectName=&name;a.Attributes=OBJ_CASE_INSENSITIVE;
    nt=f(out,access|FILE_READ_ATTRIBUTES|SYNCHRONIZE,&a,&io,NULL,FILE_ATTRIBUTE_NORMAL,
         WC_SHARE,disposition,WC_OPTIONS,NULL,0);
    WC_TRACE("wc_open NtCreateFile name=%ls disposition=%lu access=0x%lx NTSTATUS=0x%08lx\n",
             leaf,(unsigned long)disposition,(unsigned long)access,(unsigned long)nt);
    if(NT_SUCCESS(nt))return FS_READ_OK;
    *out=INVALID_HANDLE_VALUE;
    if(nt==(NTSTATUS)0xC0000034L||nt==(NTSTATUS)0xC000003AL)return FS_READ_MISSING;
    if(nt==(NTSTATUS)0xC0000035L)return FS_READ_DENIED;
    /* STATUS_DELETE_PENDING: the name was deleted but a holder still has it
       open. Nothing is wrong with the tree; retry after the holder closes. */
    if(nt==(NTSTATUS)0xC0000056L)return FS_READ_PENDING;
    return FS_READ_DENIED;
}
/* No path-based renames or deletion. The object is always accessed through
   the source handle; target names are simple leaves of held directories. */
static int wc_set_name(HANDLE source,HANDLE dir,const WCHAR *leaf,ULONG type)
{
    WC_NT_SET f=wc_set_fn();IO_STATUS_BLOCK io;WC_NAME_INFO *v;size_t n=wcslen(leaf);
    size_t size=offsetof(WC_NAME_INFO,name)+(n+1)*sizeof(WCHAR);NTSTATUS st;
    if(!f||!n||n>240||wcschr(leaf,L'/')||wcschr(leaf,L'\\'))return 0;
    v=(WC_NAME_INFO*)calloc(1,size);if(!v)return 0;
    v->replace=FALSE;v->root=dir;v->length=(ULONG)(n*sizeof(WCHAR));
    memcpy(v->name,leaf,(n+1)*sizeof(WCHAR));
    st=f(source,&io,v,(ULONG)size,type);
    WC_TRACE("wc_set_name NtSetInformationFile type=%lu leaf=%ls NTSTATUS=0x%08lx\n",
             (unsigned long)type,leaf,(unsigned long)st);
    free(v);return NT_SUCCESS(st);
}
static int wr_replace_name(HANDLE source,HANDLE dir,const WCHAR *leaf)
{
    WC_NT_SET f=wc_set_fn();IO_STATUS_BLOCK io;WC_NAME_INFO *v;size_t n=wcslen(leaf);
    size_t size=offsetof(WC_NAME_INFO,name)+(n+1)*sizeof(WCHAR);NTSTATUS st;
    if(!f||!n||n>240||wcschr(leaf,L'/')||wcschr(leaf,L'\\'))return 0;
    v=(WC_NAME_INFO*)calloc(1,size);if(!v)return 0;
    v->replace=TRUE;v->root=dir;v->length=(ULONG)(n*sizeof(WCHAR));
    memcpy(v->name,leaf,(n+1)*sizeof(WCHAR));
    st=f(source,&io,v,(ULONG)size,WC_NT_RENAME);
    WC_TRACE("wr_replace_name leaf=%ls NTSTATUS=0x%08lx\n",leaf,(unsigned long)st);
    free(v);return NT_SUCCESS(st);
}
static int wc_delete(HANDLE h)
{
    FILE_DISPOSITION_INFO d;
    d.DeleteFile=TRUE;
    BOOL ok=SetFileInformationByHandle(h,FileDispositionInfo,&d,sizeof(d));
    WC_TRACE("wc_delete SetFileInformationByHandle ok=%d error=%lu\n",
             (int)ok,(unsigned long)(ok?0:GetLastError()));
    return ok!=FALSE;
}
static FS_READ_STATUS wc_lookup(HANDLE dir,const WCHAR *name,const FILE_ID_INFO *id,int *exists)
{
    HANDLE h=INVALID_HANDLE_VALUE;FILE_ID_INFO found;FS_READ_META m;
    FS_READ_STATUS s=wc_open(dir,name,FILE_READ_ATTRIBUTES,FILE_OPEN,&h);
    /* The caller never treats a missing name as a matching identity. */
    if(s==FS_READ_MISSING){*exists=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    s=win_meta(h,&m);
    if(s!=FS_READ_OK||m.kind!=FS_KIND_FILE||!wc_id(h,&found)||!wc_same(&found,id))s=FS_READ_DENIED;
    else {*exists=1;s=FS_READ_OK;}
    CloseHandle(h);return s;
}
static FS_READ_STATUS wc_remove(HANDLE dir,const WCHAR *name,const FILE_ID_INFO *id)
{
    HANDLE h=INVALID_HANDLE_VALUE;FILE_ID_INFO found;FS_READ_META m;
    FS_READ_STATUS s=wc_open(dir,name,DELETE,FILE_OPEN,&h);
    if(s==FS_READ_MISSING)return FS_READ_OK;
    if(s!=FS_READ_OK)return s;
    s=win_meta(h,&m);
    if(s!=FS_READ_OK||m.kind!=FS_KIND_FILE||!wc_id(h,&found)||!wc_same(&found,id))s=FS_READ_DENIED;
    else if(!wc_delete(h))s=FS_READ_PENDING;
    else s=FS_READ_OK;
    CloseHandle(h);return s;
}
/* Remove a replace pin. A pin is a hard link to the original file, and the
   READONLY attribute lives on the file, not on the link, so a pin of a
   read-only target cannot be deleted (access denied) and would stay
   behind as an orphan that makes the target 2-link. Clear READONLY only for
   the delete, then restore the original attributes on the same handle so the
   target keeps its attribute. Used for replace pins and for the remove and
   move recovery links (the journal pin and the target link of a rolled back
   move); never for user files. */
static FS_READ_STATUS wr_remove_pin(HANDLE dir,const WCHAR *name,const FILE_ID_INFO *id)
{
    HANDLE h=INVALID_HANDLE_VALUE;FILE_ID_INFO found;FS_READ_META m;
    FILE_BASIC_INFO basic,orig;int cleared=0;
    FS_READ_STATUS s=wc_open(dir,name,DELETE|FILE_WRITE_ATTRIBUTES,FILE_OPEN,&h);
    if(s==FS_READ_MISSING)return FS_READ_OK;
    if(s!=FS_READ_OK)return s;
    s=win_meta(h,&m);
    if(s!=FS_READ_OK||m.kind!=FS_KIND_FILE||!wc_id(h,&found)||!wc_same(&found,id)){
        s=FS_READ_DENIED;goto done;}
    if(wc_delete(h)){s=FS_READ_OK;goto done;}
    if(!GetFileInformationByHandleEx(h,FileBasicInfo,&orig,sizeof(orig))||
       !(orig.FileAttributes&FILE_ATTRIBUTE_READONLY)){s=FS_READ_PENDING;goto done;}
    basic=orig;
    basic.FileAttributes=orig.FileAttributes&~(DWORD)FILE_ATTRIBUTE_READONLY;
    if(!basic.FileAttributes)basic.FileAttributes=FILE_ATTRIBUTE_NORMAL;
    cleared=SetFileInformationByHandle(h,FileBasicInfo,&basic,sizeof(basic))!=FALSE;
    WC_TRACE("wr_remove_pin clear READONLY ok=%d\n",cleared);
    if(!cleared){s=FS_READ_PENDING;goto done;}
    if(wc_delete(h))s=FS_READ_OK;else s=FS_READ_PENDING;
    /* The attribute belongs to the file other names still reach: put it back. */
    if(!SetFileInformationByHandle(h,FileBasicInfo,&orig,sizeof(orig))){
        WC_TRACE("wr_remove_pin restore READONLY failed error=%lu\n",(unsigned long)GetLastError());
        if(s==FS_READ_OK)s=FS_READ_IO;
    }
done:
    CloseHandle(h);return s;
}
static int wc_random(WCHAR out[48],const WCHAR *prefix)
{
    unsigned char b[16];size_t n=wcslen(prefix);
    HMODULE dll=LoadLibraryW(L"advapi32.dll");
    typedef BOOLEAN (APIENTRY *WC_RANDOM)(PVOID,ULONG);
    WC_RANDOM f=dll?(WC_RANDOM)(void*)GetProcAddress(dll,"SystemFunction036"):NULL;
    int ok=n+32<48&&f&&f(b,sizeof(b));
    WC_TRACE("wc_random LoadLibrary=%d SystemFunction036=%d result=%d error=%lu\n",
             dll!=NULL,f!=NULL,ok,(unsigned long)(ok?0:GetLastError()));
    if(dll)FreeLibrary(dll);
    if(!ok)return 0;
    wcscpy(out,prefix);
    for(size_t i=0;i<sizeof(b);i++)swprintf(out+n+2*i,3,L"%02x",(unsigned)b[i]);
    return 1;
}
static void wc_probe_dir(HANDLE dir)
{
    /* Diagnostic only: no success or durability claim depends on this call. */
    if(!FlushFileBuffers(dir)){
#ifdef FS_CREATE_TEST_CRASH
        fprintf(stderr,"NTFS directory FlushFileBuffers: failed GetLastError=%lu (diagnostic)\n",GetLastError());
#endif
    }else{
#ifdef FS_CREATE_TEST_CRASH
        fprintf(stderr,"NTFS directory FlushFileBuffers: succeeded GetLastError=0 (diagnostic)\n");
#endif
    }
}
#ifdef FS_CREATE_TEST_CRASH
static void wc_crash(int point)
{
    const char *v=getenv("FS_WIN_JOURNAL_CRASH");
    if(v&&atoi(v)==point)ExitProcess((UINT)(80+point));
}
#else
#define wc_crash(n) ((void)0)
#endif
static FS_READ_STATUS wc_record_read(HANDLE root,const WCHAR *name,WC_RECORD *r,FILE_ID_INFO *record_id,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;DWORD n=0;FS_READ_META m;
    FS_READ_STATUS s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=sizeof(*r)||
       !wc_id(h,record_id)||!ReadFile(h,r,sizeof(*r),&n,NULL)||n!=sizeof(*r)||
       r->magic!=WC_MAGIC||r->version!=1||r->reserved||
       r->checksum!=wc_checksum(r)||
       !wc_terminated(r->target,WC_MAX_PATH)||
       !wc_terminated(r->stage,48)||!wc_terminated(r->pin,48)||
       wc_target_status(r->target)!=FS_READ_OK||!wcscmp(r->stage,r->pin)||
       !wc_random_name(r->stage,L".fst-")||
       !wc_random_name(r->pin,L".fsp-")||
       r->size>FS_READ_MAX||r->mode>0777){s=FS_READ_DENIED;}
    else s=FS_READ_OK;
    CloseHandle(h);return s;
}
static FS_READ_STATUS wo_publish(HANDLE root,const WCHAR *final,const WO_RECORD *r)
{
    HANDLE h=INVALID_HANDLE_VALUE;WCHAR tmp[48];DWORD written=0;FS_READ_STATUS s;
    if(!wc_random(tmp,L".fsj-"))return FS_READ_IO;
    s=wc_open(root,tmp,GENERIC_WRITE|DELETE,FILE_CREATE,&h);if(s!=FS_READ_OK)return s;
    if(!WriteFile(h,r,sizeof(*r),&written,NULL)||written!=sizeof(*r)||
       !FlushFileBuffers(h)){s=FS_READ_IO;goto done;}
    if(!wc_set_name(h,root,final,WC_NT_RENAME)){s=FS_READ_PENDING;goto done;}
    wc_probe_dir(root);s=FS_READ_OK;
done:CloseHandle(h);return s;
}
static FS_READ_STATUS wo_read(HANDLE root,const WCHAR *name,unsigned kind,
                             WO_RECORD *r,FILE_ID_INFO *record_id,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;DWORD n=0;FS_READ_META m;
    FS_READ_STATUS s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=sizeof(*r)||
       !wc_id(h,record_id)||!ReadFile(h,r,sizeof(*r),&n,NULL)||n!=sizeof(*r)||
       r->magic!=WO_MAGIC||r->version!=1||r->kind!=kind||
       r->checksum!=wo_checksum(r)||r->size>FS_READ_MAX||
       !wc_terminated(r->source,WC_MAX_PATH)||!wc_terminated(r->target,WC_MAX_PATH)||
       !wc_terminated(r->pin,48)||!wc_random_name(r->pin,kind==1?L".fsrm-":L".fsmv-")||
       wc_target_status(r->source)!=FS_READ_OK||
       (kind==2?(wc_target_status(r->target)!=FS_READ_OK||
                  !_wcsicmp(r->source,r->target)):r->target[0]!=0))s=FS_READ_DENIED;
    else s=FS_READ_OK;
    CloseHandle(h);return s;
}
static FS_READ_STATUS wo_bytes(HANDLE h,const FILE_ID_INFO *id,size_t size,unsigned long long digest);
static FS_READ_STATUS wr_publish(HANDLE root,const WCHAR *final,const WR_RECORD *r)
{
    HANDLE h=INVALID_HANDLE_VALUE;WCHAR tmp[48];DWORD written=0;FS_READ_STATUS s;
    if(!wc_random(tmp,L".fsj-"))return FS_READ_IO;
    s=wc_open(root,tmp,GENERIC_WRITE|DELETE,FILE_CREATE,&h);if(s!=FS_READ_OK)return s;
    if(!WriteFile(h,r,sizeof(*r),&written,NULL)||written!=sizeof(*r)||
       !FlushFileBuffers(h)){s=FS_READ_IO;goto done;}
    if(!wc_set_name(h,root,final,WC_NT_RENAME)){s=FS_READ_PENDING;goto done;}
    wc_probe_dir(root);s=FS_READ_OK;
done:CloseHandle(h);return s;
}
static FS_READ_STATUS wr_read(HANDLE root,const WCHAR *name,WR_RECORD *r,
                             FILE_ID_INFO *record_id,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;DWORD n=0;FS_READ_META m;
    FS_READ_STATUS s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=sizeof(*r)||
       !wc_id(h,record_id)||!ReadFile(h,r,sizeof(*r),&n,NULL)||n!=sizeof(*r)||
       r->magic!=WR_MAGIC||r->version!=1||r->reserved||
       r->checksum!=wr_checksum(r)||r->old_size>FS_READ_MAX||r->new_size>FS_READ_MAX||
       !wc_terminated(r->target,WC_MAX_PATH)||
       !wc_terminated(r->old_pin,48)||!wc_terminated(r->new_stage,48)||
       !wc_terminated(r->new_pin,48)||!wc_terminated(r->rollback,48)||
       wc_target_status(r->target)!=FS_READ_OK||
       !wc_random_name(r->old_pin,L".fsrp-")||
       !(wc_random_name(r->new_stage,L".fsrs-")||wc_random_name(r->new_stage,L".fsrp-"))||
       !wc_random_name(r->new_pin,L".fsrp-")||
       !wc_random_name(r->rollback,L".fsrb-")||
       !wcscmp(r->old_pin,r->new_pin)||!wcscmp(r->old_pin,r->new_stage)||
       !wcscmp(r->new_pin,r->new_stage)||
       wc_same(&r->old_id,&r->new_id))s=FS_READ_DENIED;
    else s=FS_READ_OK;
    CloseHandle(h);return s;
}
#ifdef FS_CREATE_TEST_CRASH
static void wr_crash(int phase)
{
    const char *v=getenv("FS_WIN_REPLACE_CRASH");
    if(v&&atoi(v)==phase)ExitProcess((UINT)(140+phase));
}
#else
#define wr_crash(p) ((void)0)
#endif
static FS_READ_STATUS wr_named(HANDLE dir,const WCHAR *leaf,const FILE_ID_INFO *id,
                               int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_META m;FILE_ID_INFO found;
    FS_READ_STATUS s=wc_open(dir,leaf,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    s=(win_meta(h,&m)==FS_READ_OK&&m.kind==FS_KIND_FILE&&
       wc_id(h,&found)&&wc_same(&found,id))?FS_READ_OK:FS_READ_DENIED;
    CloseHandle(h);return s;
}
static FS_READ_STATUS wr_verify(HANDLE dir,const WCHAR *leaf,
       const FILE_ID_INFO *id,size_t len,unsigned long long digest)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_STATUS s=wc_open(dir,leaf,GENERIC_READ,FILE_OPEN,&h);
    if(s!=FS_READ_OK)return s;
    s=wo_bytes(h,id,len,digest);CloseHandle(h);return s;
}
static FS_READ_STATUS wc_record_publish(HANDLE root,const WCHAR *final,const WC_RECORD *r)
{
    HANDLE h=INVALID_HANDLE_VALUE;WCHAR tmp[48];DWORD written=0;FS_READ_STATUS s;
    if(!wc_random(tmp,L".fsj-"))return FS_READ_IO;
    s=wc_open(root,tmp,GENERIC_WRITE|DELETE,FILE_CREATE,&h);if(s!=FS_READ_OK)return s;
    if(!WriteFile(h,r,sizeof(*r),&written,NULL)||written!=sizeof(*r)||
       !FlushFileBuffers(h)){s=FS_READ_IO;goto done;}
    if(!wc_set_name(h,root,final,WC_NT_RENAME)){s=FS_READ_DENIED;goto done;}
    wc_probe_dir(root);s=FS_READ_OK;
done:
    /* An uncertain rename result may have published the record. Leave its
       random source name for forensic cleanup, never delete its handle. */
    CloseHandle(h);return s;
}
static FS_READ_STATUS wc_lock(HANDLE root,HANDLE *out,OVERLAPPED *overlap)
{
    FS_READ_STATUS s=wc_open(root,WC_LOCK,GENERIC_READ|GENERIC_WRITE,FILE_OPEN_IF,out);
    FS_READ_META m;
    if(s!=FS_READ_OK)return s;
    if(win_meta(*out,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=0){s=FS_READ_DENIED;goto bad;}
    memset(overlap,0,sizeof(*overlap));
    {BOOL ok=LockFileEx(*out,LOCKFILE_EXCLUSIVE_LOCK,0,1,0,overlap);
     WC_TRACE("wc_lock LockFileEx ok=%d error=%lu\n",(int)ok,
              (unsigned long)(ok?0:GetLastError()));
     if(!ok){s=FS_READ_IO;goto bad;}}
    return FS_READ_OK;
bad:CloseHandle(*out);*out=INVALID_HANDLE_VALUE;return s;
}
static void wc_unlock(HANDLE h,OVERLAPPED *o)
{if(h!=INVALID_HANDLE_VALUE){UnlockFileEx(h,0,1,0,o);CloseHandle(h);}}
static FS_READ_STATUS wc_parent(const FS_READ_ROOT *r,const WCHAR *target,HANDLE *dir,const WCHAR **leaf)
{
    WCHAR path[WC_MAX_PATH];WCHAR *slash;char utf8[WC_MAX_PATH*3];int n;
    size_t length=wcslen(target);if(length>=WC_MAX_PATH)return FS_READ_DENIED;
    wcscpy(path,target);slash=wcsrchr(path,L'/');
    if(slash){*slash=0;*leaf=target+(slash-path)+1;}
    else {*path=0;*leaf=target;}
    if(!**leaf||wcslen(*leaf)>240)return FS_READ_DENIED;
    n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,path,-1,utf8,sizeof(utf8),NULL,NULL);
    if(!n||!valid_relative(utf8,1))return FS_READ_DENIED;
    {FS_READ_STATUS s=win_open(r,utf8,dir);
#ifndef FS_PARENT_MUTANT
     /* win_open opens the last component without FILE_DIRECTORY_FILE, so a
        regular file would come back as the "parent". Refuse it here, before
        any stage, pin or intent exists. Test-only build FS_PARENT_MUTANT drops
        this check (tests/test_fs_win_fileparent.c); never defined in
        production targets. */
     if(s==FS_READ_OK){FS_READ_META m;
        if(win_meta(*dir,&m)!=FS_READ_OK||m.kind!=FS_KIND_DIR){
            CloseHandle(*dir);*dir=INVALID_HANDLE_VALUE;return FS_READ_DENIED;}}
#endif
     return s;}
}
/* All operation families share one workspace lock. An unknown control name
   blocks rather than having its content interpreted by an unrelated writer. */
static FS_READ_STATUS wo_clear_other(HANDLE root,unsigned self)
{
    const WCHAR *names[]={WC_INTENT,WC_COMMIT,WO_REMOVE,WO_RCOMMIT,
        WO_MOVE,WO_MCOMMIT,L".fstxn.batch",L".fstxn.commit",
        WR_INTENT,WR_COMMIT};
    for(unsigned k=0;k<sizeof(names)/sizeof(names[0]);k++){
        HANDLE h=INVALID_HANDLE_VALUE;FS_READ_STATUS s;
        if((self==0&&k<2)||(self==1&&k>=2&&k<4)||
           (self==2&&k>=4&&k<6)||(self==3&&k>=8&&k<10)||
           (self==4&&k>=6&&k<8))continue;
        s=wc_open(root,names[k],FILE_READ_ATTRIBUTES,FILE_OPEN,&h);
        if(s==FS_READ_OK){CloseHandle(h);return FS_READ_DENIED;}
        if(s!=FS_READ_MISSING)return FS_READ_DENIED;
    }
    return FS_READ_OK;
}
static FS_READ_STATUS wo_bytes(HANDLE h,const FILE_ID_INFO *id,
                              size_t size,unsigned long long digest)
{
    FILE_ID_INFO check;FS_READ_META m;LARGE_INTEGER zero;unsigned char buf[8192];
    DWORD got;size_t total=0;unsigned long long hash=14695981039346656037ULL;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=size||
       !wc_id(h,&check)||!wc_same(&check,id))return FS_READ_DENIED;
    zero.QuadPart=0;if(!SetFilePointerEx(h,zero,NULL,FILE_BEGIN))return FS_READ_DENIED;
    do {
        if(!ReadFile(h,buf,sizeof(buf),&got,NULL))return FS_READ_DENIED;
        if(total+got>FS_READ_MAX)return FS_READ_DENIED;
        for(DWORD k=0;k<got;k++)hash=(hash^buf[k])*1099511628211ULL;
        total+=got;
    }while(got);
    return total==size&&hash==digest?FS_READ_OK:FS_READ_DENIED;
}
static FS_READ_STATUS wo_named(HANDLE dir,const WCHAR *leaf,const FILE_ID_INFO *id,
                               int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_META m;FILE_ID_INFO found;
    FS_READ_STATUS s=wc_open(dir,leaf,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    s=(win_meta(h,&m)==FS_READ_OK&&m.kind==FS_KIND_FILE&&
       wc_id(h,&found)&&wc_same(&found,id))?FS_READ_OK:FS_READ_DENIED;
    CloseHandle(h);return s;
}
#ifdef FS_CREATE_TEST_CRASH
static void wo_crash(unsigned kind,int phase)
{
    const char *v=getenv("FS_WIN_OP_CRASH");
    if(v&&atoi(v)==(int)(kind*10+phase))ExitProcess((UINT)(100+kind*10+phase));
}
#else
#define wo_crash(k,p) ((void)0)
#endif
/* Remove/move recovery deletes its pin and, when rolling a move back, the
   target link (both hard links to the user's file) with the READONLY-aware
   helper, as the replace path does. FS_PIN_MUTANT switches both sites back to
   the plain delete, which a READONLY file refuses (mutant MC4). */
#ifdef FS_PIN_MUTANT
#define wo_remove_pin wc_remove
#else
#define wo_remove_pin wr_remove_pin
#endif
static FS_READ_STATUS wo_recover_locked(const FS_READ_ROOT *r,unsigned kind)
{
    const WCHAR *intent_name=kind==1?WO_REMOVE:WO_MOVE;
    const WCHAR *mark_name=kind==1?WO_RCOMMIT:WO_MCOMMIT;
    WO_RECORD i,c;FILE_ID_INFO iid,cid,rid,pid,tid;
    HANDLE source_dir=INVALID_HANDLE_VALUE,target_dir=INVALID_HANDLE_VALUE;
    HANDLE pin_h=INVALID_HANDLE_VALUE,target_h=INVALID_HANDLE_VALUE;
    const WCHAR *source_leaf=NULL,*target_leaf=NULL;
    int intent=0,mark=0,pin=0,source=0,target=0;
    FS_READ_STATUS s=wo_clear_other(r->handle,kind);
    if(s!=FS_READ_OK)return s;
    s=wo_read(r->handle,intent_name,kind,&i,&iid,&intent);if(s!=FS_READ_OK)return s;
    s=wo_read(r->handle,mark_name,kind,&c,&cid,&mark);if(s!=FS_READ_OK)return s;
    if(!intent&&!mark)return FS_READ_OK;
    if(intent&&mark&&memcmp(&i,&c,sizeof(i)))return FS_READ_DENIED;
    if(!intent)i=c;
    if(!wc_id(r->handle,&rid)||!wc_same(&rid,&i.root)||!wc_ntfs(r->handle))return FS_READ_DENIED;
    s=wc_parent(r,i.source,&source_dir,&source_leaf);if(s!=FS_READ_OK)goto done;
    if(!wc_id(source_dir,&pid)||!wc_same(&pid,&i.source_parent)||
       pid.VolumeSerialNumber!=rid.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
    if(kind==2){
        s=wc_parent(r,i.target,&target_dir,&target_leaf);if(s!=FS_READ_OK)goto done;
        if(!wc_id(target_dir,&tid)||!wc_same(&tid,&i.target_parent)||
           tid.VolumeSerialNumber!=rid.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
    }
    s=wo_named(source_dir,source_leaf,&i.file,&source);if(s!=FS_READ_OK)goto done;
    if(kind==2){s=wo_named(target_dir,target_leaf,&i.file,&target);if(s!=FS_READ_OK)goto done;}
    s=wo_named(r->handle,i.pin,&i.file,&pin);if(s!=FS_READ_OK)goto done;
    if(!pin){s=FS_READ_DENIED;goto done;}
    s=wc_open(r->handle,i.pin,GENERIC_READ|DELETE,FILE_OPEN,&pin_h);
    if(s!=FS_READ_OK)goto done;
    s=wo_bytes(pin_h,&i.file,(size_t)i.size,i.digest);if(s!=FS_READ_OK)goto done;
    {FILE_STANDARD_INFO links;
     unsigned expected_links=1u+(unsigned)source+(unsigned)target;
     if(!GetFileInformationByHandleEx(pin_h,FileStandardInfo,&links,sizeof(links))||
        links.NumberOfLinks!=expected_links){s=FS_READ_DENIED;goto done;}}

    if(mark){
        if(source||(kind==2&&!target)){s=FS_READ_DENIED;goto done;}
        if(kind==2){
            s=wc_open(target_dir,target_leaf,GENERIC_READ,FILE_OPEN,&target_h);
            if(s!=FS_READ_OK)goto done;
            s=wo_bytes(target_h,&i.file,(size_t)i.size,i.digest);
            if(s!=FS_READ_OK)goto done;
        }
        if(intent){s=wc_remove(r->handle,intent_name,&iid);if(s!=FS_READ_OK)goto done;}
        wo_crash(kind,9); /* marker-only, with verified pin still present */
        /* Keep pin through marker retirement: marker-only remove still has
           the original bytes to verify. A crash after marker retirement
           leaves an untracked, never-auto-deleted pin. */
        s=wc_remove(r->handle,mark_name,&cid);if(s!=FS_READ_OK)goto done;
        wo_crash(kind,5);
        CloseHandle(pin_h);pin_h=INVALID_HANDLE_VALUE;
        s=wo_remove_pin(r->handle,i.pin,&i.file);
    }else{
        if(!source){
            if(!wc_set_name(pin_h,source_dir,source_leaf,WC_NT_LINK)){s=FS_READ_PENDING;goto done;}
            wo_crash(kind,6);
            s=wo_named(source_dir,source_leaf,&i.file,&source);
            if(s!=FS_READ_OK||!source){s=FS_READ_DENIED;goto done;}
        }
        if(target){
            s=wc_open(target_dir,target_leaf,GENERIC_READ,FILE_OPEN,&target_h);
            if(s!=FS_READ_OK)goto done;
            s=wo_bytes(target_h,&i.file,(size_t)i.size,i.digest);if(s!=FS_READ_OK)goto done;
            CloseHandle(target_h);target_h=INVALID_HANDLE_VALUE;
            s=wo_remove_pin(target_dir,target_leaf,&i.file);if(s!=FS_READ_OK)goto done;
        }
        wc_probe_dir(source_dir);if(kind==2)wc_probe_dir(target_dir);
        wo_crash(kind,7);
        s=wc_remove(r->handle,intent_name,&iid);if(s!=FS_READ_OK)goto done;
        wo_crash(kind,8);
        CloseHandle(pin_h);pin_h=INVALID_HANDLE_VALUE;
        s=wo_remove_pin(r->handle,i.pin,&i.file);
    }
    wc_probe_dir(r->handle);
done:
    if(target_h!=INVALID_HANDLE_VALUE)CloseHandle(target_h);
    if(pin_h!=INVALID_HANDLE_VALUE)CloseHandle(pin_h);
    if(target_dir!=INVALID_HANDLE_VALUE)CloseHandle(target_dir);
    if(source_dir!=INVALID_HANDLE_VALUE)CloseHandle(source_dir);
    return s;
}
/* Defined in fs_batch_win.inc. With no replace intent and no marker, a writer
   killed before its intent was written (phases 0-2) can have left .fsrp- pins and a .fsrs- stage;
   the same exact-shape sweep as the batch calls removes them (shared names only,
   under the workspace lock, no journal of any protocol). */
static void wb_sweep_orphans(const FS_READ_ROOT *r);
static FS_READ_STATUS wr_recover_locked(const FS_READ_ROOT *r)
{
    WR_RECORD i,c;FILE_ID_INFO iid,cid,rid,pid;
    HANDLE parent=INVALID_HANDLE_VALUE,old_h=INVALID_HANDLE_VALUE;
    const WCHAR *leaf=NULL;int intent=0,mark=0,oldpin=0,newpin=0,stage=0,rb=0;
    int target_old=0,target_new=0;FS_READ_STATUS s=wo_clear_other(r->handle,3);
    if(s!=FS_READ_OK)return s;
    s=wr_read(r->handle,WR_INTENT,&i,&iid,&intent);if(s!=FS_READ_OK)return s;
    s=wr_read(r->handle,WR_COMMIT,&c,&cid,&mark);if(s!=FS_READ_OK)return s;
    if(!intent&&!mark){wb_sweep_orphans(r);return FS_READ_OK;}
    if(intent&&mark&&memcmp(&i,&c,sizeof(i)))return FS_READ_DENIED;
    if(!intent)i=c;
    if(!wc_id(r->handle,&rid)||!wc_same(&rid,&i.root)||!wc_ntfs(r->handle))return FS_READ_DENIED;
    s=wc_parent(r,i.target,&parent,&leaf);if(s!=FS_READ_OK)goto done;
    if(!wc_id(parent,&pid)||!wc_same(&pid,&i.parent)||
       pid.VolumeSerialNumber!=rid.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
    s=wr_named(r->handle,i.old_pin,&i.old_id,&oldpin);if(s!=FS_READ_OK)goto done;
    s=wr_named(r->handle,i.new_pin,&i.new_id,&newpin);if(s!=FS_READ_OK)goto done;
    s=wr_named(r->handle,i.new_stage,&i.new_id,&stage);if(s!=FS_READ_OK)goto done;
    s=wr_named(parent,i.rollback,&i.old_id,&rb);if(s!=FS_READ_OK)goto done;
    if(!oldpin||!newpin){s=FS_READ_DENIED;goto done;}
    s=wr_verify(r->handle,i.old_pin,&i.old_id,(size_t)i.old_size,i.old_digest);
    if(s!=FS_READ_OK)goto done;
    s=wr_verify(r->handle,i.new_pin,&i.new_id,(size_t)i.new_size,i.new_digest);
    if(s!=FS_READ_OK)goto done;
    if(stage){s=wr_verify(r->handle,i.new_stage,&i.new_id,(size_t)i.new_size,i.new_digest);
       if(s!=FS_READ_OK)goto done;}
    if(rb){s=wr_verify(parent,i.rollback,&i.old_id,(size_t)i.old_size,i.old_digest);
       if(s!=FS_READ_OK)goto done;}
    {HANDLE target=INVALID_HANDLE_VALUE;FILE_ID_INFO found;FS_READ_META m;
     s=wc_open(parent,leaf,GENERIC_READ,FILE_OPEN,&target);
     if(s!=FS_READ_OK)goto done;
     if(win_meta(target,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||!wc_id(target,&found)){
         CloseHandle(target);s=FS_READ_DENIED;goto done;}
     target_old=wc_same(&found,&i.old_id);target_new=wc_same(&found,&i.new_id);
     CloseHandle(target);
     if(!target_old&&!target_new){s=FS_READ_DENIED;goto done;}}
    {HANDLE h=INVALID_HANDLE_VALUE;FILE_STANDARD_INFO links;
     s=wc_open(r->handle,i.old_pin,GENERIC_READ,FILE_OPEN,&h);if(s!=FS_READ_OK)goto done;
     if(!GetFileInformationByHandleEx(h,FileStandardInfo,&links,sizeof(links))||
        links.NumberOfLinks!=(DWORD)(1+target_old+rb)){
        CloseHandle(h);s=FS_READ_DENIED;goto done;}
     CloseHandle(h);h=INVALID_HANDLE_VALUE;
     s=wc_open(r->handle,i.new_pin,GENERIC_READ,FILE_OPEN,&h);if(s!=FS_READ_OK)goto done;
     if(!GetFileInformationByHandleEx(h,FileStandardInfo,&links,sizeof(links))||
        links.NumberOfLinks!=(DWORD)(1+stage+target_new)){
        CloseHandle(h);s=FS_READ_DENIED;goto done;}
     CloseHandle(h);
    }
    if(mark){
        if(!target_new||rb){s=FS_READ_DENIED;goto done;}
        s=wr_verify(parent,leaf,&i.new_id,(size_t)i.new_size,i.new_digest);
        if(s!=FS_READ_OK)goto done;
        if(intent){s=wc_remove(r->handle,WR_INTENT,&iid);if(s!=FS_READ_OK)goto done;}
        wr_crash(9); /* marker only, both pins remain */
        if(stage){s=wc_remove(r->handle,i.new_stage,&i.new_id);if(s!=FS_READ_OK)goto done;}
        s=wc_remove(r->handle,WR_COMMIT,&cid);if(s!=FS_READ_OK)goto done;
        wr_crash(10); /* no journal, old/new pins still available as orphans */
    }else{
        if(target_new){
            s=wr_verify(parent,leaf,&i.new_id,(size_t)i.new_size,i.new_digest);
            if(s!=FS_READ_OK)goto done;
            if(!rb){
                s=wc_open(r->handle,i.old_pin,GENERIC_READ,FILE_OPEN,&old_h);
                if(s!=FS_READ_OK)goto done;
                if(!wc_set_name(old_h,parent,i.rollback,WC_NT_LINK)){
                    s=FS_READ_PENDING;goto done;}
                CloseHandle(old_h);old_h=INVALID_HANDLE_VALUE;
                wr_crash(7); /* rollback temp old, target new */
                s=wr_verify(parent,i.rollback,&i.old_id,(size_t)i.old_size,i.old_digest);
                if(s!=FS_READ_OK)goto done;
            }
            s=wr_verify(parent,leaf,&i.new_id,(size_t)i.new_size,i.new_digest);
            if(s!=FS_READ_OK)goto done;
            s=wc_open(parent,i.rollback,DELETE,FILE_OPEN,&old_h);
            if(s!=FS_READ_OK)goto done;
            if(!wr_replace_name(old_h,parent,leaf)){s=FS_READ_PENDING;goto done;}
            CloseHandle(old_h);old_h=INVALID_HANDLE_VALUE;
            wr_crash(8); /* old target restored, intent remains */
        }else if(rb){
            /* A crash after the rename may leave an old-name alias only if
               another actor intervened. Never discard it without identity. */
            s=FS_READ_DENIED;goto done;
        }
        s=wr_verify(parent,leaf,&i.old_id,(size_t)i.old_size,i.old_digest);
        if(s!=FS_READ_OK)goto done;
        wc_probe_dir(parent);
        if(intent){s=wc_remove(r->handle,WR_INTENT,&iid);if(s!=FS_READ_OK)goto done;}
        if(stage){s=wc_remove(r->handle,i.new_stage,&i.new_id);if(s!=FS_READ_OK)goto done;}
    }
    /* Both pins survive as long as a checked record exists. An interruption
       after record retirement can leave old bytes in a manual-cleanup pin. */
    s=wr_remove_pin(r->handle,i.old_pin,&i.old_id);if(s!=FS_READ_OK)goto done;
    s=wr_remove_pin(r->handle,i.new_pin,&i.new_id);
    wc_probe_dir(r->handle);
done:
    if(old_h!=INVALID_HANDLE_VALUE)CloseHandle(old_h);
    if(parent!=INVALID_HANDLE_VALUE)CloseHandle(parent);
    return s;
}
static FS_READ_STATUS wc_recover_locked(const FS_READ_ROOT *r)
{
    WC_RECORD i,c;FILE_ID_INFO iid,cid,rid,pid;HANDLE parent=INVALID_HANDLE_VALUE;
    const WCHAR *leaf;int intent=0,commit=0,stage=0,pin=0,target=0;
    FS_READ_STATUS s=wc_record_read(r->handle,WC_INTENT,&i,&iid,&intent);
    if(s!=FS_READ_OK)return s;
    s=wc_record_read(r->handle,WC_COMMIT,&c,&cid,&commit);
    if(s!=FS_READ_OK||(!intent&&!commit))return s;
    if(commit&&intent&&memcmp(&i,&c,sizeof(i)))return FS_READ_DENIED;
    if(!intent)i=c;
    if(!wc_id(r->handle,&rid)||!wc_same(&rid,&i.root)||
       !wc_ntfs(r->handle))return FS_READ_DENIED;
    s=wc_parent(r,i.target,&parent,&leaf);if(s!=FS_READ_OK)return s;
    if(!wc_id(parent,&pid)||!wc_same(&pid,&i.parent)||
       pid.VolumeSerialNumber!=i.root.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
    s=wc_lookup(r->handle,i.pin,&i.file,&pin);if(s!=FS_READ_OK)goto done;
    s=wc_lookup(r->handle,i.stage,&i.file,&stage);if(s!=FS_READ_OK)goto done;
    s=wc_lookup(parent,leaf,&i.file,&target);if(s!=FS_READ_OK)goto done;
    /* A pinned hard link protects identity, not bytes from in-place edits. */
    if(pin){
        HANDLE h=INVALID_HANDLE_VALUE;unsigned char buf[8192];DWORD got;size_t total=0;
        unsigned long long digest=14695981039346656037ULL;
        s=wc_open(r->handle,i.pin,GENERIC_READ,FILE_OPEN,&h);if(s!=FS_READ_OK)goto done;
        FILE_ID_INFO id;
        if(!wc_id(h,&id)||!wc_same(&id,&i.file)){CloseHandle(h);s=FS_READ_DENIED;goto done;}
        {BOOL ok=TRUE;
         do {ok=ReadFile(h,buf,sizeof(buf),&got,NULL);
             if(!ok)break;
             for(DWORD k=0;k<got;k++)digest=(digest^buf[k])*1099511628211ULL;
             total+=got;
         }while(got&&total<=FS_READ_MAX);
         if(!ok){CloseHandle(h);s=FS_READ_DENIED;goto done;}
        }
        CloseHandle(h);
        if(total!=i.size||digest!=i.digest){s=FS_READ_DENIED;goto done;}
    }
    if(!commit&&!pin){s=FS_READ_DENIED;goto done;}
    if(intent&&!commit&&target)wc_crash(8);
    if(commit&&!target){s=FS_READ_DENIED;goto done;}
    if(!commit&&target){s=wc_remove(parent,leaf,&i.file);if(s!=FS_READ_OK)goto done;wc_probe_dir(parent);wc_crash(9);}
    if(stage){s=wc_remove(r->handle,i.stage,&i.file);if(s!=FS_READ_OK)goto done;}
    if(commit){
        if(pin){s=wc_remove(r->handle,i.pin,&i.file);if(s!=FS_READ_OK)goto done;}
        /* Set readonly only after removing all hard links. Its attribute
           belongs to the inode, so setting it earlier can block cleanup. */
        if(commit&&i.mode<=0777){
            HANDLE t=INVALID_HANDLE_VALUE;FILE_ID_INFO check;
            s=wc_open(parent,leaf,FILE_WRITE_ATTRIBUTES,FILE_OPEN,&t);
            if(s!=FS_READ_OK)goto done;
            if(!wc_id(t,&check)||!wc_same(&check,&i.file)){CloseHandle(t);s=FS_READ_DENIED;goto done;}
            if(!(i.mode&0222)){FILE_BASIC_INFO basic;
                if(!GetFileInformationByHandleEx(t,FileBasicInfo,&basic,sizeof(basic))){CloseHandle(t);s=FS_READ_IO;goto done;}
                basic.FileAttributes|=FILE_ATTRIBUTE_READONLY;
                if(!SetFileInformationByHandle(t,FileBasicInfo,&basic,sizeof(basic))){CloseHandle(t);s=FS_READ_PENDING;goto done;}
            }CloseHandle(t);
        }
        wc_crash(10);wc_probe_dir(r->handle);
        if(intent){s=wc_remove(r->handle,WC_INTENT,&iid);if(s!=FS_READ_OK)goto done;}
        wc_crash(11);
        /* The marker owns the recovery record until its very last deletion. */
        s=wc_lookup(parent,leaf,&i.file,&target);if(s!=FS_READ_OK||!target){s=FS_READ_DENIED;goto done;}
        s=wc_remove(r->handle,WC_COMMIT,&cid);if(s!=FS_READ_OK)goto done;
    }else{
        /* Keep the pin until the intent is gone: another process can always
           replay a crash before intent deletion, then orphan cleanup is manual. */
        wc_crash(12);
        s=wc_remove(r->handle,WC_INTENT,&iid);if(s!=FS_READ_OK)goto done;
        wc_crash(13);
        if(pin){s=wc_remove(r->handle,i.pin,&i.file);if(s!=FS_READ_OK)goto done;}
    }
    wc_probe_dir(r->handle);s=FS_READ_OK;
done:CloseHandle(parent);return s;
}
FS_READ_STATUS FsCreateRecover(const FS_READ_ROOT *r)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED o;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    s=wc_lock(r->handle,&lock,&o);if(s!=FS_READ_OK)return s;
    s=wo_clear_other(r->handle,0);if(s==FS_READ_OK)s=wc_recover_locked(r);
    wc_unlock(lock,&o);return s;
}
/* Source verification is repeated at the last gate before staging copy bytes.
   The workspace lock coordinates our writers, not an external writer editing
   the same FileId in place; no snapshot guarantee is claimed against that. */
static FS_READ_STATUS wc_copy_source(const FS_READ_ROOT *r,const char *src,
  HANDLE held,const FILE_ID_INFO *id,const unsigned char *expected,size_t len)
{
    HANDLE current=INVALID_HANDLE_VALUE;FILE_ID_INFO check;FS_READ_META m;
    unsigned char *buf=NULL;DWORD got=0,extra=0;unsigned char one;
    LARGE_INTEGER zero;FS_READ_STATUS s=win_open(r,src,&current);
    if(s!=FS_READ_OK)return s;
    if(win_meta(current,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||
       m.size!=len||!wc_id(current,&check)||!wc_same(&check,id)||
       !wc_id(held,&check)||!wc_same(&check,id)){
        s=FS_READ_DENIED;goto done;
    }
    buf=(unsigned char*)malloc(len+1);if(!buf){s=FS_READ_IO;goto done;}
    zero.QuadPart=0;
    if(!SetFilePointerEx(held,zero,NULL,FILE_BEGIN)||
       !ReadFile(held,buf,(DWORD)len,&got,NULL)||got!=len||
       !ReadFile(held,&one,1,&extra,NULL)||extra!=0){s=FS_READ_DENIED;goto done;}
    s=(len&&memcmp(buf,expected,len))?FS_READ_DENIED:FS_READ_OK;
done:free(buf);CloseHandle(current);return s;
}
static FS_READ_STATUS wc_create_locked(const FS_READ_ROOT *r,const char *rel,
  const void *bytes,size_t len,unsigned mode,const char *src,
  HANDLE source,const FILE_ID_INFO *source_id)
{
    FS_READ_STATUS s;HANDLE parent=INVALID_HANDLE_VALUE;
    HANDLE stage=INVALID_HANDLE_VALUE;WC_RECORD rec={0};
    WCHAR *target=NULL;const WCHAR *leaf;FILE_ID_INFO rootid,pid;
    DWORD written;size_t used=0;int intent=0,commit=0,started=0;
    FILE_ID_INFO ignored;WC_RECORD other;
    if(!r||!valid_relative(rel,0)||(!bytes&&len)||len>FS_READ_MAX||mode>0777)
        return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    target=win_wide(rel);if(!target)return FS_READ_INVALID;
    if(wcslen(target)>=WC_MAX_PATH){s=FS_READ_INVALID;goto end;}
    s=wc_target_status(target);if(s!=FS_READ_OK)goto end;
    s=wc_parent(r,target,&parent,&leaf);
    WC_TRACE("wc_create parent open status=%d\n",(int)s);
    if(s!=FS_READ_OK)goto end;
    if(!wc_id(r->handle,&rootid)||!wc_id(parent,&pid)||rootid.VolumeSerialNumber!=pid.VolumeSerialNumber){
        s=FS_READ_UNSUPPORTED;goto end;
    }
    s=wc_record_read(r->handle,WC_INTENT,&other,&ignored,&intent);
    WC_TRACE("wc_create intent probe status=%d present=%d\n",(int)s,intent);
    if(s!=FS_READ_OK||intent){s=s==FS_READ_OK?FS_READ_PENDING:s;goto end;}
    s=wc_record_read(r->handle,WC_COMMIT,&other,&ignored,&commit);
    WC_TRACE("wc_create commit probe status=%d present=%d\n",(int)s,commit);
    if(s!=FS_READ_OK||commit){s=s==FS_READ_OK?FS_READ_PENDING:s;goto end;}
    s=wo_clear_other(r->handle,0);if(s!=FS_READ_OK)goto end;
    {HANDLE existing=INVALID_HANDLE_VALUE;
     s=wc_open(parent,leaf,FILE_READ_ATTRIBUTES,FILE_OPEN,&existing);
     WC_TRACE("wc_create target absence probe name=%ls status=%d\n",leaf,(int)s);
     if(s==FS_READ_OK){CloseHandle(existing);s=FS_READ_DENIED;goto end;}
     if(s!=FS_READ_MISSING)goto end;
    }
    if(src){s=wc_copy_source(r,src,source,source_id,bytes,len);if(s!=FS_READ_OK)goto end;}
    if(!wc_random(rec.stage,L".fst-")||!wc_random(rec.pin,L".fsp-")){
        s=FS_READ_IO;goto end;
    }
    s=wc_open(r->handle,rec.stage,GENERIC_WRITE|GENERIC_READ|DELETE,FILE_CREATE,&stage);
    WC_TRACE("wc_create stage open status=%d\n",(int)s);
    if(s!=FS_READ_OK)goto end;
    started=1;wc_crash(1); /* A: stage only */
    while(used<len){
        if(!WriteFile(stage,(const unsigned char*)bytes+used,(DWORD)(len-used),&written,NULL)||!written){s=FS_READ_IO;goto end;}
        used+=written;
    }
    if(!FlushFileBuffers(stage)||!wc_id(stage,&rec.file)||
       rec.file.VolumeSerialNumber!=rootid.VolumeSerialNumber){
        s=FS_READ_IO;goto end;
    }
    if(!wc_set_name(stage,r->handle,rec.pin,WC_NT_LINK)){
        s=FS_READ_UNSUPPORTED;goto end;
    }
    {int pin_exists=0;FS_READ_STATUS verified=wc_lookup(r->handle,rec.pin,&rec.file,&pin_exists);
     if(verified!=FS_READ_OK||!pin_exists){s=FS_READ_PENDING;goto end;}}
    wc_crash(2); /* B: stage plus pin */
    rec.magic=WC_MAGIC;rec.version=1;rec.root=rootid;rec.parent=pid;
    rec.size=len;rec.digest=wc_hash((const unsigned char*)bytes,len);rec.mode=mode;
    wcscpy(rec.target,target);rec.checksum=wc_checksum(&rec);
    wc_probe_dir(r->handle);
    s=wc_record_publish(r->handle,WC_INTENT,&rec);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    wc_crash(3); /* C: complete intent */
    if(!wc_set_name(stage,parent,leaf,WC_NT_RENAME)){
        s=FS_READ_PENDING;goto end;
    }
    wc_crash(4); /* D: target published, no marker */
    {int target_exists=0;FS_READ_STATUS verified=wc_lookup(parent,leaf,&rec.file,&target_exists);
     if(verified!=FS_READ_OK||!target_exists){s=FS_READ_PENDING;goto end;}}
    wc_probe_dir(parent);
    wc_crash(5); /* E: after parent probe, rollback still needed */
    s=wc_record_publish(r->handle,WC_COMMIT,&rec);
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    wc_crash(6); /* F: marker and intent */
    CloseHandle(stage);stage=INVALID_HANDLE_VALUE;
    s=wc_recover_locked(r);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    wc_crash(7); /* G: committed cleanup complete */
    s=FS_READ_OK;
end:
    WC_TRACE("wc_create return status=%d started=%d\n",(int)s,started);
    if(started&&s!=FS_READ_OK&&s!=FS_READ_PENDING)s=FS_READ_PENDING;
    if(stage!=INVALID_HANDLE_VALUE)CloseHandle(stage);
    if(parent!=INVALID_HANDLE_VALUE)CloseHandle(parent);
    free(target);return s;
}

FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *r,const char *rel,
                             const void *bytes,size_t len,unsigned mode)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;FS_READ_STATUS s;
    if(!r||!valid_relative(rel,0)||(!bytes&&len)||len>FS_READ_MAX||mode>0777)
        return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    {WCHAR *target=win_wide(rel);
     if(!target)return FS_READ_INVALID;
     s=wcslen(target)>=WC_MAX_PATH?FS_READ_INVALID:wc_target_status(target);
     free(target);if(s!=FS_READ_OK)return s;}
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)return s;
    s=wc_create_locked(r,rel,bytes,len,mode,NULL,INVALID_HANDLE_VALUE,NULL);
    wc_unlock(lock,&overlap);return s;
}
FS_READ_STATUS FsCopyFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t len)
{
    HANDLE lock=INVALID_HANDLE_VALUE,source=INVALID_HANDLE_VALUE;
    OVERLAPPED overlap;FILE_ID_INFO id,rootid;FS_READ_META m;
    FS_READ_STATUS s;WCHAR *ws=NULL,*wd=NULL;
    if(!r||!src||!dst||!expected||len>FS_READ_MAX||
       !valid_relative(src,0)||!valid_relative(dst,0))return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    ws=win_wide(src);wd=win_wide(dst);
    if(!ws||!wd||wcslen(ws)>=WC_MAX_PATH||wcslen(wd)>=WC_MAX_PATH){s=FS_READ_INVALID;goto done;}
    s=wc_target_status(ws);if(s!=FS_READ_OK)goto done;
    s=wc_target_status(wd);if(s!=FS_READ_OK)goto done;
    if(!_wcsicmp(ws,wd)){s=FS_READ_DENIED;goto done;}
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)goto done;
    s=win_open(r,src,&source);if(s!=FS_READ_OK)goto done;
    s=win_meta(source,&m);
    if(s!=FS_READ_OK)goto done;
    if(m.kind!=FS_KIND_FILE||m.size!=len||
       !wc_id(source,&id)||!wc_id(r->handle,&rootid)||
       id.VolumeSerialNumber!=rootid.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
    s=wc_copy_source(r,src,source,&id,(const unsigned char*)expected,len);
    if(s!=FS_READ_OK)goto done;
    s=wc_create_locked(r,dst,expected,len,m.mode,src,source,&id);
done:
    if(source!=INVALID_HANDLE_VALUE)CloseHandle(source);
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    free(ws);free(wd);return s;
}
static FS_READ_STATUS wo_write(const FS_READ_ROOT *r,unsigned kind,
  const char *src,const char *dst,const void *expected,size_t len)
{
    HANDLE lock=INVALID_HANDLE_VALUE,source_dir=INVALID_HANDLE_VALUE;
    HANDLE target_dir=INVALID_HANDLE_VALUE,source=INVALID_HANDLE_VALUE;
    HANDLE check=INVALID_HANDLE_VALUE;OVERLAPPED overlap;
    const WCHAR *src_leaf=NULL,*dst_leaf=NULL;
    WCHAR *ws=NULL,*wd=NULL;WO_RECORD rec={0};
    FILE_STANDARD_INFO standard;FS_READ_META m;FS_READ_STATUS s=FS_READ_IO;
    int operation_started=0;unsigned long long digest;
    if(!r||!src||!expected||len>FS_READ_MAX||!valid_relative(src,0)||
       (kind==2&&(!dst||!valid_relative(dst,0))))return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    ws=win_wide(src);wd=kind==2?win_wide(dst):NULL;
    if(!ws||(kind==2&&!wd)||wcslen(ws)>=WC_MAX_PATH||
       (wd&&wcslen(wd)>=WC_MAX_PATH)){s=FS_READ_INVALID;goto done;}
    s=wc_target_status(ws);if(s!=FS_READ_OK)goto done;
    if(wd){s=wc_target_status(wd);if(s!=FS_READ_OK)goto done;
       if(!_wcsicmp(ws,wd)){s=FS_READ_DENIED;goto done;}}
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)goto done;
    s=wo_clear_other(r->handle,kind);if(s!=FS_READ_OK)goto done;
    {WO_RECORD other;FILE_ID_INFO ignored;int present=0;
     s=wo_read(r->handle,kind==1?WO_REMOVE:WO_MOVE,kind,&other,&ignored,&present);
     if(s!=FS_READ_OK||present){s=s==FS_READ_OK?FS_READ_PENDING:s;goto done;}
     s=wo_read(r->handle,kind==1?WO_RCOMMIT:WO_MCOMMIT,kind,&other,&ignored,&present);
     if(s!=FS_READ_OK||present){s=s==FS_READ_OK?FS_READ_PENDING:s;goto done;}}
    s=wc_parent(r,ws,&source_dir,&src_leaf);if(s!=FS_READ_OK)goto done;
    if(wd){s=wc_parent(r,wd,&target_dir,&dst_leaf);if(s!=FS_READ_OK)goto done;}
    if(!wc_id(r->handle,&rec.root)||!wc_id(source_dir,&rec.source_parent)||
       rec.root.VolumeSerialNumber!=rec.source_parent.VolumeSerialNumber||
       (wd&&(!wc_id(target_dir,&rec.target_parent)||
        rec.root.VolumeSerialNumber!=rec.target_parent.VolumeSerialNumber))){s=FS_READ_UNSUPPORTED;goto done;}
    s=wc_open(source_dir,src_leaf,GENERIC_READ|DELETE,FILE_OPEN,&source);
    if(s!=FS_READ_OK)goto done;
    if(win_meta(source,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=len||
       !wc_id(source,&rec.file)||rec.file.VolumeSerialNumber!=rec.root.VolumeSerialNumber||
       !GetFileInformationByHandleEx(source,FileStandardInfo,&standard,sizeof(standard))||
       standard.NumberOfLinks!=1){s=FS_READ_DENIED;goto done;}
    digest=wc_hash((const unsigned char*)expected,len);
    s=wo_bytes(source,&rec.file,len,digest);if(s!=FS_READ_OK)goto done;
    {int source_present=0;
     s=wo_named(source_dir,src_leaf,&rec.file,&source_present);
     if(s!=FS_READ_OK||!source_present){s=FS_READ_DENIED;goto done;}}
    if(wd){
        s=wc_open(target_dir,dst_leaf,FILE_READ_ATTRIBUTES,FILE_OPEN,&check);
        if(s==FS_READ_OK){s=FS_READ_DENIED;goto done;}
        if(s!=FS_READ_MISSING)goto done;
    }
    /* Revalidate identity and bytes immediately before the first journal effect.
       External writers editing the same FileId in place are not serialized. */
    s=wc_open(source_dir,src_leaf,GENERIC_READ,FILE_OPEN,&check);
    if(s!=FS_READ_OK)goto done;
    s=wo_bytes(check,&rec.file,len,digest);if(s!=FS_READ_OK)goto done;
    CloseHandle(check);check=INVALID_HANDLE_VALUE;
    s=wo_bytes(source,&rec.file,len,digest);if(s!=FS_READ_OK)goto done;
    if(!wc_random(rec.pin,kind==1?L".fsrm-":L".fsmv-")){s=FS_READ_IO;goto done;}
    if(!wc_set_name(source,r->handle,rec.pin,WC_NT_LINK)){s=FS_READ_UNSUPPORTED;goto done;}
    operation_started=1;wo_crash(kind,0);
    {int exists=0;s=wo_named(r->handle,rec.pin,&rec.file,&exists);
     if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto done;}}
    rec.magic=WO_MAGIC;rec.version=1;rec.kind=kind;
    rec.size=len;rec.digest=digest;wcscpy(rec.source,ws);
    if(wd)wcscpy(rec.target,wd);
    rec.checksum=wo_checksum(&rec);
    s=wo_publish(r->handle,kind==1?WO_REMOVE:WO_MOVE,&rec);
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    wo_crash(kind,1);
    if(wd){
        if(!wc_set_name(source,target_dir,dst_leaf,WC_NT_LINK)){s=FS_READ_PENDING;goto done;}
        wo_crash(kind,2);
        {int exists=0;s=wo_named(target_dir,dst_leaf,&rec.file,&exists);
         if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto done;}}
    }
    /* The handle-relative delete helper reopens by name and checks FileId.
       Cooperative writers are serialized; an external same-FileId editor is
       outside this protocol's isolation guarantee. */
    {int exists=0;s=wo_named(source_dir,src_leaf,&rec.file,&exists);
     if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto done;}}
    CloseHandle(source);source=INVALID_HANDLE_VALUE;
    s=wc_remove(source_dir,src_leaf,&rec.file);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    {HANDLE absent=INVALID_HANDLE_VALUE;
     s=wc_open(source_dir,src_leaf,FILE_READ_ATTRIBUTES,FILE_OPEN,&absent);
     if(s==FS_READ_OK){CloseHandle(absent);s=FS_READ_PENDING;goto done;}
     if(s!=FS_READ_MISSING){s=FS_READ_PENDING;goto done;}}
    wo_crash(kind,3);
    wc_probe_dir(source_dir);if(wd)wc_probe_dir(target_dir);
    s=wo_publish(r->handle,kind==1?WO_RCOMMIT:WO_MCOMMIT,&rec);
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    wo_crash(kind,4);
    if(check!=INVALID_HANDLE_VALUE){CloseHandle(check);check=INVALID_HANDLE_VALUE;}
    s=wo_recover_locked(r,kind);if(s!=FS_READ_OK)s=FS_READ_PENDING;
done:
    if(check!=INVALID_HANDLE_VALUE)CloseHandle(check);
    if(source!=INVALID_HANDLE_VALUE)CloseHandle(source);
    if(operation_started&&s!=FS_READ_OK)s=FS_READ_PENDING;

    if(target_dir!=INVALID_HANDLE_VALUE)CloseHandle(target_dir);
    if(source_dir!=INVALID_HANDLE_VALUE)CloseHandle(source_dir);
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    free(ws);free(wd);return s;
}
FS_READ_STATUS FsRemoveRecover(const FS_READ_ROOT *r)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)return s;
    s=wo_recover_locked(r,1);wc_unlock(lock,&overlap);return s;
}
FS_READ_STATUS FsMoveRecover(const FS_READ_ROOT *r)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)return s;
    s=wo_recover_locked(r,2);wc_unlock(lock,&overlap);return s;
}
FS_READ_STATUS FsRemoveFile(const FS_READ_ROOT *r,const char *path,
                            const void *expected,size_t len)
{return wo_write(r,1,path,NULL,expected,len);}
FS_READ_STATUS FsMoveFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t len)
{return wo_write(r,2,src,dst,expected,len);}
FS_READ_STATUS FsReplaceRecover(const FS_READ_ROOT *r)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)return s;
    s=wr_recover_locked(r);wc_unlock(lock,&overlap);return s;
}
FS_READ_STATUS FsReplaceFile(const FS_READ_ROOT *r,const char *path,
 const void *expected,size_t expected_len,const void *replacement,size_t replacement_len)
{
    HANDLE lock=INVALID_HANDLE_VALUE,parent=INVALID_HANDLE_VALUE;
    HANDLE old=INVALID_HANDLE_VALUE,new_file=INVALID_HANDLE_VALUE;
    HANDLE target=INVALID_HANDLE_VALUE;OVERLAPPED overlap;WR_RECORD rec={0};
    FILE_STANDARD_INFO standard;FS_READ_META meta;FILE_ID_INFO found;
    WCHAR *name=NULL;const WCHAR *leaf=NULL;FS_READ_STATUS s=FS_READ_IO;
    DWORD wrote;size_t used=0;int started=0;
    if(!r||!path||!expected||(!replacement&&replacement_len)||
       expected_len>FS_READ_MAX||replacement_len>FS_READ_MAX||
       !valid_relative(path,0))return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    name=win_wide(path);
    if(!name||wcslen(name)>=WC_MAX_PATH){s=FS_READ_INVALID;goto done;}
    s=wc_target_status(name);if(s!=FS_READ_OK)goto done;
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK)goto done;
    s=wo_clear_other(r->handle,3);if(s!=FS_READ_OK)goto done;
    {WR_RECORD other;FILE_ID_INFO ignored;int present=0;
     s=wr_read(r->handle,WR_INTENT,&other,&ignored,&present);
     if(s!=FS_READ_OK||present){s=s==FS_READ_OK?FS_READ_PENDING:s;goto done;}
     s=wr_read(r->handle,WR_COMMIT,&other,&ignored,&present);
     if(s!=FS_READ_OK||present){s=s==FS_READ_OK?FS_READ_PENDING:s;goto done;}}
    s=wc_parent(r,name,&parent,&leaf);if(s!=FS_READ_OK)goto done;
    if(!wc_id(r->handle,&rec.root)||!wc_id(parent,&rec.parent)||
       rec.root.VolumeSerialNumber!=rec.parent.VolumeSerialNumber){s=FS_READ_UNSUPPORTED;goto done;}
    s=wc_open(parent,leaf,GENERIC_READ|DELETE,FILE_OPEN,&old);if(s!=FS_READ_OK)goto done;
    if(win_meta(old,&meta)!=FS_READ_OK||meta.kind!=FS_KIND_FILE||
       meta.size!=expected_len||!wc_id(old,&rec.old_id)||
       rec.old_id.VolumeSerialNumber!=rec.root.VolumeSerialNumber||
       !GetFileInformationByHandleEx(old,FileStandardInfo,&standard,sizeof(standard))||
       standard.NumberOfLinks!=1){s=FS_READ_DENIED;goto done;}
    rec.old_size=expected_len;
    rec.old_digest=wc_hash((const unsigned char*)expected,expected_len);
    rec.new_size=replacement_len;
    rec.new_digest=wc_hash((const unsigned char*)replacement,replacement_len);
    s=wo_bytes(old,&rec.old_id,expected_len,rec.old_digest);if(s!=FS_READ_OK)goto done;
    if(!wc_random(rec.old_pin,L".fsrp-")||!wc_random(rec.new_stage,L".fsrs-")||
       !wc_random(rec.new_pin,L".fsrp-")||!wc_random(rec.rollback,L".fsrb-")){
       s=FS_READ_IO;goto done;}
    if(!wc_set_name(old,r->handle,rec.old_pin,WC_NT_LINK)){s=FS_READ_UNSUPPORTED;goto done;}
    started=1;wr_crash(0);
    s=wc_open(r->handle,rec.new_stage,GENERIC_WRITE|GENERIC_READ|DELETE,FILE_CREATE,&new_file);
    if(s!=FS_READ_OK)goto done;
    while(used<replacement_len){
        if(!WriteFile(new_file,(const unsigned char*)replacement+used,
                      (DWORD)(replacement_len-used),&wrote,NULL)||!wrote){s=FS_READ_IO;goto done;}
        used+=wrote;
    }
    if(!FlushFileBuffers(new_file)||!wc_id(new_file,&rec.new_id)||
       rec.new_id.VolumeSerialNumber!=rec.root.VolumeSerialNumber||
       wc_same(&rec.old_id,&rec.new_id)){s=FS_READ_IO;goto done;}
    wr_crash(1);
    if(!wc_set_name(new_file,r->handle,rec.new_pin,WC_NT_LINK)){s=FS_READ_PENDING;goto done;}
    wr_crash(2);
    {int exists=0;
     s=wr_named(r->handle,rec.old_pin,&rec.old_id,&exists);
     if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto done;}
     s=wr_named(r->handle,rec.new_pin,&rec.new_id,&exists);
     if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto done;}}
    s=wr_verify(r->handle,rec.old_pin,&rec.old_id,expected_len,rec.old_digest);
    if(s!=FS_READ_OK)goto done;
    s=wr_verify(r->handle,rec.new_pin,&rec.new_id,replacement_len,rec.new_digest);
    if(s!=FS_READ_OK)goto done;
    /* Revalidate old identity and exact bytes at the final pre-intent gate. */
    s=wr_verify(parent,leaf,&rec.old_id,expected_len,rec.old_digest);
    if(s!=FS_READ_OK)goto done;
    rec.magic=WR_MAGIC;rec.version=1;wcscpy(rec.target,name);
    rec.checksum=wr_checksum(&rec);
    s=wr_publish(r->handle,WR_INTENT,&rec);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    wr_crash(3);
    s=wc_open(parent,leaf,GENERIC_READ,FILE_OPEN,&target);if(s!=FS_READ_OK)goto done;
    if(!wc_id(target,&found)||!wc_same(&found,&rec.old_id)){
        s=FS_READ_PENDING;goto done;}
    CloseHandle(target);target=INVALID_HANDLE_VALUE;
    s=wr_verify(parent,leaf,&rec.old_id,expected_len,rec.old_digest);
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    /* ReplaceIfExists applies only within the held parent. A non-cooperating
       external swap between this check and rename is outside the guarantee. */
    CloseHandle(old);old=INVALID_HANDLE_VALUE;
    if(!wr_replace_name(new_file,parent,leaf)){s=FS_READ_PENDING;goto done;}
    wr_crash(4);
    s=wr_verify(parent,leaf,&rec.new_id,replacement_len,rec.new_digest);
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    wc_probe_dir(parent);wr_crash(5);
    s=wr_publish(r->handle,WR_COMMIT,&rec);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto done;}
    wr_crash(6);
    CloseHandle(new_file);new_file=INVALID_HANDLE_VALUE;
    CloseHandle(old);old=INVALID_HANDLE_VALUE;
    s=wr_recover_locked(r);if(s!=FS_READ_OK)s=FS_READ_PENDING;
done:
    if(target!=INVALID_HANDLE_VALUE)CloseHandle(target);
    if(new_file!=INVALID_HANDLE_VALUE)CloseHandle(new_file);
    if(old!=INVALID_HANDLE_VALUE)CloseHandle(old);
    if(started&&s!=FS_READ_OK)s=FS_READ_PENDING;
    if(parent!=INVALID_HANDLE_VALUE)CloseHandle(parent);
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    free(name);return s;
}
#include "fs_batch_win.inc"
