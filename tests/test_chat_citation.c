/* Per-emission receipts must never be inferred from retrieval history. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
static int failures;
static void check(int ok,const char*n){printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)failures++;}
/* Supplied records: MATCH never certifies prose extraction or truth. */
#include "research/question_support_contract.h"
static QSRequest qs_q;
static QSAssertionSet qs_s;
static QSResult qs_r;
static const unsigned char qs_query[]="mode returns blue set Luma Vela 2";
static const unsigned char qs_source[]="mode returns blue set Luma Vela 2 clear Neri Arin 3 stores export";
static QSField qs_field(uint32_t kind,uint32_t first,uint32_t end){
 QSField f;memset(&f,0,sizeof(f));f.kind=kind;f.presence=QS_EXPLICIT;f.requirement=QS_MANDATORY;
 f.value.first=first;f.value.end=end;f.origin_count=1;f.origins[0]=f.value;return f;
}
static void qs_reset(void){
 static const uint32_t spans[7][2]={{0,4},{5,12},{13,17},{18,21},{22,26},{27,31},{32,33}};
 memset(&qs_q,0,sizeof(qs_q));memset(&qs_s,0,sizeof(qs_s));
 qs_q.identity.byte_count=sizeof(qs_query)-1;TLSha256(qs_query,sizeof(qs_query)-1,qs_q.identity.sha256);
 qs_q.state=QS_COMPLETE;qs_q.goal_count=1;qs_q.field_count=7;qs_q.completeness_origin=1;
 qs_s.count=1;qs_s.enumeration_state=1;QSAssertion*a=&qs_s.assertions[0];
 a->identity.byte_count=sizeof(qs_source)-1;TLSha256(qs_source,sizeof(qs_source)-1,a->identity.sha256);
 a->assertion_span.end=sizeof(qs_source)-1;a->extraction_state=QS_MANUAL;a->assertion_kind=QS_AFFIRMATIVE;a->field_count=7;
 for(uint32_t i=0;i<7;i++)qs_q.fields[i]=a->fields[i]=qs_field(i+1,spans[i][0],spans[i][1]);
}
static uint32_t qs_call(void){return QSCheckRecords(qs_query,sizeof(qs_query)-1,qs_source,sizeof(qs_source)-1,&qs_q,&qs_s,&qs_r);}
static void qs_expect(uint32_t expected,const char*label){check(qs_call()==expected,label);check(qs_r.semantic_evidence==QS_UNVERIFIED_EXTRACTION,"QS manual annotations always UNVERIFIED_EXTRACTION");}
static void qs_tests(void){
 printf("QS metadata sizes: span=%zu identity=%zu field=%zu request=%zu assertion=%zu set=%zu result=%zu\n",sizeof(QSSpan),sizeof(QSIdentity),sizeof(QSField),sizeof(QSRequest),sizeof(QSAssertion),sizeof(QSAssertionSet),sizeof(QSResult));
 qs_reset();qs_expect(QS_MATCH,"QS P01 annotation-relative exact match, not answer support");
 for(uint32_t i=0;i<7;i++){
  qs_reset();qs_q.fields[i].value=qs_q.fields[(i+1)%7].value;qs_q.fields[i].origins[0]=qs_q.fields[i].value;
  qs_expect(QS_NO_MATCH,"QS N01-N06 each mandatory field mismatch excludes");
 }
 qs_reset();qs_s.count=0;qs_expect(QS_NO_MATCH,"QS empty complete set no match");
 qs_s.enumeration_state=2;qs_expect(QS_UNKNOWN,"QS empty incomplete set unknown");
 qs_reset();qs_q.state=QS_PARTIAL;qs_expect(QS_UNKNOWN,"QS U04 manual incomplete request unknown");
 qs_reset();qs_q.goal_count=2;qs_expect(QS_UNSUPPORTED_FORM,"QS U05 multigoal unsupported");
 for(uint32_t i=0;i<7;i++){
  qs_reset();qs_s.assertions[0].fields[i].presence=QS_FIELD_UNKNOWN;qs_s.assertions[0].fields[i].value=(QSSpan){0,0};
  qs_expect(QS_UNKNOWN,"QS U01-U03 potentially compatible unknown blocks no-match");
 }
 qs_reset();qs_s.assertions[0].fields[3].presence=QS_FIELD_UNKNOWN;qs_s.assertions[0].fields[3].value=(QSSpan){0,0};
 qs_s.assertions[0].fields[5]=qs_field(6,44,48);qs_expect(QS_NO_MATCH,"QS decisive platform mismatch excludes unknown condition");
 qs_reset();qs_s.assertions[0].extraction_state=QS_EXTRACTION_UNKNOWN;qs_expect(QS_UNKNOWN,"QS uncertain extraction blocks match");
 qs_s.assertions[0].fields[5]=qs_field(6,44,48);qs_expect(QS_NO_MATCH,"QS decisive mismatch excludes extraction-uncertain candidate");
 qs_reset();qs_s.assertions[0].assertion_kind=QS_NEGATIVE;qs_expect(QS_OPPOSITE,"QS C01 annotation-relative opposite proposition");
 qs_s.assertions[0].fields[3]=qs_field(4,34,39);qs_expect(QS_NO_MATCH,"QS C02 conditional denial does not transfer");
 qs_reset();qs_s.assertions[0].assertion_kind=QS_NO_INFORMATION;qs_expect(QS_UNKNOWN,"QS C03 no-information is not negative fact");
 qs_reset();qs_s.count=2;qs_s.assertions[1]=qs_s.assertions[0];qs_s.assertions[1].assertion_kind=QS_NEGATIVE;
 qs_expect(QS_UNKNOWN,"QS C04 same-scope conflict");
 {QSAssertion t=qs_s.assertions[0];qs_s.assertions[0]=qs_s.assertions[1];qs_s.assertions[1]=t;}qs_expect(QS_UNKNOWN,"QS E09 reversed conflict unchanged");
 qs_reset();qs_s.count=2;qs_s.assertions[1]=qs_s.assertions[0];qs_s.assertions[1].fields[4]=qs_field(5,40,44);
 qs_expect(QS_MATCH,"QS N07 mismatched implementation cannot transfer");
 {QSAssertion t=qs_s.assertions[0];qs_s.assertions[0]=qs_s.assertions[1];qs_s.assertions[1]=t;}qs_expect(QS_MATCH,"QS E09 candidate permutation unchanged");
 qs_reset();qs_q.identity.sha256[0]^=1;qs_expect(QS_INVALID,"QS E05 stale query hash");
 qs_reset();qs_s.assertions[0].identity.sha256[0]^=1;qs_expect(QS_INVALID,"QS E05 stale source hash");
 qs_reset();qs_q.fields[0].value=(QSSpan){UINT32_MAX,3};qs_expect(QS_INVALID,"QS E06 reversed overflow-shaped span");
 qs_reset();qs_q.fields[0].origins[0]=(QSSpan){5,12};qs_expect(QS_INVALID,"QS valid origins but value outside origins");
 qs_reset();qs_q.fields[1].kind=1;qs_expect(QS_INVALID,"QS E07 duplicate kind even identical disallowed");
 qs_reset();qs_q.fields[1]=qs_q.fields[0];qs_expect(QS_INVALID,"QS identical duplicate disallowed");
 qs_reset();qs_q.identity.reserved=1;qs_expect(QS_INVALID,"QS reserved identity word nonzero");
 qs_reset();qs_q.state=99;qs_expect(QS_INVALID,"QS invalid request enum");
 qs_reset();qs_s.assertions[0].extraction_state=99;qs_expect(QS_INVALID,"QS invalid extraction enum");
 qs_reset();qs_q.fields[0].presence=99;qs_expect(QS_INVALID,"QS invalid field enum");
 qs_reset();qs_q.fields[0].origin_count=UINT32_MAX;qs_expect(QS_INVALID,"QS origin count checked before access");
 qs_reset();qs_q.field_count=UINT32_MAX;qs_expect(QS_UNSUPPORTED_FORM,"QS E08 request count limit before access");
 qs_reset();qs_s.count=UINT32_MAX;qs_expect(QS_UNSUPPORTED_FORM,"QS set count limit before access");
 qs_reset();qs_s.assertions[0].field_count=UINT32_MAX;qs_expect(QS_UNSUPPORTED_FORM,"QS assertion count limit before access");
 qs_reset();check(QSCheckRecords(qs_query,4097,qs_source,sizeof(qs_source)-1,&qs_q,&qs_s,&qs_r)==QS_UNSUPPORTED_FORM,"QS query resource limit before byte read");
 qs_reset();check(QSCheckRecords(qs_query,sizeof(qs_query)-1,qs_source,65537,&qs_q,&qs_s,&qs_r)==QS_UNSUPPORTED_FORM,"QS source resource limit before byte read");
 qs_reset();qs_q.completeness_origin=2;qs_expect(QS_UNKNOWN,"QS completeness asserted unknown not inferred from spans");
 qs_reset();
 /* False role annotation: bytes Luma denote implementation in the synthetic
    sentence, but both supplied records falsely tag Luma as platform. */
 qs_q.fields[4].kind=6;qs_q.fields[5].kind=5;
 qs_s.assertions[0].fields[4].kind=6;qs_s.assertions[0].fields[5].kind=5;
 qs_expect(QS_MATCH,"QS E11 false role annotations can record-match with valid byte origins");check(qs_r.semantic_evidence==QS_UNVERIFIED_EXTRACTION,"QS E11 operational unknown semantic annotation, never prose support");
 qs_reset();qs_r.relation=QS_MATCH;qs_q.identity.reserved=1;qs_expect(QS_INVALID,"QS stale successful result cleared on invalid call");check(!qs_r.comparison_count,"QS invalid result has no old comparisons");
 qs_reset();check(QSCheckRecords(NULL,1,qs_source,sizeof(qs_source)-1,&qs_q,&qs_s,&qs_r)==QS_INVALID,"QS null bytes invalid");
 check(QSCheckRecords(qs_query,sizeof(qs_query)-1,qs_source,sizeof(qs_source)-1,&qs_q,&qs_s,NULL)==QS_INVALID,"QS null result invalid");
 qs_reset();qs_expect(QS_MATCH,"QS E10 no session input or persistence");
 check(qs_r.hash_bytes==sizeof(qs_query)+sizeof(qs_source)-2&&qs_r.comparison_count==7,"QS explicit work counters");

 {static QSRequest before_q;static QSAssertionSet before_s;
  qs_reset();before_q=qs_q;before_s=qs_s;qs_call();
  check(!memcmp(&before_q,&qs_q,sizeof(qs_q))&&!memcmp(&before_s,&qs_s,sizeof(qs_s)),"QS input records unchanged");
 }
 qs_reset();qs_s.count=2;qs_s.assertions[1]=qs_s.assertions[0];qs_s.assertions[1].extraction_state=QS_EXTRACTION_UNKNOWN;
 qs_expect(QS_UNKNOWN,"QS relevant unknown defeats otherwise matching candidate");
 qs_reset();qs_s.assertions[0].fields[1]=qs_s.assertions[0].fields[0];qs_expect(QS_INVALID,"QS duplicate assertion kind invalid");
 qs_reset();qs_s.assertions[0].assertion_span.end=UINT32_MAX;qs_expect(QS_INVALID,"QS assertion source overflow bounds");
 qs_reset();qs_s.enumeration_state=99;qs_expect(QS_INVALID,"QS invalid set enum");
 qs_reset();qs_s.assertions[0].rule_id=1;qs_expect(QS_INVALID,"QS grammar adapter rule not enabled");
 qs_reset();qs_q.fields[0].origins[1]=(QSSpan){1,2};qs_expect(QS_INVALID,"QS unused origin nonzero invalid");

 for(uint32_t scenario=0;scenario<3;scenario++){
  qs_reset();qs_q.fields[3].presence=QS_FIELD_UNKNOWN;qs_q.fields[3].value=(QSSpan){0,0};
  if(scenario==0)qs_s.count=0;
  if(scenario==1)qs_s.assertions[0].fields[5]=qs_field(6,44,48);
  qs_expect(QS_UNKNOWN,"QS unknown mandatory request prevents closure for empty/irrelevant/matching set");
 }
 for(uint32_t negative=0;negative<2;negative++){
  qs_reset();qs_q.fields[7]=qs_field(8,0,4);qs_q.field_count=8;
  qs_s.assertions[0].assertion_kind=negative?QS_NEGATIVE:QS_AFFIRMATIVE;
  qs_expect(QS_UNSUPPORTED_FORM,"QS explicit request polarity field rejects duplicate polarity encoding");
  qs_reset();qs_s.assertions[0].fields[7]=qs_field(8,0,4);qs_s.assertions[0].field_count=8;
  qs_s.assertions[0].assertion_kind=negative?QS_NEGATIVE:QS_AFFIRMATIVE;
  qs_expect(QS_UNSUPPORTED_FORM,"QS explicit assertion polarity field rejects duplicate polarity encoding");
 }

 qs_reset();qs_q.fields[7]=qs_field(11,0,4);qs_q.field_count=8;qs_expect(QS_UNSUPPORTED_FORM,"QS explicit mode encoding unsupported");
 /* Attain the maximum byte-compare count with overlapping explicit fields. */
 {static unsigned char qmax[4096],smax[65536];
  memset(qmax,'a',sizeof(qmax));memset(smax,'a',sizeof(smax));qs_reset();
  qs_q.identity.byte_count=sizeof(qmax);TLSha256(qmax,sizeof(qmax),qs_q.identity.sha256);qs_q.field_count=12;
  qs_s.count=8;
  for(uint32_t k=0;k<12;k++)qs_q.fields[k]=qs_field(k+1,0,4096);
  for(uint32_t k=7;k<=10;k+=3){qs_q.fields[k].presence=QS_NOT_REQUESTED;qs_q.fields[k].requirement=QS_NOT_APPLICABLE;qs_q.fields[k].value=(QSSpan){0,0};}
  for(uint32_t i=0;i<8;i++){
   QSAssertion*a=&qs_s.assertions[i];memset(a,0,sizeof(*a));a->identity.byte_count=sizeof(smax);
   TLSha256(smax,sizeof(smax),a->identity.sha256);a->assertion_span.end=sizeof(smax);
   a->extraction_state=QS_MANUAL;a->assertion_kind=QS_AFFIRMATIVE;a->field_count=12;
   for(uint32_t k=0;k<12;k++)a->fields[k]=qs_field(k+1,0,4096);
   for(uint32_t k=7;k<=10;k+=3){a->fields[k].presence=QS_NOT_REQUESTED;a->fields[k].requirement=QS_NOT_APPLICABLE;a->fields[k].value=(QSSpan){0,0};}
  }
  check(QSCheckRecords(qmax,sizeof(qmax),smax,sizeof(smax),&qs_q,&qs_s,&qs_r)==QS_UNKNOWN,"QS maximum matching candidates conservatively ambiguous");
  check(qs_r.compare_bytes==327680&&qs_r.comparison_count==80&&qs_r.hash_bytes==69632,"QS measured worst-case 327680 byte comparisons plus 69632 hashed bytes");
  printf("QS max work: compare_bytes=%u comparisons=%u hash_bytes=%u\n",qs_r.compare_bytes,qs_r.comparison_count,qs_r.hash_bytes);
 }

}
static CHAT *create(const char*p){CHAT*c=calloc(1,sizeof(*c));if(c){ChatInit(c,p);c->episodic.auto_save=0;}return c;}
static void destroy(CHAT*c){if(c){ChatDestroy(c);free(c);}}
static int call(CHAT*c,const char*q,char*out,size_t n,int mode,CHAT_CITATION*r,GoalCause*cause){
 if(mode==0)return r?ChatResolveLineEx(c,q,out,n,NULL,0,NULL,0,cause,r):ChatResolveLine(c,q,out,n,NULL,0,NULL,0,cause);
 if(mode==1)return r?ChatTryTextLineEx(c,q,out,n,r):ChatTryTextLine(c,q,out,n);
 return r?ChatHandleToBufEx(c,q,out,n,r):ChatHandleToBuf(c,q,out,n);
}
static void compare(const char*p,int mode,size_t n){
 CHAT*a=create(p),*b=create(p);char x[8193],y[8193];CHAT_CITATION r;GoalCause ca=CAUSE_NONE,cb=CAUSE_NONE;
 const char*qs[]={"seconds","continualo","zzyyzzcitationabsent20261013","hola","what is seconds?","postulado"};
 check(a&&b&&a->ntfiles==1&&b->ntfiles==1,"receipt fixture loaded");if(!a||!b){destroy(a);destroy(b);return;}
 for(unsigned i=0;i<sizeof(qs)/sizeof(*qs);i++){
  memset(x,0xa5,sizeof(x));memset(y,0xa5,sizeof(y));x[0]=y[0]=0;memset(&r,0xa5,sizeof(r));
  int ra=call(a,qs[i],x,n,mode,NULL,&ca),rb=call(b,qs[i],y,n,mode,&r,&cb);
  check(ra==rb&&ca==cb&&strcmp(x,y)==0,"opt-in legacy output/status unchanged");
  check(a->tnw==b->tnw&&a->ntshown==b->ntshown&&memcmp(a->tshown,b->tshown,a->ntshown*sizeof(*a->tshown))==0,"opt-in cache unchanged");
  check((unsigned char)y[n]==0xa5,"receipt output bounds");
  if(r.state==CHAT_CITE_VERIFIED){
   size_t needed=0;unsigned char raw[4096];char display[2048];const TEXTLEX*tl=&b->tlex[r.file_slot];
   check(r.source.version==1&&r.quote_first<r.visible_quote_end&&r.visible_quote_end<=r.quote_end&&r.copied_bytes==strlen(y),"receipt visible display span valid");
   check(TextLexCitationCopy(tl,&r.source,raw,sizeof(raw),&needed)==TL_CITE_OK&&needed==r.source.end-r.source.first&&memcmp(raw,tl->image+r.source.first,needed)==0,"receipt byte-exact source");
   check((TextLexSentenceText(tl,r.sentence_index,tl->image,tl->imagelen,display,sizeof(display))>0&&memcmp(y+r.quote_first,display,r.visible_quote_end-r.quote_first)==0)||(TextLexSentenceSourceText(tl,r.sentence_index,tl->image,tl->imagelen,display,sizeof(display))>0&&memcmp(y+r.quote_first,display,r.visible_quote_end-r.quote_first)==0),"receipt source identity binds display prefix");
   check(r.output_truncated==(r.copied_bytes<r.answer_bytes),"receipt presentation truncation explicit");
   TL_CITATION wrong=r.source;wrong.source_sha256[0]^=1;check(TextLexCitationCopy(tl,&wrong,raw,sizeof(raw),&needed)==TL_CITE_SOURCE_MISMATCH&&needed==0,"receipt source mismatch fails closed");
  }else check(r.source.version==0,"no stale source receipt");
  if(((mode==1&&i==5)||(mode!=1&&i==0))&&n>=32)check(r.state==CHAT_CITE_VERIFIED,"direct quote receipt nonvacuous");
  if(i==0&&n<=16)check(r.state!=CHAT_CITE_VERIFIED,"header-only presentation has no verified quote");
  if(i==2||(i==3&&mode==2))check(r.state!=CHAT_CITE_VERIFIED,"absent and greeting cannot inherit receipt");
 }
 destroy(a);destroy(b);
}
int main(void){
 qs_tests();
 const char*p="chat_citation_fixture.txt";FILE*f=fopen(p,"wb");if(!f)return 2;
 fputs("And those gamma rays came just 1.7 seconds after the gravitational wave signal.\nAnother seconds statement follows distinct observations.\nPrimer postulado (principio de relatividad) explains distant observations.\n",f);fclose(f);
 size_t caps[]={1,8,14,15,16,32,64,4096,8192};for(int m=0;m<3;m++)for(unsigned i=0;i<sizeof(caps)/sizeof(*caps);i++)compare(p,m,caps[i]);
 {CHAT*c=create(p);CHAT_CITATION r;char out[4096];memset(&r,0xa5,sizeof(r));check(ChatHandleToBufEx(NULL,"seconds",out,sizeof(out),&r)==0&&r.state==CHAT_CITE_NONE&&r.source.version==0,"invalid call clears receipt");check(ChatTryTextLineEx(c,"seconds",out,0,&r)==0&&r.state==CHAT_CITE_NONE,"zero size clears receipt");destroy(c);}
 remove(p);printf("test_chat_citation: %d failed\n",failures);return failures?1:0;
}
