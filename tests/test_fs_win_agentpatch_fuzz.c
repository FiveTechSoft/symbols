/* m220 (S3a): AgentPatch fuzz on Windows. Not a diff parser: a plan is one target and hunks of four strings.
   Generated files (LF only, CRLF only, mixed, a lone CR inside a line, no final newline; no NUL byte, that is S3b)
   and one hunk per round, taken from the file or broken on purpose. Oracle, no second copy of the EOL rules:
   a refused plan leaves the file byte-identical; an applied plan keeps the bytes before and after the replaced
   line byte-identical, writes the replacement with the replaced line's own ending, equals applied_content, and
   PatchRollback restores the original bytes. Outside targets are refused and a sentinel outside stays. The root
   holds only the target and .fstxn.lock after each round. One runner, NTFS, cooperating writers, no crash. */
#ifdef _WIN32
#include "agent_patch.h"
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define WS "test_agent_patch_fuzz_win_scratch"
#define OUT "test_agent_patch_fuzz_win_outside.c"
#define SENT "int x = 1;\n"
#define MAXF 8192
#define NL 8
static uint32_t rs;
static uint32_t rnd(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
static int g_k,g_kind,g_cls;
static void ck(int ok,const char *m)
{if(!ok){fprintf(stderr,"FAIL %s (iteration %d, class %d, kind %d)\n",m,g_k,g_cls,g_kind);exit(1);}}
static void wipe(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(WS "\\*",&d);char p[512];
 if(f!=INVALID_HANDLE_VALUE){do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")){
   snprintf(p,sizeof p,WS "\\%s",d.cFileName);SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);DeleteFileA(p);}}while(FindNextFileA(f,&d));FindClose(f);}
 _rmdir(WS);}
static int names(void)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(WS "\\*",&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(strcmp(d.cFileName,".")&&strcmp(d.cFileName,"..")&&strcmp(d.cFileName,".fstxn.lock"))n++;}while(FindNextFileA(f,&d));
 FindClose(f);return n;}
static void put(const char *p,const char *b,size_t n)
{FILE *f=fopen(p,"wb");ck(f!=NULL,"put open");ck(fwrite(b,1,n,f)==n,"put write");ck(fclose(f)==0,"put close");}
static size_t slurp(const char *p,char *b,size_t cap)
{FILE *f=fopen(p,"rb");size_t n;ck(f!=NULL,"slurp open");n=fread(b,1,cap,f);ck(fclose(f)==0,"slurp close");return n;}
static char file[MAXF],after[MAXF],line[NL][160];
static size_t start[NL],llen[NL];static int eol[NL]; /* 0 none, 1 LF, 2 CRLF */
static int nlines,dup_a,dup_b;
/* Build the file. Line i is "L<i> <letters>" plus an ending. Lines dup_a and dup_b hold the same text "DUP x". */
static size_t build(int cls)
{size_t n=0;int i;nlines=5+(int)(rnd()%3);dup_a=1;dup_b=3;
 for(i=0;i<nlines;i++){
  size_t w=0;int j,m=3+(int)(rnd()%12);
  if(i==dup_a||i==dup_b)w=(size_t)sprintf(line[i],"DUP x");
  else{w=(size_t)sprintf(line[i],"L%d ",i);for(j=0;j<m;j++)line[i][w++]=(char)('a'+rnd()%26);
   if(cls==3&&i==2){line[i][w++]='\r';line[i][w++]='q';}line[i][w]=0;}
  eol[i]=cls==0?1:cls==1?2:cls==2?(1+(int)(rnd()&1)):1;
  if(cls==4&&i==nlines-1)eol[i]=0;
  start[i]=n;memcpy(file+n,line[i],w);n+=w;
  if(eol[i]==2)file[n++]='\r';
  if(eol[i])file[n++]='\n';
  llen[i]=n-start[i];}
 return n;}
static void round1(uint32_t k)
{PATCH_PLAN p;size_t fn,an;int cls=(int)(k%5),kind=(int)((k/5)%5),i,rc;char tgt[200],rep[200];
 g_k=(int)k;g_cls=cls;g_kind=kind;
 fn=build(cls);
 wipe();ck(_mkdir(WS)==0,"mkdir");put(WS "\\a.c",file,fn);put(OUT,SENT,sizeof SENT-1);
 PatchPlanInit(&p,WS "/a.c");PatchPlanSetWorkspace(&p,WS);
 i=(kind==0)?(int)(rnd()%(uint32_t)nlines):0;
 if(kind==0){while(i==dup_a||i==dup_b)i=(i+1)%nlines;}
 if(kind==0){
  snprintf(tgt,sizeof tgt,"%s%s",line[i],eol[i]?"\n":"");
  snprintf(rep,sizeof rep,"R%u new %u%s",(unsigned)k,(unsigned)(rnd()%1000),eol[i]?"\n":"");
  PatchPlanAddHunk(&p,0,"",tgt,rep,"");
  rc=PatchApplyAtomic(&p);ck(rc==1,"valid hunk applies");
  an=slurp(WS "/a.c",after,MAXF);
  {size_t pre=start[i],suf=fn-start[i]-llen[i],rl=strlen(rep)-(eol[i]?1:0);
   size_t xl=rl+(eol[i]==2?2:eol[i]==1?1:0);char want[200];
   ck(an==pre+xl+suf,"applied size");
   ck(!memcmp(after,file,pre),"bytes before the line unchanged");
   ck(!memcmp(after+pre+xl,file+start[i]+llen[i],suf),"bytes after the line unchanged");
   memcpy(want,rep,rl);if(eol[i]==2){want[rl]='\r';want[rl+1]='\n';}else if(eol[i]==1)want[rl]='\n';
   ck(!memcmp(after+pre,want,xl),"replacement carries the replaced line ending");
   ck(p.applied_size==an&&!memcmp(p.applied_content,after,an),"applied_content equals the file");}
  rc=PatchRollback(&p);ck(rc==1,"rollback ok");
  an=slurp(WS "/a.c",after,MAXF);ck(an==fn&&!memcmp(after,file,fn),"rollback restores the original bytes");
 }else{
  const char *before="";
  if(kind==1)snprintf(tgt,sizeof tgt,"ZZZ not there\n");
  else if(kind==2)snprintf(tgt,sizeof tgt,"DUP x\n");
  else if(kind==3){snprintf(tgt,sizeof tgt,"%s\n",line[0]);before="Q no such context\n";}
  else{static const char *v[4]={WS "\\..\\" OUT,WS "/../" OUT,OUT,""};char ab[400];int vi=(int)(k%4);
   PatchPlanFree(&p);
   if(vi==3){if(!_getcwd(ab,sizeof ab))ck(0,"cwd");strcat(ab,"\\" OUT);PatchPlanInit(&p,ab);}
   else PatchPlanInit(&p,v[vi]);
   PatchPlanSetWorkspace(&p,WS);snprintf(tgt,sizeof tgt,SENT);}
  snprintf(rep,sizeof rep,"REPLACED\n");
  PatchPlanAddHunk(&p,0,before,tgt,rep,"");
  rc=PatchApplyAtomic(&p);ck(rc!=1,"broken or outside plan is refused");
  an=slurp(WS "/a.c",after,MAXF);ck(an==fn&&!memcmp(after,file,fn),"refused plan leaves the file unchanged");
  an=slurp(OUT,after,MAXF);ck(an==sizeof SENT-1&&!memcmp(after,SENT,an),"outside sentinel unchanged");
 }
 ck(names()==1,"root holds only the target");
 PatchPlanFree(&p);}
int main(void)
{static const uint32_t seeds[4]={1786707969u,1u,7u,12648430u};unsigned total=0;int s;uint32_t k;
 for(s=0;s<4;s++){rs=seeds[s];for(k=0;k<100;k++){round1(k);total++;}
  printf("agentpatch fuzz seed=%u rounds=100\n",(unsigned)seeds[s]);fflush(stdout);}
 wipe();DeleteFileA(OUT);
 printf("agentpatch fuzz done: seeds=4 rounds=400 ok=%u\n",total);
 return total==400?0:1;}
#else
int main(void){return 0;}
#endif
