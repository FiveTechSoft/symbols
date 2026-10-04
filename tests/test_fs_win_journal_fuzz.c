#ifdef _WIN32
#include "fs_replace.h"
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <stddef.h>
/* Seeded byte fuzz of the Windows REPLACE journal (M1 criterion 5, journal part, Windows, one kind).
   A child process is killed at a real crash phase (FS_WIN_REPLACE_CRASH, phases 3 to 6 are the ones that
   leave a journal), one journal file is damaged here, then FsReplaceRecover runs. Claim checked: a refusal
   (FS_READ_DENIED) leaves every file in the workspace unchanged; an OK leaves the target with the old or the
   new bytes, a single link, no pin, stage, rollback or journal name left, the decoy file unchanged, and a
   second recovery is OK and changes nothing. "Sealed" damage recomputes the FNV-1a checksum, so only the
   semantic checks of recovery can refuse it. Every cell also asserts the outcome predicted before the first
   run (see PRED below). Reproduce with FS_WJFUZZ_SEED=<n> FS_WJFUZZ_ITERS=<n> (iterations per phase and
   class). Not covered: the other Windows journals, power loss, damage to two files differently except the
   pair-mismatch class, a recovery that hangs (it would time out). */
#define ROOT "test_fs_winjfuzz_scratch"
#define INS ROOT "\\inside"
#define JI ROOT "\\.fstxn.replace"
#define JM ROOT "\\.fstxn.pcommit"
typedef struct {
 unsigned magic,version,checksum,reserved;
 FILE_ID_INFO root,parent,old_id,new_id;
 unsigned long long old_size,old_digest,new_size,new_digest;
 WCHAR target[4096],old_pin[48],new_stage[48],new_pin[48],rollback[48];
} TEST_WR_RECORD;
static unsigned record_checksum(const TEST_WR_RECORD *r)
{const unsigned char *p=(const unsigned char*)r;unsigned h=2166136261u;size_t i;
 for(i=0;i<sizeof(*r);i++){
  if(i>=offsetof(TEST_WR_RECORD,checksum)&&i<offsetof(TEST_WR_RECORD,checksum)+sizeof(r->checksum))continue;
  h=(h^p[i])*16777619u;
 }return h;}
static unsigned long long rs=1;
static unsigned long long rnd(void)
{rs^=rs>>12;rs^=rs<<25;rs^=rs>>27;return rs*2685821657736338717ULL;}
static void ck(int ok,const char *label)
{if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",label,GetLastError());exit(1);}}
static void failcell(int phase,int cls,int it,const char *what)
{fprintf(stderr,"FAIL cell phase=%d class=%d iter=%d seed=%llu: %s\n",phase,cls,it,
        (unsigned long long)rs,what);exit(1);}
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
static void fixture(void)
{ck(_mkdir(ROOT)==0&&_mkdir(INS)==0,"mkdir");
 put(INS "\\target","old");put(INS "\\decoy","old");}
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
 for(i=0;i<n;i++)h=(h^p[i])*1099511628211ULL;
 return h;}
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
 sprintf(v,"%d",phase);_putenv_s("FS_WIN_REPLACE_CRASH",v);
 s=FsReplaceFile(r,"inside/target","old",3,"new",3);
 fprintf(stderr,"replace child phase=%d status=%d\n",phase,(int)s);
 FsReadClose(r);ExitProcess(200);}
/* classes */
enum { C_BITFLIP, C_SETBYTE, C_TRUNC, C_APPEND, C_NAME, C_SCALAR, C_SHAPE, C_DELETE, C_PAIR, NCLS };
static const char *CLS[NCLS]={"bitflip","setbyte","truncate","append","name_sealed","scalar_sealed",
  "shape_sealed","delete","pair_mismatch"};
#define NNAME 5
#define NSCALAR 11
/* PRED: outcomes written before the first run (see docs). 1 = recovery must refuse (DENIED, workspace
   unchanged); 2 = recovery must be OK with the state below; 0 = either (the safety oracle only). */
static int predicted(int phase,int cls,int it,int *state_new)
{*state_new=(phase==6);
 switch(cls){
 case C_BITFLIP:case C_SETBYTE:case C_TRUNC:case C_APPEND:return 1;
 case C_NAME:{int f=it%NNAME; /* 0 target, 1 old_pin, 2 new_pin, 3 new_stage, 4 rollback */
   if(f<=2)return 1;
   if(f==3)return phase==3?1:2; /* the stage link exists only at phase 3 */
   return 2;}
 case C_SCALAR:case C_SHAPE:case C_PAIR:return 1;
 default:return 0;}}
static void hexname(WCHAR *dst,const WCHAR *orig)
{size_t n=wcslen(orig),i;static const WCHAR hx[]=L"0123456789abcdef";
 ck(n>6&&n<48,"name length");wcscpy(dst,orig);
 for(i=6;i<n;i++)dst[i]=hx[rnd()&15];
 if(!wcscmp(dst,orig))dst[n-1]=(dst[n-1]==L'0')?L'1':L'0';}
static void seal_all(const char **files,int nf,const TEST_WR_RECORD *rec)
{TEST_WR_RECORD x=*rec;int i;x.checksum=record_checksum(&x);
 for(i=0;i<nf;i++)ck(spit(files[i],&x,sizeof(x)),"write sealed record");}
static void mutate_record(TEST_WR_RECORD *rec,int cls,int it)
{switch(cls){
 case C_NAME:switch(it%NNAME){
   case 0:{size_t n=wcslen(rec->target);ck(n>=6&&!wcscmp(rec->target+n-6,L"target"),"target field");
           wcscpy(rec->target+n-6,L"decoy");break;}
   case 1:{WCHAR t[48];hexname(t,rec->old_pin);wcscpy(rec->old_pin,t);break;}
   case 2:{WCHAR t[48];hexname(t,rec->new_pin);wcscpy(rec->new_pin,t);break;}
   case 3:{WCHAR t[48];hexname(t,rec->new_stage);wcscpy(rec->new_stage,t);break;}
   default:{WCHAR t[48];hexname(t,rec->rollback);wcscpy(rec->rollback,t);break;}}
   break;
 case C_SCALAR:switch(it%NSCALAR){
   case 0:rec->magic^=1;break;
   case 1:rec->version=2;break;
   case 2:rec->reserved=1;break;
   case 3:rec->old_id.FileId.Identifier[0]^=1;break;
   case 4:rec->new_id.FileId.Identifier[0]^=1;break;
   case 5:rec->old_digest^=1;break;
   case 6:rec->new_digest^=1;break;
   case 7:rec->old_size+=1;break;
   case 8:rec->new_size+=1;break;
   case 9:rec->root.FileId.Identifier[0]^=1;break;
   default:rec->parent.FileId.Identifier[0]^=1;break;}
   break;
 case C_SHAPE:switch(it%4){
   case 0:rec->old_pin[1]=L'x';break;
   case 1:rec->new_pin[10]=L'g';break;
   case 2:{size_t i;for(i=0;i<4096;i++)rec->target[i]=L'a';break;}
   default:wcscpy(rec->old_pin,rec->new_pin);break;}
   break;
 case C_PAIR:rec->new_digest^=1;break;
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
   b=slurp(f,&n);ck(b&&n==sizeof(TEST_WR_RECORD),"journal size");
   if(cls==C_BITFLIP){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1u<<(rnd()&7));}
   else if(cls==C_SETBYTE){size_t o=(size_t)(rnd()%n);b[o]^=(unsigned char)(1+rnd()%255);}
   else if(cls==C_TRUNC){n=(size_t)(rnd()%n);}
   else{size_t k=1+(size_t)(rnd()%16),j;b=(unsigned char*)realloc(b,n+k);ck(b!=NULL,"realloc");
        for(j=0;j<k;j++)b[n+j]=(unsigned char)rnd();n+=k;}
   ck(spit(f,b,n),"write damaged journal");free(b);return 1;}
 case C_NAME:case C_SCALAR:case C_SHAPE:{
   TEST_WR_RECORD rec;b=slurp(files[0],&n);ck(b&&n==sizeof(rec),"journal size");
   memcpy(&rec,b,sizeof(rec));free(b);
   ck(rec.checksum==record_checksum(&rec),"fixture record is sealed");
   mutate_record(&rec,cls,it);seal_all(files,nf,&rec);return 1;}
 case C_PAIR:{
   TEST_WR_RECORD rec;b=slurp(JM,&n);ck(b&&n==sizeof(rec),"marker size");
   memcpy(&rec,b,sizeof(rec));free(b);mutate_record(&rec,cls,it);
   seal_all(&files[1],1,&rec);return 1;}
 case C_DELETE:{
   int v=(phase==6)?it%3:0;
   if(v==0||v==2)ck(DeleteFileA(JI),"delete intent");
   if(v==1||v==2)ck(DeleteFileA(JM),"delete marker");
   return 1;}
 default:return 0;}
 (void)i;}
static int leftover_names(void)
{return count_pat(ROOT "\\.fsrp-*")+count_pat(ROOT "\\.fsrs-*")+count_pat(ROOT "\\.fsrb-*")+
        count_pat(ROOT "\\.fst-*")+count_pat(ROOT "\\.fstxn*")+count_pat(INS "\\.fsrb-*")+
        count_pat(INS "\\.fsrp-*")+count_pat(INS "\\.fsrs-*")+count_pat(INS "\\.fst*");}
int main(int argc,char **argv)
{char exe[768];DWORD got;int phase,cls,it,iters=20;unsigned long long seed=1786707969ULL;
 static char before[65536],after[65536],again[65536];
 long cells=0,refused=0,ok_old=0,ok_new=0;
 if(argc==3&&!strcmp(argv[1],"child")){child(atoi(argv[2]));return 200;}
 if(getenv("FS_WJFUZZ_SEED"))seed=strtoull(getenv("FS_WJFUZZ_SEED"),NULL,10);
 if(getenv("FS_WJFUZZ_ITERS"))iters=atoi(getenv("FS_WJFUZZ_ITERS"));
 ck(iters>0&&iters<=1000,"iters");
 got=GetModuleFileNameA(NULL,exe,sizeof(exe));ck(got&&got<sizeof(exe),"exe");
 for(phase=3;phase<=6;phase++)for(cls=0;cls<NCLS;cls++)for(it=0;it<iters;it++){
   FS_READ_ROOT *r;FS_READ_STATUS s;int want,state_new,took;
   rs=seed*1000003ULL+(unsigned long long)(phase*100+cls*10)*7919ULL+(unsigned long long)it*104729ULL+1ULL;
   if(!rs)rs=1;
   if(cls==C_PAIR&&phase!=6)continue;
   fixture();ck(FsReadOpen(ROOT,&r)==FS_READ_OK,"open");
   ck(run_child(exe,phase)==140+phase,"writer phase kill");
   took=damage(phase,cls,it);ck(took,"damage applied");
   snap(before,sizeof(before));
   want=predicted(phase,cls,it,&state_new);
   s=FsReplaceRecover(r);cells++;
   if(s==FS_READ_DENIED){
     refused++;
     snap(after,sizeof(after));
     if(strcmp(before,after))failcell(phase,cls,it,"refusal changed the workspace");
     if(want==2)failcell(phase,cls,it,"predicted OK, recovery refused");
   }else if(s==FS_READ_OK){
     int is_old=file_is(INS "\\target","old"),is_new=file_is(INS "\\target","new");
     if(want==1)failcell(phase,cls,it,"predicted refusal, recovery returned OK (damage followed or ignored)");
     if(!is_old&&!is_new)failcell(phase,cls,it,"OK but the target is neither old nor new");
     if(want==2&&is_new!=state_new)failcell(phase,cls,it,"OK with the other state than predicted");
     if(links_of(INS "\\target")!=1)failcell(phase,cls,it,"OK but the target is not a single link");
     if(!file_is(INS "\\decoy","old"))failcell(phase,cls,it,"OK but the decoy changed");
     if(leftover_names())failcell(phase,cls,it,"OK but a pin, stage, rollback or journal name is left");
     if(is_old)ok_old++;else ok_new++;
     snap(after,sizeof(after));
     if(FsReplaceRecover(r)!=FS_READ_OK)failcell(phase,cls,it,"second recovery not OK");
     snap(again,sizeof(again));
     if(strcmp(after,again))failcell(phase,cls,it,"second recovery changed the workspace");
   }else{
     fprintf(stderr,"status=%d\n",(int)s);failcell(phase,cls,it,"recovery returned neither OK nor DENIED");
   }
   FsReadClose(r);cleanup();
 }
 printf("windows replace journal fuzz: %ld cells, seed %llu, %d per phase and class: refused %ld, ok old %ld, ok new %ld\n",
        cells,seed,iters,refused,ok_old,ok_new);
 (void)CLS;return 0;}
#else
int main(void){return 0;}
#endif
