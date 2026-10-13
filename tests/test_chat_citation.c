/* Per-emission receipts must never be inferred from retrieval history. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
static int failures;
static void check(int ok,const char*n){printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)failures++;}
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
   check(TextLexSentenceText(tl,r.sentence_index,tl->image,tl->imagelen,display,sizeof(display))>0&&memcmp(y+r.quote_first,display,r.visible_quote_end-r.quote_first)==0,"receipt source identity binds display prefix");
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
 const char*p="chat_citation_fixture.txt";FILE*f=fopen(p,"wb");if(!f)return 2;
 fputs("And those gamma rays came just 1.7 seconds after the gravitational wave signal.\nAnother seconds statement follows distinct observations.\nPrimer postulado (principio de relatividad) explains distant observations.\n",f);fclose(f);
 size_t caps[]={1,8,14,15,16,32,64,4096,8192};for(int m=0;m<3;m++)for(unsigned i=0;i<sizeof(caps)/sizeof(*caps);i++)compare(p,m,caps[i]);
 {CHAT*c=create(p);CHAT_CITATION r;char out[4096];memset(&r,0xa5,sizeof(r));check(ChatHandleToBufEx(NULL,"seconds",out,sizeof(out),&r)==0&&r.state==CHAT_CITE_NONE&&r.source.version==0,"invalid call clears receipt");check(ChatTryTextLineEx(c,"seconds",out,0,&r)==0&&r.state==CHAT_CITE_NONE,"zero size clears receipt");destroy(c);}
 remove(p);printf("test_chat_citation: %d failed\n",failures);return failures?1:0;
}
