#include "fs_txn_plan.h"
#include <stdlib.h>
#include <string.h>
#define FS_TXN_MAX_OPS 64
#define FS_TXN_MAX_IMAGE (1024U*1024U)
static char *copy_path(const char *p)
{
    size_t n=strlen(p)+1;char *d=(char*)malloc(n);
    if(d)memcpy(d,p,n);
    return d;
}
void FsTxnFree(FS_TXN_PLAN *p)
{
    if(!p)return;
    for(size_t i=0;i<p->count;i++){
        free(p->before[i].path);free(p->before[i].bytes);
    }
    free(p->before);FsManifestFree(&p->effects);
    memset(p,0,sizeof(*p));
}
static FS_READ_STATUS snapshot(const FS_READ_ROOT *root,const char *path,
                               const void *expected,size_t expected_len,
                               FS_TXN_IMAGE *out)
{
    FS_READ_META m;unsigned char *bytes=NULL;size_t len=0;
    FS_READ_STATUS s;
    if(!expected)return FS_READ_INVALID;
    if(expected_len>FS_TXN_MAX_IMAGE)return FS_READ_UNSUPPORTED;
    s=FsReadFile(root,path,&bytes,&len,&m);
    if(s!=FS_READ_OK)return s;
    if(len!=expected_len||(!expected&&len)||
       (len&&memcmp(bytes,expected,len))){free(bytes);return FS_READ_DENIED;}
    out->path=copy_path(path);if(!out->path){free(bytes);return FS_READ_IO;}
    out->bytes=bytes;out->len=len;out->mode=m.mode;out->existed=1;
    return FS_READ_OK;
}
FS_READ_STATUS FsTxnPlan(const FS_READ_ROOT *root,const FS_TXN_REQUEST *req,
                          size_t count,FS_TXN_PLAN *out)
{
    FS_TXN_PLAN pending={0};FS_OP_REQUEST plain[FS_TXN_MAX_OPS];
    FS_READ_STATUS s;
    if(!out)return FS_READ_INVALID;
    memset(out,0,sizeof(*out));
    if(!root||!req||!count||count>FS_TXN_MAX_OPS)return FS_READ_INVALID;
    for(size_t i=0;i<count;i++)plain[i]=req[i].operation;
    s=FsManifestPlan(root,plain,count,&pending.effects);
    if(s!=FS_READ_OK)return s;
    pending.before=(FS_TXN_IMAGE*)calloc(count*2,sizeof(FS_TXN_IMAGE));
    if(!pending.before){s=FS_READ_IO;goto fail;}
    for(size_t i=0;i<count;i++){
        const FS_TXN_REQUEST *r=&req[i];FS_OP_KIND kind=r->operation.kind;
        if(kind==FS_OP_CREATE||kind==FS_OP_REPLACE){
            if(r->expected_source||r->expected_source_len){s=FS_READ_INVALID;goto fail;}
        }
        if((kind==FS_OP_MOVE||kind==FS_OP_COPY||kind==FS_OP_REMOVE)&&
           !r->expected_source){s=FS_READ_INVALID;goto fail;}
        if(kind==FS_OP_REPLACE&&!r->expected_target){s=FS_READ_INVALID;goto fail;}
        if(kind!=FS_OP_REPLACE&&
           (r->expected_target||r->expected_target_len)){s=FS_READ_INVALID;goto fail;}
        if(kind==FS_OP_REPLACE){
            FS_TXN_IMAGE *image=&pending.before[pending.count];
            s=snapshot(root,r->operation.target,r->expected_target,
                       r->expected_target_len,image);
            if(s!=FS_READ_OK)goto fail;
            pending.count++;
        }else if(kind==FS_OP_MOVE||kind==FS_OP_COPY||kind==FS_OP_REMOVE){
            FS_TXN_IMAGE *image=&pending.before[pending.count];
            s=snapshot(root,r->operation.source,r->expected_source,
                       r->expected_source_len,image);
            if(s!=FS_READ_OK)goto fail;
            pending.count++;
        }
        if(kind==FS_OP_CREATE||kind==FS_OP_COPY||kind==FS_OP_MOVE){
            FS_TXN_IMAGE *image=&pending.before[pending.count];
            image->path=copy_path(r->operation.target);
            if(!image->path){s=FS_READ_IO;goto fail;}
            image->existed=0;pending.count++;
        }
    }
    pending.state=FS_TXN_PLANNED;*out=pending;return FS_READ_OK;
fail:FsTxnFree(&pending);return s;
}
