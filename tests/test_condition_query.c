/* Frozen GOLD SHA256 bf81f73e517174540ead78766b1f9eea648fb598eb8b1694861ce3b146448f7d */
#include "condition_query.h"
#include "tl_sha256.h"
#include <stdio.h>
#include <string.h>
typedef struct {const char *id,*q,*s; size_t nq,ns; const char *pin; CQ_STATUS status; const char *object,*hash; uint64_t first,end;} CASE;
static const CASE cases[]={
{"a_match","What does s17 r23 during k11=v19 and k29=v37?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",45,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_ASSERTION_MATCH,"o31","c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",0,39},
{"a_pair_permutation","What does s17 r23 during k29=v37 and k11=v19?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",45,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_ASSERTION_MATCH,"o31","c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",0,39},
{"a_other_condition","What does s17 r23 during k11=v43 and k29=v37?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",45,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_ASSERTION_MATCH,"o41","c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",40,79},
{"a_missing_qualifier","What does s17 r23 during k11=v19?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",33,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_UNKNOWN,"","",0,0},
{"a_wrong_condition","What does s17 r23 during k11=v191 and k29=v37?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",46,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_UNKNOWN,"","",0,0},
{"a_source_pair_permutation","What does s17 r23 during k11=v19 and k29=v37?","s17 r23 o31 during k29=v37 and k11=v19.\012s17 r23 o41 during k11=v43 and k29=v37.\012",45,80,"5b4d0725e8b39ae55f5f182be9a4dcf61e67a2da97371791669efc34bc94ffe1",CQ_ASSERTION_MATCH,"o31","5b4d0725e8b39ae55f5f182be9a4dcf61e67a2da97371791669efc34bc94ffe1",0,39},
{"a_line_permutation","What does s17 r23 during k11=v19 and k29=v37?","s17 r23 o41 during k11=v43 and k29=v37.\012s17 r23 o31 during k11=v19 and k29=v37.\012",45,80,"309ec46ec73b13e44893f485515698d7844bb7f345ada759b69e3c75cce71489",CQ_ASSERTION_MATCH,"o31","309ec46ec73b13e44893f485515698d7844bb7f345ada759b69e3c75cce71489",40,79},
{"b_negative","What does s47 r53 during k59=v61?","s47 does not r53 during k59=v61.\012s47 r53 o67 during k59=v71.\012",33,61,"4105893a7a6266a01e102ee635ccdde9824f04bf179c16d3e945f6acb0a4d775",CQ_ASSERTION_REFUTED,"","4105893a7a6266a01e102ee635ccdde9824f04bf179c16d3e945f6acb0a4d775",0,32},
{"b_positive","What does s47 r53 during k59=v71?","s47 does not r53 during k59=v61.\012s47 r53 o67 during k59=v71.\012",33,61,"4105893a7a6266a01e102ee635ccdde9824f04bf179c16d3e945f6acb0a4d775",CQ_ASSERTION_MATCH,"o67","4105893a7a6266a01e102ee635ccdde9824f04bf179c16d3e945f6acb0a4d775",33,60},
{"b_unknown","What does s47 r53 during k59=v73?","s47 does not r53 during k59=v61.\012s47 r53 o67 during k59=v71.\012",33,61,"4105893a7a6266a01e102ee635ccdde9824f04bf179c16d3e945f6acb0a4d775",CQ_UNKNOWN,"","",0,0},
{"c_object_conflict","What does s73 r79 during k89=v97?","s73 r79 o83 during k89=v97.\012s73 r79 o101 during k89=v97.\012s107 r109 o113 during k127=v131.\012",33,90,"17078926efed81815cda05743ec9227295663042f8d85c294442552c7c3e2068",CQ_CONFLICT,"","",0,0},
{"c_unrelated_conflict","What does s107 r109 during k127=v131?","s73 r79 o83 during k89=v97.\012s73 r79 o101 during k89=v97.\012s107 r109 o113 during k127=v131.\012",37,90,"17078926efed81815cda05743ec9227295663042f8d85c294442552c7c3e2068",CQ_ASSERTION_MATCH,"o113","17078926efed81815cda05743ec9227295663042f8d85c294442552c7c3e2068",57,89},
{"d_polarity_conflict","What does s137 r139 during k151=v157?","s137 r139 o149 during k151=v157.\012s137 does not r139 during k151=v157.\012s163 r167 o173 during k179=v181.\012",37,103,"779788c09dd556e7123816edc4f97cbc8ebe40f38cdb221945428806dd1b186c",CQ_CONFLICT,"","",0,0},
{"d_unrelated_polarity","What does s163 r167 during k179=v181?","s137 r139 o149 during k151=v157.\012s137 does not r139 during k151=v157.\012s163 r167 o173 during k179=v181.\012",37,103,"779788c09dd556e7123816edc4f97cbc8ebe40f38cdb221945428806dd1b186c",CQ_ASSERTION_MATCH,"o173","779788c09dd556e7123816edc4f97cbc8ebe40f38cdb221945428806dd1b186c",70,102},
{"d_late_malformed","What does s163 r167 during k179=v181?","s137 r139 o149 during k151=v157.\012s137 does not r139 during k151=v157.\012s163 r167 o173 during k179=v181.\012broken.\012",37,111,"673e7fd6f217742668c91d06b5a8a74871f2d8551154d7e475286eb7385b449e",CQ_INVALID,"","",0,0},
{"d_stale_pin","What does s163 r167 during k179=v181?","s137 r139 o149 during k151=v157.\012s137 does not r139 during k151=v157.\012s163 r167 o173 during k179=v181.\012",37,103,"0000000000000000000000000000000000000000000000000000000000000000",CQ_INVALID,"","",0,0},
{"d_lf","What does s163 r167 during k179=v181?","s163 r167 o173 during k179=v181.\012",37,33,"d9340cab315c9c55ab9b94bf3ddc35fec23d58a1b7491702b2e6cac7208ebc67",CQ_ASSERTION_MATCH,"o173","d9340cab315c9c55ab9b94bf3ddc35fec23d58a1b7491702b2e6cac7208ebc67",0,32},
{"d_crlf","What does s163 r167 during k179=v181?","s163 r167 o173 during k179=v181.\015\012",37,34,"b8b065d973e88757210d33f35c0f7fb9c7ff7d82e12a72e36802d811f7d586e8",CQ_ASSERTION_MATCH,"o173","b8b065d973e88757210d33f35c0f7fb9c7ff7d82e12a72e36802d811f7d586e8",0,32},
{"d_no_lf","What does s163 r167 during k179=v181?","s163 r167 o173 during k179=v181.",37,32,"550504a80ae8dd4a13c5da693dc0516e78a07d59141c6b0d9e370c4431c555d0",CQ_ASSERTION_MATCH,"o173","550504a80ae8dd4a13c5da693dc0516e78a07d59141c6b0d9e370c4431c555d0",0,32},
{"d_duplicate","What does s163 r167 during k179=v181?","s163 r167 o173 during k179=v181.\012s163 r167 o173 during k179=v181.\012",37,66,"6340f92cd809fa5b583166c9f7013aff426a5bd21a221e57b5c42c428dd24fe6",CQ_ASSERTION_MATCH,"o173","6340f92cd809fa5b583166c9f7013aff426a5bd21a221e57b5c42c428dd24fe6",0,32},
{"d_blank","What does s163 r167 during k179=v181?","\012\015\012",37,3,"7393ba4f11e19a5f6bee10ed995b0d959a52c4470855f6d68d4d1e34e26cb70f",CQ_UNKNOWN,"","",0,0},
{"invalid_duplicate_key","What does s17 r23 during k11=v19 and k11=v37?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",45,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_INVALID,"","",0,0},
{"invalid_third_pair","What does s17 r23 during k11=v19 and k29=v37 and k31=v41?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",57,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_INVALID,"","",0,0},
{"invalid_suffix","What does s17 r23 during k11=v19? extra","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",39,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_INVALID,"","",0,0},
{"invalid_nonascii","What does s17 r23 during k11=v\303\251?","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",33,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_INVALID,"","",0,0},
{"invalid_nul","What does s17 r23 during k11=v19?\000","s17 r23 o31 during k11=v19 and k29=v37.\012s17 r23 o41 during k11=v43 and k29=v37.\012",34,80,"c00a02a5c57e3bf7a0b203b19a0bb445725b340b165d00dbce7900a225da3398",CQ_INVALID,"","",0,0},
};

static int fails;
static void ck(int ok,const char *id){if(!ok){printf("FAIL %s\n",id);fails++;}}
static void decode(const char *hex,unsigned char *h){size_t i;for(i=0;i<32;i++){unsigned v=0;sscanf(hex+2*i,"%2x",&v);h[i]=(unsigned char)v;}}
static void check(const CASE *c,CQ_RESULT *out){
 unsigned char pin[32],hash[32]; CQ_RESULT expected;
 decode(c->pin,pin); memset(&expected,0,sizeof(expected));
 expected.version=1;expected.status=c->status;strcpy(expected.object,c->object);
 if(c->hash[0]){decode(c->hash,hash);memcpy(expected.source_sha256,hash,32);expected.origin=CM_EXTRACTOR_ASSERTED;expected.first=c->first;expected.end=c->end;}
 ck(ConditionQuery((const unsigned char*)c->q,c->nq,(const unsigned char*)c->s,c->ns,pin,out)==c->status,c->id);
 ck(!memcmp(out,&expected,sizeof(expected)),c->id);
}
static void boundary(const char *id,unsigned char *q,size_t nq,unsigned char *s,size_t ns,CQ_STATUS status,int null_arg){
 unsigned char pin[32],before[32],qbefore[32],after[32]; CQ_RESULT out,expected;
 TLSha256(s,ns,pin);memcpy(before,pin,32);TLSha256(q,nq,qbefore);
 memset(&out,0xa5,sizeof(out));memset(&expected,0,sizeof(expected));expected.version=1;expected.status=status;
 if(status==CQ_ASSERTION_MATCH){strcpy(expected.object,strstr((char*)s,"o173")?"o173":"o31");expected.origin=CM_EXTRACTOR_ASSERTED;memcpy(expected.source_sha256,pin,32);expected.end=(uint64_t)(strchr((char*)s,'\n')-(char*)s);}
 ck(ConditionQuery(null_arg==1?NULL:q,nq,null_arg==2?NULL:s,ns,null_arg==3?NULL:pin,null_arg==4?NULL:&out)==status,id);
 if(null_arg!=4)ck(!memcmp(&out,&expected,sizeof(out)),id);
 TLSha256(s,ns,after);ck(!memcmp(before,after,32),id);TLSha256(q,nq,after);ck(!memcmp(qbefore,after,32),id);
}
int main(void){
 size_t i; CQ_RESULT out; unsigned char s[65538],q[514];
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){unsigned char a[32],b[32];TLSha256((const unsigned char*)cases[i].s,cases[i].ns,a);TLSha256((const unsigned char*)cases[i].q,cases[i].nq,b);memset(&out,0xa5,sizeof(out));check(&cases[i],&out);{unsigned char h[32];TLSha256((const unsigned char*)cases[i].s,cases[i].ns,h);ck(!memcmp(a,h,32),"source immutable");TLSha256((const unsigned char*)cases[i].q,cases[i].nq,h);ck(!memcmp(b,h,32),"question immutable");}}
 {const size_t seq[]={0,14,12,9,0};for(i=0;i<5;i++){CQ_RESULT fresh;memset(&fresh,0x37,sizeof(fresh));check(&cases[seq[i]],&fresh);check(&cases[seq[i]],&out);ck(!memcmp(&fresh,&out,sizeof(out)),"state sequence");}}
 /* Boundary constructions are frozen pre-code scenarios, not engine labels. */
 strcpy((char*)q,cases[0].q);memcpy(s,cases[0].s,cases[0].ns);memset(s+cases[0].ns,'\n',65537-cases[0].ns);s[65537]=0;
 boundary("source 65536",q,cases[0].nq,s,65536,CQ_ASSERTION_MATCH,0);
 boundary("source 65537",q,cases[0].nq,s,65537,CQ_INVALID,0);
 {const CASE *c=&cases[16];strcpy((char*)q,c->q);for(i=0;i<65;i++)memcpy(s+i*c->ns,c->s,c->ns);s[65*c->ns]=0;
 boundary("64 claims",q,c->nq,s,64*c->ns,CQ_ASSERTION_MATCH,0);boundary("65 claims",q,c->nq,s,65*c->ns,CQ_INVALID,0);}
 for(i=63;i<=64;i++){
 memset(s,'s',i);strcpy((char*)s+i," r167 o173 during k179=v181.\n");strcpy((char*)q,"What does ");memset(q+10,'s',i);strcpy((char*)q+10+i," r167 during k179=v181?");
 boundary(i==63?"63 identifier":"64 identifier",q,strlen((char*)q),s,strlen((char*)s),i==63?CQ_ASSERTION_MATCH:CQ_INVALID,0);}
 strcpy((char*)s,cases[0].s);strcpy((char*)q,cases[0].q);memset(q+cases[0].nq,' ',513-cases[0].nq);q[513]=0;
 boundary("512 question",q,512,s,cases[0].ns,CQ_INVALID,0);boundary("513 question",q,513,s,cases[0].ns,CQ_INVALID,0);
 strcpy((char*)q,cases[0].q);
 for(i=1024;i<=1025;i++){memcpy(s,cases[0].s,cases[0].ns);memset(s+cases[0].ns,'x',i);s[cases[0].ns+i]='\n';s[cases[0].ns+i+1]=0;boundary("line limit",q,cases[0].nq,s,cases[0].ns+i+1,CQ_INVALID,0);}
 strcpy((char*)s,cases[16].s);strcpy((char*)q,cases[16].q);
 boundary("empty source",q,cases[16].nq,s,0,CQ_INVALID,0);
 for(i=1;i<=4;i++)boundary("null input/output",q,cases[16].nq,s,cases[16].ns,CQ_INVALID,(int)i);
 strcpy((char*)s,cases[18].s);strcat((char*)s,"\r");boundary("bare CR",q,cases[16].nq,s,strlen((char*)s),CQ_INVALID,0);
 printf("42 gold/boundary scenarios + 5 state transitions: %d failures\n",fails);return fails?1:0;
}
