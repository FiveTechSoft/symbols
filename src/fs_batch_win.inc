/* Windows batch (W1): journal record formats and batch-level validation.
   Compiled only into the native test binaries (FS_CREATE_TEST_CRASH). Nothing
   here is reachable from FsBatchCreate, FsBatchReplace or FsBatchRecover, which
   still return FS_READ_UNSUPPORTED on Windows. Included at the end of
   fs_create_win.inc, after wc_target_status and the other wc_ helpers. */
#ifdef FS_CREATE_TEST_CRASH
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
/* Test hooks: the record layouts stay private to this file. */
FS_READ_STATUS FsWinBatchTestValidateCreate(const FS_BATCH_CREATE *e,size_t n){return wb_validate_create(e,n);}
FS_READ_STATUS FsWinBatchTestValidateReplace(const FS_BATCH_REPLACE *e,size_t n){return wb_validate_replace(e,n);}
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
