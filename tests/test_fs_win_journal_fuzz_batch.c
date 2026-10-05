#ifdef _WIN32
#include "fs_batch.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
/* Seeded byte fuzz of the Windows BATCH journals (M1 criterion 5, journal part, Windows, last kind).
   Kind 1 = FsBatchCreate of two files (inside/a 0644, inside/b 0444), kind 2 = FsBatchReplace of two files.
   A child process is killed at a real crash point (FS_WIN_BATCH_CRASH; create 3..6 = journal complete, first
   target published, all published, marker; replace 24..27 likewise), one journal file is damaged here, then
   FsBatchRecover runs. Claim checked: a refusal (FS_READ_DENIED) leaves every file in the workspace unchanged;
   an OK leaves BOTH targets in the same state (create: both absent or both with the new bytes; replace: both
   old or both new), a single link each, no stage, pin, rollback or journal name left, the decoy unchanged, and
   a second recovery is OK and changes nothing. A created target with mode 0444 must carry the read-only
   attribute. "Sealed" damage recomputes the FNV-1a checksum, so only the semantic checks of recovery can
   refuse it. Every cell also asserts the outcome predicted before the first run (see predicted below). The
   strict oracle has no excused limits in this first round; misses are grouped.
   Reproduce with FS_WJFUZZ_SEED=<n> FS_WJFUZZ_ITERS=<n>. Not covered: crash points with no journal (1, 2, 21,
   22, 23), a batch of more than two items, power loss, two journal files damaged differently except the
   pair-mismatch class. */
#define ROOT "test_fs_winjfuzz_b_scratch"
#define INS ROOT "\\inside"
#define JB ROOT "\\.fstxn.batch"
#define JC ROOT "\\.fstxn.commit"
typedef struct {
 FILE_ID_INFO parent,file;
 unsigned long long size,digest;
 unsigned mode,reserved;
 WCHAR target[4096],stage[48],pin[48];
} TW_ITEM;
typedef struct { unsigned magic,version,checksum,count; FILE_ID_INFO root; TW_ITEM items[8]; } TW_REC;
typedef struct {
 FILE_ID_INFO parent,old_id,new_id;
 unsigned long long old_size,old_digest,new_size,new_digest;
 WCHAR target[4096],old_pin[48],new_stage[48],new_pin[48],rollback[48];
} TR_ITEM;
typedef struct { unsigned magic,version,checksum,count; FILE_ID_INFO root; TR_ITEM items[8]; } TR_REC;
static unsigned fnv32(const void *rec,size_t size)
{const unsigned char *p=(const unsigned char*)rec;unsigned h=2166136261u;size_t i;
 for(i=0;i<size;i++){if(i>=8&&i<12)continue;h=(h^p[i])*16777619u;}return h;}
static unsigned long long rs=1;
static unsigned long long rnd(void)
{rs^=rs>>12;rs^=rs<<25;rs^=rs>>27;return rs*2685821657736338717ULL;}
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
/* A failing cell does not stop the run: it is recorded under (phase, class, sub-case) with its first
   example, the iteration is cleaned up, and the run goes on. At the end every distinct miss is printed and
   the test fails. One CI round then shows all the misses, not only the first. */
#define MAXMISS 64
static char g_ex[512];                 /* names found by the last leftover scan, first example only */
static struct {int phase,cls,sub,n,it;char what[96],ex[512];} miss[MAXMISS];
static int nmiss,total_miss;
#define FAIL(msg) do{note(phase,cls,it,sub,msg);goto done_iter;}while(0)
static void note(int phase,int cls,int it,int sub,const char *what)
{int i;total_miss++;
 for(i=0;i<nmiss;i++)if(miss[i].phase==phase&&miss[i].cls==cls&&miss[i].sub==sub&&!strcmp(miss[i].what,what)){miss[i].n++;return;}
 if(nmiss<MAXMISS){miss[nmiss].phase=phase;miss[nmiss].cls=cls;miss[nmiss].sub=sub;miss[nmiss].n=1;miss[nmiss].it=it;
   snprintf(miss[nmiss].what,sizeof(miss[nmiss].what),"%s",what);
   snprintf(miss[nmiss].ex,sizeof(miss[nmiss].ex),"%s",g_ex);nmiss++;}}
static void put(const char *p,const char *v)
{FILE *f=fopen(p,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");}
static unsigned char *slurp(const char *p,size_t *n)
{FILE *f=fopen(p,"rb");unsigned char *b;long len;
 if(!f)return NULL;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return NULL;}
 len=ftell(f);if(len<0||fseek(f,0,SEEK_SET)!=0){fclose(f);return NULL;}
 b=(unsigned char*)malloc((size_t)len+1);
 if(!b||fread(b,1,(size_t)len,f)!=(size_t)len){fclose(f);free(b);return NULL;}
 fclose(f);*n=(size_t)len;return b;}
static int spit(const char *p,const void *b,size_t n)
{FILE *f=fopen(p,"wb");if(!f)return 0;
 if(n&&fwrite(b,1,n,f)!=n){fclose(f);return 0;}
 return fclose(f)==0;}
static int exists(const char *p){return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;}
static void cleanup(void);
static void sweep_dir(const char *dir)
{WIN32_FIND_DATAA d;char pat[600],p[700];HANDLE f;
 snprintf(pat,sizeof(pat),"%s\\*",dir);f=FindFirstFileA(pat,&d);
 if(f==INVALID_HANDLE_VALUE)return;
 do{if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
    if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
    snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);
    SetFileAttributesA(p,FILE_ATTRIBUTE_NORMAL);ck(DeleteFileA(p),"cleanup file");
 }while(FindNextFileA(f,&d));FindClose(f);}
static void cleanup(void)
{sweep_dir(INS);sweep_dir(ROOT);ck(_rmdir(INS)==0&&_rmdir(ROOT)==0,"cleanup dirs");}
static int count_pat(const char *pat)
{WIN32_FIND_DATAA d;HANDLE f=FindFirstFileA(pat,&d);int n=0;
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{n++;}while(FindNextFileA(f,&d));FindClose(f);return n;}
static unsigned long long fnv64(const unsigned char *p,size_t n)
{unsigned long long h=14695981039346656037ULL;size_t i;
 for(i=0;i<n;i++)h=(h^p[i])*1099511628211ULL;return h;}
static size_t snap_dir(const char *dir,char *out,size_t cap,size_t at)
{WIN32_FIND_DATAA d;char pat[600],p[700];HANDLE f;
 snprintf(pat,sizeof(pat),"%s\\*",dir);f=FindFirstFileA(pat,&d);
 if(f==INVALID_HANDLE_VALUE)return at;
 do{size_t n=0;unsigned char *b;
    if(!strcmp(d.cFileName,".")||!strcmp(d.cFileName,".."))continue;
    if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
        at+=(size_t)snprintf(out+at,cap-at,"%s\\%s/dir;",dir,d.cFileName);continue;}
    snprintf(p,sizeof(p),"%s\\%s",dir,d.cFileName);b=slurp(p,&n);
    ck(b!=NULL,"snapshot read");
    at+=(size_t)snprintf(out+at,cap-at,"%s\\%s:%lu:%llx;",dir,d.cFileName,(unsigned long)n,fnv64(b,n));
    free(b);ck(at<cap-200,"snapshot buffer");
 }while(FindNextFileA(f,&d));FindClose(f);return at;}
static void snap(char *out,size_t cap)
{size_t at;out[0]=0;at=snap_dir(ROOT,out,cap,0);snap_dir(INS,out,cap,at);}
static int file_is(const char *p,const char *v)
{size_t n=0;unsigned char *b=slurp(p,&n);int ok=b&&n==strlen(v)&&!memcmp(b,v,n);free(b);return ok;}
static int links_of(const char *p)
{FILE_STANDARD_INFO st;HANDLE h=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
   NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);int n=-1;
 if(h==INVALID_HANDLE_VALUE)return -1;
 if(GetFileInformationByHandleEx(h,FileStandardInfo,&st,sizeof(st)))n=(int)st.NumberOfLinks;
 CloseHandle(h);return n;}
static int g_kind;
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(INS)==0,"mkdir");
 put(INS "\\decoy","old");
 if(g_kind==2){put(INS "\\a","olda");put(INS "\\b","oldb");}}
static int collect(const char *dir,const char *pat)
{WIN32_FIND_DATAA d;char full[700];HANDLE f;int n=0;
 snprintf(full,sizeof(full),"%s\\%s",dir,pat);f=FindFirstFileA(full,&d);
 if(f==INVALID_HANDLE_VALUE)return 0;
 do{size_t k=strlen(g_ex);n++;
    if(k+strlen(dir)+strlen(d.cFileName)+4<sizeof(g_ex))snprintf(g_ex+k,sizeof(g_ex)-k,"%s\\%s ",dir,d.cFileName);
 }while(FindNextFileA(f,&d));FindClose(f);return n;}
static int leftover_names(void)
{static const char *J[]={".fstxn.intent",".fstxn.ccommit",".fstxn.remove",".fstxn.rcommit",".fstxn.move",
   ".fstxn.mcommit",".fstxn.replace",".fstxn.pcommit",".fstxn.batch",".fstxn.commit"};
 int n=0;size_t i;g_ex[0]=0;
 n+=collect(ROOT,".fsrp-*");n+=collect(ROOT,".fsrs-*");n+=collect(ROOT,".fsrb-*");n+=collect(ROOT,".fst-*");
 n+=collect(ROOT,".fsp-*");n+=collect(INS,".fsp-*");n+=collect(INS,".fsrp-*");n+=collect(INS,".fsrs-*");n+=collect(INS,".fsrb-*");n+=collect(INS,".fst-*");
 for(i=0;i<sizeof(J)/sizeof(J[0]);i++)n+=collect(ROOT,J[i]);
 return n;}
static void remove_stale(void)
{sweep_dir(INS);sweep_dir(ROOT);_rmdir(INS);_rmdir(ROOT);ck(!exists(ROOT),"stale scratch removed");}
static int run_child(const char *exe,int kind,int point)
{char cmd[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD code=0;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));
 si.cb=sizeof(si);sprintf(cmd,"\"%s\" child %d %d",exe,kind,point);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,60000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static int crash_of(int kind,int point){return kind==1?point:point+21;}
static void child(int kind,int point)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 static const FS_BATCH_CREATE cr[2]={{"inside/a","newa",4,0644},{"inside/b","newb",4,0444}};
 static const FS_BATCH_REPLACE rp[2]={{"inside/a","olda",4,"newa",4},{"inside/b","oldb",4,"newb",4}};
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",crash_of(kind,point));_putenv_s("FS_WIN_BATCH_CRASH",v);
 s=kind==1?FsBatchCreate(r,cr,2):FsBatchReplace(r,rp,2);
 fprintf(stderr,"batch child kind=%d point=%d status=%d\n",kind,point,(int)s);
 FsReadClose(r);ExitProcess(200);}
/* classes */
enum { C_BITFLIP, C_SETBYTE, C_TRUNC, C_APPEND, C_NAME, C_SCALAR, C_SHAPE, C_DELETE, C_PAIR, NCLS };
static const char *CLS[NCLS]={"bitflip","setbyte","truncate","append","name_sealed","scalar_sealed",
  "shape_sealed","delete","pair_mismatch"};
#define NSCALAR 14
#define NSHAPE 7
/* sub-case and item selector of a cell. name_sealed: sub = it % 8 (create) or it % 12 (replace), field =
   sub / 2, item = sub % 2. scalar_sealed: sub = it % 14, item = (it / 14) % 2. shape_sealed: sub = it % 7,
   item = (it / 7) % 2. delete at the marker point: sub = it % 3. */
static int sub_of(int kind,int point,int cls,int it)
{switch(cls){
 case C_NAME:return it%(kind==1?8:12);
 case C_SCALAR:return it%NSCALAR;
 case C_SHAPE:return it%NSHAPE;
 case C_DELETE:return point==6?it%3:0;
 default:return 0;}}
static int sel_of(int cls,int it)
{switch(cls){
 case C_NAME:return it&1;
 case C_SCALAR:return (it/NSCALAR)&1;
 case C_SHAPE:return (it/NSHAPE)&1;
 default:return 0;}}
/* Predictions, written before the first run (see the m180 report). 1 = recovery must refuse (DENIED,
   workspace unchanged); 2 = recovery must be OK with *state_new (1 = the batch is complete: create both new,
   replace both new; 0 = rolled back). Predicted leftovers and mixed states are NOT excused: they show up as
   misses in the strict oracle. */
static int predicted(int kind,int point,int cls,int it,int *state_new)
{int sub=sub_of(kind,point,cls,it),sel=sel_of(cls,it);
 *state_new=(point==6);
 switch(cls){
 case C_BITFLIP:case C_SETBYTE:case C_TRUNC:case C_APPEND:return 1;
 case C_NAME:{int field=sub/2;
   if(kind==1)switch(field){ /* 0 target->decoy, 1 target->missing, 2 stage, 3 pin */
     case 0:return 1;
     case 1:if(point==6)return 1;*state_new=0;return 2;
     case 2:return 2;
     default:return point==6?2:1;}
   switch(field){ /* 0 target->decoy, 1 target->missing, 2 old_pin, 3 new_pin, 4 new_stage, 5 rollback */
     case 0:case 1:return 1;
     case 2:case 3:return point==6?2:1;
     case 4:if(point==3)return 1;if(point==4)return sel==0?2:1;return 2;
     default:return 2;}}
 case C_SCALAR:return (kind==1&&sub==13)?2:1;
 case C_SHAPE:case C_PAIR:return 1;
 case C_DELETE:
   if(point==3)*state_new=0;
   if(point==4||point==5)*state_new=1;
   if(point==6)*state_new=(sub!=1);
   return 2;
 default:return 0;}}
static void hexname(WCHAR *dst,const WCHAR *orig)
{size_t n=wcslen(orig),i;static const WCHAR hx[]=L"0123456789abcdef";
 ck(n>6&&n<48,"name length");wcscpy(dst,orig);
 for(i=6;i<n;i++)dst[i]=hx[rnd()&15];
 if(!wcscmp(dst,orig))dst[n-1]=(dst[n-1]==L'0')?L'1':L'0';}
static void set_leaf(WCHAR *target,const WCHAR *leaf)
{size_t n=wcslen(target);ck(n>=8&&(!wcscmp(target+n-8,L"inside/a")||!wcscmp(target+n-8,L"inside/b")),"target field");
 wcscpy(target+n-1,leaf);}
static void fill_a(WCHAR *t){size_t i;for(i=0;i<4096;i++)t[i]=L'a';}
static void mutate_c(TW_REC *rec,int cls,int it,int kind_pair)
{int sub=sub_of(1,3,cls,it),sel=sel_of(cls,it);TW_ITEM *x=&rec->items[sel];
 if(kind_pair){rec->items[0].digest^=1;return;}
 switch(cls){
 case C_NAME:{int field=sub/2;
   switch(field){
   case 0:set_leaf(x->target,L"decoy");break;
   case 1:set_leaf(x->target,L"other");break;
   case 2:{WCHAR t[48];hexname(t,x->stage);wcscpy(x->stage,t);break;}
   default:{WCHAR t[48];hexname(t,x->pin);wcscpy(x->pin,t);break;}}
   break;}
 case C_SCALAR:switch(sub){
   case 0:rec->magic^=1;break;
   case 1:rec->version=2;break;
   case 2:rec->count=1;break;
   case 3:rec->count=3;break;
   case 4:rec->root.FileId.Identifier[0]^=1;break;
   case 5:rec->root.VolumeSerialNumber^=1;break;
   case 6:x->parent.FileId.Identifier[0]^=1;break;
   case 7:x->parent.VolumeSerialNumber^=1;break;
   case 8:x->file.FileId.Identifier[0]^=1;break;
   case 9:x->digest^=1;break;
   case 10:x->size+=1;break;
   case 11:x->reserved=1;break;
   case 12:x->mode=01000;break;
   default:x->mode^=0222;break;}
   break;
 case C_SHAPE:switch(sub){
   case 0:x->stage[1]=L'x';break;
   case 1:x->pin[10]=L'g';break;
   case 2:fill_a(x->target);break;
   case 3:wcscpy(x->stage,x->pin);break;
   case 4:rec->items[2].size=1;break;
   case 5:wcscpy(rec->items[1].target,rec->items[0].target);break;
   default:wcscpy(rec->items[1].stage,rec->items[0].stage);break;}
   break;
 default:break;}}
static void mutate_r(TR_REC *rec,int cls,int it,int kind_pair)
{int sub=sub_of(2,3,cls,it),sel=sel_of(cls,it);TR_ITEM *x=&rec->items[sel];
 if(kind_pair){rec->items[0].new_digest^=1;return;}
 switch(cls){
 case C_NAME:{int field=sub/2;WCHAR t[48];
   switch(field){
   case 0:set_leaf(x->target,L"decoy");break;
   case 1:set_leaf(x->target,L"other");break;
   case 2:hexname(t,x->old_pin);wcscpy(x->old_pin,t);break;
   case 3:hexname(t,x->new_pin);wcscpy(x->new_pin,t);break;
   case 4:hexname(t,x->new_stage);wcscpy(x->new_stage,t);break;
   default:hexname(t,x->rollback);wcscpy(x->rollback,t);break;}
   break;}
 case C_SCALAR:switch(sub){
   case 0:rec->magic^=1;break;
   case 1:rec->version=2;break;
   case 2:rec->count=1;break;
   case 3:rec->count=3;break;
   case 4:rec->root.FileId.Identifier[0]^=1;break;
   case 5:rec->root.VolumeSerialNumber^=1;break;
   case 6:x->parent.FileId.Identifier[0]^=1;break;
   case 7:x->parent.VolumeSerialNumber^=1;break;
   case 8:x->old_id.FileId.Identifier[0]^=1;break;
   case 9:x->new_id.FileId.Identifier[0]^=1;break;
   case 10:x->old_size+=1;break;
   case 11:x->old_digest^=1;break;
   case 12:x->new_size+=1;break;
   default:x->new_digest^=1;break;}
   break;
 case C_SHAPE:switch(sub){
   case 0:x->old_pin[1]=L'x';break;
   case 1:x->new_pin[10]=L'g';break;
   case 2:fill_a(x->target);break;
   case 3:wcscpy(x->new_stage,x->new_pin);break;
   case 4:rec->items[2].old_size=1;break;
   case 5:x->new_id=x->old_id;break;
   default:wcscpy(x->rollback,x->old_pin);break;}
   break;
 default:break;}}
static void seal_all(const char **files,int nf,void *rec,size_t size)
{int i;unsigned c=fnv32(rec,size);memcpy((unsigned char*)rec+8,&c,4);
 for(i=0;i<nf;i++)ck(spit(files[i],rec,size),"write sealed record");}
/* damage; returns 0 when the class has no cell at this point */
static int damage(int kind,int point,int cls,int it)
{const char *files[2];int nf=0;size_t n=0,rs_=kind==1?sizeof(TW_REC):sizeof(TR_REC);unsigned char *b;
 if(exists(JB))files[nf++]=JB;
 if(exists(JC))files[nf++]=JC;
 ck(nf>=1,"a journal exists");
 if(point==6)ck(nf==2,"journal and marker at point 6");else ck(nf==1,"journal only before the marker");
 if(cls==C_PAIR&&point!=6)return 0;
 switch(cls){
 case C_BITFLIP:case C_SETBYTE:case C_TRUNC:case C_APPEND:{
   const char *f=files[(int)(rnd()%(unsigned long long)nf)];
   b=slurp(f,&n);ck(b&&n==rs_,"journal size");
   if(cls==C_BITFLIP){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1u<<(rnd()&7));}
   else if(cls==C_SETBYTE){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1+rnd()%255);}
   else if(cls==C_TRUNC){n=(size_t)(rnd()%n);}
   else{size_t k=1+(size_t)(rnd()%16),j;b=(unsigned char*)realloc(b,n+k);ck(b!=NULL,"realloc");
        for(j=0;j<k;j++)b[n+j]=(unsigned char)rnd();n+=k;}
   ck(spit(f,b,n),"write damaged journal");free(b);return 1;}
 case C_NAME:case C_SCALAR:case C_SHAPE:case C_PAIR:{
   int pair=(cls==C_PAIR);const char **fl=pair?&files[1]:files;int cnt=pair?1:nf;
   b=slurp(pair?JC:files[0],&n);ck(b&&n==rs_,"journal size");
   ck(fnv32(b,n)==*(unsigned*)(b+8),"fixture record is sealed");
   if(kind==1)mutate_c((TW_REC*)b,cls,it,pair);else mutate_r((TR_REC*)b,cls,it,pair);
   seal_all(fl,cnt,b,n);free(b);return 1;}
 case C_DELETE:{
   int v=(point==6)?it%3:0;
   if(v==0||v==2)ck(DeleteFileA(JB),"delete journal");
   if(v==1||v==2)ck(DeleteFileA(JC),"delete marker");
   return 1;}
 default:return 0;}}
int main(int argc,char **argv)
{char exe[768];DWORD got;int kind,point,cls,it,iters=28;unsigned long long seed=1786707969ULL;
 static char before[65536],after[65536],again[65536];
 long cells=0,refused=0,ok_old=0,ok_new=0;
 if(argc==4&&!strcmp(argv[1],"child")){child(atoi(argv[2]),atoi(argv[3]));return 200;}
 remove_stale();
 if(getenv("FS_WJFUZZ_SEED"))seed=strtoull(getenv("FS_WJFUZZ_SEED"),NULL,10);
 if(getenv("FS_WJFUZZ_ITERS"))iters=atoi(getenv("FS_WJFUZZ_ITERS"));
 ck(iters>0&&iters<=1000,"iters");
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 for(kind=1;kind<=2;kind++)for(point=3;point<=6;point++)for(cls=0;cls<NCLS;cls++)for(it=0;it<iters;it++){
   FS_READ_ROOT *r;FS_READ_STATUS s;int want,state_new,took,sub,phase=kind*10+point;
   g_kind=kind;sub=sub_of(kind,point,cls,it);
   g_ex[0]=0;
   rs=seed*1000003ULL+(unsigned long long)(phase*100+cls*10)*7919ULL+(unsigned long long)it*104729ULL+1ULL;
   if(!rs)rs=1;
   if(cls==C_PAIR&&point!=6)continue;
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
   ck(run_child(exe,kind,point)==80+crash_of(kind,point),"writer point kill");
   took=damage(kind,point,cls,it);ck(took,"damage applied");
   snap(before,sizeof(before));
   want=predicted(kind,point,cls,it,&state_new);
   s=FsBatchRecover(r);cells++;
   if(s==FS_READ_DENIED){
     refused++;
     snap(after,sizeof(after));
     if(strcmp(before,after))FAIL("refusal changed the workspace");
     if(want==2)FAIL("predicted OK, recovery refused");
   }else if(s==FS_READ_OK){
     int na,nb;
     if(want==1)FAIL("predicted refusal, recovery returned OK (damage followed or ignored)");
     if(kind==1){
       na=exists(INS "\\a")?(file_is(INS "\\a","newa")?1:-1):0;
       nb=exists(INS "\\b")?(file_is(INS "\\b","newb")?1:-1):0;
     }else{
       na=file_is(INS "\\a","newa")?1:file_is(INS "\\a","olda")?0:-1;
       nb=file_is(INS "\\b","newb")?1:file_is(INS "\\b","oldb")?0:-1;
     }
     if(na<0||nb<0)FAIL("OK but a target is neither the old/absent state nor the new bytes");
     if(na!=nb)FAIL("OK but the batch is MIXED (one target new, the other not)");
     if(want==2&&na!=state_new)FAIL("OK with the other state than predicted");
     if(exists(INS "\\a")&&links_of(INS "\\a")!=1)FAIL("OK but target a is not a single link");
     if(exists(INS "\\b")&&links_of(INS "\\b")!=1)FAIL("OK but target b is not a single link");
     if(!file_is(INS "\\decoy","old"))FAIL("OK but the decoy changed");
     if(kind==1&&na==1&&cls!=C_SCALAR){
       DWORD at=GetFileAttributesA(INS "\\b");
       if(at==INVALID_FILE_ATTRIBUTES||!(at&FILE_ATTRIBUTE_READONLY))FAIL("OK new but the 0444 target b lacks the read-only attribute");
     }
     if(leftover_names())FAIL("OK but a stage, pin, rollback or journal name is left");
     if(na==1)ok_new++;else ok_old++;
     snap(after,sizeof(after));
     if(FsBatchRecover(r)!=FS_READ_OK)FAIL("second recovery not OK");
     snap(again,sizeof(again));
     if(strcmp(after,again))FAIL("second recovery changed the workspace");
   }else{
     fprintf(stderr,"status=%d\n",(int)s);FAIL("recovery returned neither OK nor DENIED");
   }
  done_iter:
   FsReadClose(r);cleanup();
   (void)sub;
 }
 if(total_miss){int m;
   for(m=0;m<nmiss;m++)fprintf(stderr,"MISS phase=%d class=%s sub=%d x%d (first iter %d): %s%s%s\n",miss[m].phase,CLS[miss[m].cls],
        miss[m].sub,miss[m].n,miss[m].it,miss[m].what,miss[m].ex[0]?" | names: ":"",miss[m].ex);
   fprintf(stderr,"FAIL %d cells missed their prediction or the oracle in %d distinct groups (seed %llu), of %ld cells\n",
        total_miss,nmiss,(unsigned long long)seed,cells);exit(1);}
 printf("windows batch journal fuzz: %ld cells, seed %llu, %d per point and class: refused %ld, ok old/absent %ld, ok new %ld\n",
        cells,seed,iters,refused,ok_old,ok_new);
 return 0;}
#else
int main(void){return 0;}
#endif
