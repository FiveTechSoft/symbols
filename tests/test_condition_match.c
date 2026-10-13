/* Fixed structured-input gold, not an extraction/entailment test. */
#include "condition_match.h"
#include "tl_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails;
static void ck(int ok,const char*n){printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)fails++;}
static void id(char*d,const char*s){snprintf(d,CM_ID_CAP,"%s",s);}
static CM_REQUIREMENT key(const char*s,const char*r,const char*k,const char*v,uint32_t p){CM_REQUIREMENT q={0};q.version=1;q.polarity=p;id(q.subject,s);id(q.relation,r);if(k){q.nqual=1;id(q.qualifiers[0].key,k);id(q.qualifiers[0].value,v);}return q;}
int main(void){
 CM_RESULT out;
static const unsigned char text_amber[]="lumen reports seven during cycle dawn.\nlumen reports nine during cycle dusk.\nlumen does not reports during cycle night.\n";
static const unsigned char text_cobalt[]="prism returns blue during phase warm.\nprism returns green during phase cold.\nprism does not returns during phase idle.\n";
static const unsigned char text_jade[]="vector stores alpha during scope local.\nvector stores beta during scope remote.\nvector does not stores during scope closed.\n";
static const unsigned char text_sienna[]="relay emits low during mode quiet.\nrelay emits high during mode active.\nrelay does not emits during mode paused.\n";
static const unsigned char text_conflict[]="lumen reports seven during cycle dawn.\nlumen reports nine during cycle dawn.\nlumen does not reports during cycle dawn.\n";
CM_SOURCE sources[]={{text_amber,sizeof(text_amber)-1},{text_cobalt,sizeof(text_cobalt)-1},{text_jade,sizeof(text_jade)-1},{text_sienna,sizeof(text_sienna)-1},{text_conflict,sizeof(text_conflict)-1}};
CM_CLAIM claims[15]={0};
claims[0].key=key("lumen","reports","cycle","dawn",CM_POSITIVE);claims[0].source=0;claims[0].origin=CM_CALLER_ASSERTED;claims[0].first=0;claims[0].end=39;
id(claims[0].object,"seven");
{unsigned char h[32]={96,216,156,31,164,167,186,32,250,225,235,5,77,230,54,46,16,4,17,22,202,197,153,171,63,15,195,226,175,210,141,192};memcpy(claims[0].source_sha256,h,32);}
claims[1].key=key("lumen","reports","cycle","dusk",CM_POSITIVE);claims[1].source=0;claims[1].origin=CM_CALLER_ASSERTED;claims[1].first=39;claims[1].end=77;
id(claims[1].object,"nine");
{unsigned char h[32]={96,216,156,31,164,167,186,32,250,225,235,5,77,230,54,46,16,4,17,22,202,197,153,171,63,15,195,226,175,210,141,192};memcpy(claims[1].source_sha256,h,32);}
claims[2].key=key("lumen","reports","cycle","night",CM_NEGATIVE);claims[2].source=0;claims[2].origin=CM_CALLER_ASSERTED;claims[2].first=77;claims[2].end=120;
{unsigned char h[32]={96,216,156,31,164,167,186,32,250,225,235,5,77,230,54,46,16,4,17,22,202,197,153,171,63,15,195,226,175,210,141,192};memcpy(claims[2].source_sha256,h,32);}
claims[3].key=key("prism","returns","phase","warm",CM_POSITIVE);claims[3].source=1;claims[3].origin=CM_CALLER_ASSERTED;claims[3].first=0;claims[3].end=38;
id(claims[3].object,"blue");
{unsigned char h[32]={158,173,136,140,129,229,88,255,204,123,112,185,212,53,181,231,212,106,12,69,173,50,207,83,196,231,122,145,213,122,190,98};memcpy(claims[3].source_sha256,h,32);}
claims[4].key=key("prism","returns","phase","cold",CM_POSITIVE);claims[4].source=1;claims[4].origin=CM_CALLER_ASSERTED;claims[4].first=38;claims[4].end=77;
id(claims[4].object,"green");
{unsigned char h[32]={158,173,136,140,129,229,88,255,204,123,112,185,212,53,181,231,212,106,12,69,173,50,207,83,196,231,122,145,213,122,190,98};memcpy(claims[4].source_sha256,h,32);}
claims[5].key=key("prism","returns","phase","idle",CM_NEGATIVE);claims[5].source=1;claims[5].origin=CM_CALLER_ASSERTED;claims[5].first=77;claims[5].end=119;
{unsigned char h[32]={158,173,136,140,129,229,88,255,204,123,112,185,212,53,181,231,212,106,12,69,173,50,207,83,196,231,122,145,213,122,190,98};memcpy(claims[5].source_sha256,h,32);}
claims[6].key=key("vector","stores","scope","local",CM_POSITIVE);claims[6].source=2;claims[6].origin=CM_CALLER_ASSERTED;claims[6].first=0;claims[6].end=40;
id(claims[6].object,"alpha");
{unsigned char h[32]={192,10,124,149,171,38,240,170,134,169,47,75,137,131,196,32,94,164,56,116,164,93,194,209,2,174,89,130,38,119,114,46};memcpy(claims[6].source_sha256,h,32);}
claims[7].key=key("vector","stores","scope","remote",CM_POSITIVE);claims[7].source=2;claims[7].origin=CM_CALLER_ASSERTED;claims[7].first=40;claims[7].end=80;
id(claims[7].object,"beta");
{unsigned char h[32]={192,10,124,149,171,38,240,170,134,169,47,75,137,131,196,32,94,164,56,116,164,93,194,209,2,174,89,130,38,119,114,46};memcpy(claims[7].source_sha256,h,32);}
claims[8].key=key("vector","stores","scope","closed",CM_NEGATIVE);claims[8].source=2;claims[8].origin=CM_CALLER_ASSERTED;claims[8].first=80;claims[8].end=124;
{unsigned char h[32]={192,10,124,149,171,38,240,170,134,169,47,75,137,131,196,32,94,164,56,116,164,93,194,209,2,174,89,130,38,119,114,46};memcpy(claims[8].source_sha256,h,32);}
claims[9].key=key("relay","emits","mode","quiet",CM_POSITIVE);claims[9].source=3;claims[9].origin=CM_CALLER_ASSERTED;claims[9].first=0;claims[9].end=35;
id(claims[9].object,"low");
{unsigned char h[32]={88,149,57,61,175,206,119,72,146,193,218,168,83,219,233,16,243,204,192,218,125,225,41,162,102,225,53,0,42,130,116,15};memcpy(claims[9].source_sha256,h,32);}
claims[10].key=key("relay","emits","mode","active",CM_POSITIVE);claims[10].source=3;claims[10].origin=CM_CALLER_ASSERTED;claims[10].first=35;claims[10].end=72;
id(claims[10].object,"high");
{unsigned char h[32]={88,149,57,61,175,206,119,72,146,193,218,168,83,219,233,16,243,204,192,218,125,225,41,162,102,225,53,0,42,130,116,15};memcpy(claims[10].source_sha256,h,32);}
claims[11].key=key("relay","emits","mode","paused",CM_NEGATIVE);claims[11].source=3;claims[11].origin=CM_CALLER_ASSERTED;claims[11].first=72;claims[11].end=113;
{unsigned char h[32]={88,149,57,61,175,206,119,72,146,193,218,168,83,219,233,16,243,204,192,218,125,225,41,162,102,225,53,0,42,130,116,15};memcpy(claims[11].source_sha256,h,32);}
claims[12].key=key("lumen","reports","cycle","dawn",CM_POSITIVE);claims[12].source=4;claims[12].origin=CM_CALLER_ASSERTED;claims[12].first=0;claims[12].end=39;
id(claims[12].object,"seven");
{unsigned char h[32]={69,157,179,83,153,182,160,244,93,142,110,95,88,29,64,57,209,4,135,63,7,138,238,58,135,233,102,71,132,8,29,184};memcpy(claims[12].source_sha256,h,32);}
claims[13].key=key("lumen","reports","cycle","dawn",CM_POSITIVE);claims[13].source=4;claims[13].origin=CM_CALLER_ASSERTED;claims[13].first=39;claims[13].end=77;
id(claims[13].object,"nine");
{unsigned char h[32]={69,157,179,83,153,182,160,244,93,142,110,95,88,29,64,57,209,4,135,63,7,138,238,58,135,233,102,71,132,8,29,184};memcpy(claims[13].source_sha256,h,32);}
claims[14].key=key("lumen","reports","cycle","dawn",CM_NEGATIVE);claims[14].source=4;claims[14].origin=CM_CALLER_ASSERTED;claims[14].first=77;claims[14].end=119;
{unsigned char h[32]={69,157,179,83,153,182,160,244,93,142,110,95,88,29,64,57,209,4,135,63,7,138,238,58,135,233,102,71,132,8,29,184};memcpy(claims[14].source_sha256,h,32);}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold amber-match1");
ck(!strcmp(out.object,"seven"),"object amber-match1");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dusk");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold amber-match2");
ck(!strcmp(out.object,"nine"),"object amber-match2");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"night");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_REFUTED,"gold amber-negative");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"storm");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold amber-absent");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("prism","returns",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"phase");id(q.qualifiers[0].value,"warm");
set[0]=claims[3];
set[1]=claims[4];
set[2]=claims[5];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold cobalt-match1");
ck(!strcmp(out.object,"blue"),"object cobalt-match1");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("prism","returns",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"phase");id(q.qualifiers[0].value,"cold");
set[0]=claims[3];
set[1]=claims[4];
set[2]=claims[5];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold cobalt-match2");
ck(!strcmp(out.object,"green"),"object cobalt-match2");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("prism","returns",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"phase");id(q.qualifiers[0].value,"idle");
set[0]=claims[3];
set[1]=claims[4];
set[2]=claims[5];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_REFUTED,"gold cobalt-negative");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("prism","returns",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"phase");id(q.qualifiers[0].value,"rain");
set[0]=claims[3];
set[1]=claims[4];
set[2]=claims[5];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold cobalt-absent");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("vector","stores",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"scope");id(q.qualifiers[0].value,"local");
set[0]=claims[6];
set[1]=claims[7];
set[2]=claims[8];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold jade-match1");
ck(!strcmp(out.object,"alpha"),"object jade-match1");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("vector","stores",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"scope");id(q.qualifiers[0].value,"remote");
set[0]=claims[6];
set[1]=claims[7];
set[2]=claims[8];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold jade-match2");
ck(!strcmp(out.object,"beta"),"object jade-match2");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("vector","stores",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"scope");id(q.qualifiers[0].value,"closed");
set[0]=claims[6];
set[1]=claims[7];
set[2]=claims[8];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_REFUTED,"gold jade-negative");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("vector","stores",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"scope");id(q.qualifiers[0].value,"wide");
set[0]=claims[6];
set[1]=claims[7];
set[2]=claims[8];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold jade-absent");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("relay","emits",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"mode");id(q.qualifiers[0].value,"quiet");
set[0]=claims[9];
set[1]=claims[10];
set[2]=claims[11];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold sienna-match1");
ck(!strcmp(out.object,"low"),"object sienna-match1");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("relay","emits",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"mode");id(q.qualifiers[0].value,"active");
set[0]=claims[9];
set[1]=claims[10];
set[2]=claims[11];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_MATCH,"gold sienna-match2");
ck(!strcmp(out.object,"high"),"object sienna-match2");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("relay","emits",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"mode");id(q.qualifiers[0].value,"paused");
set[0]=claims[9];
set[1]=claims[10];
set[2]=claims[11];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_REFUTED,"gold sienna-negative");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("relay","emits",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"mode");id(q.qualifiers[0].value,"dense");
set[0]=claims[9];
set[1]=claims[10];
set[2]=claims[11];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold sienna-absent");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=0;
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold control-unqualified");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=2;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
id(q.qualifiers[1].key,"region");id(q.qualifiers[1].value,"east");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold control-extra_scope");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_NEGATIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_REFUTED,"gold control-negative_request");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=2;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
id(q.qualifiers[1].key,"cycle");id(q.qualifiers[1].value,"dawn");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_INVALID,"gold control-duplicate_key");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("Lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_UNDETERMINED,"gold control-wrong_subject");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=2;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
set[0]=claims[0];
set[1]=claims[1];
set[2]=claims[2];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_INVALID,"gold control-wrong_version");
}
{CM_CLAIM set[3];CM_REQUIREMENT q=key("lumen","reports",NULL,NULL,CM_POSITIVE);q.version=1;q.nqual=1;
id(q.qualifiers[0].key,"cycle");id(q.qualifiers[0].value,"dawn");
set[0]=claims[12];
set[1]=claims[13];
set[2]=claims[14];
ck(ConditionMatch(&q,set,3,sources,5,&out)==CM_CONFLICT,"gold conflict-all");
}

 {CM_REQUIREMENT q=claims[0].key;CM_CLAIM c[3]={claims[0],claims[1],claims[2]};
 CM_STATUS expected=CM_MATCH;ConditionMatch(&q,c,3,sources,5,&out);CM_CLAIM picked=claims[0];
 for(int i=0;i<3;i++)for(int j=0;j<3;j++)if(i!=j){int k=3-i-j;CM_CLAIM p[3]={c[i],c[j],c[k]};ck(ConditionMatch(&q,p,3,sources,5,&out)==expected&&out.claim_index<3&&!memcmp(p[out.claim_index].source_sha256,picked.source_sha256,32)&&p[out.claim_index].first==picked.first,"claim permutation stable evidence");}
 CM_CLAIM conflict[3]={claims[12],claims[13],claims[14]};
 ck(ConditionMatch(&q,conflict,2,sources,5,&out)==CM_CONFLICT,"different positive objects conflict");
 CM_CLAIM opposed[2]={claims[12],claims[14]};ck(ConditionMatch(&q,opposed,2,sources,5,&out)==CM_CONFLICT,"opposite polarity conflict");
 for(int i=0;i<3;i++)for(int j=0;j<3;j++)if(i!=j){int k=3-i-j;CM_CLAIM p[3]={conflict[i],conflict[j],conflict[k]};ck(ConditionMatch(&q,p,3,sources,5,&out)==CM_CONFLICT&&out.claim_index==UINT32_MAX&&!out.object[0],"conflict permutations clear result");}
 c[0].key.nqual=2;id(c[0].key.qualifiers[1].key,"region");id(c[0].key.qualifiers[1].value,"east");q=c[0].key;
 CM_QUALIFIER tmp=q.qualifiers[0];q.qualifiers[0]=q.qualifiers[1];q.qualifiers[1]=tmp;
 ck(ConditionMatch(&q,c,1,sources,5,&out)==CM_MATCH,"qualifier ordering is irrelevant");
 q=claims[0].key;ck(ConditionMatch(&q,c,1,sources,5,&out)==CM_UNDETERMINED,"narrow claim cannot broaden requirement");
 }
 {CM_REQUIREMENT q=claims[0].key;CM_CLAIM bad=claims[0];
 bad.source_sha256[0]^=1;ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID&&out.claim_index==UINT32_MAX&&!out.object[0],"stale source hash refused");
 bad=claims[0];bad.end=sources[0].size+1;ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID,"end outside image refused");
 bad=claims[0];bad.first=bad.end;ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID,"empty span refused");
 bad=claims[0];bad.source=5;ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID,"wrong source index refused");
 bad=claims[0];bad.origin=0;ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID,"unlabeled assertion refused");
 bad=claims[0];memset(bad.object,'x',CM_ID_CAP);ck(ConditionMatch(&q,&bad,1,sources,5,&out)==CM_INVALID,"unterminated id refused");
 q.subject[0]=(char)0xc0;q.subject[1]=(char)0x80;q.subject[2]=0;ck(ConditionMatch(&q,claims,1,sources,5,&out)==CM_INVALID,"overlong UTF8 refused");
 q=claims[0].key;id(q.subject,"lumen\xc3\xa9");ck(ConditionMatch(&q,claims,1,sources,5,&out)==CM_UNDETERMINED,"valid UTF8 remains exact");
 q=claims[0].key;q.nqual=CM_QUAL_MAX+1;ck(ConditionMatch(&q,claims,1,sources,5,&out)==CM_INVALID,"qualifier limit refused");
 q=claims[0].key;ck(ConditionMatch(&q,claims,CM_CLAIM_MAX+1,sources,5,&out)==CM_INVALID,"claim limit refused before dereference");
 ck(ConditionMatch(&q,claims,1,sources,CM_SOURCE_MAX+1,&out)==CM_INVALID,"source limit refused before dereference");
 CM_CLAIM all[CM_CLAIM_MAX];for(unsigned i=0;i<CM_CLAIM_MAX;i++)all[i]=claims[0];ck(ConditionMatch(&q,all,CM_CLAIM_MAX,sources,5,&out)==CM_MATCH,"claim limit boundary accepted");
 CM_SOURCE big={sources[0].bytes,CM_SOURCE_BYTES_MAX+1};ck(ConditionMatch(&q,claims,1,&big,1,&out)==CM_INVALID,"source byte limit refused before hashing");
 unsigned char *large=malloc(CM_SOURCE_BYTES_MAX);if(!large)return 2;memset(large,'x',CM_SOURCE_BYTES_MAX);CM_SOURCE many[5];for(unsigned i=0;i<5;i++){many[i].bytes=large;many[i].size=CM_SOURCE_BYTES_MAX;}
 ck(ConditionMatch(&q,NULL,0,many,4,&out)==CM_UNDETERMINED,"total byte limit boundary accepted");
 ck(ConditionMatch(&q,NULL,0,many,5,&out)==CM_INVALID,"total byte limit refused");free(large);
 ck(ConditionMatch(NULL,claims,1,sources,5,&out)==CM_INVALID,"null requirement refused");
 ck(ConditionMatch(&q,NULL,1,sources,5,&out)==CM_INVALID,"null claims refused");
 ck(ConditionMatch(&q,claims,1,NULL,1,&out)==CM_INVALID,"null sources refused");
 ck(ConditionMatch(&q,claims,1,sources,5,NULL)==CM_INVALID,"null output refused");
 ck(ConditionMatch(&q,NULL,0,NULL,0,&out)==CM_UNDETERMINED&&out.claim_index==UINT32_MAX&&!out.object[0],"fresh empty call cannot inherit MATCH");
 }
 printf("test_condition_match: %d failed\n",fails);return fails?1:0;
}
