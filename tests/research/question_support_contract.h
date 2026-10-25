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
enum { QS_MANUAL=1, QS_EXTRACTION_UNKNOWN };
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
QS_SIZE_CHECK(QSRequest,728); QS_SIZE_CHECK(QSAssertion,736);
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
  if(a->field_count>QS_FIELDS) return QSFinish(result,QS_UNSUPPORTED_FORM,QS_REASON_LIMIT);
  if(a->identity.reserved || a->identity.byte_count!=source_bytes ||
     a->extraction_state<QS_MANUAL || a->extraction_state>QS_EXTRACTION_UNKNOWN ||
     a->rule_id!=0 || a->assertion_kind<QS_AFFIRMATIVE || a->assertion_kind>QS_NO_INFORMATION ||
     !QSSpanInside(a->assertion_span,sr) || !QSFieldsValid(a->fields,a->field_count,a->assertion_span))
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
