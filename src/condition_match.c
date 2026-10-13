#include "condition_match.h"
#include "tl_sha256.h"
#include <string.h>

static int identifier(const char s[CM_ID_CAP], int allow_empty)
{
    size_t n=0, i=0;
    while(n<CM_ID_CAP && s[n]) n++;
    if(n==CM_ID_CAP || (!n && !allow_empty)) return 0;
    while(i<n) {
        unsigned char c=(unsigned char)s[i++]; uint32_t cp; unsigned more, j;
        if(c<0x80) { if(c<0x20 || c==0x7f) return 0; continue; }
        if(c>=0xc2 && c<=0xdf) {cp=c&31u;more=1;}
        else if(c>=0xe0 && c<=0xef) {cp=c&15u;more=2;}
        else if(c>=0xf0 && c<=0xf4) {cp=c&7u;more=3;}
        else return 0;
        if(n-i<more) return 0;
        for(j=0;j<more;j++){c=(unsigned char)s[i++];if((c&0xc0)!=0x80)return 0;cp=(cp<<6)|(c&63u);}
        if((more==1 && cp<0x80)||(more==2 && cp<0x800)||(more==3 && cp<0x10000)||cp>0x10ffff||(cp>=0xd800 && cp<=0xdfff))return 0;
    }
    return 1;
}
static int valid_key(const CM_REQUIREMENT *r)
{
    uint32_t i,j;
    if(r->version!=CM_VERSION || (r->polarity!=CM_POSITIVE && r->polarity!=CM_NEGATIVE)||r->nqual>CM_QUAL_MAX || !identifier(r->subject,0)||!identifier(r->relation,0))return 0;
    for(i=0;i<r->nqual;i++) {
        if(!identifier(r->qualifiers[i].key,0)||!identifier(r->qualifiers[i].value,0))return 0;
        for(j=0;j<i;j++)if(!strcmp(r->qualifiers[i].key,r->qualifiers[j].key))return 0;
    }
    return 1;
}
static int same_key(const CM_REQUIREMENT *a,const CM_REQUIREMENT *b)
{
    uint32_t i,j;
    if(strcmp(a->subject,b->subject)||strcmp(a->relation,b->relation))return 0;
#ifndef CM_MUTANT_IGNORE_QUALIFIERS
    if(a->nqual!=b->nqual)return 0;
    for(i=0;i<a->nqual;i++) {
        for(j=0;j<b->nqual;j++)if(!strcmp(a->qualifiers[i].key,b->qualifiers[j].key)&&!strcmp(a->qualifiers[i].value,b->qualifiers[j].value))break;
        if(j==b->nqual)return 0;
    }
#else
    (void)i;(void)j;
#endif
    return 1;
}
static int evidence_less(const CM_CLAIM *a,const CM_CLAIM *b)
{
    int cmp=memcmp(a->source_sha256,b->source_sha256,32);
    if(cmp)return cmp<0;
    if(a->first!=b->first)return a->first<b->first;
    if(a->end!=b->end)return a->end<b->end;
    return strcmp(a->object,b->object)<0;
}
CM_STATUS ConditionMatch(const CM_REQUIREMENT *req,const CM_CLAIM *claims,size_t nc,const CM_SOURCE *sources,size_t ns,CM_RESULT *out)
{
    unsigned char hashes[CM_SOURCE_MAX][32];size_t i,total=0;uint32_t pos=UINT32_MAX,neg=UINT32_MAX;int conflict=0;
    if(!out)return CM_INVALID;
    memset(out,0,sizeof(*out));out->claim_index=UINT32_MAX;
    if(!req||!valid_key(req)||nc>CM_CLAIM_MAX||ns>CM_SOURCE_MAX||(nc&&!claims)||(ns&&!sources))return CM_INVALID;
    for(i=0;i<ns;i++) {
        if(!sources[i].bytes||!sources[i].size||sources[i].size>CM_SOURCE_BYTES_MAX||sources[i].size>CM_TOTAL_BYTES_MAX-total)return CM_INVALID;
        total+=sources[i].size;TLSha256(sources[i].bytes,sources[i].size,hashes[i]);
    }
    for(i=0;i<nc;i++) {
        const CM_CLAIM *c=&claims[i];
        if(!valid_key(&c->key)||!identifier(c->object,c->key.polarity==CM_NEGATIVE)||(c->key.polarity==CM_NEGATIVE && c->object[0])||(c->origin!=CM_CALLER_ASSERTED && c->origin!=CM_EXTRACTOR_ASSERTED)||c->source>=ns||c->first>=c->end||c->end>sources[c->source].size||memcmp(c->source_sha256,hashes[c->source],32))return CM_INVALID;
    }
    for(i=0;i<nc;i++) {
        const CM_CLAIM *c=&claims[i];uint32_t *pick;
        if(!same_key(req,&c->key))continue;
#ifdef CM_MUTANT_IGNORE_POLARITY
        pick=&pos;
#else
        pick=c->key.polarity==CM_POSITIVE?&pos:&neg;
#endif
        if(*pick!=UINT32_MAX && pick==&pos && strcmp(claims[*pick].object,c->object))conflict=1;
        if(*pick==UINT32_MAX || evidence_less(c,&claims[*pick]))*pick=(uint32_t)i;
    }
#ifndef CM_MUTANT_IGNORE_CONFLICT
    if(conflict||(pos!=UINT32_MAX && neg!=UINT32_MAX)){out->status=CM_CONFLICT;return out->status;}
#else
    (void)conflict;
#endif
    {uint32_t same=req->polarity==CM_POSITIVE?pos:neg,opposite=req->polarity==CM_POSITIVE?neg:pos;
    if(same!=UINT32_MAX){out->status=CM_MATCH;out->claim_index=same;memcpy(out->object,claims[same].object,CM_ID_CAP);}
    else if(opposite!=UINT32_MAX){out->status=CM_REFUTED;out->claim_index=opposite;memcpy(out->object,claims[opposite].object,CM_ID_CAP);}
    else out->status=CM_UNDETERMINED;
    }
    return out->status;
}
