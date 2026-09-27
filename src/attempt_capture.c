/* Private, opt-in per-attempt tree snapshots. The publication unit is one
   before+after directory; no v1 FNV fingerprints are interpreted as images. */
#include "attempt_capture.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define MAKE_DIR(p) (_mkdir(p)==0)
#define SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(p) (mkdir(p,0700)==0)
#define SEP '/'
#endif
#define MAX_FILES 64
#define MAX_FILE (256u*1024u)
#define MAX_TOTAL (4u*1024u*1024u)
#define MAX_PATH 1024
#define SHA_STR 65
/* FIPS 180-4 SHA-256, byte input, no external crypto dependency. */
typedef struct {uint32_t h[8];uint64_t bits;unsigned char buf[64];size_t used;} SHA;
static uint32_t rr(uint32_t v,unsigned n){return (v>>n)|(v<<(32-n));}
static const uint32_t K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static void sha_init(SHA *s){static const uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memset(s,0,sizeof(*s));memcpy(s->h,h,sizeof(h));}
static void sha_block(SHA *s,const unsigned char *b){uint32_t w[64],a,c,d,e,f,g,h,j,t1,t2;
 for(unsigned i=0;i<16;i++)w[i]=(uint32_t)b[4*i]<<24|(uint32_t)b[4*i+1]<<16|(uint32_t)b[4*i+2]<<8|b[4*i+3];
 for(unsigned i=16;i<64;i++)w[i]=(rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10))+w[i-7]+(rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3))+w[i-16];
 a=s->h[0];c=s->h[1];d=s->h[2];e=s->h[3];f=s->h[4];g=s->h[5];h=s->h[6];j=s->h[7];
 for(unsigned i=0;i<64;i++){t1=j+(rr(f,6)^rr(f,11)^rr(f,25))+((f&g)^(~f&h))+K[i]+w[i];t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&c)^(a&d)^(c&d));j=h;h=g;g=f;f=e+t1;e=d;d=c;c=a;a=t1+t2;}
 s->h[0]+=a;s->h[1]+=c;s->h[2]+=d;s->h[3]+=e;s->h[4]+=f;s->h[5]+=g;s->h[6]+=h;s->h[7]+=j;}
static void sha_update(SHA *s,const void *v,size_t n){const unsigned char *p=v;s->bits+=(uint64_t)n*8;
 while(n){size_t k=64-s->used;if(k>n)k=n;memcpy(s->buf+s->used,p,k);p+=k;n-=k;s->used+=k;if(s->used==64){sha_block(s,s->buf);s->used=0;}}}
static void sha_final(SHA *s,char out[SHA_STR]){uint64_t bits=s->bits;unsigned char pad[72]={0x80};size_t n=s->used<56?56-s->used:120-s->used;
 sha_update(s,pad,n);for(unsigned i=0;i<8;i++)pad[i]=(unsigned char)(bits>>(56-8*i));sha_update(s,pad,8);
 for(unsigned i=0;i<8;i++)sprintf(out+8*i,"%08x",s->h[i]);
 out[64]=0;}
typedef struct {char rel[MAX_PATH];unsigned char *bytes;size_t size;char digest[SHA_STR];} ITEM;
typedef struct {ITEM item[MAX_FILES];size_t n,total;} TREE;
static void tree_free(TREE *t){for(size_t i=0;i<t->n;i++)free(t->item[i].bytes);memset(t,0,sizeof(*t));}
static int cmp_item(const void *a,const void *b){return strcmp(((const ITEM*)a)->rel,((const ITEM*)b)->rel);}
static int join(char out[MAX_PATH],const char *a,const char *b){int n=snprintf(out,MAX_PATH,"%s%c%s",a,SEP,b);return n>0&&n<MAX_PATH;}
static int name_ok(const char *s){return *s&&strcmp(s,".")&&strcmp(s,"..")&&strchr(s,'/')==NULL&&strchr(s,'\\')==NULL;}
static int read_file(TREE *t,const char *path,const char *rel){if(t->n==MAX_FILES)return 0;
 FILE *f=fopen(path,"rb");if(!f)return 0;
 if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
 long end=ftell(f);if(end<0||end>MAX_FILE||fseek(f,0,SEEK_SET)){fclose(f);return 0;}
 size_t n=(size_t)end;
 if(t->total+n>MAX_TOTAL){fclose(f);return 0;}
 unsigned char *b=malloc(n?n:1);if(!b){fclose(f);return 0;}
 int ok=fread(b,1,n,f)==n&&!ferror(f);if(fclose(f))ok=0;
 if(!ok||memchr(b,0,n)){free(b);return 0;}
 ITEM *i=&t->item[t->n++];snprintf(i->rel,sizeof(i->rel),"%s",rel);i->bytes=b;i->size=n;t->total+=n;
 SHA s;sha_init(&s);sha_update(&s,b,n);sha_final(&s,i->digest);return 1;}
static int scan(TREE *t,const char *root,const char *rel,unsigned depth,int source){if(depth>8)return 0;
 char dir[MAX_PATH];if(rel[0]){if(!join(dir,root,rel))return 0;}else {if(strlen(root)>=sizeof(dir))return 0;strcpy(dir,root);}
#ifdef _WIN32
 char pattern[MAX_PATH];WIN32_FIND_DATAA fd;if(!join(pattern,dir,"*"))return 0;
 HANDLE f=FindFirstFileA(pattern,&fd);if(f==INVALID_HANDLE_VALUE)return 0;
 int ok=1;do {const char *n=fd.cFileName;if(!strcmp(n,".")||!strcmp(n,".."))continue;
 if(source&&!strcmp(n,".symbols")&&!rel[0]){
  /* Ignore the audit directory, but never silently skip a symlink. */
  if((fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
     !(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){ok=0;break;}
  continue;
 }
 if(!name_ok(n)||!strcmp(n,".git")|| (fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){ok=0;break;}
 char child[MAX_PATH],path[MAX_PATH];int length=snprintf(child,sizeof(child),"%s%s%s",rel,rel[0]?"/":"",n);
 if(length<=0||length>=MAX_PATH||!join(path,root,child)){ok=0;break;}
 if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)ok=scan(t,root,child,depth+1,source);
 else {
  HANDLE h=CreateFileA(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
  BY_HANDLE_FILE_INFORMATION info;ok=h!=INVALID_HANDLE_VALUE&&GetFileInformationByHandle(h,&info)&&info.nNumberOfLinks==1;
  if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);if(ok)ok=read_file(t,path,child);
 }
 }while(ok&&FindNextFileA(f,&fd));FindClose(f);return ok;
#else
 DIR *d=opendir(dir);if(!d)return 0;struct dirent *e;int ok=1;
 while(ok&&(e=readdir(d))){const char *n=e->d_name;if(!strcmp(n,".")||!strcmp(n,".."))continue;
 if(source&&!strcmp(n,".symbols")&&!rel[0]){char ignored[MAX_PATH];struct stat st;
  if(!join(ignored,dir,n)||lstat(ignored,&st)||!S_ISDIR(st.st_mode)){ok=0;break;}continue;}
 if(!name_ok(n)||!strcmp(n,".git")){ok=0;break;}
 char child[MAX_PATH],path[MAX_PATH];int length=snprintf(child,sizeof(child),"%s%s%s",rel,rel[0]?"/":"",n);
 struct stat st;if(length<=0||length>=MAX_PATH||!join(path,root,child)||lstat(path,&st)){ok=0;break;}
 if(S_ISDIR(st.st_mode))ok=scan(t,root,child,depth+1,source);
 else if(S_ISREG(st.st_mode)&&st.st_nlink==1)ok=read_file(t,path,child);
 else ok=0;
 }if(closedir(d))ok=0;return ok;
#endif
}
static int tree_read(TREE *t,const char *root,int source){memset(t,0,sizeof(*t));
#ifdef _WIN32
 DWORD attrs=GetFileAttributesA(root);if(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)||
    (attrs&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
#else
 struct stat st;if(lstat(root,&st)||!S_ISDIR(st.st_mode))return 0;
#endif
 if(!scan(t,root,"",0,source)){tree_free(t);return 0;}qsort(t->item,t->n,sizeof(t->item[0]),cmp_item);
#ifdef _WIN32
 for(size_t i=1;i<t->n;i++)if(!_stricmp(t->item[i-1].rel,t->item[i].rel)){
  tree_free(t);return 0;
 }
#endif
 return 1;}
/* Canonical unambiguous sequence: each path length (32-bit BE), path bytes,
   content length (64-bit BE), content SHA-256 raw hex. Empty tree is valid. */
static void tree_hash(const TREE *t,char out[SHA_STR]){SHA s;sha_init(&s);
 for(size_t i=0;i<t->n;i++){const ITEM *x=&t->item[i];size_t len=strlen(x->rel);unsigned char p[12];
  for(unsigned j=0;j<4;j++)p[j]=(unsigned char)(len>>(24-8*j));
  for(unsigned j=0;j<8;j++)p[4+j]=(unsigned char)(x->size>>(56-8*j));
  sha_update(&s,p,4);sha_update(&s,x->rel,len);sha_update(&s,p+4,8);sha_update(&s,x->digest,64);
 }sha_final(&s,out);}
static int mkdir_private(const char *path){if(!MAKE_DIR(path))return 0;
#ifndef _WIN32
 if(chmod(path,0700))return 0;
#endif
 return 1;}
static int write_private(const char *path,const void *data,size_t n){FILE *f=fopen(path,"wb");if(!f)return 0;
#ifndef _WIN32
 if(chmod(path,0600)){fclose(f);return 0;}
#endif
 int ok=(!n||fwrite(data,1,n,f)==n)&&!fflush(f);if(fclose(f))ok=0;return ok;}
static int copy_tree(const TREE *tree,const char *root){
 for(size_t k=0;k<tree->n;k++){char path[MAX_PATH],part[MAX_PATH];const ITEM *i=&tree->item[k];
  if(!join(path,root,i->rel))return 0;
  for(char *p=path+strlen(root)+1;*p;p++)if(*p=='/'){
   *p=0;snprintf(part,sizeof(part),"%s",path);
   /* Reused parents inside the new unpublished tree only. */
   if(!MAKE_DIR(part)){
if(errno!=EEXIST)return 0;
   }
#ifndef _WIN32
   if(chmod(part,0700))return 0;
#endif
   *p=SEP;
  }
  if(!write_private(path,i->bytes,i->size))return 0;
 }return 1;}
static int own_run(const char *run){size_t n=strlen(run);if(!n||n>80)return 0;
 for(size_t i=0;i<n;i++)if(!((run[i]>='0'&&run[i]<='9')||(run[i]>='a'&&run[i]<='z')||
 (run[i]>='A'&&run[i]<='Z')||run[i]=='-'))return 0;
 return 1;}
static int env_root(char out[MAX_PATH],const char *workspace){const char *v=getenv("SYMBOLS_ATTEMPT_CAPTURE");
 if(!v||!*v||strlen(v)>MAX_PATH-100||strlen(workspace)>MAX_PATH-100)return 0;
#ifdef _WIN32
 if(!(((v[0]>='A'&&v[0]<='Z')||(v[0]>='a'&&v[0]<='z'))&&
       v[1]==':'&&(v[2]=='/'||v[2]=='\\')))return 0;
#else
 if(v[0]!='/')return 0;
#endif
 /* The capture must be outside the workspace. Caller supplies a private
    absolute destination; no relative paths can self-poison a snapshot. */
 size_t a=strlen(v),b=strlen(workspace);
 if(strstr(v,"/../")||strstr(workspace,"/../")||strstr(v,"/./")||strstr(workspace,"/./")||
    (a>=3&&!strcmp(v+a-3,"/.."))||(b>=3&&!strcmp(workspace+b-3,"/..")))return 0;
 if((a>=b&&!strncmp(v,workspace,b)&&(v[b]=='/'||!v[b]))||
    (b>=a&&!strncmp(workspace,v,a)&&(workspace[a]=='/'||!workspace[a])))return 0;
 strcpy(out,v);return 1;}
static int ordinal(char out[16],unsigned attempt){int n=snprintf(out,16,"%03u",attempt);return attempt>0&&attempt<=999&&n>0&&n<16;}
static int publish(const char *src,const char *dst){
#ifdef _WIN32
 return MoveFileExA(src,dst,MOVEFILE_WRITE_THROUGH)!=0;
#else
 /* rename of a nonempty directory does not overwrite an existing target;
    explicit existence check also refuses an empty destination. */
 struct stat st;if(lstat(dst,&st)==0||errno!=ENOENT)return 0;
 return rename(src,dst)==0;
#endif
}
static int paths(const ATTEMPT_CAPTURE *c,char *pending,char *final)
{
 char n[16],tmp[32];
 if(!ordinal(n,c->attempt)||!join(final,c->root,n))return 0;
 snprintf(tmp,sizeof(tmp),"%s.pending",n);
 return join(pending,c->root,tmp);
}
static void mark_unavailable(const ATTEMPT_CAPTURE *c)
{
 char pending[MAX_PATH],final[MAX_PATH],path[MAX_PATH];
 if(!paths(c,pending,final)||!join(path,pending,"UNAVAILABLE"))return;
 (void)write_private(path,"capture_failed\n",15);
}
int AttemptCaptureBegin(ATTEMPT_CAPTURE *c,const char *workspace,const char *run,unsigned attempt)
{
 char base[MAX_PATH],pending[MAX_PATH],final[MAX_PATH],before[MAX_PATH];
 if(!c||!workspace||!run)return 0;
 memset(c,0,sizeof(*c));
 if(!env_root(base,workspace)||!own_run(run)||!ordinal((char[16]){0},attempt))return 0;
 if(!join(c->root,base,run))return 0;
 snprintf(c->run,sizeof(c->run),"%s",run);c->attempt=attempt;
#ifdef _WIN32
 DWORD attrs=GetFileAttributesA(base);
 if(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY)||
    (attrs&FILE_ATTRIBUTE_REPARSE_POINT))return 0;
#else
 struct stat st;
 if(lstat(base,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||
    (st.st_mode&077)!=0)return 0;
#endif
 if(!paths(c,pending,final)||!join(before,pending,"before"))return 0;
#ifdef _WIN32
 if(GetFileAttributesA(final)!=INVALID_FILE_ATTRIBUTES||
    GetFileAttributesA(pending)!=INVALID_FILE_ATTRIBUTES)return 0;
#else
 if(lstat(final,&st)==0||errno!=ENOENT||lstat(pending,&st)==0||errno!=ENOENT)return 0;
#endif
 if(attempt==1){if(!mkdir_private(c->root))return 0;}
 else {
  if(!AttemptCaptureValidate(base,run,attempt-1))return 0;
#ifdef _WIN32
  if(GetFileAttributesA(c->root)==INVALID_FILE_ATTRIBUTES)return 0;
#else
  if(lstat(c->root,&st)||!S_ISDIR(st.st_mode))return 0;
#endif
 }
 if(!mkdir_private(pending))return 0;
 TREE *t=calloc(1,sizeof(*t));if(!t){mark_unavailable(c);return 0;}
 int ok=tree_read(t,workspace,1);
 if(ok)ok=mkdir_private(before)&&copy_tree(t,before);
 if(ok&&attempt>1){char prev[MAX_PATH],num[16],prev_after[MAX_PATH],a[SHA_STR],b[SHA_STR];
     TREE *prior=calloc(1,sizeof(*prior));
     ok=prior&&ordinal(num,attempt-1)&&join(prev,c->root,num)&&
        join(prev_after,prev,"after")&&tree_read(prior,prev_after,0);
     if(ok){tree_hash(prior,a);tree_hash(t,b);ok=!strcmp(a,b);}
     if(prior){tree_free(prior);free(prior);}
 }
 tree_free(t);free(t);
 c->ready=ok;if(!ok)mark_unavailable(c);
 return ok;
}
static int manifest(const char *path,const ATTEMPT_CAPTURE *c,const char *outcome,
                    const char *bh,const char *ah)
{
 char text[512];
 int n=snprintf(text,sizeof(text),"SYMBOLS-ATTEMPT-CAPTURE\t2\nrun\t%s\nattempt\t%u\nparent\t%u\noutcome\t%s\nbefore\t%s\nafter\t%s\n",
                c->run,c->attempt,c->attempt-1,outcome,bh,ah);
 return n>0&&(size_t)n<sizeof(text)&&write_private(path,text,(size_t)n);
}
int AttemptCaptureEnd(ATTEMPT_CAPTURE *c,const char *workspace,const char *outcome)
{
 char pending[MAX_PATH],final[MAX_PATH],before[MAX_PATH],after[MAX_PATH],meta[MAX_PATH];
 char bh[SHA_STR],ah[SHA_STR];
 if(!c||!c->ready||!workspace||!outcome||!(*outcome)||strlen(outcome)>63)return 0;
 for(const char *p=outcome;*p;p++)if(!((*p>='a'&&*p<='z')||*p=='_')){
     mark_unavailable(c);c->ready=0;return 0;
 }
 if(!paths(c,pending,final)||!join(before,pending,"before")||
    !join(after,pending,"after")||!join(meta,pending,"manifest.v2")){
    mark_unavailable(c);c->ready=0;return 0;
 }
 TREE *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
 if(!a||!b){free(a);free(b);mark_unavailable(c);return 0;}
 int ok=tree_read(a,before,0)&&tree_read(b,workspace,1);
 if(ok){tree_hash(a,bh);tree_hash(b,ah);ok=mkdir_private(after)&&copy_tree(b,after);}
 if(ok)ok=manifest(meta,c,outcome,bh,ah);
 if(ok){TREE *x=calloc(1,sizeof(*x));char h[SHA_STR];
  ok=x&&tree_read(x,before,0);
  if(ok){tree_hash(x,h);ok=!strcmp(h,bh);}
  if(ok){tree_free(x);ok=tree_read(x,after,0);}
  if(ok){tree_hash(x,h);ok=!strcmp(h,ah);}
  if(x){tree_free(x);free(x);}
 }
 #ifdef ATTEMPT_CAPTURE_TEST_CRASH
 {const char *hook=getenv("SYMBOLS_ATTEMPT_CRASH");if(hook&&atoi(hook)==1) {
#ifdef _WIN32
 ExitProcess(191);
#else
 _exit(191);
#endif
 }}
#endif
 if(ok)ok=publish(pending,final);
#ifdef ATTEMPT_CAPTURE_TEST_CRASH
 {const char *hook=getenv("SYMBOLS_ATTEMPT_CRASH");if(ok&&hook&&atoi(hook)==2) {
#ifdef _WIN32
 ExitProcess(192);
#else
 _exit(192);
#endif
 }}
#endif
 if(!ok)mark_unavailable(c);
 tree_free(a);tree_free(b);free(a);free(b);c->ready=0;return ok;
}
static int pair_shape(const char *root)
{
#ifdef _WIN32
 char pattern[MAX_PATH];WIN32_FIND_DATAA fd;
 if(!join(pattern,root,"*"))return 0;
 HANDLE f=FindFirstFileA(pattern,&fd);if(f==INVALID_HANDLE_VALUE)return 0;
 int before=0,after=0,meta=0,ok=1;
 do {const char *n=fd.cFileName;if(!strcmp(n,".")||!strcmp(n,".."))continue;
  if(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT){ok=0;break;}
  if(!strcmp(n,"before")&&(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))before++;
  else if(!strcmp(n,"after")&&(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))after++;
  else if(!strcmp(n,"manifest.v2")&&!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))meta++;
  else {ok=0;break;}
 }while(FindNextFileA(f,&fd));FindClose(f);return ok&&before==1&&after==1&&meta==1;
#else
 DIR *d=opendir(root);if(!d)return 0;struct dirent *e;int before=0,after=0,meta=0,ok=1;
 while(ok&&(e=readdir(d))){const char *n=e->d_name;if(!strcmp(n,".")||!strcmp(n,".."))continue;
  char path[MAX_PATH];struct stat st;if(!join(path,root,n)||lstat(path,&st)){ok=0;break;}
  if(!strcmp(n,"before")&&S_ISDIR(st.st_mode))before++;
  else if(!strcmp(n,"after")&&S_ISDIR(st.st_mode))after++;
  else if(!strcmp(n,"manifest.v2")&&S_ISREG(st.st_mode)&&st.st_nlink==1)meta++;
  else {ok=0;break;}
 }if(closedir(d))ok=0;return ok&&before==1&&after==1&&meta==1;
#endif
}
static int exact_manifest(const char *path,ATTEMPT_CAPTURE *c,char bh[SHA_STR],char ah[SHA_STR])
{
 FILE *f=fopen(path,"rb");if(!f)return 0;
 char text[512],prefix[230],outcome[64],extra;
 size_t n=fread(text,1,sizeof(text),f);int ok=!ferror(f)&&fclose(f)==0&&n<sizeof(text);
 if(!ok)return 0;
 text[n]=0;
 int len=snprintf(prefix,sizeof(prefix),"SYMBOLS-ATTEMPT-CAPTURE\t2\nrun\t%s\nattempt\t%u\nparent\t%u\noutcome\t",
                  c->run,c->attempt,c->attempt-1);
 if(len<0||(size_t)len>=sizeof(prefix)||strncmp(text,prefix,(size_t)len))return 0;
 int count=sscanf(text+len,"%63[a-z_]\nbefore\t%64[0-9a-f]\nafter\t%64[0-9a-f]\n%c",outcome,bh,ah,&extra);
 if(count!=3||strlen(bh)!=64||strlen(ah)!=64)return 0;
 char expected[512];int m=snprintf(expected,sizeof(expected),"%s%s\nbefore\t%s\nafter\t%s\n",prefix,outcome,bh,ah);
 return m>0&&(size_t)m==n&&!memcmp(expected,text,n);
}
static int run_shape(const char *root,unsigned count)
{
#ifdef _WIN32
 char pattern[MAX_PATH];WIN32_FIND_DATAA fd;if(!join(pattern,root,"*"))return 0;
 HANDLE f=FindFirstFileA(pattern,&fd);if(f==INVALID_HANDLE_VALUE)return 0;
 int ok=1;unsigned found=0;
 do{const char *n=fd.cFileName;if(!strcmp(n,".")||!strcmp(n,".."))continue;
  unsigned k=0;char expected[16];if(strlen(n)!=3||sscanf(n,"%3u",&k)!=1||
    !ordinal(expected,k)||strcmp(n,expected)||k>count||
    !(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
    (fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)){ok=0;break;}found++;
 }while(FindNextFileA(f,&fd));FindClose(f);return ok&&found==count;
#else
 DIR *d=opendir(root);if(!d)return 0;struct dirent *e;int ok=1;unsigned found=0;
 while(ok&&(e=readdir(d))){const char *n=e->d_name;if(!strcmp(n,".")||!strcmp(n,".."))continue;
  unsigned k=0;char expected[16],path[MAX_PATH];struct stat st;
  if(strlen(n)!=3||sscanf(n,"%3u",&k)!=1||!ordinal(expected,k)||strcmp(n,expected)||k>count||
     !join(path,root,n)||lstat(path,&st)||!S_ISDIR(st.st_mode)){ok=0;break;}found++;
 }if(closedir(d))ok=0;return ok&&found==count;
#endif
}
int AttemptCaptureValidate(const char *capture_root,const char *run,unsigned count)
{
 if(!capture_root||!own_run(run)||!count||count>999||strlen(capture_root)>MAX_PATH-100)return 0;
 ATTEMPT_CAPTURE c={0};if(!join(c.root,capture_root,run))return 0;
 snprintf(c.run,sizeof(c.run),"%s",run);if(!run_shape(c.root,count))return 0;
 char previous[SHA_STR]={0};
 for(unsigned i=1;i<=count;i++){
  c.attempt=i;char pending[MAX_PATH],final[MAX_PATH],before[MAX_PATH],after[MAX_PATH],meta[MAX_PATH];
  char bh[SHA_STR],ah[SHA_STR],h[SHA_STR];
  if(!paths(&c,pending,final)||!join(before,final,"before")||
     !join(after,final,"after")||!join(meta,final,"manifest.v2"))return 0;
  TREE *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));if(!a||!b){free(a);free(b);return 0;}
  int ok=pair_shape(final)&&exact_manifest(meta,&c,bh,ah)&&tree_read(a,before,0)&&tree_read(b,after,0);
  if(ok){tree_hash(a,h);ok=!strcmp(h,bh);tree_hash(b,h);ok=ok&&!strcmp(h,ah);}
  if(ok&&i>1)ok=!strcmp(previous,bh);
  if(ok)strcpy(previous,ah);
  tree_free(a);tree_free(b);free(a);free(b);if(!ok)return 0;
 }
 return 1;
}
