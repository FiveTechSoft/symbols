#include "duration_join_research.h"
#include "tl_sha256.h"
#include "windows_timeout_fact_research.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
/* The one reviewed sidecar is byte-pinned. A new fact format/version requires
   review and parser extension; arbitrary JSON cannot alter executable rules.
   Range/sentinel/conditions below are the reviewed contract, not NLP guesses. */
static unsigned char *readall(const char*p,size_t*n){FILE*f=fopen(p,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long z=ftell(f);if(z<0||z>1048576||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}unsigned char*b=malloc((size_t)z+1);if(!b){fclose(f);return NULL;}*n=fread(b,1,(size_t)z,f);fclose(f);if(*n!=(size_t)z||memchr(b,0,*n)){free(b);return NULL;}b[*n]=0;return b;}
static void digest(const unsigned char*b,size_t n,char*h){unsigned char d[32];TLSha256(b,n,d);for(unsigned k=0;k<32;k++)sprintf(h+2*k,"%02x",d[k]);}
static int numeric(const char*s,long*v){if(!s||!*s||*s=='+')return 0;const char*p=*s=='-'?s+1:s;if(!*p||(*p=='0'&&p[1]))return 0;for(;*p;p++)if(*p<'0'||*p>'9')return 0;errno=0;char*end;*v=strtol(s,&end,10);return !errno&&!*end;}
/* Deliberately controlled assertion grammar, not prose inference:
   one ASCII activity label ending in "ing", followed by " uses ".
   Quotes, condition punctuation, shell operators and extra subject words are
   refused. A line outside this syntax cannot establish a positive assertion. */
static int assertion_scope(const char *line,const char *uses) {
 if(!uses||uses==line)return 0;
 size_t subject=(size_t)(uses-line);
 if(subject<4||memcmp(line+subject-3,"ing",3))return 0;
 for(size_t i=0;i<subject;i++)if(!((line[i]>='A'&&line[i]<='Z')||(line[i]>='a'&&line[i]<='z')))return 0;
 for(const char *p=line;*p;p++)if(strchr("\"'`;|&()[]{}<>:\\",*p))return 0;
 /* Every coordinate clause must be complete, not just the selected one. */
 const char *cursor=uses+6;
 while(*cursor){
  const char *sep=strstr(cursor,", ");if(!sep)sep=strstr(cursor," and ");
  const char *end=sep?sep:line+strlen(line);if(end>cursor&&end[-1]=='.')end--;
  if(!strncmp(cursor,"and ",4))cursor+=4;
  const char *in=strstr(cursor," in ");if(!in||in>=end)return 0;
  const char *sh=in+4;size_t n=(size_t)(end-sh);
  if(!((n==3&&!memcmp(sh,"CMD",3))||(n==4&&!memcmp(sh,"Bash",4))||(n==10&&!memcmp(sh,"PowerShell",10))))return 0;
  for(const char *p=cursor;p<in;p++)if(*p=='.'||*p==',')return 0;
  if(!sep)break;cursor=sep+(!strncmp(sep,", ",2)?2:5);
 }
 return 1;
}
static int join_envelope(const char*q,const char*corpus,const char*fact,char*out,size_t cap){
 if(!q||strncmp(q,"duration-fact ",14))return 0;
 if(!out||!cap)return 1;out[0]=0;
 char platform[32],shell[32],value[32],unit[32],qualifier[32],tail;long requested;
 int fields=sscanf(q,"duration-fact platform=%31s shell=%31s value=%31s unit=%31s qualifier=%31s %c",platform,shell,value,unit,qualifier,&tail);
 if(fields!=5||!numeric(value,&requested)||requested<0){snprintf(out,cap,"UNSUPPORTED_REQUEST: explicit interpreted numeric envelope required; natural-language normalization is not implemented.");return 1;}
 if(strcmp(platform,"Windows")||strcmp(shell,"CMD")){snprintf(out,cap,"OUT_OF_SCOPE: reviewed fact is Windows CMD only.");return 1;}
 if(strcmp(unit,"seconds")){snprintf(out,cap,"UNSUPPORTED_UNIT: no conversion rule is loaded.");return 1;}
 if(strcmp(qualifier,"default")&&strcmp(qualifier,"ignore_keyboard")&&strcmp(qualifier,"exact_wall")){snprintf(out,cap,"UNSUPPORTED_QUALIFIER");return 1;}
 size_t fn,cn;unsigned char*fb=readall(fact,&fn);char fh[65],ch[65];if(!fb){snprintf(out,cap,"MISSING_FACT: no reviewed parameter evidence loaded.");return 1;}digest(fb,fn,fh);free(fb);
 if(strcmp(fh,WINDOWS_TIMEOUT_FACT_SHA)){snprintf(out,cap,"UNVERIFIED_FACT: sidecar differs from the reviewed byte identity.");return 1;}
 unsigned char*cb=readall(corpus,&cn);if(!cb){snprintf(out,cap,"UNSUPPORTED_SOURCE: corpus bytes unavailable or unsupported.");return 1;}digest(cb,cn,ch);
 unsigned count=0;size_t chosen_a=0,chosen_b=0;char chosen[512]="";int chosen_nobreak=0;size_t offset=0;
 while(offset<cn){size_t end=offset;while(end<cn&&cb[end]!='\n')end++;size_t line_end=end;while(line_end>offset&&cb[line_end-1]=='\r')line_end--;char*line=malloc(line_end-offset+1);if(!line){free(cb);snprintf(out,cap,"UNSUPPORTED_SOURCE: allocation failure.");return 1;}memcpy(line,cb+offset,line_end-offset);line[line_end-offset]=0;char*uses=strstr(line," uses ");
  if(assertion_scope(line,uses)){char*cursor=uses+6;while(*cursor){char*sep=strstr(cursor,", ");if(!sep)sep=strstr(cursor," and ");char*last=sep?sep:line+strlen(line);size_t len=(size_t)(last-cursor);char segment[512];if(len<sizeof(segment)){memcpy(segment,cursor,len);segment[len]=0;if(!strncmp(segment,"and ",4))memmove(segment,segment+4,strlen(segment+4)+1);char*in=strstr(segment," in ");if(in){*in=0;char*sh=in+4;size_t sl=strlen(sh);if(sl&&sh[sl-1]=='.')sh[--sl]=0;
    if(!strcmp(sh,shell)){int nb=0;
     if(WindowsTimeoutFactMatches(segment,requested,qualifier,&nb)){
      size_t command_len=strlen(segment);char*original=cursor;if(!strncmp(original,"and ",4))original+=4;count++;chosen_a=offset+(size_t)(original-line);chosen_b=chosen_a+command_len;snprintf(chosen,sizeof(chosen),"%s",segment);chosen_nobreak=nb;
     }
    }
   }}if(!sep)break;cursor=sep+(!strncmp(sep,", ",2)?2:5);}}
  free(line);offset=end<cn?end+1:end;
 }
 if(count==0)snprintf(out,cap,"NO_SUPPORT: no unambiguous source clause and reviewed parameter fact support this numeric request and qualifier. No duration, sentinel or unit conversion is inferred.");
 else if(count>1)snprintf(out,cap,"AMBIGUOUS: %u source clauses match; no command selected.",count);
 else if(chosen_b>cn||memcmp(cb+chosen_a,chosen,chosen_b-chosen_a))snprintf(out,cap,"UNSUPPORTED_SOURCE: evidence range mismatch.");
 else {int w=snprintf(out,cap,"DOCUMENTATION-SUPPORTED, not Windows execution\nCommand from corpus: %s\nInterpreted request: platform=%s shell=%s value=%ld unit=%s qualifier=%s\nRule: same shell + command timeout + parameter /t + integer value + seconds\nKeyboard: %s. No exact wall-clock guarantee.\nCorpus: %s sha256=%s bytes[%zu,%zu)\nParameter fact: microsoft.cmd.timeout.t.seconds sha256=%s\nSource: %s\nFact excerpt: /t specifies seconds, range -1..99999; -1 is indefinite until keystroke. /nobreak ignores key strokes.\nNatural-language morphology and number words are NOT interpreted by this diagnostic.",chosen,platform,shell,requested,unit,qualifier,chosen_nobreak?"key strokes ignored":"a key stroke may resume early",corpus,ch,chosen_a,chosen_b,fh,WINDOWS_TIMEOUT_SOURCE_URL);if(w<0||(size_t)w>=cap)snprintf(out,cap,"UNSUPPORTED_OUTPUT: evidence would be truncated.");}
 free(cb);return 1;
}

/* Bounded English query grammar. This is not arbitrary QA or a semantic parser.
   Every token must be consumed; unsupported polarity, conditions and actions
   cannot be silently dropped. Numbers are decimal or English zero..ninety nine.
   No command is produced here: selection still requires the source/fact join. */
static int word_number(const char *s) {
 static const char *ones[]={"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen"};
 static const char *tens[]={"twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"};
 for(int i=0;i<20;i++)if(!strcmp(s,ones[i]))return i;
 for(int i=0;i<8;i++)if(!strcmp(s,tens[i]))return (i+2)*10;
 return -1;
}
int DurationJoinResearch(const char *query,const char *corpus,const char *fact,char *out,size_t cap) {
 if(!query)return 0;
 if(!strncmp(query,"duration-fact ",14))return join_envelope(query,corpus,fact,out,cap);
 char tokens[48][40];size_t starts[48],ends[48];unsigned n=0;size_t pos=0;
 int malformed=0,has_duration=0;
 while(query[pos]) {
  if(query[pos]==' '||query[pos]=='\t'||query[pos]=='\r'||query[pos]=='\n'){pos++;continue;}
  if(query[pos]=='?'){pos++;while(query[pos]==' '||query[pos]=='\t'||query[pos]=='\r'||query[pos]=='\n')pos++;if(query[pos])malformed=1;break;}
  if(n==48){malformed=1;break;}
  size_t a=pos,z=0;
  while(query[pos]&&query[pos]!=' '&&query[pos]!='\t'&&query[pos]!='\r'&&query[pos]!='\n'&&query[pos]!='?') {
   unsigned char c=(unsigned char)query[pos++];
   if(z+1>=sizeof(tokens[0])||c>127){malformed=1;continue;}
   tokens[n][z++]=(char)(c>='A'&&c<='Z'?c+('a'-'A'):c);
  }
  tokens[n][z]=0;starts[n]=a;ends[n]=pos;
  if(!strcmp(tokens[n],"sleep")||!strcmp(tokens[n],"sleeps")||!strcmp(tokens[n],"wait")||!strcmp(tokens[n],"waits")||!strcmp(tokens[n],"pause")||!strcmp(tokens[n],"pauses"))has_duration=1;
  n++;
 }
 if(!n||(strcmp(tokens[0],"which")&&strcmp(tokens[0],"what")&&strcmp(tokens[0],"how"))||!has_duration)return 0;
 if(!out||!cap)return 1;
 #define IS(k,s) ((k)<n&&!strcmp(tokens[(k)],(s)))
 unsigned i=1;const char *platform="Windows",*shell=NULL;long value=-1;unsigned number_at=0,number_end=0;
 int how=IS(0,"how");
 if(how){
  if(!IS(i,"do")||!IS(i+1,"i"))malformed=1;else i+=2;
  if(!(IS(i,"sleep")||IS(i,"wait")||IS(i,"pause")))malformed=1;else i++;
  if(!IS(i,"for"))malformed=1;else i++;
 }else{
  if(IS(i,"windows")||IS(i,"linux")){platform=IS(i,"windows")?"Windows":"Linux";i++;}
  if(IS(i,"command")&&IS(i+1,"in")){i+=2;if(i<n)shell=tokens[i++];}
  else {if(i<n)shell=tokens[i++];if(!IS(i,"command"))malformed=1;else i++;}
  if(!(IS(i,"sleeps")||IS(i,"waits")||IS(i,"pauses")))malformed=1;else i++;
  if(IS(i,"for"))i++; /* Optional only in the explicit command question. */
 }
 const char *qualifier="default";
 if(IS(i,"exactly")){qualifier="exact_wall";i++;}
 number_at=i;
 if(i<n){if(numeric(tokens[i],&value)){i++;}else {int w=word_number(tokens[i]);if(w>=0){value=w;i++;if(w>=20&&i<n){int u=word_number(tokens[i]);if(u>0&&u<10){value+=u;i++;}}}}}
 number_end=i;
 const char *unit=NULL;if(i<n){unit=tokens[i++];if(!strcmp(unit,"second"))unit="seconds";}
 if(how){if(!IS(i,"in"))malformed=1;else i++;if(i<n)shell=tokens[i++];}
 if(IS(i,"without")&&IS(i+1,"keyboard")&&IS(i+2,"interruption")){if(strcmp(qualifier,"default"))malformed=1;qualifier="ignore_keyboard";i+=3;}
 if(malformed||!shell||value<0||!unit||i!=n||number_at==number_end){snprintf(out,cap,"UNSUPPORTED_NATURAL_REQUEST: bounded duration grammar requires a complete positive question, explicit shell, finite number, unit and supported condition; no tokens were dropped.");return 1;}
 char envelope[256];int w=snprintf(envelope,sizeof(envelope),"duration-fact platform=%s shell=%s value=%ld unit=%s qualifier=%s",platform,!strcmp(shell,"cmd")?"CMD":shell,value,unit,qualifier);
 if(w<0||(size_t)w>=sizeof(envelope)){snprintf(out,cap,"UNSUPPORTED_NATURAL_REQUEST: interpreted query overflow.");return 1;}
 int r=join_envelope(envelope,corpus,fact,out,cap);
 const char *old="Natural-language morphology and number words are NOT interpreted by this diagnostic.";
 char *note=strstr(out,old);if(note)*note=0;
 size_t used=strlen(out);w=snprintf(out+used,cap-used,"\nNormalization: bounded English duration question; number bytes[%zu,%zu) -> %ld; verb=sleep/wait/pause inflection. All query tokens consumed. No arbitrary QA or Windows execution.",starts[number_at],ends[number_end-1],value);
 if(w<0||(size_t)w>=cap-used)snprintf(out,cap,"UNSUPPORTED_OUTPUT: evidence would be truncated.");
 return r;
 #undef IS
}
