/* Windows batch: journal record formats, batch-level validation (W1),
   FsBatchCreate / FsBatchRecover (W2) and FsBatchReplace (W4). Included at the end of fs_create_win.inc, after the wc_
   helpers it uses. */
#define WB_MAGIC 0x57425431u
#define WBR_MAGIC 0x57425231u
#define WB_LIMIT 8
/* Create record: one item per entry. Targets use the same bound as the single
   operations (WC_MAX_PATH). Names live in the workspace root like the single
   create's stage and pin. */
typedef struct {
    FILE_ID_INFO parent,file;
    unsigned long long size,digest;
    unsigned mode,reserved;
    WCHAR target[WC_MAX_PATH],stage[48],pin[48];
} WB_ITEM;
typedef struct {
    unsigned magic,version,checksum,count;
    FILE_ID_INFO root;
    WB_ITEM items[WB_LIMIT];
} WB_RECORD;
/* Replace record: same size-distinguishing role as FS_BREPL_RECORD on POSIX,
   here also told apart by its own magic. Name prefixes follow the role:
   old_pin .fsrp- (pin of the old inode), new_stage .fst- and new_pin .fsp-
   (same roles as the create stage and pin), rollback .fsrb-. */
typedef struct {
    FILE_ID_INFO parent,old_id,new_id;
    unsigned long long old_size,old_digest,new_size,new_digest;
    WCHAR target[WC_MAX_PATH],old_pin[48],new_stage[48],new_pin[48],rollback[48];
} WBR_ITEM;
typedef struct {
    unsigned magic,version,checksum,count;
    FILE_ID_INFO root;
    WBR_ITEM items[WB_LIMIT];
} WBR_RECORD;
static unsigned wb_sum(const void *record,size_t size,size_t skip)
{
    const unsigned char *p=(const unsigned char*)record;unsigned h=2166136261u;
    for(size_t i=0;i<size;i++){
        if(i>=skip&&i<skip+sizeof(unsigned))continue;
        h=(h^p[i])*16777619u;
    }return h;
}
static unsigned wb_checksum(const WB_RECORD *r){return wb_sum(r,sizeof(*r),offsetof(WB_RECORD,checksum));}
static unsigned wbr_checksum(const WBR_RECORD *r){return wb_sum(r,sizeof(*r),offsetof(WBR_RECORD,checksum));}
/* NTFS compares names with its case table. Ordinal ignore-case is the closest
   Win32 comparison and errs toward refusing (two names that fold together are
   treated as one file). */
static int wb_equal_prefix(const WCHAR *a,const WCHAR *b,int length)
{return CompareStringOrdinal(a,length,b,length,TRUE)==CSTR_EQUAL;}
/* Two targets conflict when they fold to the same path, or when one is a
   directory prefix of the other (file "a" and file "a/b" cannot both exist). */
static int wb_conflict(const WCHAR *a,const WCHAR *b)
{
    size_t la=wcslen(a),lb=wcslen(b);
    if(la>lb){const WCHAR *t=a;a=b;b=t;size_t n=la;la=lb;lb=n;}
    if(!wb_equal_prefix(a,b,(int)la))return 0;
    return la==lb||b[la]==L'/';
}
static int wb_target_ok(const WCHAR *t)
{return wc_terminated(t,WC_MAX_PATH)&&wcslen(t)&&wc_target_status(t)==FS_READ_OK;}
static int wb_distinct(const WCHAR *const *names,size_t n)
{
    for(size_t i=0;i<n;i++)for(size_t j=0;j<i;j++)if(!wcscmp(names[i],names[j]))return 0;
    return 1;
}
static int wb_zero(const void *p,size_t n)
{const unsigned char *b=(const unsigned char*)p;for(size_t i=0;i<n;i++)if(b[i])return 0;return 1;}
static int wb_name(const WCHAR *s,const WCHAR *prefix)
{return wc_terminated(s,48)&&wc_random_name(s,prefix);}
static int wb_create_record_valid(const WB_RECORD *r)
{
    const WCHAR *names[2*WB_LIMIT];size_t used=0;
    if(r->magic!=WB_MAGIC||r->version!=1||r->count<2||r->count>WB_LIMIT)return 0;
    if(r->checksum!=wb_checksum(r)||!r->root.VolumeSerialNumber)return 0;
    for(unsigned k=0;k<r->count;k++){
        const WB_ITEM *i=&r->items[k];
        if(!wb_target_ok(i->target)||!wb_name(i->stage,L".fst-")||!wb_name(i->pin,L".fsp-"))return 0;
        if(i->parent.VolumeSerialNumber!=r->root.VolumeSerialNumber||
           i->file.VolumeSerialNumber!=r->root.VolumeSerialNumber||
           i->mode>0777||i->size>FS_READ_MAX||i->reserved)return 0;
        for(unsigned j=0;j<k;j++)if(wb_conflict(i->target,r->items[j].target))return 0;
        names[used++]=i->stage;names[used++]=i->pin;
    }
    for(unsigned k=r->count;k<WB_LIMIT;k++)if(!wb_zero(&r->items[k],sizeof(r->items[k])))return 0;
    return wb_distinct(names,used);
}
static int wb_replace_record_valid(const WBR_RECORD *r)
{
    const WCHAR *names[4*WB_LIMIT];size_t used=0;
    if(r->magic!=WBR_MAGIC||r->version!=1||r->count<2||r->count>WB_LIMIT)return 0;
    if(r->checksum!=wbr_checksum(r)||!r->root.VolumeSerialNumber)return 0;
    for(unsigned k=0;k<r->count;k++){
        const WBR_ITEM *i=&r->items[k];
        if(!wb_target_ok(i->target)||!wb_name(i->old_pin,L".fsrp-")||
           !wb_name(i->new_stage,L".fst-")||!wb_name(i->new_pin,L".fsp-")||
           !wb_name(i->rollback,L".fsrb-"))return 0;
        if(i->parent.VolumeSerialNumber!=r->root.VolumeSerialNumber||
           i->old_id.VolumeSerialNumber!=r->root.VolumeSerialNumber||
           i->new_id.VolumeSerialNumber!=r->root.VolumeSerialNumber||
           wc_same(&i->old_id,&i->new_id)||
           i->old_size>FS_READ_MAX||i->new_size>FS_READ_MAX)return 0;
        for(unsigned j=0;j<k;j++)if(wb_conflict(i->target,r->items[j].target))return 0;
        names[used++]=i->old_pin;names[used++]=i->new_stage;
        names[used++]=i->new_pin;names[used++]=i->rollback;
    }
    for(unsigned k=r->count;k<WB_LIMIT;k++)if(!wb_zero(&r->items[k],sizeof(r->items[k])))return 0;
    return wb_distinct(names,used);
}
/* Batch-level request validation. Every refusal is FS_READ_INVALID, like the
   POSIX batch (batch_name_ok failures and bad counts are INVALID there too). */
static FS_READ_STATUS wb_path_ok(const char *path,WCHAR **wide)
{
    WCHAR *w;
    *wide=NULL;
    if(!path||!valid_relative(path,0))return FS_READ_INVALID;
    w=win_wide(path);if(!w)return FS_READ_INVALID;
    if(wcslen(w)>=WC_MAX_PATH||wc_target_status(w)!=FS_READ_OK){free(w);return FS_READ_INVALID;}
    *wide=w;return FS_READ_OK;
}
static FS_READ_STATUS wb_validate_paths(const char *const *paths,size_t n)
{
    WCHAR *wide[WB_LIMIT]={0};FS_READ_STATUS s=FS_READ_OK;
    for(size_t k=0;k<n&&s==FS_READ_OK;k++){
        s=wb_path_ok(paths[k],&wide[k]);
        for(size_t j=0;j<k&&s==FS_READ_OK;j++)if(wb_conflict(wide[k],wide[j]))s=FS_READ_INVALID;
    }
    for(size_t k=0;k<n;k++)free(wide[k]);
    return s;
}
static FS_READ_STATUS wb_validate_create(const FS_BATCH_CREATE *e,size_t n)
{
    const char *paths[WB_LIMIT];
    if(!e||n<2||n>WB_LIMIT)return FS_READ_INVALID;
    for(size_t k=0;k<n;k++){
        if((!e[k].bytes&&e[k].len)||e[k].len>FS_READ_MAX||e[k].mode>0777)return FS_READ_INVALID;
        paths[k]=e[k].target;
    }
    return wb_validate_paths(paths,n);
}
static FS_READ_STATUS wb_validate_replace(const FS_BATCH_REPLACE *e,size_t n)
{
    const char *paths[WB_LIMIT];
    if(!e||n<2||n>WB_LIMIT)return FS_READ_INVALID;
    for(size_t k=0;k<n;k++){
        if(!e[k].expected||(!e[k].replacement&&e[k].replacement_len)||
           e[k].expected_len>FS_READ_MAX||e[k].replacement_len>FS_READ_MAX)return FS_READ_INVALID;
        paths[k]=e[k].target;
    }
    return wb_validate_paths(paths,n);
}
/* ---- FsBatchCreate / FsBatchRecover (W2) ---------------------------------
   Protocol, same as the POSIX batch: stage and pin every file, publish a
   complete journal, publish each target by rename without replace, publish a
   commit marker, then finish in recovery. Names live in the workspace root:
   journal .fstxn.batch, marker .fstxn.commit (a byte-identical copy of the
   journal, so a marker-only state still recovers), stage .fst-<32hex>, pin
   .fsp-<32hex> (a hard link to the staged inode that keeps its identity
   reachable after the stage name is renamed away).
   Cooperating writers only: not atomic visibility to readers, no power-loss
   claim (directory flush is diagnostic, as in the single operations). */
#define WB_JOURNAL L".fstxn.batch"
#define WB_MARKER L".fstxn.commit"
#ifdef FS_CREATE_TEST_CRASH
static void wb_crash(int point)
{
    const char *v=getenv("FS_WIN_BATCH_CRASH");
    if(v&&atoi(v)==point)ExitProcess((UINT)(80+point));
}
/* Test only: deliberate protocol breaks. The test runs each one and requires
   its own checks to fail; a mutant that passes means a missing check. */
static int wb_mutant(int n)
{
    const char *v=getenv("FS_WIN_BATCH_MUTANT");
    return v&&atoi(v)==n;
}
/* Test only: make the publish of item k fail as if its rename was refused. */
static int wb_inject_publish_failure(unsigned k)
{
    const char *v=getenv("FS_WIN_BATCH_FAIL_PUBLISH");
    return v&&(unsigned)atoi(v)==k;
}
/* Test only: called before the publish of item k of a replace batch, so a test
   can change a target between the final check and the rename. */
static void (*wb_before_publish)(unsigned k)=NULL;
#define WB_BEFORE_PUBLISH(k) do{if(wb_before_publish)wb_before_publish(k);}while(0)
#else
#define WB_BEFORE_PUBLISH(k) ((void)0)
#define wb_crash(n) ((void)0)
#define wb_mutant(n) 0
#define wb_inject_publish_failure(k) 0
#endif
static FS_READ_STATUS wb_publish(HANDLE root,const WCHAR *final,const void *rec,size_t size)
{
    HANDLE h=INVALID_HANDLE_VALUE;WCHAR tmp[48];DWORD written=0;FS_READ_STATUS s;
    size_t done=0;
    if(!wc_random(tmp,L".fsj-"))return FS_READ_IO;
    s=wc_open(root,tmp,GENERIC_WRITE|DELETE,FILE_CREATE,&h);if(s!=FS_READ_OK)return s;
    while(done<size){
        if(!WriteFile(h,(const unsigned char*)rec+done,(DWORD)(size-done),&written,NULL)||!written){s=FS_READ_IO;goto out;}
        done+=written;
    }
    if(!FlushFileBuffers(h)){s=FS_READ_IO;goto out;}
    if(!wc_set_name(h,root,final,WC_NT_RENAME)){s=FS_READ_DENIED;goto out;}
    wc_probe_dir(root);s=FS_READ_OK;
out:
    /* An uncertain rename may have published the record: never delete by handle. */
    CloseHandle(h);return s;
}
static FS_READ_STATUS wb_read(HANDLE root,const WCHAR *name,WB_RECORD *r,FILE_ID_INFO *id,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_META m;FS_READ_STATUS s;size_t done=0;DWORD got=0;
    s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=sizeof(*r)||!wc_id(h,id)){s=FS_READ_DENIED;goto out;}
    while(done<sizeof(*r)){
        if(!ReadFile(h,(unsigned char*)r+done,(DWORD)(sizeof(*r)-done),&got,NULL)||!got){s=FS_READ_DENIED;goto out;}
        done+=got;
    }
    s=wb_create_record_valid(r)?FS_READ_OK:FS_READ_DENIED;
out:CloseHandle(h);return s;
}
/* The bytes behind a pin are checked against the journal digest: a hard link
   protects identity, not content from an in-place edit. */
static FS_READ_STATUS wb_pin_bytes(HANDLE root,const WCHAR *name,const FILE_ID_INFO *file,
                                   unsigned long long size,unsigned long long digest)
{
    HANDLE h=INVALID_HANDLE_VALUE;unsigned char buf[8192];DWORD got=0;FILE_ID_INFO id;
    unsigned long long total=0,hash=14695981039346656037ULL;FS_READ_STATUS s;
    s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);if(s!=FS_READ_OK)return s;
    if(!wc_id(h,&id)||!wc_same(&id,file)){CloseHandle(h);return FS_READ_DENIED;}
    for(;;){
        if(!ReadFile(h,buf,sizeof(buf),&got,NULL)){CloseHandle(h);return FS_READ_DENIED;}
        if(!got)break;
        for(DWORD k=0;k<got;k++)hash=(hash^buf[k])*1099511628211ULL;
        total+=got;if(total>FS_READ_MAX)break;
    }
    CloseHandle(h);
    return total==size&&hash==digest?FS_READ_OK:FS_READ_DENIED;
}
#ifndef WB_HARNESS
static FS_READ_STATUS wb_make_readonly(HANDLE parent,const WCHAR *leaf,const FILE_ID_INFO *file)
{
    HANDLE t=INVALID_HANDLE_VALUE;FILE_ID_INFO check;FILE_BASIC_INFO basic;FS_READ_STATUS s;
    s=wc_open(parent,leaf,FILE_WRITE_ATTRIBUTES,FILE_OPEN,&t);if(s!=FS_READ_OK)return s;
    if(!wc_id(t,&check)||!wc_same(&check,file)){CloseHandle(t);return FS_READ_DENIED;}
    if(!GetFileInformationByHandleEx(t,FileBasicInfo,&basic,sizeof(basic))){CloseHandle(t);return FS_READ_IO;}
    basic.FileAttributes|=FILE_ATTRIBUTE_READONLY;
    if(!SetFileInformationByHandle(t,FileBasicInfo,&basic,sizeof(basic))){CloseHandle(t);return FS_READ_PENDING;}
    CloseHandle(t);return FS_READ_OK;
}
#endif
/* Recovery. Journal only: roll back, removing exactly the published targets
   whose identity matches the journal. Marker present: keep every target.
   Every stage, pin and target identity is verified before the first delete,
   so a foreign file under a journaled name stops recovery with nothing removed.
   Idempotent: each step tolerates an already finished earlier step. */
static FS_READ_STATUS wbr_recover_locked(const FS_READ_ROOT *r);
/* Which record, if any, a batch file holds: 1 create, 2 replace. A batch file
   of any other shape is DENIED (the caller sees present=1). */
static FS_READ_STATUS wb_kind(HANDLE root,const WCHAR *name,int *kind,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_META m;DWORD got=0;unsigned magic=0;FS_READ_STATUS s;
    *kind=0;*present=0;
    s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING)return FS_READ_OK;
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||
       !ReadFile(h,&magic,sizeof(magic),&got,NULL)||got!=sizeof(magic)){CloseHandle(h);return FS_READ_DENIED;}
    CloseHandle(h);
    if(magic==WB_MAGIC&&m.size==sizeof(WB_RECORD))*kind=1;
    else if(magic==WBR_MAGIC&&m.size==sizeof(WBR_RECORD))*kind=2;
    else return FS_READ_DENIED;
    return FS_READ_OK;
}
static FS_READ_STATUS wb_recover_locked(const FS_READ_ROOT *r)
{
    WB_RECORD *j=NULL,*m=NULL;
    const WB_RECORD *rec;FILE_ID_INFO jid,mid,rid,pid;
    HANDLE parent[WB_LIMIT];const WCHAR *leaf[WB_LIMIT];
    int stage[WB_LIMIT]={0},pin[WB_LIMIT]={0},target[WB_LIMIT]={0};
    int journal=0,marker=0;unsigned count=0,k;FS_READ_STATUS s;
    int jk=0,mk=0,jp=0,mp=0;
    s=wb_kind(r->handle,WB_JOURNAL,&jk,&jp);if(s!=FS_READ_OK)return s;
    s=wb_kind(r->handle,WB_MARKER,&mk,&mp);if(s!=FS_READ_OK)return s;
    if((jp&&jk==2)||(mp&&mk==2)){
        /* A replace batch: journal and marker must both be replace records. */
        if((jp&&jk!=2)||(mp&&mk!=2))return FS_READ_DENIED;
        return wbr_recover_locked(r);
    }
    j=(WB_RECORD*)calloc(1,sizeof(WB_RECORD));m=(WB_RECORD*)calloc(1,sizeof(WB_RECORD));
    for(k=0;k<WB_LIMIT;k++){parent[k]=INVALID_HANDLE_VALUE;leaf[k]=NULL;}
    if(!j||!m){s=FS_READ_IO;goto done;}
    s=wb_read(r->handle,WB_JOURNAL,j,&jid,&journal);if(s!=FS_READ_OK)goto done;
    s=wb_read(r->handle,WB_MARKER,m,&mid,&marker);if(s!=FS_READ_OK)goto done;
    if(!journal&&!marker){s=FS_READ_OK;goto done;}
    if(wb_mutant(1)&&journal&&!marker){marker=1;mid=jid;} /* mutant 1: roll forward without a marker */
    if(journal&&marker&&memcmp(j,m,sizeof(*j))){s=FS_READ_DENIED;goto done;}
    rec=journal?j:m;count=rec->count;
    if(!wc_id(r->handle,&rid)||!wc_same(&rid,&rec->root)||!wc_ntfs(r->handle)){s=FS_READ_DENIED;goto done;}
    if(wb_mutant(2)){ /* mutant 2: delete stage names before any identity check */
        for(k=0;k<count;k++){
            HANDLE h=INVALID_HANDLE_VALUE;
            if(wc_open(r->handle,rec->items[k].stage,DELETE,FILE_OPEN,&h)==FS_READ_OK){wc_delete(h);CloseHandle(h);}
        }
    }
    for(k=0;k<count;k++){
        const WB_ITEM *i=&rec->items[k];
        s=wc_parent(r,i->target,&parent[k],&leaf[k]);if(s!=FS_READ_OK)goto done;
        if(!wc_id(parent[k],&pid)||!wc_same(&pid,&i->parent)||
           pid.VolumeSerialNumber!=rec->root.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
        s=wc_lookup(r->handle,i->pin,&i->file,&pin[k]);if(s!=FS_READ_OK)goto done;
        s=wc_lookup(r->handle,i->stage,&i->file,&stage[k]);if(s!=FS_READ_OK)goto done;
        s=wc_lookup(parent[k],leaf[k],&i->file,&target[k]);if(s!=FS_READ_OK)goto done;
        if(pin[k]){s=wb_pin_bytes(r->handle,i->pin,&i->file,i->size,i->digest);if(s!=FS_READ_OK)goto done;}
        /* Before the marker the pin is the only proof of what was staged. */
        if(!marker&&!pin[k]){s=FS_READ_DENIED;goto done;}
        if(marker&&!target[k]){s=FS_READ_DENIED;goto done;}
    }
    if(!marker){
        wb_crash(8);
        for(k=count;k-->0;){
            if(target[k]){s=wc_remove(parent[k],leaf[k],&rec->items[k].file);if(s!=FS_READ_OK)goto done;wc_probe_dir(parent[k]);}
            if(k==count-1)wb_crash(9);
        }
        for(k=count;k-->0;){
            if(stage[k]){s=wc_remove(r->handle,rec->items[k].stage,&rec->items[k].file);if(s!=FS_READ_OK)goto done;}
        }
        wb_crash(10);
        /* Pins stay until the journal is gone: a crash before this point can
           always be replayed with full identity proof. */
        s=wc_remove(r->handle,WB_JOURNAL,&jid);if(s!=FS_READ_OK)goto done;
        wb_crash(11);
        for(k=0;k<count;k++)if(pin[k]){s=wc_remove(r->handle,rec->items[k].pin,&rec->items[k].file);if(s!=FS_READ_OK)goto done;}
    }else{
        for(k=0;k<count;k++){
            if(stage[k]){s=wc_remove(r->handle,rec->items[k].stage,&rec->items[k].file);if(s!=FS_READ_OK)goto done;}
            if(pin[k]){s=wc_remove(r->handle,rec->items[k].pin,&rec->items[k].file);if(s!=FS_READ_OK)goto done;}
        }
        /* The read-only attribute belongs to the inode: set it only after every
           other hard link is gone. */
        for(k=0;k<count;k++){
            if(!(rec->items[k].mode&0222)){s=wb_make_readonly(parent[k],leaf[k],&rec->items[k].file);if(s!=FS_READ_OK)goto done;}
        }
        wb_crash(12);wc_probe_dir(r->handle);
        if(wb_mutant(3)){ /* mutant 3: retire the marker before the journal */
            s=wc_remove(r->handle,WB_MARKER,&mid);if(s!=FS_READ_OK)goto done;
            marker=0;wb_crash(13);
        }
        if(journal){s=wc_remove(r->handle,WB_JOURNAL,&jid);if(s!=FS_READ_OK)goto done;}
        wb_crash(13);
        /* The marker owns the recovery record until its very last deletion. */
        for(k=0;k<count;k++){
            int still=0;
            s=wc_lookup(parent[k],leaf[k],&rec->items[k].file,&still);
            if(s!=FS_READ_OK||!still){s=FS_READ_DENIED;goto done;}
        }
        if(marker){s=wc_remove(r->handle,WB_MARKER,&mid);if(s!=FS_READ_OK)goto done;}
    }
    wc_probe_dir(r->handle);s=FS_READ_OK;
done:
    for(k=0;k<WB_LIMIT;k++)if(parent[k]!=INVALID_HANDLE_VALUE)CloseHandle(parent[k]);
    free(j);free(m);return s;
}
static FS_READ_STATUS wb_batch_recover(const FS_READ_ROOT *r)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED o;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    s=wc_lock(r->handle,&lock,&o);if(s!=FS_READ_OK)return s;
    s=wo_clear_other(r->handle,4);if(s==FS_READ_OK)s=wb_recover_locked(r);
    wc_unlock(lock,&o);return s;
}
/* Best-effort removal of everything staged before a journal exists. Nothing
   else refers to these names yet, so deleting them by the handles we hold and
   by the pin identity is safe. */
static void wb_discard_unjournaled(const FS_READ_ROOT *r,WB_RECORD *rec,HANDLE *stage,unsigned count)
{
    for(unsigned k=0;k<count;k++){
        if(stage[k]!=INVALID_HANDLE_VALUE){wc_delete(stage[k]);CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
        if(rec->items[k].file.VolumeSerialNumber&&rec->items[k].pin[0])
            (void)wc_remove(r->handle,rec->items[k].pin,&rec->items[k].file);
    }
}
static FS_READ_STATUS wb_batch_create(const FS_READ_ROOT *r,const FS_BATCH_CREATE *e,size_t n)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;WB_RECORD *rec=NULL,*other=NULL;
    HANDLE parent[WB_LIMIT],stage[WB_LIMIT];WCHAR *wide[WB_LIMIT];const WCHAR *leaf[WB_LIMIT];
    FILE_ID_INFO ignored;int journal=0,marker=0,journaled=0,committed=0;unsigned k;
    FS_READ_STATUS s;
    for(k=0;k<WB_LIMIT;k++){parent[k]=stage[k]=INVALID_HANDLE_VALUE;wide[k]=NULL;leaf[k]=NULL;}
    if(!r)return FS_READ_INVALID;
    s=wb_validate_create(e,n);if(s!=FS_READ_OK)return s;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    rec=(WB_RECORD*)calloc(1,sizeof(*rec));other=(WB_RECORD*)calloc(1,sizeof(*other));
    if(!rec||!other){free(rec);free(other);return FS_READ_IO;}
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK){free(rec);free(other);return s;}
    s=wo_clear_other(r->handle,4);if(s!=FS_READ_OK)goto end;
    /* A pending batch (journal or marker) blocks new batches until recovery. */
    s=wb_read(r->handle,WB_JOURNAL,other,&ignored,&journal);
    if(s==FS_READ_OK)s=wb_read(r->handle,WB_MARKER,other,&ignored,&marker);
    if(s!=FS_READ_OK)goto end;
    if(journal||marker){s=FS_READ_DENIED;goto end;}
    rec->magic=WB_MAGIC;rec->version=1;rec->count=(unsigned)n;
    if(!wc_id(r->handle,&rec->root)){s=FS_READ_UNSUPPORTED;goto end;}
    /* Resolve and check every target before creating anything. */
    for(k=0;k<n;k++){
        WB_ITEM *i=&rec->items[k];HANDLE existing=INVALID_HANDLE_VALUE;
        wide[k]=win_wide(e[k].target);if(!wide[k]){s=FS_READ_INVALID;goto end;}
        s=wc_parent(r,wide[k],&parent[k],&leaf[k]);if(s!=FS_READ_OK)goto end;
        if(!wc_id(parent[k],&i->parent)||i->parent.VolumeSerialNumber!=rec->root.VolumeSerialNumber){
            s=FS_READ_UNSUPPORTED;goto end;}
        s=wc_open(parent[k],leaf[k],FILE_READ_ATTRIBUTES,FILE_OPEN,&existing);
        if(s==FS_READ_OK){CloseHandle(existing);s=FS_READ_DENIED;goto end;}
        if(s!=FS_READ_MISSING)goto end;
        wcscpy(i->target,wide[k]);i->size=e[k].len;i->mode=e[k].mode;
        i->digest=wc_hash((const unsigned char*)e[k].bytes,e[k].len);
    }
    /* Stage and pin. No journal exists yet: a failure here discards our own
       stage names and pins and leaves the tree as it was. */
    for(k=0;k<n;k++){
        WB_ITEM *i=&rec->items[k];size_t used=0;DWORD wrote=0;
        if(!wc_random(i->stage,L".fst-")||!wc_random(i->pin,L".fsp-")){s=FS_READ_IO;goto unjournaled;}
        s=wc_open(r->handle,i->stage,GENERIC_WRITE|GENERIC_READ|DELETE,FILE_CREATE,&stage[k]);
        if(s!=FS_READ_OK)goto unjournaled;
        if(k==0)wb_crash(1); /* A: first stage only */
        while(used<e[k].len){
            if(!WriteFile(stage[k],(const unsigned char*)e[k].bytes+used,(DWORD)(e[k].len-used),&wrote,NULL)||!wrote){
                s=FS_READ_IO;goto unjournaled;}
            used+=wrote;
        }
        if(!FlushFileBuffers(stage[k])||!wc_id(stage[k],&i->file)||
           i->file.VolumeSerialNumber!=rec->root.VolumeSerialNumber){s=FS_READ_IO;goto unjournaled;}
        if(!wc_set_name(stage[k],r->handle,i->pin,WC_NT_LINK)){s=FS_READ_UNSUPPORTED;goto unjournaled;}
        {int exists=0;FS_READ_STATUS v=wc_lookup(r->handle,i->pin,&i->file,&exists);
         if(v!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto unjournaled;}}
    }
    wb_crash(2); /* B: every stage and pin */
    rec->checksum=wb_checksum(rec);
    if(!wb_create_record_valid(rec)){s=FS_READ_INVALID;goto unjournaled;}
    wc_probe_dir(r->handle);
    s=wb_publish(r->handle,WB_JOURNAL,rec,sizeof(*rec));
    if(s!=FS_READ_OK){
        /* The rename may or may not have happened: if a journal now exists it
           owns the pins, so leave everything for recovery. */
        int now=0;if(wb_read(r->handle,WB_JOURNAL,other,&ignored,&now)==FS_READ_OK&&!now)goto unjournaled;
        s=FS_READ_PENDING;journaled=1;goto end;
    }
    journaled=1;wb_crash(3); /* C: complete journal */
    for(k=0;k<n;k++){
        if(wb_inject_publish_failure(k)||!wc_set_name(stage[k],parent[k],leaf[k],WC_NT_RENAME)){
            s=FS_READ_DENIED;goto end;
        }
        if(k==0)wb_crash(4); /* D: first target published */
    }
    wb_crash(5); /* E: all targets published, no marker */
    for(k=0;k<n;k++){
        int present=0;s=wc_lookup(parent[k],leaf[k],&rec->items[k].file,&present);
        if(s!=FS_READ_OK||!present){s=FS_READ_PENDING;goto end;}
        wc_probe_dir(parent[k]);
    }
    s=wb_publish(r->handle,WB_MARKER,rec,sizeof(*rec));
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    committed=1;wb_crash(6); /* F: marker and journal */
    for(k=0;k<n;k++)if(stage[k]!=INVALID_HANDLE_VALUE){CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
    s=wb_recover_locked(r);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    wb_crash(7); /* G: committed cleanup complete */
    s=FS_READ_OK;goto end;
unjournaled:
    wb_discard_unjournaled(r,rec,stage,(unsigned)n);
end:
    /* mutant 4: the in-process rollback runs with the stage handles still open
       (and they are never closed). */
    for(k=0;k<WB_LIMIT&&!wb_mutant(4);k++)if(stage[k]!=INVALID_HANDLE_VALUE){CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
    /* Journal published but no marker: restore the tree now (our handles are
       closed, so no stage name is held open). A failed rollback leaves the
       journal for FsBatchRecover and reports PENDING. */
    if(journaled&&!committed&&s!=FS_READ_PENDING){
        FS_READ_STATUS rs=wb_recover_locked(r);
        if(rs!=FS_READ_OK)s=FS_READ_PENDING;
    }
    for(k=0;k<WB_LIMIT;k++){if(parent[k]!=INVALID_HANDLE_VALUE)CloseHandle(parent[k]);free(wide[k]);}
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    free(rec);free(other);return s;
}
/* ---- FsBatchReplace (W4) -------------------------------------------------
   Same journal and marker names as the create batch; the record is a WBR_RECORD
   (own magic and size), so recovery tells them apart. Per item, in the root:
   old_pin .fsrp- (hard link to the old inode), new_stage .fst-, new_pin .fsp-
   (hard link to the staged inode); in the target's directory a rollback name
   .fsrb- is linked to the old inode only while recovery restores it. The
   single-file replace uses its own names and checks; none are reused.
   Order: validate every target (identity, single link, not read-only, exact
   expected bytes) -> pin every old inode -> stage and pin every new image ->
   complete journal -> per item: close the old handle, rename the stage over the
   target (ReplaceIfExists) -> verify -> marker -> recovery rolls forward.
   Before the marker, recovery restores the old inode of every published item
   in reverse order by linking the old pin to the rollback name and renaming
   that over the target. A foreign open handle makes the rename fail with
   0xc0000022: that is PENDING, with what was published rolled back. */
#ifndef WB_HARNESS
static int wb_links(HANDLE h,DWORD *n)
{FILE_STANDARD_INFO s;if(!GetFileInformationByHandleEx(h,FileStandardInfo,&s,sizeof(s)))return 0;*n=s.NumberOfLinks;return 1;}
#endif
static FS_READ_STATUS wb_named_links(HANDLE dir,const WCHAR *leaf,DWORD *n)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_STATUS s=wc_open(dir,leaf,GENERIC_READ,FILE_OPEN,&h);
    if(s!=FS_READ_OK)return s;
    s=wb_links(h,n)?FS_READ_OK:FS_READ_IO;CloseHandle(h);return s;
}
static FS_READ_STATUS wbr_read(HANDLE root,const WCHAR *name,WBR_RECORD *r,FILE_ID_INFO *id,int *present)
{
    HANDLE h=INVALID_HANDLE_VALUE;FS_READ_META m;FS_READ_STATUS s;size_t done=0;DWORD got=0;
    s=wc_open(root,name,GENERIC_READ,FILE_OPEN,&h);
    if(s==FS_READ_MISSING){*present=0;return FS_READ_OK;}
    if(s!=FS_READ_OK)return s;
    *present=1;
    if(win_meta(h,&m)!=FS_READ_OK||m.kind!=FS_KIND_FILE||m.size!=sizeof(*r)||!wc_id(h,id)){s=FS_READ_DENIED;goto out;}
    while(done<sizeof(*r)){
        if(!ReadFile(h,(unsigned char*)r+done,(DWORD)(sizeof(*r)-done),&got,NULL)||!got){s=FS_READ_DENIED;goto out;}
        done+=got;
    }
    s=wb_replace_record_valid(r)?FS_READ_OK:FS_READ_DENIED;
out:CloseHandle(h);return s;
}
/* Recovery for a replace batch. Journal only: restore every old inode. Marker
   present: keep every new inode. All identities, pin bytes and link counts are
   verified before the first change. */
static FS_READ_STATUS wbr_recover_locked(const FS_READ_ROOT *r)
{
    WBR_RECORD *j=(WBR_RECORD*)calloc(1,sizeof(WBR_RECORD)),*m=(WBR_RECORD*)calloc(1,sizeof(WBR_RECORD));
    const WBR_RECORD *rec;FILE_ID_INFO jid,mid,rid,pid;
    HANDLE parent[WB_LIMIT];const WCHAR *leaf[WB_LIMIT];
    int oldpin[WB_LIMIT]={0},newpin[WB_LIMIT]={0},stage[WB_LIMIT]={0},rb[WB_LIMIT]={0};
    int told[WB_LIMIT]={0},tnew[WB_LIMIT]={0};
    int journal=0,marker=0;unsigned count=0,k;FS_READ_STATUS s;
    for(k=0;k<WB_LIMIT;k++){parent[k]=INVALID_HANDLE_VALUE;leaf[k]=NULL;}
    if(!j||!m){s=FS_READ_IO;goto done;}
    s=wbr_read(r->handle,WB_JOURNAL,j,&jid,&journal);if(s!=FS_READ_OK)goto done;
    s=wbr_read(r->handle,WB_MARKER,m,&mid,&marker);if(s!=FS_READ_OK)goto done;
    if(!journal&&!marker){s=FS_READ_OK;goto done;}
    if(journal&&marker&&memcmp(j,m,sizeof(*j))){s=FS_READ_DENIED;goto done;}
    rec=journal?j:m;count=rec->count;
    if(!wc_id(r->handle,&rid)||!wc_same(&rid,&rec->root)||!wc_ntfs(r->handle)){s=FS_READ_DENIED;goto done;}
    for(k=0;k<count;k++){
        const WBR_ITEM *i=&rec->items[k];HANDLE t=INVALID_HANDLE_VALUE;FILE_ID_INFO found;FS_READ_META tm;
        DWORD links=0;
        s=wc_parent(r,i->target,&parent[k],&leaf[k]);if(s!=FS_READ_OK)goto done;
        if(!wc_id(parent[k],&pid)||!wc_same(&pid,&i->parent)||
           pid.VolumeSerialNumber!=rec->root.VolumeSerialNumber){s=FS_READ_DENIED;goto done;}
        s=wr_named(r->handle,i->old_pin,&i->old_id,&oldpin[k]);if(s!=FS_READ_OK)goto done;
        s=wr_named(r->handle,i->new_pin,&i->new_id,&newpin[k]);if(s!=FS_READ_OK)goto done;
        s=wr_named(r->handle,i->new_stage,&i->new_id,&stage[k]);if(s!=FS_READ_OK)goto done;
        s=wr_named(parent[k],i->rollback,&i->old_id,&rb[k]);if(s!=FS_READ_OK)goto done;
        /* Before the marker the pins are the only proof of both images. */
        if(!marker&&(!oldpin[k]||!newpin[k])){s=FS_READ_DENIED;goto done;}
        if(oldpin[k]){s=wb_pin_bytes(r->handle,i->old_pin,&i->old_id,i->old_size,i->old_digest);if(s!=FS_READ_OK)goto done;}
        if(newpin[k]){s=wb_pin_bytes(r->handle,i->new_pin,&i->new_id,i->new_size,i->new_digest);if(s!=FS_READ_OK)goto done;}
        if(stage[k]){s=wb_pin_bytes(r->handle,i->new_stage,&i->new_id,i->new_size,i->new_digest);if(s!=FS_READ_OK)goto done;}
        if(rb[k]){s=wr_verify(parent[k],i->rollback,&i->old_id,(size_t)i->old_size,i->old_digest);if(s!=FS_READ_OK)goto done;}
        s=wc_open(parent[k],leaf[k],GENERIC_READ,FILE_OPEN,&t);
        if(s==FS_READ_MISSING)s=FS_READ_DENIED;
        if(s!=FS_READ_OK)goto done;
        if(win_meta(t,&tm)!=FS_READ_OK||tm.kind!=FS_KIND_FILE||!wc_id(t,&found)){CloseHandle(t);s=FS_READ_DENIED;goto done;}
        told[k]=wc_same(&found,&i->old_id);tnew[k]=wc_same(&found,&i->new_id);CloseHandle(t);
        if(!told[k]&&!tnew[k]){s=FS_READ_DENIED;goto done;}
        if(told[k]&&rb[k]){s=FS_READ_DENIED;goto done;} /* an alias nobody explains */
        if(oldpin[k]){
            s=wb_named_links(r->handle,i->old_pin,&links);if(s!=FS_READ_OK)goto done;
            if(links!=(DWORD)(1+told[k]+rb[k])){s=FS_READ_DENIED;goto done;}
        }
        if(newpin[k]){
            s=wb_named_links(r->handle,i->new_pin,&links);if(s!=FS_READ_OK)goto done;
            if(links!=(DWORD)(1+stage[k]+tnew[k])){s=FS_READ_DENIED;goto done;}
        }
        if(marker){
            if(!tnew[k]||rb[k]){s=FS_READ_DENIED;goto done;}
            s=wr_verify(parent[k],leaf[k],&i->new_id,(size_t)i->new_size,i->new_digest);if(s!=FS_READ_OK)goto done;
        }
    }
    if(!marker){
        wb_crash(29);
        for(k=count;k-->0;){
            const WBR_ITEM *i=&rec->items[k];
            if(tnew[k]){
                HANDLE h=INVALID_HANDLE_VALUE;
                if(!rb[k]){
                    s=wc_open(r->handle,i->old_pin,GENERIC_READ,FILE_OPEN,&h);if(s!=FS_READ_OK)goto done;
                    if(!wc_set_name(h,parent[k],i->rollback,WC_NT_LINK)){CloseHandle(h);s=FS_READ_PENDING;goto done;}
                    CloseHandle(h);
                    s=wr_verify(parent[k],i->rollback,&i->old_id,(size_t)i->old_size,i->old_digest);if(s!=FS_READ_OK)goto done;
                }
                s=wr_verify(parent[k],leaf[k],&i->new_id,(size_t)i->new_size,i->new_digest);if(s!=FS_READ_OK)goto done;
                s=wc_open(parent[k],i->rollback,DELETE,FILE_OPEN,&h);if(s!=FS_READ_OK)goto done;
                if(!wr_replace_name(h,parent[k],leaf[k])){CloseHandle(h);s=FS_READ_PENDING;goto done;}
                CloseHandle(h);
                if(k==count-1)wb_crash(30);
            }
            s=wr_verify(parent[k],leaf[k],&i->old_id,(size_t)i->old_size,i->old_digest);if(s!=FS_READ_OK)goto done;
            wc_probe_dir(parent[k]);
        }
        wb_crash(31);
        for(k=count;k-->0;)if(stage[k]){s=wc_remove(r->handle,rec->items[k].new_stage,&rec->items[k].new_id);if(s!=FS_READ_OK)goto done;}
        wb_crash(32);
        /* Pins stay until the journal is gone, as for create. */
        s=wc_remove(r->handle,WB_JOURNAL,&jid);if(s!=FS_READ_OK)goto done;
        wb_crash(33);
        for(k=0;k<count;k++){
            if(newpin[k]){s=wc_remove(r->handle,rec->items[k].new_pin,&rec->items[k].new_id);if(s!=FS_READ_OK)goto done;}
            if(oldpin[k]){s=wc_remove(r->handle,rec->items[k].old_pin,&rec->items[k].old_id);if(s!=FS_READ_OK)goto done;}
        }
    }else{
        for(k=0;k<count;k++)if(stage[k]){s=wc_remove(r->handle,rec->items[k].new_stage,&rec->items[k].new_id);if(s!=FS_READ_OK)goto done;}
        wb_crash(34);
        for(k=0;k<count;k++){
            if(newpin[k]){s=wc_remove(r->handle,rec->items[k].new_pin,&rec->items[k].new_id);if(s!=FS_READ_OK)goto done;}
            if(oldpin[k]){s=wc_remove(r->handle,rec->items[k].old_pin,&rec->items[k].old_id);if(s!=FS_READ_OK)goto done;}
        }
        wb_crash(35);wc_probe_dir(r->handle);
        if(journal){s=wc_remove(r->handle,WB_JOURNAL,&jid);if(s!=FS_READ_OK)goto done;}
        wb_crash(36);
        /* The marker owns the recovery record until its very last deletion. */
        for(k=0;k<count;k++){
            s=wr_verify(parent[k],leaf[k],&rec->items[k].new_id,(size_t)rec->items[k].new_size,rec->items[k].new_digest);
            if(s!=FS_READ_OK)goto done;
        }
        s=wc_remove(r->handle,WB_MARKER,&mid);if(s!=FS_READ_OK)goto done;
    }
    wc_probe_dir(r->handle);s=FS_READ_OK;
done:
    for(k=0;k<WB_LIMIT;k++)if(parent[k]!=INVALID_HANDLE_VALUE)CloseHandle(parent[k]);
    free(j);free(m);return s;
}
/* Before a journal exists nothing else refers to these names. */
static void wbr_discard_unjournaled(const FS_READ_ROOT *r,WBR_RECORD *rec,HANDLE *stage,unsigned count)
{
    for(unsigned k=0;k<count;k++){
        WBR_ITEM *i=&rec->items[k];
        if(stage[k]!=INVALID_HANDLE_VALUE){wc_delete(stage[k]);CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
        if(i->new_id.VolumeSerialNumber&&i->new_pin[0])(void)wc_remove(r->handle,i->new_pin,&i->new_id);
        if(i->old_id.VolumeSerialNumber&&i->old_pin[0])(void)wc_remove(r->handle,i->old_pin,&i->old_id);
    }
}
static FS_READ_STATUS wb_batch_replace(const FS_READ_ROOT *r,const FS_BATCH_REPLACE *e,size_t n)
{
    HANDLE lock=INVALID_HANDLE_VALUE;OVERLAPPED overlap;WBR_RECORD *rec=NULL;
    HANDLE parent[WB_LIMIT],old[WB_LIMIT],stage[WB_LIMIT];WCHAR *wide[WB_LIMIT];const WCHAR *leaf[WB_LIMIT];
    int journaled=0,committed=0,rollback=0;unsigned k;FS_READ_STATUS s;
    for(k=0;k<WB_LIMIT;k++){parent[k]=old[k]=stage[k]=INVALID_HANDLE_VALUE;wide[k]=NULL;leaf[k]=NULL;}
    if(!r)return FS_READ_INVALID;
    s=wb_validate_replace(e,n);if(s!=FS_READ_OK)return s;
    if(!wc_ntfs(r->handle))return FS_READ_UNSUPPORTED;
    rec=(WBR_RECORD*)calloc(1,sizeof(*rec));
    if(!rec)return FS_READ_IO;
    s=wc_lock(r->handle,&lock,&overlap);if(s!=FS_READ_OK){free(rec);return s;}
    s=wo_clear_other(r->handle,4);if(s!=FS_READ_OK)goto end;
    {int kind=0,present=0;
     /* Any pending batch, create or replace, blocks a new one until recovery. */
     s=wb_kind(r->handle,WB_JOURNAL,&kind,&present);if(present){s=FS_READ_DENIED;goto end;}if(s!=FS_READ_OK)goto end;
     s=wb_kind(r->handle,WB_MARKER,&kind,&present);if(present){s=FS_READ_DENIED;goto end;}if(s!=FS_READ_OK)goto end;}
    rec->magic=WBR_MAGIC;rec->version=1;rec->count=(unsigned)n;
    if(!wc_id(r->handle,&rec->root)){s=FS_READ_UNSUPPORTED;goto end;}
    /* Validate every target before anything is linked or created. The link
       count is read before the pin exists, because the pin makes it 2. */
    for(k=0;k<n;k++){
        WBR_ITEM *i=&rec->items[k];FS_READ_META meta;DWORD links=0;
        wide[k]=win_wide(e[k].target);if(!wide[k]){s=FS_READ_INVALID;goto end;}
        s=wc_parent(r,wide[k],&parent[k],&leaf[k]);if(s!=FS_READ_OK)goto end;
        if(!wc_id(parent[k],&i->parent)||i->parent.VolumeSerialNumber!=rec->root.VolumeSerialNumber){
            s=FS_READ_UNSUPPORTED;goto end;}
        s=wc_open(parent[k],leaf[k],GENERIC_READ|DELETE,FILE_OPEN,&old[k]);if(s!=FS_READ_OK)goto end;
        if(win_meta(old[k],&meta)!=FS_READ_OK||meta.kind!=FS_KIND_FILE||
           meta.size!=e[k].expected_len||!wc_id(old[k],&i->old_id)||
           i->old_id.VolumeSerialNumber!=rec->root.VolumeSerialNumber){s=FS_READ_DENIED;goto end;}
        /* A read-only target cannot be renamed over (0xc0000022): refuse now. */
        if(!(meta.mode&0222)){s=FS_READ_DENIED;goto end;}
        if(!wb_mutant(6)&&(!wb_links(old[k],&links)||links!=1)){s=FS_READ_DENIED;goto end;}
        i->old_size=e[k].expected_len;i->old_digest=wc_hash((const unsigned char*)e[k].expected,e[k].expected_len);
        i->new_size=e[k].replacement_len;i->new_digest=wc_hash((const unsigned char*)e[k].replacement,e[k].replacement_len);
        s=wo_bytes(old[k],&i->old_id,e[k].expected_len,i->old_digest);if(s!=FS_READ_OK)goto end;
        wcscpy(i->target,wide[k]);
        if(!wc_random(i->old_pin,L".fsrp-")||!wc_random(i->new_stage,L".fst-")||
           !wc_random(i->new_pin,L".fsp-")||!wc_random(i->rollback,L".fsrb-")){s=FS_READ_IO;goto end;}
    }
    /* Pin every old inode, then stage and pin every new image. No journal
       exists yet: a failure discards our own names and leaves the tree as it was. */
    for(k=0;k<n;k++){
        if(!wc_set_name(old[k],r->handle,rec->items[k].old_pin,WC_NT_LINK)){s=FS_READ_UNSUPPORTED;goto unjournaled;}
        if(k==0)wb_crash(21); /* first old pin */
    }
    for(k=0;k<n;k++){
        WBR_ITEM *i=&rec->items[k];size_t used=0;DWORD wrote=0;int exists=0;
        s=wc_open(r->handle,i->new_stage,GENERIC_WRITE|GENERIC_READ|DELETE,FILE_CREATE,&stage[k]);
        if(s!=FS_READ_OK)goto unjournaled;
        if(k==0)wb_crash(22); /* first stage */
        while(used<e[k].replacement_len){
            if(!WriteFile(stage[k],(const unsigned char*)e[k].replacement+used,(DWORD)(e[k].replacement_len-used),&wrote,NULL)||!wrote){
                s=FS_READ_IO;goto unjournaled;}
            used+=wrote;
        }
        if(!FlushFileBuffers(stage[k])||!wc_id(stage[k],&i->new_id)||
           i->new_id.VolumeSerialNumber!=rec->root.VolumeSerialNumber||wc_same(&i->old_id,&i->new_id)){
            s=FS_READ_IO;goto unjournaled;}
        if(!wc_set_name(stage[k],r->handle,i->new_pin,WC_NT_LINK)){s=FS_READ_UNSUPPORTED;goto unjournaled;}
        s=wr_named(r->handle,i->old_pin,&i->old_id,&exists);if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto unjournaled;}
        s=wr_named(r->handle,i->new_pin,&i->new_id,&exists);if(s!=FS_READ_OK||!exists){s=FS_READ_PENDING;goto unjournaled;}
        s=wb_pin_bytes(r->handle,i->old_pin,&i->old_id,i->old_size,i->old_digest);if(s!=FS_READ_OK)goto unjournaled;
        s=wb_pin_bytes(r->handle,i->new_pin,&i->new_id,i->new_size,i->new_digest);if(s!=FS_READ_OK)goto unjournaled;
    }
    wb_crash(23); /* every pin and stage, no journal */
    rec->checksum=wbr_checksum(rec);
    if(!wb_replace_record_valid(rec)){s=FS_READ_INVALID;goto unjournaled;}
    wc_probe_dir(r->handle);
    s=wb_publish(r->handle,WB_JOURNAL,rec,sizeof(*rec));
    if(s!=FS_READ_OK){
        /* The rename may or may not have happened: if a journal now exists it
           owns the pins, so leave everything for recovery. */
        int kind=0,now=0;
        if(wb_kind(r->handle,WB_JOURNAL,&kind,&now)==FS_READ_OK&&!now)goto unjournaled;
        s=FS_READ_PENDING;journaled=1;goto end;
    }
    journaled=1;wb_crash(24); /* complete journal */
    for(k=0;k<n;k++){
        const WBR_ITEM *i=&rec->items[k];
        WB_BEFORE_PUBLISH(k);
        /* Final gate: the target is still the pinned inode with the expected bytes. */
        s=wr_verify(parent[k],leaf[k],&i->old_id,(size_t)i->old_size,i->old_digest);
        if(s!=FS_READ_OK){s=FS_READ_DENIED;rollback=1;goto end;}
        /* Close-then-rename: our own old handle must not be what blocks it. */
        CloseHandle(old[k]);old[k]=INVALID_HANDLE_VALUE;
        if(wb_inject_publish_failure(k)||!wr_replace_name(stage[k],parent[k],leaf[k])){
            s=FS_READ_PENDING;rollback=1;goto end;}
        if(k==0)wb_crash(25); /* first target replaced */
    }
    wb_crash(26); /* every target replaced, no marker */
    for(k=0;k<n;k++){
        const WBR_ITEM *i=&rec->items[k];
        s=wr_verify(parent[k],leaf[k],&i->new_id,(size_t)i->new_size,i->new_digest);
        if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
        wc_probe_dir(parent[k]);
    }
    s=wb_publish(r->handle,WB_MARKER,rec,sizeof(*rec));
    if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    committed=1;wb_crash(27); /* marker and journal */
    for(k=0;k<n;k++)if(stage[k]!=INVALID_HANDLE_VALUE){CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
    s=wb_recover_locked(r);if(s!=FS_READ_OK){s=FS_READ_PENDING;goto end;}
    wb_crash(28); /* committed cleanup complete */
    s=FS_READ_OK;goto end;
unjournaled:
    wbr_discard_unjournaled(r,rec,stage,(unsigned)n);
end:
    for(k=0;k<WB_LIMIT;k++){
        if(stage[k]!=INVALID_HANDLE_VALUE){CloseHandle(stage[k]);stage[k]=INVALID_HANDLE_VALUE;}
        if(old[k]!=INVALID_HANDLE_VALUE){CloseHandle(old[k]);old[k]=INVALID_HANDLE_VALUE;}
    }
    /* Journal published, no marker, and the failure is one we can undo now
       (changed target or refused rename): restore the tree. A failed rollback
       leaves the journal for FsBatchRecover and reports PENDING. */
    if(journaled&&!committed&&rollback){
        FS_READ_STATUS rs=wb_recover_locked(r);
        if(rs!=FS_READ_OK)s=FS_READ_PENDING;
    }
    for(k=0;k<WB_LIMIT;k++){if(parent[k]!=INVALID_HANDLE_VALUE)CloseHandle(parent[k]);free(wide[k]);}
    if(lock!=INVALID_HANDLE_VALUE)wc_unlock(lock,&overlap);
    free(rec);return s;
}
#ifdef FS_CREATE_TEST_CRASH
/* Test hooks: the record layouts stay private to this file. */
FS_READ_STATUS FsWinBatchTestValidateCreate(const FS_BATCH_CREATE *e,size_t n){return wb_validate_create(e,n);}
FS_READ_STATUS FsWinBatchTestValidateReplace(const FS_BATCH_REPLACE *e,size_t n){return wb_validate_replace(e,n);}
/* Test only: the status wc_open reports for a leaf of the root (used to
   measure the STATUS_DELETE_PENDING mapping). */
void FsWinBatchTestSetBeforePublish(void (*f)(unsigned k)){wb_before_publish=f;}
FS_READ_STATUS FsWinBatchTestOpenStatus(const FS_READ_ROOT *r,const char *leaf)
{
    WCHAR *w=win_wide(leaf);HANDLE h=INVALID_HANDLE_VALUE;FS_READ_STATUS s;
    if(!w||!r)return FS_READ_INVALID;
    s=wc_open(r->handle,w,FILE_READ_ATTRIBUTES,FILE_OPEN,&h);
    if(s==FS_READ_OK)CloseHandle(h);
    free(w);return s;
}
static void wb_hexname(WCHAR out[48],const WCHAR *prefix,unsigned tag,unsigned k)
{
    size_t p=wcslen(prefix);
    wcscpy(out,prefix);
    for(size_t i=0;i<32;i++)out[p+i]=L'0';
    out[p]=L"0123456789abcdef"[tag&15];out[p+1]=L"0123456789abcdef"[k&15];out[p+32]=0;
}
static void wb_id(FILE_ID_INFO *id,unsigned volume,unsigned seed)
{memset(id,0,sizeof(*id));id->VolumeSerialNumber=volume;id->FileId.Identifier[0]=(BYTE)seed;id->FileId.Identifier[1]=1;}
static void wb_target(WCHAR *t,unsigned k)
{wcscpy(t,L"dir/file0");t[8]=(WCHAR)(L'0'+k);}
/* kind 0 = create record, 1 = replace record. Returns the record size, or 0. */
size_t FsWinBatchTestRecordSize(int kind){return kind==0?sizeof(WB_RECORD):kind==1?sizeof(WBR_RECORD):0;}
int FsWinBatchTestBuild(int kind,unsigned count,void *out)
{
    if(count>WB_LIMIT)return 0;
    if(kind==0){
        WB_RECORD *r=(WB_RECORD*)out;memset(r,0,sizeof(*r));
        r->magic=WB_MAGIC;r->version=1;r->count=count;wb_id(&r->root,7,1);
        for(unsigned k=0;k<count;k++){
            WB_ITEM *i=&r->items[k];wb_target(i->target,k);
            wb_hexname(i->stage,L".fst-",1,k);wb_hexname(i->pin,L".fsp-",2,k);
            wb_id(&i->parent,7,2);wb_id(&i->file,7,(unsigned)(10+k));i->size=3;i->mode=0644;
        }
        r->checksum=wb_checksum(r);return 1;
    }
    if(kind==1){
        WBR_RECORD *r=(WBR_RECORD*)out;memset(r,0,sizeof(*r));
        r->magic=WBR_MAGIC;r->version=1;r->count=count;wb_id(&r->root,7,1);
        for(unsigned k=0;k<count;k++){
            WBR_ITEM *i=&r->items[k];wb_target(i->target,k);
            wb_hexname(i->old_pin,L".fsrp-",1,k);wb_hexname(i->new_stage,L".fst-",2,k);
            wb_hexname(i->new_pin,L".fsp-",3,k);wb_hexname(i->rollback,L".fsrb-",4,k);
            wb_id(&i->parent,7,2);wb_id(&i->old_id,7,(unsigned)(10+k));
            wb_id(&i->new_id,7,(unsigned)(40+k));i->old_size=3;i->new_size=4;
        }
        r->checksum=wbr_checksum(r);return 1;
    }
    return 0;
}
int FsWinBatchTestValid(int kind,const void *rec)
{
    return kind==0?wb_create_record_valid((const WB_RECORD*)rec):
           kind==1?wb_replace_record_valid((const WBR_RECORD*)rec):0;
}
/* Mutations of a valid record with at least 3 items. Returns 1 when applied,
   0 when the mutation does not apply to that record kind. Mutation 1 leaves the
   checksum stale on purpose; every other mutation reseals it, so only the
   semantic check can refuse it. */
int FsWinBatchTestMutate(int kind,void *rec,int m)
{
    WB_RECORD *c=(WB_RECORD*)rec;WBR_RECORD *p=(WBR_RECORD*)rec;
    WCHAR *t0,*t1,*stage1,*pin1,*stage0;unsigned *count,*magic,*version;FILE_ID_INFO *parent1;
    if(kind==0){t0=c->items[0].target;t1=c->items[1].target;stage0=c->items[0].stage;stage1=c->items[1].stage;
        pin1=c->items[1].pin;count=&c->count;magic=&c->magic;version=&c->version;parent1=&c->items[1].parent;}
    else if(kind==1){t0=p->items[0].target;t1=p->items[1].target;stage0=p->items[0].new_stage;stage1=p->items[1].new_stage;
        pin1=p->items[1].new_pin;count=&p->count;magic=&p->magic;version=&p->version;parent1=&p->items[1].parent;}
    else return 0;
    switch(m){
    case 1:t0[0]^=1;return 1;                                  /* stale checksum */
    case 2:*magic^=1;break;
    case 3:*version=2;break;
    case 4:*count=1;break;
    case 5:*count=WB_LIMIT+1;break;
    case 6:wcscpy(t1,t0);t1[4]=L'F';break;                     /* same file, other case */
    case 7:wcscpy(stage1,stage0);break;
    case 8:wcscpy(t0,L".fstxn.batch");break;
    case 9:for(size_t i=0;i<WC_MAX_PATH;i++)t0[i]=L'a';break;   /* no terminator */
    case 10:stage1[1]=L'x';break;                              /* wrong prefix */
    case 11:stage1[10]=L'g';break;                             /* not hex */
    case 12:wcscpy(pin1,stage1);break;                         /* pin is stage */
    case 13:parent1->VolumeSerialNumber=8;break;
    case 14:if(kind==0)c->items[WB_LIMIT-1].size=1;else p->items[WB_LIMIT-1].old_size=1;break;
    case 15:wcscpy(t1,t0);wcscat(t1,L"/x");break;               /* a and a/x */
    case 16:wcscpy(t0,L"dir/CON");break;
    case 17:if(kind==0)return 0;p->items[0].new_id=p->items[0].old_id;break;
    case 18:{WCHAR tmp[48];wcscpy(tmp,stage1);wcscpy(stage1,pin1);wcscpy(pin1,tmp);break;} /* roles swapped */
    default:return 0;
    }
    if(kind==0)c->checksum=wb_checksum(c);else p->checksum=wbr_checksum(p);
    return 1;
}
#endif
