#ifdef _WIN32
#include "fs_replace.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
/* Seeded byte fuzz of the Windows CREATE journal (M1 criterion 5, journal part, Windows, second kind).
   A child process is killed at a real crash point (FS_WIN_JOURNAL_CRASH, points 3 to 6 are the ones that
   leave a journal), one journal file is damaged here, then FsCreateRecover runs. Claim checked: a refusal
   (FS_READ_DENIED) leaves every file in the workspace unchanged; an OK leaves the target absent (rolled back)
   or with exactly the new bytes and a single link, no stage, pin or journal name left, the decoy unchanged,
   and a second recovery is OK and changes nothing. "Sealed" damage recomputes the FNV-1a checksum, so only
   the semantic checks of recovery can refuse it. Every cell also asserts the outcome predicted before the
   first run (see predicted below). The strict oracle treats any leftover as a miss except the pinned known limits (limit_kind); misses are grouped.
   Reproduce with FS_WJFUZZ_SEED=<n> FS_WJFUZZ_ITERS=<n>. Not covered: crash points 1 and 2 (stage, or stage
   and pin, with no journal: recovery returns OK and leaves them by design), remove, move and batch journals,
   power loss, two journal files damaged differently except the pair-mismatch class. */
#define ROOT "test_fs_winjfuzz_c_scratch"
#define INS ROOT "\\inside"
#define JI ROOT "\\.fstxn.intent"
#define JM ROOT "\\.fstxn.ccommit"
typedef struct { /* mirror of WC_RECORD in src/fs_create_win.inc */
 unsigned magic,version,checksum,reserved;
 FILE_ID_INFO root,parent,file;
 unsigned long long size,digest;
 unsigned mode;
 WCHAR target[4096],stage[48],pin[48];
} TEST_WC_RECORD;
static unsigned record_checksum(const TEST_WC_RECORD *r)
{const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;size_t i;
 for(i=0;i<sizeof(*r);i++){
  if(i>=offsetof(TEST_WC_RECORD,checksum)&&i<offsetof(TEST_WC_RECORD,checksum)+sizeof(r->checksum))continue;
  h=(h^p[i])*16777619u;
 }return h;}
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
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(INS)==0,"mkdir");
 put(INS "\\decoy","old");}
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
static int run_child(const char *exe,int phase)
{char cmd[1024];STARTUPINFOA si;PROCESS_INFORMATION pi;DWORD code=0;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));
 si.cb=sizeof(si);sprintf(cmd,"\"%s\" child %d",exe,phase);
 ck(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi),"spawn");
 ck(WaitForSingleObject(pi.hProcess,60000)==WAIT_OBJECT_0,"wait");
 ck(GetExitCodeProcess(pi.hProcess,&code),"exit");
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;}
static void child(int phase)
{FS_READ_ROOT *r;char v[20];FS_READ_STATUS s;
 ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"child open");
 sprintf(v,"%d",phase);_putenv_s("FS_WIN_JOURNAL_CRASH",v);
 s=FsCreateFile(r,"inside/newfile","newbytes",8,0644);
 fprintf(stderr,"create child phase=%d status=%d\n",phase,(int)s);
 FsReadClose(r);ExitProcess(200);}
/* classes */
enum { C_BITFLIP, C_SETBYTE, C_TRUNC, C_APPEND, C_NAME, C_SCALAR, C_SHAPE, C_DELETE, C_PAIR, NCLS };
static const char *CLS[NCLS]={"bitflip","setbyte","truncate","append","name_sealed","scalar_sealed",
  "shape_sealed","delete","pair_mismatch"};
#define NNAME 4
#define NSCALAR 11
#define NSHAPE 5
/* Predictions, written before the first run (see the m176 report). 1 = recovery must refuse (DENIED,
   workspace unchanged); 2 = recovery must be OK with *state_new (1 = the new file is there, 0 = absent,
   rolled back). Predicted leftovers are NOT excused here: they show up as misses in the strict oracle. */
static int predicted(int phase,int cls,int it,int *state_new)
{*state_new=(phase==6);
 switch(cls){
 case C_BITFLIP:case C_SETBYTE:case C_TRUNC:case C_APPEND:return 1;
 case C_NAME:switch(it%NNAME){ /* 0 target->decoy, 1 target->missing leaf, 2 stage, 3 pin */
   case 0:return 1;
   case 1:if(phase==6)return 1;*state_new=(phase!=3);return 2;
   case 2:return 2;
   default:return phase==6?2:1;}
 case C_SCALAR:return it%NSCALAR==10?2:1; /* 10 = mode flip: followed, benign for the bytes */
 case C_SHAPE:case C_PAIR:return 1;
 case C_DELETE:{int sub=(phase==6)?it%3:0;
   if(phase==6&&sub==1)*state_new=0;
   if(phase==4||phase==5)*state_new=1;
   if(phase==3)*state_new=0;
   return 2;}
 default:return 0;}}
static void hexname(WCHAR *dst,const WCHAR *orig)
{size_t n=wcslen(orig),i;static const WCHAR hx[]=L"0123456789abcdef";
 ck(n>6&&n<48,"name length");wcscpy(dst,orig);
 for(i=6;i<n;i++)dst[i]=hx[rnd()&15];
 if(!wcscmp(dst,orig))dst[n-1]=(dst[n-1]==L'0')?L'1':L'0';}
static void seal_all(const char **files,int nf,const TEST_WC_RECORD *rec)
{TEST_WC_RECORD x=*rec;int i;x.checksum=record_checksum(&x);
 for(i=0;i<nf;i++)ck(spit(files[i],&x,sizeof(x)),"write sealed record");}
static void mutate_record(TEST_WC_RECORD *rec,int cls,int it)
{switch(cls){
 case C_NAME:switch(it%NNAME){
   case 0:{size_t n=wcslen(rec->target);ck(n>=7&&!wcscmp(rec->target+n-7,L"newfile"),"target field");
           wcscpy(rec->target+n-7,L"decoy");break;}
   case 1:{size_t n=wcslen(rec->target);ck(n>=7&&!wcscmp(rec->target+n-7,L"newfile"),"target field");
           wcscpy(rec->target+n-7,L"other");break;}
   case 2:{WCHAR t[48];hexname(t,rec->stage);wcscpy(rec->stage,t);break;}
   default:{WCHAR t[48];hexname(t,rec->pin);wcscpy(rec->pin,t);break;}}
   break;
 case C_SCALAR:switch(it%NSCALAR){
   case 0:rec->magic^=1;break;
   case 1:rec->version=2;break;
   case 2:rec->reserved=1;break;
   case 3:rec->file.FileId.Identifier[0]^=1;break;
   case 4:rec->digest^=1;break;
   case 5:rec->size+=1;break;
   case 6:rec->root.FileId.Identifier[0]^=1;break;
   case 7:rec->parent.FileId.Identifier[0]^=1;break;
   case 8:rec->root.VolumeSerialNumber^=1;break;
   case 9:rec->parent.VolumeSerialNumber^=1;break;
   default:rec->mode^=0222;break;}
   break;
 case C_SHAPE:switch(it%NSHAPE){
   case 0:rec->stage[1]=L'x';break;
   case 1:rec->pin[10]=L'g';break;
   case 2:{size_t i;for(i=0;i<4096;i++)rec->target[i]=L'a';break;}
   case 3:wcscpy(rec->stage,rec->pin);break;
   default:rec->mode=01000;break;}
   break;
 case C_PAIR:rec->digest^=1;break;
 default:break;}}
/* damage; returns 0 when the class has no cell at this phase */
static int damage(int phase,int cls,int it)
{const char *files[2];int nf=0,i;size_t n=0;unsigned char *b;
 if(exists(JI))files[nf++]=JI;
 if(exists(JM))files[nf++]=JM;
 ck(nf>=1,"a journal exists");
 if(phase==6)ck(nf==2,"intent and marker at phase 6");else ck(nf==1,"intent only before the swap");
 if(cls==C_PAIR&&phase!=6)return 0;
 switch(cls){
 case C_BITFLIP:case C_SETBYTE:case C_TRUNC:case C_APPEND:{
   const char *f=files[(int)(rnd()%(unsigned long long)nf)];
   b=slurp(f,&n);ck(b&&n==sizeof(TEST_WC_RECORD),"journal size");
   if(cls==C_BITFLIP){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1u<<(rnd()&7));}
   else if(cls==C_SETBYTE){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1+rnd()%255);}
   else if(cls==C_TRUNC){n=(size_t)(rnd()%n);}
   else{size_t k=1+(size_t)(rnd()%16),j;b=(unsigned char*)realloc(b,n+k);ck(b!=NULL,"realloc");
        for(j=0;j<k;j++)b[n+j]=(unsigned char)rnd();n+=k;}
   ck(spit(f,b,n),"write damaged journal");free(b);return 1;}
 case C_NAME:case C_SCALAR:case C_SHAPE:{
   TEST_WC_RECORD rec;b=slurp(files[0],&n);ck(b&&n==sizeof(rec),"journal size");
   memcpy(&rec,b,sizeof(rec));free(b);
   ck(rec.checksum==record_checksum(&rec),"fixture record is sealed");
   mutate_record(&rec,cls,it);seal_all(files,nf,&rec);return 1;}
 case C_PAIR:{
   TEST_WC_RECORD rec;b=slurp(JM,&n);ck(b&&n==sizeof(rec),"marker size");
   memcpy(&rec,b,sizeof(rec));free(b);mutate_record(&rec,cls,it);
   seal_all(&files[1],1,&rec);return 1;}
 case C_DELETE:{
   int v=(phase==6)?it%3:0;
   if(v==0||v==2)ck(DeleteFileA(JI),"delete intent");
   if(v==1||v==2)ck(DeleteFileA(JM),"delete marker");
   return 1;}
 default:return 0;}
 (void)i;}
/* The workspace lock file .fstxn.lock is a control file that stays by design (WC_LOCK, opened FILE_OPEN_IF in
   src/fs_create_win.inc), so it is not a leftover. The first version of this scan matched ".fstxn*" and
   counted it: that was a mistake in this test, not a finding. Journal and marker names are listed exactly. */
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
/* Known limits (measured in m176, 2 seeds x 660 cells, same table on both seeds, msvc and asan), pinned in
   m177 with the exact outcome. Kind 1: phase 3, stage name resealed to a missing name: the stage lookup of a
   missing name is accepted, so the real .fst- stays (one name, 1 link, new bytes), target absent. Kind 2: the
   pin is left as a second link of a finished target (target new, 2 links, one .fsp- name, new bytes): phase 6
   with the pin name resealed in both files (sub 3 of name_sealed, a recomputing writer), and the journal
   deleted after the publish (delete class at phases 4 and 5, and both files at phase 6): nothing is left
   to recover from, same class as the POSIX limit F5. Kind 3: phase 3 with the intent deleted: stage and
   pin both stay (2 names, each 2 links, new bytes), target absent. */
static int limit_kind(int phase,int cls,int sub)
{if(cls==C_NAME&&phase==3&&sub==2)return 1;
 if(cls==C_NAME&&phase==6&&sub==3)return 2;
 if(cls==C_DELETE&&phase==3)return 3;
 if(cls==C_DELETE&&(phase==4||phase==5)&&sub==0)return 2;
 if(cls==C_DELETE&&phase==6&&sub==2)return 2;
 return 0;}
/* g_ex holds "dir\\name " entries from leftover_names(); copy them into paths[] */
static int split_names(char paths[4][600])
{int n=0;char *p=g_ex;
 while(*p&&n<4){char *e=strchr(p,' ');size_t l=e?(size_t)(e-p):strlen(p);
   if(l>=600)l=599;memcpy(paths[n],p,l);paths[n][l]=0;n++;
   if(!e)break;p=e+1;}
 return n;}
static void remove_stale(void)
{sweep_dir(INS);sweep_dir(ROOT);_rmdir(INS);_rmdir(ROOT);ck(!exists(ROOT),"stale scratch removed");}
int main(int argc,char **argv)
{char exe[768];DWORD got;int phase,cls,it,iters=20;unsigned long long seed=1786707969ULL;
 static char before[65536],after[65536],again[65536];
 long cells=0,refused=0,ok_old=0,ok_new=0,limit_hits=0;
 if(argc==3&&!strcmp(argv[1],"child")){child(atoi(argv[2]));return 200;}
 remove_stale();
 if(getenv("FS_WJFUZZ_SEED"))seed=strtoull(getenv("FS_WJFUZZ_SEED"),NULL,10);
 if(getenv("FS_WJFUZZ_ITERS"))iters=atoi(getenv("FS_WJFUZZ_ITERS"));
 ck(iters>0&&iters<=1000,"iters");
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 for(phase=3;phase<=6;phase++)for(cls=0;cls<NCLS;cls++)for(it=0;it<iters;it++){
   FS_READ_ROOT *r;FS_READ_STATUS s;int want,state_new,took,sub;
   sub=cls==C_NAME?it%NNAME:cls==C_SCALAR?it%NSCALAR:cls==C_SHAPE?it%NSHAPE:cls==C_DELETE?(phase==6?it%3:0):0;
   g_ex[0]=0;
   rs=seed*1000003ULL+(unsigned long long)(phase*100+cls*10)*7919ULL+(unsigned long long)it*104729ULL+1ULL;
   if(!rs)rs=1;
   if(cls==C_PAIR&&phase!=6)continue;
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
   ck(run_child(exe,phase)==80+phase,"writer phase kill");
   took=damage(phase,cls,it);ck(took,"damage applied");
   snap(before,sizeof(before));
   want=predicted(phase,cls,it,&state_new);
   s=FsCreateRecover(r);cells++;
   if(s==FS_READ_DENIED){
     refused++;
     snap(after,sizeof(after));
     if(strcmp(before,after))FAIL("refusal changed the workspace");
     if(want==2)FAIL("predicted OK, recovery refused");
   }else if(s==FS_READ_OK){
     int lk,is_abs=!exists(INS "\\newfile"),is_new=file_is(INS "\\newfile","newbytes");
     if(want==1)FAIL("predicted refusal, recovery returned OK (damage followed or ignored)");
     if(!is_abs&&!is_new)FAIL("OK but the target is neither absent nor the new bytes");
     if(want==2&&is_new!=state_new)FAIL("OK with the other state than predicted");
     if(is_new&&links_of(INS "\\newfile")!=(limit_kind(phase,cls,sub)==2?2:1))FAIL("OK but the target is not a single link");
     if(!file_is(INS "\\decoy","old"))FAIL("OK but the decoy changed");
     lk=limit_kind(phase,cls,sub);
     if(lk){
       int n=leftover_names();char names[4][600];int k;int want_n=(lk==3)?2:1;
       if(n!=want_n||split_names(names)!=want_n)FAIL("limit cell: unexpected number of leftover names");
       if(lk==2&&!is_new)FAIL("limit cell: the state is not new");
       if(lk!=2&&!is_abs)FAIL("limit cell: the target is not absent");
       for(k=0;k<want_n;k++){
         if(!file_is(names[k],"newbytes"))FAIL("limit cell: a leftover does not hold the new bytes");
         if(links_of(names[k])!=((lk==1)?1:2))FAIL("limit cell: unexpected link count of a leftover");
       }
       if(lk==1&&strncmp(names[0],ROOT "\\.fst-",strlen(ROOT "\\.fst-")))FAIL("limit cell: the leftover is not a .fst- stage");
       if(lk==2&&strncmp(names[0],ROOT "\\.fsp-",strlen(ROOT "\\.fsp-")))FAIL("limit cell: the leftover is not a .fsp- pin");
       if(lk==3){int st=0,pn=0;
         for(k=0;k<2;k++){if(!strncmp(names[k],ROOT "\\.fst-",strlen(ROOT "\\.fst-")))st++;
                         if(!strncmp(names[k],ROOT "\\.fsp-",strlen(ROOT "\\.fsp-")))pn++;}
         if(st!=1||pn!=1)FAIL("limit cell: the leftovers are not one stage and one pin");}
       limit_hits++;
     }else if(leftover_names())FAIL("OK but a stage, pin or journal name is left");
     if(is_abs)ok_old++;else ok_new++;
     snap(after,sizeof(after));
     if(FsCreateRecover(r)!=FS_READ_OK)FAIL("second recovery not OK");
     snap(again,sizeof(again));
     if(strcmp(after,again))FAIL("second recovery changed the workspace");
   }else{
     fprintf(stderr,"status=%d\n",(int)s);FAIL("recovery returned neither OK nor DENIED");
   }
  done_iter:
   FsReadClose(r);cleanup();
 }
 if(total_miss){int m;
   for(m=0;m<nmiss;m++)fprintf(stderr,"MISS phase=%d class=%s sub=%d x%d (first iter %d): %s%s%s\n",miss[m].phase,CLS[miss[m].cls],
        miss[m].sub,miss[m].n,miss[m].it,miss[m].what,miss[m].ex[0]?" | names: ":"",miss[m].ex);
   fprintf(stderr,"FAIL %d cells missed their prediction or the oracle in %d distinct groups (seed %llu), of %ld cells\n",
        total_miss,nmiss,(unsigned long long)seed,cells);exit(1);}
 printf("windows create journal fuzz: %ld cells, seed %llu, %d per phase and class: refused %ld, ok absent %ld, ok new %ld, known-limit hits %ld\n",
        cells,seed,iters,refused,ok_old,ok_new,limit_hits);
 (void)CLS;return 0;}
#else
int main(void){return 0;}
#endif
