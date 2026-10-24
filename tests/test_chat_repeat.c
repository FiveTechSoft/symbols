#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
static unsigned fails;static void ck(int p,const char*m){printf("%s %s\n",p?"PASS":"FAIL",m);fails+=!p;}
int main(void){
const char*path="chat_repeat_fixture.txt";FILE*f=fopen(path,"wb");if(!f)return 2;
fputs("The Lumen telescope measures infrared radiation.\nThe Lumen telescope is cooled using liquid nitrogen.\nThe orchard Juno grows peaches.\nThe violet sensor measures pressure.\nThe planet Mira has rings.\n",f);fclose(f);
const char*q="Which telescope measures infrared radiation?";const char*gold="Lumen telescope measures";CHAT*c=calloc(1,sizeof(*c));char out[8192];CHAT_CITATION a,b;
ChatInit(c,path);c->episodic.auto_save=0;
ChatHandleToBufEx(c,q,out,sizeof(out),&a);ck(strstr(out,gold)!=NULL,"first explicit answer");
ChatHandleToBufEx(c,q,out,sizeof(out),&b);ck(strstr(out,gold)!=NULL,"repeated explicit answer");ck(a.state==CHAT_CITE_VERIFIED&&b.state==CHAT_CITE_VERIFIED&&a.sentence_index==b.sentence_index,"same verified source reused");
ChatHandleToBufEx(c,"explicamelo",out,sizeof(out),&b);ck(b.state==CHAT_CITE_VERIFIED&&a.sentence_index!=b.sentence_index,"continuation still advances");
ChatHandleToBufEx(c,q,out,sizeof(out),&b);ck(strstr(out,gold)!=NULL,"explicit answer after continuation");
ChatHandleToBufEx(c,"Which orchard carries engines?",out,sizeof(out),&b);ck(strstr(out,"Segun el texto")==NULL,"unsupported constraint not enabled by reuse");
ChatDestroy(c);free(c);remove(path);return fails?1:0;}
