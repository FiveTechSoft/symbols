#include "engineering_episode.h"
#include "atomic_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#define HEADER "SYMBOLS-ENGINEERING-EPISODES\t1\n"
#define NF 20
static const size_t offsets[] = {
    offsetof(ENGINEERING_EPISODE, episode_id), offsetof(ENGINEERING_EPISODE, run_id),
    offsetof(ENGINEERING_EPISODE, attempt_id), offsetof(ENGINEERING_EPISODE, parent_id),
    offsetof(ENGINEERING_EPISODE, repo_sha), offsetof(ENGINEERING_EPISODE, workspace_before),
    offsetof(ENGINEERING_EPISODE, workspace_after), offsetof(ENGINEERING_EPISODE, task_signature),
    offsetof(ENGINEERING_EPISODE, goal_provenance), offsetof(ENGINEERING_EPISODE, engine),
    offsetof(ENGINEERING_EPISODE, op), offsetof(ENGINEERING_EPISODE, patch_hash),
    offsetof(ENGINEERING_EPISODE, oracle), offsetof(ENGINEERING_EPISODE, oracle_version),
    offsetof(ENGINEERING_EPISODE, diagnostic), offsetof(ENGINEERING_EPISODE, outcome),
    offsetof(ENGINEERING_EPISODE, rollback)
};
static int valid(const char *s, int empty)
{
    size_t n = 0;
    while (n < EE_STR && s[n]) ++n;
    if (n == EE_STR || (!empty && !n)) return 0;
    for (size_t i=0; i<n; ++i) if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127) return 0;
    return 1;
}
static int row_valid(const ENGINEERING_EPISODE *r, const EPISODE_STORE *s)
{
    for (int i=0;i<17;++i)
        if (!valid((const char *)r+offsets[i], i != 0 && i != 1 && i != 2)) return 0;
    if (strcmp(r->outcome,"verified") && strcmp(r->outcome,"refuted") &&
        strcmp(r->outcome,"abstained") && strcmp(r->outcome,"write_failed") &&
        strcmp(r->outcome,"rollback_failed") && strcmp(r->outcome,"verification_incomplete")) return 0;
    if (strcmp(r->rollback,"not_needed") && strcmp(r->rollback,"confirmed") &&
        strcmp(r->rollback,"failed") && strcmp(r->rollback,"unknown")) return 0;
    if (!strcmp(r->outcome,"verified") && !strcmp(r->rollback,"failed")) return 0;
    if (!strcmp(r->outcome,"rollback_failed") && strcmp(r->rollback,"failed")) return 0;
    for (size_t i=0;i<s->count;++i) {
        const ENGINEERING_EPISODE *p=&s->rows[i];
        if (!strcmp(p->episode_id,r->episode_id) ||
            (!strcmp(p->run_id,r->run_id) && !strcmp(p->attempt_id,r->attempt_id))) return 0;
    }
    if (r->parent_id[0]) {
        int found=0;
        for (size_t i=0;i<s->count;++i)
            if (!strcmp(s->rows[i].run_id,r->run_id) &&
                !strcmp(s->rows[i].attempt_id,r->parent_id)) found=1;
        if (!found) return 0;
    }
    return 1;
}
static uint64_t digest(const unsigned char *p, size_t n)
{
    uint64_t h=UINT64_C(1469598103934665603);
    for (size_t i=0;i<n;++i) {h^=p[i];h*=UINT64_C(1099511628211);}
    return h;
}
static int put_field(char **dst, size_t *left, const char *s)
{
    static const char h[]="0123456789ABCDEF";
    for (;*s;++s) {
        unsigned char c=(unsigned char)*s;
        if (*left<4) return 0;
        if (c=='%' || c=='\t' || c=='\n' || c=='\r') {
            (*dst)[0]='%';(*dst)[1]=h[c>>4];(*dst)[2]=h[c&15];*dst+=3;*left-=3;
        } else { *(*dst)++=(char)c;--*left; }
    }
    return 1;
}
static int serialize(const EPISODE_STORE *s, char *b, size_t cap, size_t *used)
{
    char *p=b;size_t left=cap;
    int n=snprintf(p,left,"%s",HEADER);if(n<0 || (size_t)n>=left)return 0;p+=n;left-=n;
    for(size_t i=0;i<s->count;++i) {
        const ENGINEERING_EPISODE *r=&s->rows[i];
        for(int k=0;k<17;++k) {
            if(k) {if(left<2)return 0;*p++='\t';--left;}
            if(!put_field(&p,&left,(const char *)r+offsets[k]))return 0;
        }
        n=snprintf(p,left,"\t%u\t%u\t%u\n",r->candidate_builds,r->probes,r->tool_calls);
        if(n<0 || (size_t)n>=left)return 0;p+=n;left-=n;
    }
    uint64_t sum=digest((const unsigned char *)b,(size_t)(p-b));
    n=snprintf(p,left,"CHECKSUM\t%016llx\n",(unsigned long long)sum);
    if(n<0 || (size_t)n>=left)return 0;p+=n;*used=(size_t)(p-b);return 1;
}
static int decode(char *out,const char *src,size_t n)
{
    size_t j=0;
    for(size_t i=0;i<n;++i) {
        unsigned char c=(unsigned char)src[i];
        if(c=='%') {
            if(i+2>=n)return 0;
            int hi=src[i+1]>='0'&&src[i+1]<='9'?src[i+1]-'0':src[i+1]>='A'&&src[i+1]<='F'?src[i+1]-'A'+10:-1;
            int lo=src[i+2]>='0'&&src[i+2]<='9'?src[i+2]-'0':src[i+2]>='A'&&src[i+2]<='F'?src[i+2]-'A'+10:-1;
            if(hi<0||lo<0)return 0;c=(unsigned char)(hi*16+lo);
            if(c!='%' && c!='\t' && c!='\n' && c!='\r')return 0;
            i+=2;
        }
        if(j+1>=EE_STR)return 0;
        out[j++]=(char)c;
    }
    out[j]=0;return valid(out,1);
}
static int number(const char *p,size_t n,uint32_t *v)
{
    if(!n || n>10 || (n>1 && *p=='0'))return 0;
    unsigned long long x=0;
    for(size_t i=0;i<n;++i) {if(p[i]<'0'||p[i]>'9')return 0;x=x*10+(unsigned)(p[i]-'0');}
    if(x>UINT32_MAX)return 0;*v=(uint32_t)x;return 1;
}
static int parse_row(const char *p,size_t len,ENGINEERING_EPISODE *r)
{
    size_t start=0;int field=0;
    for(size_t i=0;i<=len;++i) if(i==len||p[i]=='\t') {
        if(field<17) {
            if(!decode((char *)r+offsets[field],p+start,i-start))return 0;
        } else if(field<20) {
            uint32_t *v=field==17?&r->candidate_builds:field==18?&r->probes:&r->tool_calls;
            if(!number(p+start,i-start,v))return 0;
        } else return 0;
        ++field;start=i+1;
    }
    return field==NF;
}
int EpisodeLoad(const char *path,EPISODE_STORE *out)
{
    if(!path || !out)return -1;
    memset(out,0,sizeof(*out));
    FILE *f=fopen(path,"rb");if(!f)return errno==ENOENT?0:-1;
    char *b=(char *)malloc(EE_MAX_IMAGE+1);if(!b){fclose(f);return -1;}
    size_t n=fread(b,1,EE_MAX_IMAGE+1,f);int error=ferror(f);if(fclose(f))error=1;
    if(error || n>EE_MAX_IMAGE || n<strlen(HEADER)+26 || memcmp(b,HEADER,strlen(HEADER)))goto bad;
    const char *p=b+strlen(HEADER),*end=b+n;
    while(p<end) {
        const char *nl=memchr(p,'\n',(size_t)(end-p));if(!nl)goto bad;
        if((size_t)(nl-p)>=9 && !memcmp(p,"CHECKSUM\t",9)) {
            if((size_t)(nl-p)!=25)goto bad;
            unsigned long long sum=0;
            for(int i=9;i<25;++i) {
                char c=p[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))goto bad;
                sum=(sum<<4)+(unsigned)(c<='9'?c-'0':c-'a'+10);
            }
            if(nl+1!=end || sum!=digest((const unsigned char *)b,(size_t)(p-b)) || !out->count)goto bad;
            /* Canonical bytes only: a checksum over a noncanonical encoding
               must not smuggle alternate meanings through the parser. */
            char *canonical=(char *)malloc(EE_MAX_IMAGE);
            size_t canonical_n=0;
            int canonical_ok=canonical && serialize(out,canonical,EE_MAX_IMAGE,&canonical_n) &&
                             canonical_n==n && !memcmp(canonical,b,n);
            free(canonical);
            if(!canonical_ok)goto bad;
            free(b);return 1;
        }
        if(out->count>=EE_MAX_RECORDS)goto bad;
        ENGINEERING_EPISODE *r=&out->rows[out->count];
        memset(r,0,sizeof(*r));
        if(!parse_row(p,(size_t)(nl-p),r)||!row_valid(r,out))goto bad;
        ++out->count;p=nl+1;
    }
bad:
    memset(out,0,sizeof(*out));free(b);return -1;
}
static int lock_store(const char *path, char *lock, size_t cap)
{
    int n=snprintf(lock,cap,"%s.lock",path);
    if(n<0 || (size_t)n>=cap)return 0;
#ifdef _WIN32
    return CreateDirectoryA(lock,NULL)!=0;
#else
    return mkdir(lock,0700)==0;
#endif
}
static void unlock_store(const char *lock)
{
#ifdef _WIN32
    RemoveDirectoryA(lock);
#else
    rmdir(lock);
#endif
}
int EpisodeAppend(const char *path,const ENGINEERING_EPISODE *row)
{
    if(!path || !row)return 0;
    /* Exclude a second writer from the entire read/validate/rewrite cycle.
       A stale lock after process death fails closed pending manual inspection. */
    if(strlen(path)>506)return 0;
    char lock[520]; if(!AtomicStorePrepareParent(path) || !lock_store(path,lock,sizeof(lock)))return 0;
    EPISODE_STORE *s=(EPISODE_STORE *)calloc(1,sizeof(*s));if(!s){unlock_store(lock);return 0;}
    int state=EpisodeLoad(path,s),ok=0;
    if(state>=0 && s->count<EE_MAX_RECORDS && row_valid(row,s)) {
        s->rows[s->count++]=*row;
        char *b=(char *)malloc(EE_MAX_IMAGE);
        size_t n=0;
        if(b && serialize(s,b,EE_MAX_IMAGE,&n))ok=AtomicStoreReplace(path,b,n);
        free(b);
    }
    free(s);unlock_store(lock);return ok;
}
