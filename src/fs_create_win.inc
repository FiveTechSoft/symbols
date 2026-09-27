/* NTFS process-termination journal for create-only writes. Included inside the
   Windows section of fs_read.c so the held-root and component opener are shared. */
#include <stddef.h>
#include <stdio.h>
#define WC_INTENT L".fstxn.intent"
#define WC_COMMIT L".fstxn.ccommit"
#define WC_LOCK L".fstxn.lock"
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
       !_wcsnicmp(leaf,L".fsrp-",6)||!_wcsnicmp(leaf,L".fsrb-",6)||
       !_wcsnicmp(leaf,L".fsrm-",6)||!_wcsnicmp(leaf,L".fsmv-",6);
}
/* Validate the whole path before classifying reserved control leaves. A malformed
   suffix must stay INVALID even if an earlier component is reserved. */
static FS_READ_STATUS wc_target_status(const WCHAR *s)
{
    WCHAR segment[241];size_t n=0;int reserved=0;
    if(!*s)return FS_READ_INVALID;
    for(const WCHAR *p=s;;p++){
        if(*p==L'/'||!*p){
            if(!n||n>240)return FS_READ_INVALID;
            segment[n]=0;
            if(segment[n-1]==L'.'||segment[n-1]==L' '||
               !wcscmp(segment,L".")||!wcscmp(segment,L".."))return FS_READ_INVALID;
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
    return win_open(r,utf8,dir);
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
    s=wc_recover_locked(r);wc_unlock(lock,&o);return s;
}
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *r,const char *rel,const void *bytes,size_t len,unsigned mode)
{
    FS_READ_STATUS s;HANDLE lock=INVALID_HANDLE_VALUE,parent=INVALID_HANDLE_VALUE;
    HANDLE stage=INVALID_HANDLE_VALUE;OVERLAPPED overlap;WC_RECORD rec={0};
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
    s=wc_lock(r->handle,&lock,&overlap);
    WC_TRACE("wc_create lock status=%d\n",(int)s);
    if(s!=FS_READ_OK)goto end;
    s=wc_record_read(r->handle,WC_INTENT,&other,&ignored,&intent);
    WC_TRACE("wc_create intent probe status=%d present=%d\n",(int)s,intent);
    if(s!=FS_READ_OK||intent){s=s==FS_READ_OK?FS_READ_PENDING:s;goto end;}
    s=wc_record_read(r->handle,WC_COMMIT,&other,&ignored,&commit);
    WC_TRACE("wc_create commit probe status=%d present=%d\n",(int)s,commit);
    if(s!=FS_READ_OK||commit){s=s==FS_READ_OK?FS_READ_PENDING:s;goto end;}
    /* Other transaction formats are not implemented on Windows, but an
       unexpected control name must never be ignored by this writer. */
    {const WCHAR *names[]={L".fstxn.batch",L".fstxn.commit",L".fstxn.remove",
      L".fstxn.rcommit",L".fstxn.move",L".fstxn.mcommit",
      L".fstxn.replace",L".fstxn.pcommit"};
     for(size_t k=0;k<sizeof(names)/sizeof(names[0]);k++){
       HANDLE other_file=INVALID_HANDLE_VALUE;
       FS_READ_STATUS found=wc_open(r->handle,names[k],FILE_READ_ATTRIBUTES,FILE_OPEN,&other_file);
       WC_TRACE("wc_create control probe name=%ls status=%d\n",names[k],(int)found);
       if(found==FS_READ_OK){CloseHandle(other_file);s=FS_READ_DENIED;goto end;}
       if(found!=FS_READ_MISSING){s=FS_READ_DENIED;goto end;}
     }
    }
    {HANDLE existing=INVALID_HANDLE_VALUE;
     s=wc_open(parent,leaf,FILE_READ_ATTRIBUTES,FILE_OPEN,&existing);
     WC_TRACE("wc_create target absence probe name=%ls status=%d\n",leaf,(int)s);
     if(s==FS_READ_OK){CloseHandle(existing);s=FS_READ_DENIED;goto end;}
     if(s!=FS_READ_MISSING)goto end;
    }
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
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    if(parent!=INVALID_HANDLE_VALUE)CloseHandle(parent);
    free(target);return s;
}
