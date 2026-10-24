/* Literal TEXTQ guard must calibrate against its selected file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
static int fails;
static void ck(int ok,const char*n){printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)fails++;}
static CHAT*make(const char*p){CHAT*c=calloc(1,sizeof(*c));if(!c)exit(2);ChatInit(c,p);c->episodic.auto_save=0;return c;}
static void done(CHAT*c){ChatDestroy(c);free(c);}
static void writefile(const char*p,const char*s){FILE*f=fopen(p,"wb");if(!f)exit(2);fputs(s,f);fclose(f);}
int main(void){
 const char*paths[]={"data/selected_guard_physics.txt","data/selected_guard_math.txt"};
 writefile(paths[0],"Einstein postul\u00f3 que una teor\u00eda de cuerpos en movimiento que fuera compatible con las ecuaciones del electromagnetismo cl\u00e1sico deb\u00eda satisfacer dos condiciones:\n1. Primer postulado (principio de relatividad)\nLa observaci\u00f3n de un fen\u00f3meno f\u00edsico por m\u00e1s de un observador inercial debe resultar en un acuerdo entre los observadores sobre la naturaleza de la realidad (es decir, la teor\u00eda debe presentar covariancia de Lorentz).\nO, la naturaleza del universo no debe cambiar para un observador si su estado inercial cambia.\nO, toda teor\u00eda f\u00edsica debe ser matem\u00e1ticamente similar para cada observador inercial, presentando a lo sumo variaciones dentro del rango de las condiciones iniciales de la misma.\nO, las leyes del universo son las mismas sin que importe el marco de referencia inercial.\n2. Segundo postulado (invariabilidad de c)\nLa Luz siempre se propaga en el vac\u00edo con una velocidad constante c que es independiente del estado de movimiento del cuerpo emisor y del estado de movimiento del observador.\nEstas dos condiciones por s\u00ed mismas, no determinan completamente la teor\u00eda especial de la relatividad y es necesario a\u00f1adir supuestos adicionales para constituir una axiomatizaci\u00f3n razonable de la teor\u00eda de la relatividad. Adem\u00e1s el primer postulado, hist\u00f3ricamente ha ocasionado cierta confusi\u00f3n, y llev\u00f3 err\u00f3neamente a pensar que el formalismo de la teor\u00eda s\u00f3lo era aplicable a sistemas de referencia inerciales (ver #Cr\u00edticas).\n");
 writefile(paths[1],"Ejemplo 3\nMediante diferenciaci\u00f3n, se puede calcular la pendiente de una curva. Consideremos [math]f(x)=x^2[/math]:\n\n[math] f'(x) = \\lim_{h\\rightarrow 0}\\frac{f(x+h)-f(x)}{h} = \\lim_{h\\rightarrow 0}\\frac{(x+h)^2-x^2}{h} = \\lim_{h\\rightarrow 0}\\frac{x^2 + 2xh + h^2 - x^2}{h} [/math]\n[math] = \\lim_{h\\rightarrow 0}\\frac{2xh + h^2}{h} = \\lim_{h\\rightarrow 0}\\frac{h(2x + h)}{h} = \\lim_{h\\rightarrow 0}(2x + h) = 2x [/math]\n\nPara cualquier punto x, la pendiente de la funci\u00f3n [math]f(x)=x^2[/math] es [math]f'(x)=2x[/math].\n");
 for(int order=0;order<2;order++)for(int small=0;small<2;small++){
  CHAT*a=make(paths[order]),*b=make(paths[order]);char cmd[128],x[8192],y[8192],z[8192];CHAT_CITATION r;size_t cap=small?32:4096;
  snprintf(cmd,sizeof(cmd),"load selected_guard_%s.txt",order?"physics":"math");
  ChatHandleToBuf(a,cmd,x,sizeof(x));ChatHandleToBufEx(b,cmd,y,sizeof(y),&r);ck(a->ntfiles==2&&b->ntfiles==2,"two sources loaded");
  const char*q=order?"postulado":"frac";
  ChatHandleToBuf(a,q,x,cap);ChatHandleToBufEx(b,q,y,cap,&r);
  ck(strcmp(x,y)==0,"legacy and Ex select same answer");
  ck(strncmp(x,"Segun el texto",14)==0,"second-file quote survives final guard");
  ck(r.state==CHAT_CITE_VERIFIED&&r.file_slot==1&&r.source.version==1,"receipt belongs to selected second source");
  ck(a->ntshown==b->ntshown&&a->ntshown>0&&a->tshown[a->ntshown-1]>>24==1,"selection state remains second source");
  ChatHandleToBuf(a,"continualo",x,cap);ChatHandleToBufEx(b,"continualo",y,cap,&r);
  ck(strcmp(x,y)==0&&strncmp(x,"Segun el texto",14)==0,"second-source continuation legacy equals Ex");
  ck(r.state==CHAT_CITE_VERIFIED&&r.file_slot==1,"continuation receipt remains selected source");
  ChatHandleToBufEx(b,"zzyyzzunsupported20261013",z,sizeof(z),&r);ck(strncmp(z,"No ",3)==0&&r.state==CHAT_CITE_NONE&&r.source.version==0,"parse failure no stale receipt");
  ChatHandleToBufEx(b,"hola",z,sizeof(z),&r);ck(strncmp(z,"Segun",5)!=0&&r.state==CHAT_CITE_NONE,"greeting no stale receipt");
  done(a);done(b);
 }
 {
  CHAT*c=make("data/shells/shell_corpus.txt"); char out[8192],expected[2048]; CHAT_CITATION r;
  FILE*f=fopen("data/shells/shell_corpus.txt","rb"); int found=0;
  if(!f)return 2;
  while(fgets(expected,sizeof(expected),f))if(strncmp(expected,"Sleeping uses ",14)==0){found=1;break;}
  fclose(f);ck(found,"literal entity source located in unchanged corpus");
  if(found){
   size_t n=strlen(expected);while(n&&(expected[n-1]=='\n'||expected[n-1]=='\r'))expected[--n]=0;
   for(int mode=0;mode<2;mode++){
    if(mode)ChatHandleToBufEx(c,"How do I sleep for 5 seconds in CMD?",out,sizeof(out),&r);
    else ChatHandleToBuf(c,"How do I sleep for 5 seconds in CMD?",out,sizeof(out));
    const char*q=strstr(out,": ");
    ck(q&&strncmp(q+2,expected,n)==0&&q[2+n]=='\n'&&q[3+n]==0,"ENTITY quote preserves source punctuation and whitespace");
   }
   ChatHandleToBuf(c,"Which CMD command sleeps for five seconds?",out,sizeof(out));
   ck(strncmp(out,"No tengo constancia suficiente",29)==0,"literal repair does not change unresolved paraphrase selection");
  }
  done(c);
 }
 remove(paths[0]);remove(paths[1]);printf("test_chat_selected_guard: %d failed\n",fails);return fails?1:0;
}
