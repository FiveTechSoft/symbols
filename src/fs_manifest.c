#include "fs_manifest.h"
#include <stdlib.h>
#include <string.h>
#define FS_MANIFEST_MAX 64
static char *copy_string(const char *s)
{
    size_t n=strlen(s)+1;char *p=(char*)malloc(n);
    if(p)memcpy(p,s,n);
    return p;
}
void FsManifestFree(FS_MANIFEST *m)
{
    if(!m)return;
    for(size_t i=0;i<m->count;i++){free(m->items[i].path);free(m->items[i].other);}
    free(m->items);m->items=NULL;m->count=0;
}
static int valid_component(const char *p,size_t n)
{
    if(!n||(n==1&&p[0]=='.')||(n==2&&p[0]=='.'&&p[1]=='.'))return 0;
#ifdef _WIN32
    /* Avoid Win32 aliases (device names, trailing dot/space and Unicode case).
       This conservative manifest is not a promise that writes are safe yet. */
    if(p[n-1]=='.'||p[n-1]==' ')return 0;
    for(size_t i=0;i<n;i++)if((unsigned char)p[i]>127 ||
       p[i]=='<'||p[i]=='>'||p[i]=='"'||p[i]=='|'||
       p[i]=='?'||p[i]=='*')return 0;
    {char name[5]={0};size_t k=0;
     while(k<n&&k<4&&p[k]!='.'){
         char c=p[k];name[k]=(c>='a'&&c<='z')?(char)(c-32):c;k++;
     }
     if((k==3&&(!memcmp(name,"CON",3)||!memcmp(name,"PRN",3)||
                !memcmp(name,"AUX",3)||!memcmp(name,"NUL",3)))||
        (k==4&&(!memcmp(name,"COM",3)||!memcmp(name,"LPT",3))&&
         name[3]>='1'&&name[3]<='9'))return 0;
    }
#endif
    return 1;
}
static int valid_path(const char *p)
{
    const char *component;
    if(!p||!*p||*p=='/'||*p=='\\')return 0;
    component=p;
    for(;*p;p++){
        if(*p=='\\'||*p==':'||(unsigned char)*p<32)return 0;
        if(*p=='/'){
            size_t n=(size_t)(p-component);
            if(!valid_component(component,n))return 0;
            component=p+1;
        }
    }
    return valid_component(component,(size_t)(p-component));
}
static FS_READ_STATUS check_leaf(const FS_READ_ROOT *root,const char *p,int must_exist)
{
    char *parent,*slash;FS_READ_META meta;FS_READ_STATUS s;
    if(!valid_path(p))return FS_READ_INVALID;
    parent=copy_string(p);if(!parent)return FS_READ_IO;
    slash=strrchr(parent,'/');if(slash)*slash=0;
    else *parent=0;
    s=FsReadStat(root,parent,&meta);free(parent);
    if(s!=FS_READ_OK)return s;
    if(meta.kind!=FS_KIND_DIR)return FS_READ_UNSUPPORTED;
    s=FsReadStat(root,p,&meta);
    if(must_exist)return s==FS_READ_OK&&meta.kind!=FS_KIND_FILE?FS_READ_UNSUPPORTED:s;
    if(s==FS_READ_MISSING)return FS_READ_OK;
    return s==FS_READ_OK?FS_READ_DENIED:s;
}
static int overlap(const char *a,const char *b)
{
    size_t na=strlen(a),nb=strlen(b);
#ifdef _WIN32
    return !_stricmp(a,b)||(na<nb&&!_strnicmp(a,b,na)&&b[na]=='/')||
           (nb<na&&!_strnicmp(a,b,nb)&&a[nb]=='/');
#else
    return !strcmp(a,b)||(na<nb&&!strncmp(a,b,na)&&b[na]=='/')||
           (nb<na&&!strncmp(a,b,nb)&&a[nb]=='/');
#endif
}
static int append(FS_MANIFEST *m,FS_EFFECT_KIND kind,const char *p,const char *other)
{
    FS_EFFECT *e=&m->items[m->count];
    e->kind=kind;e->path=copy_string(p);e->other=other?copy_string(other):NULL;
    if(!e->path||(other&&!e->other)){free(e->path);free(e->other);return 0;}
    m->count++;return 1;
}
FS_READ_STATUS FsManifestPlan(const FS_READ_ROOT *root,const FS_OP_REQUEST *req,
                              size_t count,FS_MANIFEST *out)
{
    FS_MANIFEST pending={0};FS_READ_STATUS s=FS_READ_INVALID;
    if(!out)return FS_READ_INVALID;
    out->items=NULL;out->count=0;
    if(!root||!req||!count||count>FS_MANIFEST_MAX)return FS_READ_INVALID;
    if(count>SIZE_MAX/2/sizeof(FS_EFFECT))return FS_READ_INVALID;
    pending.items=(FS_EFFECT*)calloc(count*2,sizeof(FS_EFFECT));
    if(!pending.items)return FS_READ_IO;
    for(size_t i=0;i<count;i++){
        const FS_OP_REQUEST *r=&req[i];int source_needed,target_needed;
        const char *paths[2];
        if(r->kind<FS_OP_CREATE||r->kind>FS_OP_REMOVE){s=FS_READ_INVALID;goto fail;}
        source_needed=r->kind==FS_OP_MOVE||r->kind==FS_OP_COPY||r->kind==FS_OP_REMOVE;
        target_needed=r->kind!=FS_OP_REMOVE;
        if((source_needed&&!valid_path(r->source))||
           (target_needed&&!valid_path(r->target))||
           (!source_needed&&r->source)||(!target_needed&&r->target)){
            s=FS_READ_INVALID;goto fail;
        }
        paths[0]=source_needed?r->source:NULL;paths[1]=target_needed?r->target:NULL;
        for(size_t j=0;j<2;j++)if(paths[j]){
            for(size_t k=0;k<i;k++){
                if(req[k].source&&overlap(paths[j],req[k].source)){s=FS_READ_DENIED;goto fail;}
                if(req[k].target&&overlap(paths[j],req[k].target)){s=FS_READ_DENIED;goto fail;}
            }
        }
        if(source_needed&&target_needed&&overlap(r->source,r->target)){
            s=FS_READ_DENIED;goto fail;
        }
        if(source_needed){s=check_leaf(root,r->source,1);if(s!=FS_READ_OK)goto fail;}
        if(target_needed){
            int exists=r->kind==FS_OP_REPLACE;
            s=check_leaf(root,r->target,exists);if(s!=FS_READ_OK)goto fail;
        }
        switch(r->kind){
        case FS_OP_CREATE:
            if(!append(&pending,FS_EFFECT_WRITE,r->target,NULL))goto oom;
            break;
        case FS_OP_REPLACE:
            if(!append(&pending,FS_EFFECT_READ,r->target,NULL)||
               !append(&pending,FS_EFFECT_WRITE,r->target,NULL))goto oom;
            break;
        case FS_OP_MOVE:
            if(!append(&pending,FS_EFFECT_READ,r->source,NULL)||
               !append(&pending,FS_EFFECT_RENAME,r->source,r->target))goto oom;
            break;
        case FS_OP_COPY:
            if(!append(&pending,FS_EFFECT_READ,r->source,NULL)||
               !append(&pending,FS_EFFECT_WRITE,r->target,NULL))goto oom;
            break;
        case FS_OP_REMOVE:
            if(!append(&pending,FS_EFFECT_READ,r->source,NULL)||
               !append(&pending,FS_EFFECT_DELETE,r->source,NULL))goto oom;
            break;
        default: s=FS_READ_INVALID;goto fail;
        }
    }
    *out=pending;return FS_READ_OK;
oom:s=FS_READ_IO;
fail:FsManifestFree(&pending);return s;
}
