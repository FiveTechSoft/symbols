#include "condition_parse.h"
#include "tl_sha256.h"
#include <string.h>

typedef struct {const unsigned char *b;size_t n,p;} SCAN;
static int literal(SCAN*s,const char*t){size_t n=strlen(t);if(n>s->n-s->p||memcmp(s->b+s->p,t,n))return 0;s->p+=n;return 1;}
static int reserved(const char*s){const char*w[]={"what","does","not","during","and"};size_t i;for(i=0;i<sizeof(w)/sizeof(*w);i++)if(!strcmp(s,w[i]))return 1;return 0;}
static int ident(SCAN*s,char out[CM_ID_CAP])
{
 size_t start=s->p,n=0;unsigned char c;
 if(start==s->n||s->b[start]<'a'||s->b[start]>'z')return 0;
 while(s->p<s->n){c=s->b[s->p];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))break;s->p++;n++;}
 if(!n||n>=CM_ID_CAP)return 0;
 memcpy(out,s->b+start,n);out[n]=0;return !reserved(out);
}
static int quals(SCAN*s,CM_REQUIREMENT*q)
{
 uint32_t i,j;
 for(i=0;i<2;i++){
  if(!ident(s,q->qualifiers[i].key)||!literal(s,"=")||!ident(s,q->qualifiers[i].value))return 0;
  for(j=0;j<i;j++){
#ifndef CP_MUTANT_DUPLICATE
   if(!strcmp(q->qualifiers[i].key,q->qualifiers[j].key))return 0;
#endif
  }
  q->nqual=i+1;
  if(s->n-s->p<5||memcmp(s->b+s->p," and ",5))break;
  s->p+=5;
 }
#ifdef CP_MUTANT_QUALIFIER
 q->qualifiers[0].value[0]='x';
#endif
 return 1;
}
static int endline(SCAN*s,const char*punct)
{
 if(!literal(s,punct))return 0;
#ifndef CP_MUTANT_ANCHOR
 return s->p==s->n;
#else
 return 1;
#endif
}
CP_STATUS ConditionParseQuestion(const unsigned char*b,size_t n,CM_REQUIREMENT*out)
{
 CM_REQUIREMENT q={0};SCAN s={b,n,0};
 if(!out)return CP_REJECTED;
 memset(out,0,sizeof(*out));
 if(!b||!n||n>CP_QUESTION_MAX)return CP_REJECTED;
 q.version=CM_VERSION;q.polarity=CM_POSITIVE;
 if(!literal(&s,"What does ")||!ident(&s,q.subject)||!literal(&s," ")||!ident(&s,q.relation)||!literal(&s," during ")||!quals(&s,&q)||!endline(&s,"?"))return CP_REJECTED;
 *out=q;return CP_PARSED;
}
static int claimline(const unsigned char*b,size_t n,CM_CLAIM*c)
{
 SCAN s={b,n,0};memset(c,0,sizeof(*c));c->key.version=CM_VERSION;c->key.polarity=CM_POSITIVE;c->origin=CM_EXTRACTOR_ASSERTED;
 if(!ident(&s,c->key.subject)||!literal(&s," "))return 0;
 if(s.n-s.p>=9&&!memcmp(s.b+s.p,"does not ",9)){
  s.p+=9;c->key.polarity=CM_NEGATIVE;
#ifdef CP_MUTANT_POLARITY
  c->key.polarity=CM_POSITIVE;
#endif
  if(!ident(&s,c->key.relation))return 0;
 }else if(!ident(&s,c->key.relation)||!literal(&s," ")||!ident(&s,c->object))return 0;
 return literal(&s," during ")&&quals(&s,&c->key)&&endline(&s,".");
}
CP_STATUS ConditionExtractClaims(const unsigned char*b,size_t n,CM_CLAIM*out,size_t cap,size_t*count)
{
 CM_CLAIM temp[CM_CLAIM_MAX];unsigned char sha[32];size_t p=0,nc=0,i;
 if(count)*count=0;
 if(cap>CM_CLAIM_MAX)return CP_REJECTED;
 if(out)memset(out,0,cap*sizeof(*out));
 if(!count||(!out&&cap)||(!b&&n)||n>CP_SOURCE_MAX)return CP_REJECTED;
 while(p<n){size_t first=p,end;
  while(p<n&&b[p]!='\n')p++;
  end=p;if(end>first&&b[end-1]=='\r'){if(p==n)return CP_REJECTED;end--;}
  if(end>first){
   if(end-first>CP_LINE_MAX||nc>=CM_CLAIM_MAX||!claimline(b+first,end-first,&temp[nc]))return CP_REJECTED;
   temp[nc].first=first;temp[nc].end=end;nc++;
  }else if(p==n&&p>first)return CP_REJECTED;
  if(p<n)p++;
 }
 if(nc>cap)return CP_REJECTED;
 TLSha256(b,n,sha);
 for(i=0;i<nc;i++){memcpy(temp[i].source_sha256,sha,32);out[i]=temp[i];}
 *count=nc;return CP_PARSED;
}
