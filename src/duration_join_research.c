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
typedef struct {
 char activity[128],command[512],shell[16];
 size_t command_a,command_b,line_a,line_b;
 int negative;
} SourceClause;
/* One complete controlled clause reader for both polarities. No text is
   rewritten: every range refers to original corpus bytes. */
static int read_line_clauses(char *line,size_t offset,size_t end,
                             SourceClause *out,size_t capacity,size_t *count){
 char *uses=strstr(line," never uses ");int negative=uses!=NULL;
 if(!uses)uses=strstr(line," uses ");if(!uses)return 1;
 size_t subject=(size_t)(uses-line);if(subject<4||subject>=128||memcmp(line+subject-3,"ing",3))return 1;
 for(size_t i=0;i<subject;i++)if(!((line[i]>='A'&&line[i]<='Z')||(line[i]>='a'&&line[i]<='z')))return 1;
 for(char *p=line;*p;p++)if(strchr("\"'`;|&()[]{}<>:\\",*p))return 1;
 char *cursor=uses+(negative?12:6);size_t original_count=*count;
 while(*cursor){
  char *comma=strstr(cursor,", "),*and=strstr(cursor," and ");
  char *sep=comma&&and?(comma<and?comma:and):(comma?comma:and);
  char *last=sep?sep:line+strlen(line);if(!sep&&last>cursor&&last[-1]=='.')last--;
  if(!strncmp(cursor,"and ",4))cursor+=4;
  char *in=strstr(cursor," in ");if(!in||in>=last){*count=original_count;return 1;}
  size_t command_len=(size_t)(in-cursor),shell_len=(size_t)(last-(in+4));
  if(!command_len||command_len>=512||!((shell_len==3&&!memcmp(in+4,"CMD",3))||(shell_len==4&&!memcmp(in+4,"Bash",4))||(shell_len==10&&!memcmp(in+4,"PowerShell",10)))){*count=original_count;return 1;}
  for(char *p=cursor;p<in;p++)if(*p=='.'||*p==','){*count=original_count;return 1;}
  if(*count==capacity)return 0;
  SourceClause *c=out?&out[*count]:NULL;(*count)++;
  if(c){memset(c,0,sizeof(*c));
  memcpy(c->activity,line,subject);memcpy(c->command,cursor,command_len);memcpy(c->shell,in+4,shell_len);
  c->negative=negative;c->line_a=offset;c->line_b=end;
  c->command_a=offset+(size_t)(cursor-line);c->command_b=c->command_a+command_len;}
  if(!sep)break;cursor=sep+(!strncmp(sep,", ",2)?2:5);
  if(!*cursor){*count=original_count;return 1;}
 }
 return 1;
}
static SourceClause *source_clauses(const unsigned char *bytes,size_t size,size_t *count){
 /* Count with the same transactional reader, then allocate actual clauses.
    Reuse one byte-sized scratch buffer across both passes. */
 char *scratch=malloc(size+1);if(!scratch)return NULL;
 SourceClause *clauses=NULL;size_t capacity=size/8+1;
 for(unsigned pass=0;pass<2;pass++){
  memcpy(scratch,bytes,size);scratch[size]=0;size_t off=0;*count=0;
  while(off<size){size_t end=off;while(end<size&&scratch[end]!='\n')end++;size_t last=end;
   if(last>off&&scratch[last-1]=='\r')last--;
   scratch[last]=0;
   size_t line_count=0;
   if(!read_line_clauses(scratch+off,off,last,NULL,size/8+1,&line_count)){free(clauses);free(scratch);return NULL;}
   if(!pass)*count+=line_count;
   else if(line_count&&!read_line_clauses(scratch+off,off,last,clauses,capacity,count)){free(clauses);free(scratch);return NULL;}
   off=end<size?end+1:end;
  }
  if(!pass){capacity=*count;clauses=malloc((capacity?capacity:1)*sizeof(*clauses));if(!clauses){free(scratch);return NULL;}}
 }
 free(scratch);return clauses;
}
static int tuple_compare(const void *av,const void *bv){
 const SourceClause *a=av,*b=bv;int c=strcmp(a->activity,b->activity);if(!c)c=strcmp(a->command,b->command);if(!c)c=strcmp(a->shell,b->shell);
 if(c)return c;return a->line_a<b->line_a?-1:a->line_a>b->line_a?1:0;
}
static const SourceClause *find_denial(const SourceClause *index,size_t count,const SourceClause *positive){
 SourceClause key=*positive;key.line_a=0;size_t lo=0,hi=count;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;if(tuple_compare(&index[mid],&key)<0)lo=mid+1;else hi=mid;}
 if(lo==count||strcmp(index[lo].activity,key.activity)||strcmp(index[lo].command,key.command)||strcmp(index[lo].shell,key.shell))return NULL;
 return &index[lo];
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
 size_t clause_count=0;SourceClause *clauses=source_clauses(cb,cn,&clause_count);
 if(!clauses){free(cb);snprintf(out,cap,"UNSUPPORTED_SOURCE_SCOPE: clause allocation or capacity failure.");return 1;}
 size_t negative_count=0;for(size_t i=0;i<clause_count;i++)if(clauses[i].negative)negative_count++;
 SourceClause *indexed=malloc((negative_count?negative_count:1)*sizeof(*indexed));
 if(!indexed){free(clauses);free(cb);snprintf(out,cap,"UNSUPPORTED_SOURCE: index allocation failure.");return 1;}
 size_t indexed_count=0;for(size_t i=0;i<clause_count;i++)if(clauses[i].negative)indexed[indexed_count++]=clauses[i];
 qsort(indexed,indexed_count,sizeof(*indexed),tuple_compare);
 unsigned count=0;size_t chosen_a=0,chosen_b=0;char chosen[512]="";int chosen_nobreak=0;int conflict=0;size_t pos_a=0,pos_b=0,neg_a=0,neg_b=0;
 for(size_t i=0;i<clause_count;i++){
  const SourceClause *c=&clauses[i];if(c->negative||strcmp(c->shell,shell))continue;
  int nb=0;if(!WindowsTimeoutFactMatches(c->command,requested,qualifier,&nb))continue;
  count++;chosen_a=c->command_a;chosen_b=c->command_b;snprintf(chosen,sizeof(chosen),"%s",c->command);chosen_nobreak=nb;
  const SourceClause *denial=find_denial(indexed,indexed_count,c);
  if(denial){conflict=1;pos_a=c->line_a;pos_b=c->line_b;neg_a=denial->line_a;neg_b=denial->line_b;}
 }
 if(conflict){int w=snprintf(out,cap,"SOURCE_CONFLICT: controlled positive and negative corpus assertions disagree on the same activity, command and shell. No command selected.\nCorpus: %s sha256=%s\nPositive assertion bytes[%zu,%zu)\nNegative assertion bytes[%zu,%zu)\nThis describes corpus assertions only; Microsoft parameter documentation is not invalidated and no general semantic conflict detection is claimed.",corpus,ch,pos_a,pos_b,neg_a,neg_b);if(w<0||(size_t)w>=cap)snprintf(out,cap,"UNSUPPORTED_OUTPUT: evidence would be truncated.");}
 else if(count==0)snprintf(out,cap,"NO_SUPPORT: no unambiguous source clause and reviewed parameter fact support this numeric request and qualifier. No duration, sentinel or unit conversion is inferred.");
 else if(count>1)snprintf(out,cap,"AMBIGUOUS: %u source clauses match; no command selected.",count);
 else if(chosen_b>cn||memcmp(cb+chosen_a,chosen,chosen_b-chosen_a))snprintf(out,cap,"UNSUPPORTED_SOURCE: evidence range mismatch.");
 else {int w=snprintf(out,cap,"DOCUMENTATION-SUPPORTED, not Windows execution\nCommand from corpus: %s\nInterpreted request: platform=%s shell=%s value=%ld unit=%s qualifier=%s\nRule: same shell + command timeout + parameter /t + integer value + seconds\nKeyboard: %s. No exact wall-clock guarantee.\nCorpus: %s sha256=%s bytes[%zu,%zu)\nParameter fact: microsoft.cmd.timeout.t.seconds sha256=%s\nSource: %s\nFact excerpt: /t specifies seconds, range -1..99999; -1 is indefinite until keystroke. /nobreak ignores key strokes.\nNatural-language morphology and number words are NOT interpreted by this diagnostic.",chosen,platform,shell,requested,unit,qualifier,chosen_nobreak?"key strokes ignored":"a key stroke may resume early",corpus,ch,chosen_a,chosen_b,fh,WINDOWS_TIMEOUT_SOURCE_URL);if(w<0||(size_t)w>=cap)snprintf(out,cap,"UNSUPPORTED_OUTPUT: evidence would be truncated.");}
 free(indexed);free(clauses);free(cb);return 1;
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
