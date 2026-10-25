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
static const unsigned char as_q[]="How are Bash variables assigned?";
static const unsigned char as_s[]="Bash variables are assigned with name=value and must not contain spaces around the equals sign.";
/* Explicit representations, never canonicalize the bytes being certified. */
static int as_checkout_span(const unsigned char*b,uint32_t n,QSSpan*out){
 static const unsigned char lf[32]={0xda,0xef,0xd5,0x08,0x82,0x51,0x0d,0x9c,0x4f,0x12,0xfa,0x84,0x7c,0x91,0x5d,0x24,0x63,0x71,0xf6,0xcc,0x59,0xe6,0x22,0x20,0x19,0x09,0xb1,0x2d,0x0e,0xc7,0xf2,0x94};
 static const unsigned char crlf[32]={0xcd,0x52,0x79,0x60,0xcf,0x24,0x7a,0x8f,0xb4,0xb6,0xa6,0x37,0x81,0xb9,0xe7,0x1e,0x2a,0x30,0xcb,0xc5,0x13,0x6d,0xba,0x98,0x8e,0x38,0x42,0x8c,0xdc,0xc8,0xf5,0x81};
 unsigned char h[32];uint32_t first=0,end,line=1,want_first,want_end;
 if(!out)return 0;
 out->first=out->end=0;
 if(!b||(n!=18094&&n!=18292))return 0;
 TLSha256(b,n,h);
 if(n==18094&&!memcmp(h,lf,32)){want_first=1457;want_end=1552;}
 else if(n==18292&&!memcmp(h,crlf,32)){want_first=1474;want_end=1569;}
 else return 0;
 while(first<n&&line<18){if(b[first++]=='\n')line++;}
 end=first;while(end<n&&b[end]!='\r'&&b[end]!='\n')end++;
 if(first!=want_first||end!=want_end||end-first!=sizeof(as_s)-1||memcmp(b+first,as_s,sizeof(as_s)-1))return 0;
 out->first=first;out->end=end;return 1;
}
static void as_image_tests(unsigned char*b,uint32_t n){
 QSSpan span;
 check(as_checkout_span(b,n,&span),"assignment explicit checkout representation SHA/length/derived span pinned");
 if(!span.end)return;
 memset(&qs_s,0,sizeof(qs_s));qs_s.count=1;qs_s.enumeration_state=1;
 check(QSParseAssignmentQuery(as_q,sizeof(as_q)-1,&qs_q)==QS_MATCH&&QSExtractAssignmentSentence(b,n,span,&qs_s.assertions[0])==QS_MATCH,"assignment checkout records use actual bytes and absolute origins");
 check(QSCheckRecords(as_q,sizeof(as_q)-1,b,n,&qs_q,&qs_s,&qs_r)==QS_MATCH&&qs_r.semantic_evidence==QS_UNVERIFIED_EXTRACTION,"assignment checkout end-to-end quoted-span-relative only");
 qs_s.assertions[0].assertion_span.first=span.first==1457?1474:1457;
 qs_s.assertions[0].assertion_span.end=span.end==1552?1569:1552;
 check(QSCheckRecords(as_q,sizeof(as_q)-1,b,n,&qs_q,&qs_s,&qs_r)==QS_INVALID,"assignment other representation span rejected against actual image");
 check(QSExtractAssignmentSentence(b,n,span,&qs_s.assertions[0])==QS_MATCH,"assignment restore actual representation record");
 qs_s.assertions[0].identity.sha256[0]^=1;
 check(QSCheckRecords(as_q,sizeof(as_q)-1,b,n,&qs_q,&qs_s,&qs_r)==QS_INVALID,"assignment mutated pin invalid even with correct span");
 check(QSExtractAssignmentSentence(b,n,span,&qs_s.assertions[0])==QS_MATCH,"assignment restore image identity");
 b[span.first]^=1;
 check(!as_checkout_span(b,n,&span)&&QSCheckRecords(as_q,sizeof(as_q)-1,b,n,&qs_q,&qs_s,&qs_r)==QS_INVALID,"assignment bitflip rejects pin and records");
 b[qs_s.assertions[0].assertion_span.first]^=1;
}
static void as_reset(void){
 memset(&qs_s,0,sizeof(qs_s));qs_s.count=1;qs_s.enumeration_state=1;
 check(QSParseAssignmentQuery(as_q,sizeof(as_q)-1,&qs_q)==QS_MATCH,"assignment query grammar parsed");
 check(QSExtractAssignmentSentence(as_s,sizeof(as_s)-1,(QSSpan){0,sizeof(as_s)-1},&qs_s.assertions[0])==QS_MATCH,"assignment real corpus line18 grammar parsed");
}
static uint32_t as_call(void){return QSCheckRecords(as_q,sizeof(as_q)-1,as_s,sizeof(as_s)-1,&qs_q,&qs_s,&qs_r);}
static void as_tests(void){
 static const char*badq[]={"How are Bash variables assigned in version 3?","How are Bash version 3 variables assigned?","How are Bash and CMD variables assigned?","How are Bash variables assigned and read?","How are Bash variables not assigned?","How are Bash variables assigned when empty?","How are Bash variables assigned? Explain quoting.","how are Bash variables assigned?","How  are Bash variables assigned?","How are Bash\nvariables assigned?","How are B\xc3\xa1sh variables assigned?","How are ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef variables assigned?"};
 static const char*bads[]={"CMD variables are assigned with set VAR=value and read by surrounding the name with percent signs as %VAR%.","Bash variables are assigned with name=value if empty.","Bash variables are assigned with name=value except root.","Bash variables are not assigned with name=value.","Bash variables are assigned with name=value. Another sentence.","# examples\nBash variables are assigned with name=value.","Bash version 3 variables are assigned with name=value.","Bash variables are assigned with name=.","Bash variables are assigned with name=value=other.","Bash variables are assigned with name=value and read with dollar.","Bash variables are assigned with name=value and must not contain spaces around the equals sign. ","Bash variables are assigned with name=value.."};
 as_reset();check(as_call()==QS_MATCH&&qs_r.semantic_evidence==QS_UNVERIFIED_EXTRACTION,"assignment end-to-end grammar-relative only, not default engine answer");
 {const QSField*v=&qs_s.assertions[0].fields[2];const char*expected="with name=value and must not contain spaces around the equals sign";
 check(v->value.end-v->value.first==strlen(expected)&&!memcmp(as_s+v->value.first,expected,strlen(expected)),"assignment value keeps entire spacing caveat");}
 for(uint32_t i=0;i<sizeof(badq)/sizeof(*badq);i++){
  memset(&qs_q,0xa5,sizeof(qs_q));check(QSParseAssignmentQuery((const unsigned char*)badq[i],(uint32_t)strlen(badq[i]),&qs_q)==QS_UNSUPPORTED_FORM,"assignment frozen query rejection");
  check(qs_q.origin_rule==0&&qs_q.field_count==0,"assignment query fail clears record");
 }
 for(uint32_t i=0;i<sizeof(bads)/sizeof(*bads);i++){
  QSAssertion*a=&qs_s.assertions[0];memset(a,0xa5,sizeof(*a));
  check(QSExtractAssignmentSentence((const unsigned char*)bads[i],(uint32_t)strlen(bads[i]),(QSSpan){0,(uint32_t)strlen(bads[i])},a)==QS_UNSUPPORTED_FORM,"assignment frozen source rejection, no clipping");
  check(a->rule_id==0&&a->field_count==0,"assignment source fail clears record");
 }
 for(uint32_t mode=0;mode<9;mode++){
  as_reset();
  switch(mode){case 0:qs_q.state=QS_PARTIAL;break;case 1:qs_q.completeness_origin=2;break;case 2:qs_q.fields[0].origins[0].end++;break;case 3:qs_q.fields[2].kind=9;break;case 4:qs_q.fields[1].value=qs_q.fields[0].value;qs_q.fields[1].origins[0]=qs_q.fields[0].value;break;case 5:qs_q.field_count=1;break;case 6:qs_q.origin_rule=99;break;case 7:qs_q.identity.reserved=1;break;case 8:qs_q.fields[11].origins[3].end=1;break;}
  check(as_call()==QS_INVALID,"assignment forged query record including unused arrays invalid");
 }
 for(uint32_t mode=0;mode<8;mode++){
  as_reset();QSAssertion*a=&qs_s.assertions[0];
  switch(mode){case 0:a->assertion_kind=QS_NEGATIVE;break;case 1:a->fields[0].origins[0].end++;break;case 2:a->fields[11].value.end=1;break;case 3:a->field_count=2;break;case 4:a->rule_id=0;break;case 5:a->extraction_state=QS_MANUAL;break;case 6:a->rule_id=99;break;case 7:a->assertion_span.first=1;break;}
  check(as_call()==QS_INVALID,"assignment forged assertion or mixed provenance invalid");
 }
 as_reset();qs_q.origin_rule=0;check(as_call()==QS_INVALID,"assignment grammar assertion cannot masquerade in manual query arm");
 {static const unsigned char lq[]="How are Luma variables assigned?",ls[]="Luma variables are assigned with key=value.";
  memset(&qs_s,0,sizeof(qs_s));qs_s.count=1;qs_s.enumeration_state=1;
  check(QSParseAssignmentQuery(lq,sizeof(lq)-1,&qs_q)==QS_MATCH&&QSExtractAssignmentSentence(ls,sizeof(ls)-1,(QSSpan){0,sizeof(ls)-1},&qs_s.assertions[0])==QS_MATCH,"assignment synthetic subject no shell dictionary");
  check(QSCheckRecords(lq,sizeof(lq)-1,ls,sizeof(ls)-1,&qs_q,&qs_s,&qs_r)==QS_MATCH&&qs_r.semantic_evidence==QS_UNVERIFIED_EXTRACTION,"assignment synthetic end-to-end");
  check(QSParseAssignmentQuery(as_q,sizeof(as_q)-1,&qs_q)==QS_MATCH&&QSCheckRecords(as_q,sizeof(as_q)-1,ls,sizeof(ls)-1,&qs_q,&qs_s,&qs_r)==QS_NO_MATCH,"assignment distinct opaque subject does not transfer");
 }
 {static const unsigned char framed[]="# obsolete examples\nBash variables are assigned with name=value.";uint32_t first=(uint32_t)strlen("# obsolete examples\n");
  QSAssertion*a=&qs_s.assertions[0];
  check(QSExtractAssignmentSentence(framed,sizeof(framed)-1,(QSSpan){first,sizeof(framed)-1},a)==QS_MATCH,"assignment standalone-span extraction does not validate heading context");
  check(a->assertion_span.first==first&&a->fields[0].value.first==first,"assignment source origins absolute into framed bytes");
 }

 /* sllm_test sets WORKING_DIRECTORY CMAKE_SOURCE_DIR (CMakeLists:160).
    Fixed repo-relative fixture path, no __FILE__ or directory discovery. */
 {static unsigned char corpus[65536],other[65536];FILE*f=fopen("data/shells/shell_corpus.txt","rb");size_t count=0;
  check(f!=NULL,"assignment real source corpus accessible");
  if(f){count=fread(corpus,1,sizeof(corpus),f);fclose(f);QSSpan actual;
   int valid=as_checkout_span(corpus,(uint32_t)count,&actual);
   check(valid,"assignment actual checkout must be one explicit pinned representation");
   if(valid){
    uint32_t m=0;as_image_tests(corpus,(uint32_t)count);
    /* Explicit test image transform only; the original corpus stays unchanged. */
    for(uint32_t i=0;i<count;i++){
     if(count==18094&&corpus[i]=='\n')other[m++]='\r';
     if(count==18292&&corpus[i]=='\r')continue;
     other[m++]=corpus[i];
    }
    as_image_tests(other,m);
    check(as_checkout_span(corpus,(uint32_t)count,&actual),"assignment original checkout restored byteexact after mutant tests");
    /* Keep length while corrupting just one EOL: mixed endings cannot match pin. */
    for(uint32_t i=0;i<count;i++)if(corpus[i]=='\n'){
     unsigned char old=corpus[i];corpus[i]='\r';
     check(!as_checkout_span(corpus,(uint32_t)count,&actual),"assignment mixed line endings rejected, no oracle refresh");
     corpus[i]=old;break;
    }
    check(!as_checkout_span(corpus,(uint32_t)count-1,&actual),"assignment mismatched checkout length rejected");
   }
  }
 }

 as_reset();qs_q.field_count=UINT32_MAX;check(as_call()==QS_INVALID,"assignment forged query count invalid before read");
 as_reset();qs_s.assertions[0].field_count=UINT32_MAX;check(as_call()==QS_INVALID,"assignment forged assertion count invalid before read");

 {static unsigned char capq[4096],caps[65536];char q31[100],q32[100],s31[120],s32[120];
  strcpy(q31,"How are ABCDEFGHIJKLMNOPQRSTUVWXYZabcde variables assigned?");
  strcpy(q32,"How are ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef variables assigned?");
  strcpy(s31,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcde variables are assigned with key=value.");
  strcpy(s32,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef variables are assigned with key=value.");
  check(QSParseAssignmentQuery((const unsigned char*)q31,(uint32_t)strlen(q31),&qs_q)==QS_MATCH,"assignment31byteID accepted");
  check(QSParseAssignmentQuery((const unsigned char*)q32,(uint32_t)strlen(q32),&qs_q)==QS_UNSUPPORTED_FORM,"assignment32byteID rejected bounded");
  check(QSExtractAssignmentSentence((const unsigned char*)s31,(uint32_t)strlen(s31),(QSSpan){0,(uint32_t)strlen(s31)},&qs_s.assertions[0])==QS_MATCH,"assignment31byte sourceID accepted");
  check(QSExtractAssignmentSentence((const unsigned char*)s32,(uint32_t)strlen(s32),(QSSpan){0,(uint32_t)strlen(s32)},&qs_s.assertions[0])==QS_UNSUPPORTED_FORM,"assignment32byte sourceID rejected bounded");
  check(QSParseAssignmentQuery((const unsigned char*)"How are  variables assigned?",28,&qs_q)==QS_UNSUPPORTED_FORM,"assignment emptyID rejected");
  check(QSParseAssignmentQuery(NULL,0,&qs_q)==QS_UNSUPPORTED_FORM,"assignment null0query unsupported empty");
  check(QSExtractAssignmentSentence(NULL,0,(QSSpan){0,0},&qs_s.assertions[0])==QS_INVALID,"assignment null0source invalid empty span");
  memset(capq,'A',sizeof(capq));memcpy(capq,"How are ",8);
  memset(caps,'A',sizeof(caps));
  check(QSParseAssignmentQuery(capq,sizeof(capq),&qs_q)==QS_UNSUPPORTED_FORM,"assignment4096byte query bounded name rejection");
  check(QSExtractAssignmentSentence(caps,sizeof(caps),(QSSpan){0,sizeof(caps)},&qs_s.assertions[0])==QS_UNSUPPORTED_FORM,"assignment65536byte source bounded name rejection");
  check(QSParseAssignmentQuery(capq,4097,&qs_q)==QS_UNSUPPORTED_FORM,"assignment overquerycap no read");
  check(QSExtractAssignmentSentence(caps,65537,(QSSpan){0,65537},&qs_s.assertions[0])==QS_UNSUPPORTED_FORM,"assignment oversourcecap no read");
 }
 as_reset();qs_q.fields[0].origin_count=UINT32_MAX;check(as_call()==QS_INVALID,"assignment origin count before array read");
 as_reset();qs_s.assertions[0].assertion_span.end=UINT32_MAX;check(as_call()==QS_INVALID,"assignment source span overflow invalid");
 check(QSParseAssignmentQuery(NULL,1,&qs_q)==QS_INVALID,"assignment null query invalid");
 check(QSExtractAssignmentSentence(as_s,sizeof(as_s)-1,(QSSpan){2,1},&qs_s.assertions[0])==QS_INVALID,"assignment reversed span invalid");
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
 qs_tests();as_tests();
 const char*p="chat_citation_fixture.txt";FILE*f=fopen(p,"wb");if(!f)return 2;
 fputs("And those gamma rays came just 1.7 seconds after the gravitational wave signal.\nAnother seconds statement follows distinct observations.\nPrimer postulado (principio de relatividad) explains distant observations.\n",f);fclose(f);
 size_t caps[]={1,8,14,15,16,32,64,4096,8192};for(int m=0;m<3;m++)for(unsigned i=0;i<sizeof(caps)/sizeof(*caps);i++)compare(p,m,caps[i]);
 {CHAT*c=create(p);CHAT_CITATION r;char out[4096];memset(&r,0xa5,sizeof(r));check(ChatHandleToBufEx(NULL,"seconds",out,sizeof(out),&r)==0&&r.state==CHAT_CITE_NONE&&r.source.version==0,"invalid call clears receipt");check(ChatTryTextLineEx(c,"seconds",out,0,&r)==0&&r.state==CHAT_CITE_NONE,"zero size clears receipt");destroy(c);}
 remove(p);printf("test_chat_citation: %d failed\n",failures);return failures?1:0;
}
