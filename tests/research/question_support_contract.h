/* Test-only annotation-relative research contract. Never serving semantics. */
#ifndef QUESTION_SUPPORT_CONTRACT_H
#define QUESTION_SUPPORT_CONTRACT_H
#include <stdint.h>
#include <string.h>
#include "tl_sha256.h"
#define QS_FIELDS 12u
#define QS_ASSERTIONS 8u
#define QS_ORIGINS 4u
enum { QS_MATCH=1, QS_OPPOSITE, QS_NO_MATCH, QS_UNKNOWN, QS_UNSUPPORTED_FORM, QS_INVALID };
enum { QS_EXPLICIT=1, QS_FIELD_UNKNOWN, QS_NOT_REQUESTED };
enum { QS_MANDATORY=1, QS_NOT_APPLICABLE };
enum { QS_COMPLETE=1, QS_PARTIAL, QS_FORM_UNSUPPORTED, QS_LIMIT };
enum { QS_MANUAL=1, QS_EXTRACTION_UNKNOWN, QS_GRAMMAR_RECORD };
enum { QS_AFFIRMATIVE=1, QS_NEGATIVE, QS_NO_INFORMATION };
enum { QS_EQUAL=1, QS_MISMATCH, QS_UNCERTAIN };
enum { QS_UNVERIFIED_EXTRACTION=1 };
enum { QS_REASON_NONE=0, QS_REASON_FORMAT, QS_REASON_IDENTITY, QS_REASON_LIMIT,
       QS_REASON_INCOMPLETE, QS_REASON_CONFLICT, QS_REASON_NO_CANDIDATE };
/* Kind 1 subject,2 relation,3 value,4 condition,5 implementation,6 platform,
   7 version,8 polarity,9 quantifier,10 source selector,11 mode,12 context. */
typedef uint32_t QSRecordRelation;
typedef struct { uint32_t first,end; } QSSpan;
typedef struct { unsigned char sha256[32]; uint32_t byte_count,reserved; } QSIdentity;
typedef struct {
 uint32_t kind,presence,requirement,origin_count;
 QSSpan value,origins[QS_ORIGINS];
} QSField;
typedef struct {
 QSIdentity identity;
 uint32_t state,goal_count,field_count,completeness_origin;
 uint32_t origin_rule; /* 0 supplied/manual,1 assignment grammar */
 QSField fields[QS_FIELDS];
} QSRequest;
typedef struct {
 QSIdentity identity; QSSpan assertion_span;
 uint32_t extraction_state,rule_id,field_count,assertion_kind;
 QSField fields[QS_FIELDS];
} QSAssertion;
typedef struct { uint32_t count,enumeration_state; QSAssertion assertions[QS_ASSERTIONS]; } QSAssertionSet;
typedef struct { uint32_t assertion_index,field_kind,comparison,reason; } QSComparison;
typedef struct {
 uint32_t relation,semantic_evidence,reason,comparison_count,hash_bytes,compare_bytes,reserved[2];
 QSComparison comparisons[QS_ASSERTIONS*QS_FIELDS];
} QSResult;
#define QS_SIZE_CHECK(T,N) typedef char QS_size_##T[(sizeof(T)==(N))?1:-1]
QS_SIZE_CHECK(QSSpan,8); QS_SIZE_CHECK(QSIdentity,40); QS_SIZE_CHECK(QSField,56);
QS_SIZE_CHECK(QSRequest,732); QS_SIZE_CHECK(QSAssertion,736);
QS_SIZE_CHECK(QSAssertionSet,5896); QS_SIZE_CHECK(QSResult,1568);
static int QSSpanInside(QSSpan s,QSSpan outer) {
 return s.first<s.end && s.first>=outer.first && s.end<=outer.end;
}
static int QSZeroSpan(QSSpan s) { return s.first==0 && s.end==0; }
static int QSFieldsValid(const QSField *f,uint32_t count,QSSpan outer) {
 uint32_t i,j,k;
 for(i=0;i<count;i++) {
  int contained=0;
  if(f[i].kind<1 || f[i].kind>QS_FIELDS || f[i].presence<QS_EXPLICIT ||
     f[i].presence>QS_NOT_REQUESTED || f[i].requirement<QS_MANDATORY ||
     f[i].requirement>QS_NOT_APPLICABLE || f[i].origin_count>QS_ORIGINS) return 0;
  for(j=0;j<i;j++) if(f[j].kind==f[i].kind) return 0;
  if(f[i].presence==QS_NOT_REQUESTED && f[i].requirement!=QS_NOT_APPLICABLE) return 0;
  if(f[i].presence==QS_EXPLICIT) {
   if(!f[i].origin_count || !QSSpanInside(f[i].value,outer)) return 0;
  } else if(!QSZeroSpan(f[i].value)) return 0;
  for(k=0;k<QS_ORIGINS;k++) {
   if(k<f[i].origin_count) {
    if(!QSSpanInside(f[i].origins[k],outer)) return 0;
    if(QSSpanInside(f[i].value,f[i].origins[k])) contained=1;
   } else if(!QSZeroSpan(f[i].origins[k])) return 0;
  }
  if(f[i].presence==QS_EXPLICIT && !contained) return 0;
 }
 return 1;
}
static const QSField *QSFind(const QSField *f,uint32_t n,uint32_t kind) {
 uint32_t i; for(i=0;i<n;i++) if(f[i].kind==kind) return &f[i]; return NULL;
}
/* Closed ASCII assignment grammar. No free subject/predicate capture. */
static int QSAlpha(unsigned char c) {return (c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static int QSTake(const unsigned char*b,uint32_t end,uint32_t*p,const char*s){
 uint32_t n=(uint32_t)strlen(s);if(*p>end||n>end-*p||memcmp(b+*p,s,n))return 0;*p+=n;return 1;
}
static int QSName(const unsigned char*b,uint32_t end,uint32_t*p,int atom){
 uint32_t start=*p;if(start>=end||(!QSAlpha(b[start])&&!(atom&&b[start]=='_')))return 0;
 while(*p<end&&(QSAlpha(b[*p])||(b[*p]>='0'&&b[*p]<='9')||b[*p]=='_')){
  if(*p-start==31)return 0; /* Inspect at most32 bytes, never scan tail. */
  (*p)++;
 }
 return *p-start<=31;
}
static QSField QSGrammarField(uint32_t kind,uint32_t first,uint32_t end){
 QSField f;memset(&f,0,sizeof(f));f.kind=kind;f.presence=QS_EXPLICIT;f.requirement=QS_MANDATORY;
 f.origin_count=1;f.value.first=first;f.value.end=end;f.origins[0]=f.value;return f;
}
static QSRecordRelation QSParseAssignmentQuery(const unsigned char*b,uint32_t n,QSRequest*out){
 uint32_t p=0,start,subject_end,relation;
 if(!out)return QS_INVALID;
 memset(out,0,sizeof(*out));
 if(!b&&n)return QS_INVALID;
 if(n>4096)return QS_UNSUPPORTED_FORM;
 if(!QSTake(b,n,&p,"How are "))return QS_UNSUPPORTED_FORM;
 start=p;
 if(!QSName(b,n,&p,0)||!QSTake(b,n,&p," variables"))return QS_UNSUPPORTED_FORM;
 subject_end=p;
 if(!QSTake(b,n,&p," "))return QS_UNSUPPORTED_FORM;
 relation=p;
 if(!QSTake(b,n,&p,"assigned?")||p!=n)return QS_UNSUPPORTED_FORM;
 out->identity.byte_count=n;TLSha256(b,n,out->identity.sha256);
 out->state=QS_COMPLETE;out->goal_count=1;out->field_count=2;out->completeness_origin=1;out->origin_rule=1;
 out->fields[0]=QSGrammarField(1,start,subject_end);out->fields[1]=QSGrammarField(2,relation,relation+8);
 return QS_MATCH;
}
static QSRecordRelation QSExtractAssignmentSentence(const unsigned char*b,uint32_t n,QSSpan span,QSAssertion*out){
 uint32_t p=span.first,start=p,subject_end,relation,value,end;
 if(!out)return QS_INVALID;
 memset(out,0,sizeof(*out));
 if((!b&&n)||!QSSpanInside(span,(QSSpan){0,n}))return QS_INVALID;
 if(n>65536)return QS_UNSUPPORTED_FORM;
 end=span.end;
 if(!QSName(b,end,&p,0)||!QSTake(b,end,&p," variables"))return QS_UNSUPPORTED_FORM;
 subject_end=p;
 if(!QSTake(b,end,&p," are "))return QS_UNSUPPORTED_FORM;
 relation=p;
 if(!QSTake(b,end,&p,"assigned "))return QS_UNSUPPORTED_FORM;
 value=p;
 if(!QSTake(b,end,&p,"with ")||!QSName(b,end,&p,1)||!QSTake(b,end,&p,"=")||!QSName(b,end,&p,1))return QS_UNSUPPORTED_FORM;
 if(p<end&&b[p]!='.'&&!QSTake(b,end,&p," and must not contain spaces around the equals sign"))return QS_UNSUPPORTED_FORM;
 if(!QSTake(b,end,&p,".")||p!=end)return QS_UNSUPPORTED_FORM;
 out->identity.byte_count=n;TLSha256(b,n,out->identity.sha256);out->assertion_span=span;
 out->extraction_state=QS_GRAMMAR_RECORD;out->rule_id=1;out->field_count=3;out->assertion_kind=QS_AFFIRMATIVE;
 out->fields[0]=QSGrammarField(1,start,subject_end);out->fields[1]=QSGrammarField(2,relation,relation+8);
 out->fields[2]=QSGrammarField(3,value,end-1);return QS_MATCH;
}
/* Explicit member comparison, no padding-dependent struct memcmp. */
static int QSIdentitySame(const QSIdentity*a,const QSIdentity*b){return !memcmp(a->sha256,b->sha256,32)&&a->byte_count==b->byte_count&&a->reserved==b->reserved;}
static int QSSpanSame(QSSpan a,QSSpan b){return a.first==b.first&&a.end==b.end;}
static int QSAllFieldsSame(const QSField*a,const QSField*b){
 uint32_t i,j;for(i=0;i<QS_FIELDS;i++){
  if(a[i].kind!=b[i].kind||a[i].presence!=b[i].presence||a[i].requirement!=b[i].requirement||a[i].origin_count!=b[i].origin_count||!QSSpanSame(a[i].value,b[i].value))return 0;
  for(j=0;j<QS_ORIGINS;j++)if(!QSSpanSame(a[i].origins[j],b[i].origins[j]))return 0;
 }return 1;
}
static int QSGrammarRequestValid(const unsigned char*b,uint32_t n,const QSRequest*q){
 QSRequest expected;if(QSParseAssignmentQuery(b,n,&expected)!=QS_MATCH)return 0;
 return QSIdentitySame(&q->identity,&expected.identity)&&q->state==expected.state&&q->goal_count==expected.goal_count&&q->field_count==expected.field_count&&q->completeness_origin==expected.completeness_origin&&q->origin_rule==expected.origin_rule&&QSAllFieldsSame(q->fields,expected.fields);
}
static int QSGrammarAssertionValid(const unsigned char*b,uint32_t n,const QSAssertion*a){
 QSAssertion expected;if(QSExtractAssignmentSentence(b,n,a->assertion_span,&expected)!=QS_MATCH)return 0;
 return QSIdentitySame(&a->identity,&expected.identity)&&QSSpanSame(a->assertion_span,expected.assertion_span)&&a->extraction_state==expected.extraction_state&&a->rule_id==expected.rule_id&&a->field_count==expected.field_count&&a->assertion_kind==expected.assertion_kind&&QSAllFieldsSame(a->fields,expected.fields);
}
static QSRecordRelation QSFinish(QSResult *r,uint32_t state,uint32_t reason) {
 r->relation=state; r->reason=reason; return state;
}
/* Supplied completeness and annotations are assertions, not prose entailment.
   Hash identity and spans are checked here; matching never authenticates text. */
static QSRecordRelation QSCheckRecords(const unsigned char *query,uint32_t query_bytes,
 const unsigned char *source,uint32_t source_bytes,const QSRequest *request,
 const QSAssertionSet *set,QSResult *result) {
 unsigned char qhash[32],shash[32]; uint32_t i,j,matches=0,opposites=0,uncertain=0;
 QSSpan qr={0,query_bytes},sr={0,source_bytes};
 if(!result) return QS_INVALID;
 memset(result,0,sizeof(*result)); result->semantic_evidence=QS_UNVERIFIED_EXTRACTION;
 if(!request || !set || (!query && query_bytes) || (!source && source_bytes))
  return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 /* Counts checked before any array access. Exceeding capacity is a limit,
    not an attempt to inspect absent records. */
 if(request->origin_rule==1 && request->field_count>QS_FIELDS)
  return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 if(request->field_count>QS_FIELDS || set->count>QS_ASSERTIONS || query_bytes>4096 || source_bytes>65536)
  return QSFinish(result,QS_UNSUPPORTED_FORM,QS_REASON_LIMIT);
 if(request->identity.reserved || request->identity.byte_count!=query_bytes ||
    request->state<QS_COMPLETE || request->state>QS_LIMIT ||
    request->goal_count==0 || request->completeness_origin<1 || request->completeness_origin>2 ||
    set->enumeration_state<1 || set->enumeration_state>2 ||
    !QSFieldsValid(request->fields,request->field_count,qr))
  return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 for(i=0;i<set->count;i++) {
  const QSAssertion *a=&set->assertions[i];
  if(a->field_count>QS_FIELDS) return QSFinish(result,a->rule_id==1?QS_INVALID:QS_UNSUPPORTED_FORM,a->rule_id==1?QS_REASON_FORMAT:QS_REASON_LIMIT);
  if(a->identity.reserved || a->identity.byte_count!=source_bytes ||
     a->extraction_state<QS_MANUAL || a->extraction_state>QS_GRAMMAR_RECORD ||
     a->rule_id>1 || a->assertion_kind<QS_AFFIRMATIVE || a->assertion_kind>QS_NO_INFORMATION ||
     !QSSpanInside(a->assertion_span,sr) || !QSFieldsValid(a->fields,a->field_count,a->assertion_span))
   return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 }
 if(request->origin_rule>1 || (request->origin_rule==1 && !QSGrammarRequestValid(query,query_bytes,request)))
  return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 for(i=0;i<set->count;i++) {
  const QSAssertion *a=&set->assertions[i];
  if((request->origin_rule==1 && (a->rule_id!=1||a->extraction_state!=QS_GRAMMAR_RECORD)) ||
     (request->origin_rule==0 && (a->rule_id!=0||a->extraction_state==QS_GRAMMAR_RECORD)) ||
     (a->rule_id==1 && !QSGrammarAssertionValid(source,source_bytes,a)))
   return QSFinish(result,QS_INVALID,QS_REASON_FORMAT);
 }
 TLSha256(query,query_bytes,qhash); TLSha256(source,source_bytes,shash);
 result->hash_bytes=query_bytes+source_bytes;
 if(memcmp(qhash,request->identity.sha256,32)) return QSFinish(result,QS_INVALID,QS_REASON_IDENTITY);
 for(i=0;i<set->count;i++) if(memcmp(shash,set->assertions[i].identity.sha256,32))
  return QSFinish(result,QS_INVALID,QS_REASON_IDENTITY);
 if(request->goal_count!=1 || request->state==QS_FORM_UNSUPPORTED || request->state==QS_LIMIT)
  return QSFinish(result,QS_UNSUPPORTED_FORM,QS_REASON_LIMIT);
 if(request->state!=QS_COMPLETE || request->completeness_origin!=1)
  return QSFinish(result,QS_UNKNOWN,QS_REASON_INCOMPLETE);
 /* Mandatory unknown request bounds block closure even for an empty set.
    Kind8 polarity and kind11 mode have no second encoding: assertion_kind
    owns source polarity; mandatory explicit request value means affirmative
    proposition, no request value means lookup. Negative requests unsupported.
    Explicit polarity fields are unsupported rather than compared as strings. */
 for(i=0;i<request->field_count;i++) {
  const QSField *f=&request->fields[i];
  if((f->kind==8 || f->kind==11) && f->presence!=QS_NOT_REQUESTED)
   return QSFinish(result,QS_UNSUPPORTED_FORM,QS_REASON_FORMAT);
  if(f->requirement==QS_MANDATORY && f->presence!=QS_EXPLICIT)
   return QSFinish(result,QS_UNKNOWN,QS_REASON_INCOMPLETE);
 }
 for(i=0;i<set->count;i++) {
  const QSField *f=QSFind(set->assertions[i].fields,set->assertions[i].field_count,8);
  const QSField *m=QSFind(set->assertions[i].fields,set->assertions[i].field_count,11);
  if((f && f->presence!=QS_NOT_REQUESTED) || (m && m->presence!=QS_NOT_REQUESTED))
   return QSFinish(result,QS_UNSUPPORTED_FORM,QS_REASON_FORMAT);
 }
 /* Subject and relation must be explicit mandatory bindings. */
 for(i=1;i<=2;i++) {
  const QSField *f=QSFind(request->fields,request->field_count,i);
  if(!f || f->presence!=QS_EXPLICIT || f->requirement!=QS_MANDATORY)
   return QSFinish(result,QS_UNKNOWN,QS_REASON_INCOMPLETE);
 }
 for(i=0;i<set->count;i++) {
  const QSAssertion *a=&set->assertions[i]; int mismatch=0,unknown=0;
  for(j=0;j<request->field_count;j++) {
   const QSField *q=&request->fields[j],*s; uint32_t cmp=QS_UNCERTAIN;
   QSComparison *c;
   if(q->requirement==QS_NOT_APPLICABLE) continue;
   s=QSFind(a->fields,a->field_count,q->kind);
   if(q->presence==QS_EXPLICIT && s && s->presence==QS_EXPLICIT) {
    uint32_t qn=q->value.end-q->value.first,sn=s->value.end-s->value.first,k=0;
    cmp=QS_MISMATCH;
    if(qn==sn) {
     while(k<qn) { result->compare_bytes++; if(query[q->value.first+k]!=source[s->value.first+k]) break; k++; }
     if(k==qn) cmp=QS_EQUAL;
    }
   }
   c=&result->comparisons[result->comparison_count++];
   c->assertion_index=i; c->field_kind=q->kind; c->comparison=cmp;
   c->reason=cmp==QS_UNCERTAIN?QS_REASON_INCOMPLETE:QS_REASON_NONE;
   if(cmp==QS_MISMATCH) mismatch=1;
   if(cmp==QS_UNCERTAIN) unknown=1;
  }
  if(mismatch) continue; /* Explicit decisive exclusion wins over unknown. */
  if(unknown || a->extraction_state==QS_EXTRACTION_UNKNOWN || a->assertion_kind==QS_NO_INFORMATION) { uncertain=1; continue; }
  {
   const QSField *value=QSFind(a->fields,a->field_count,3);
   if(!value || value->presence!=QS_EXPLICIT) { uncertain=1; continue; }
  }
  if(a->assertion_kind==QS_NEGATIVE) {
   const QSField *qv=QSFind(request->fields,request->field_count,3);
   if(!qv || qv->presence!=QS_EXPLICIT || qv->requirement!=QS_MANDATORY) uncertain=1; else opposites++;
  } else matches++;
 }
 if(uncertain || (matches && opposites) || matches>1 || opposites>1)
  return QSFinish(result,QS_UNKNOWN,(matches && opposites)||matches>1||opposites>1?QS_REASON_CONFLICT:QS_REASON_INCOMPLETE);
 if(matches) return QSFinish(result,QS_MATCH,QS_REASON_NONE);
 if(opposites) return QSFinish(result,QS_OPPOSITE,QS_REASON_NONE);
 if(set->enumeration_state!=1) return QSFinish(result,QS_UNKNOWN,QS_REASON_INCOMPLETE);
 return QSFinish(result,QS_NO_MATCH,QS_REASON_NO_CANDIDATE);
}
#endif
