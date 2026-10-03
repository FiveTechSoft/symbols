#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_manifest.h"
#include "fs_txn_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdint.h>
/* Seeded fuzz of malformed paths and operation manifests (M1 criterion 5).
   The assertions are confinement and no-change, not exact error codes:
   a rejected operation must leave the workspace tree, a directory outside
   the workspace and a file outside the workspace byte-for-byte and
   mode-for-mode unchanged, and leave no journal, stage or marker behind.
   Reproduce a failure with FS_FUZZ_SEED=<n> FS_FUZZ_ITERS=<n>. */
#define SCRATCH "test_fs_fuzz_ops_scratch"
#define OUTDIR "test_fs_fuzz_ops_outside_dir"
#define OUTFILE OUTDIR "/target"
static uint32_t g_seed=0x20d1854u;
static unsigned g_iter=0;
static const char *g_what="setup";
static const char *g_path="";
static void ck(int x,const char *m)
{
    if(!x){
        fprintf(stderr,"FAIL %s (seed=%u iter=%u op=%s path=[%s])\n",m,
                (unsigned)g_seed,g_iter,g_what,g_path);
        exit(1);
    }
}
static uint32_t rs;
static uint32_t rnd(void){rs=rs*1664525u+1013904223u;return rs;}
static void put(const char *p,const char *v,mode_t mode)
{
    FILE *f=fopen(p,"wb");
    ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");
    ck(chmod(p,mode)==0,"fixture mode");
}
/* Serialize a tree: name, kind, mode, size, bytes or link target. The
   workspace lock file is internal and excluded. */
typedef struct {char *b;size_t n,cap;} BUF;
static void bput(BUF *o,const void *p,size_t n)
{
    if(o->n+n+1>o->cap){o->cap=(o->n+n+1)*2;o->b=realloc(o->b,o->cap);ck(o->b!=NULL,"oom");}
    memcpy(o->b+o->n,p,n);o->n+=n;o->b[o->n]=0;
}
static int cmpname(const void *a,const void *b){return strcmp(*(char*const*)a,*(char*const*)b);}
static void walk(BUF *o,const char *dir,const char *rel)
{
    DIR *d=opendir(dir);struct dirent *e;char *names[256];size_t n=0;
    ck(d!=NULL,"walk open");
    while((e=readdir(d))){
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        if(!strcmp(e->d_name,".fstxn.lock"))continue;
        ck(n<256,"walk size");names[n++]=strdup(e->d_name);
    }
    closedir(d);qsort(names,n,sizeof(*names),cmpname);
    for(size_t i=0;i<n;i++){
        char path[512],line[640],link[512];struct stat st;
        snprintf(path,sizeof(path),"%s/%s",dir,names[i]);
        ck(lstat(path,&st)==0,"walk lstat");
        if(S_ISLNK(st.st_mode)){
            ssize_t k=readlink(path,link,sizeof(link)-1);ck(k>=0,"readlink");link[k]=0;
            snprintf(line,sizeof(line),"L %s/%s -> %s\n",rel,names[i],link);bput(o,line,strlen(line));
        }else if(S_ISDIR(st.st_mode)){
            snprintf(line,sizeof(line),"D %s/%s %o\n",rel,names[i],(unsigned)(st.st_mode&0777));
            bput(o,line,strlen(line));
            walk(o,path,line+2);
        }else{
            FILE *f=fopen(path,"rb");char data[512];size_t got;
            ck(f!=NULL,"walk read");got=fread(data,1,sizeof(data),f);fclose(f);
            snprintf(line,sizeof(line),"F %s/%s %o %zu ",rel,names[i],(unsigned)(st.st_mode&0777),got);
            bput(o,line,strlen(line));bput(o,data,got);bput(o,"\n",1);
        }
        free(names[i]);
    }
}
static char *snap(void)
{
    BUF o={0};
    bput(&o,"S\n",2);walk(&o,SCRATCH,"s");
    bput(&o,"O\n",2);walk(&o,OUTDIR,"o");
    return o.b;
}
static void same(const char *before,const char *what)
{
    char *now=snap();
    ck(!strcmp(before,now),what);free(now);
}
static void fixtures(void)
{
    ck(system("rm -rf " SCRATCH " " OUTDIR)==0,"pre-clean");
    ck(mkdir(SCRATCH,0700)==0&&mkdir(SCRATCH "/sub",0700)==0&&mkdir(OUTDIR,0700)==0,"dirs");
    put(SCRATCH "/safe","SAFE",0640);
    put(SCRATCH "/sub/inner","INNER",0600);
    put(OUTFILE,"outside",0644);
    ck(symlink("../" OUTDIR,SCRATCH "/linkdir")==0,"linkdir");
    ck(symlink("../" OUTFILE,SCRATCH "/linkfile")==0,"linkfile");
}
static const char *POOL[]={"..",".","","safe","sub","\\","C:","linkdir","linkfile",
    "target",".fsrp-0123456789abcdef0123456789abcdef",
    ".fsrb-0123456789abcdef0123456789abcdef",".fstxn.batch",".fstxn.commit",
    ".fstxn.lock","ctl\001x","a\\b","/"};
#define NPOOL (sizeof(POOL)/sizeof(*POOL))
/* A path that names a plain new leaf in the workspace or in "sub" is a legal
   target, so a writer may accept it. Everything else must be rejected. */
static int innocuous(unsigned count,const char *seg[])
{
    const char *last=seg[count-1];
    for(unsigned k=0;k+1<count;k++)if(strcmp(seg[k],"sub"))return 0;
    if(count==1)return !strcmp(last,"safe")||!strcmp(last,"sub")||!strcmp(last,"target");
    /* Control names are reserved at the workspace root only; inside "sub"
       they are ordinary file names (documented in the test notes). */
    return strcmp(last,"..")&&strcmp(last,".")&&*last&&strcmp(last,"\\")&&
           strcmp(last,"C:")&&strcmp(last,"/")&&strcmp(last,"a\\b")&&
           strcmp(last,"ctl\001x");
}
static void gen_path(char *out,size_t cap,int *skip)
{
    const char *seg[3];unsigned n=1+rnd()%3;
    for(unsigned k=0;k<n;k++)seg[k]=POOL[rnd()%NPOOL];
    *skip=innocuous(n,seg);
    out[0]=0;
    for(unsigned k=0;k<n;k++){
        if(k)strncat(out,"/",cap-strlen(out)-1);
        strncat(out,seg[k],cap-strlen(out)-1);
    }
}
static unsigned g_ops=0;
static void rejected(FS_READ_STATUS s,const char *base,const char *what)
{
    g_what=what;g_ops++;
    ck(s!=FS_READ_OK,"operation with a generated path was accepted");
    same(base,"rejected operation changed the tree or the outside");
}
static void path_ops(FS_READ_ROOT *root,const char *p,const char *base)
{
    static const char *exp[]={"outside","SAFE","INNER","x"};
    g_path=p;
    for(int e=0;e<4;e++){
        const char *x=exp[e];size_t xl=strlen(x);
        rejected(FsReplaceFile(root,p,x,xl,"NEW",3),base,"replace");
        rejected(FsRemoveFile(root,p,x,xl),base,"remove");
    }
    rejected(FsCreateFile(root,p,"x",1,0600),base,"create");
    rejected(FsMoveFile(root,p,"moved","SAFE",4),base,"move-from");
    rejected(FsMoveFile(root,"safe",p,"SAFE",4),base,"move-to");
    rejected(FsCopyFile(root,p,"copied","SAFE",4),base,"copy-from");
    rejected(FsCopyFile(root,"safe",p,"SAFE",4),base,"copy-to");
    {FS_BATCH_CREATE b[2]={{"fresh-a","x",1,0600},{p,"x",1,0600}};
     rejected(FsBatchCreate(root,b,2),base,"batch-create");}
    {FS_BATCH_REPLACE b[2]={{"safe","SAFE",4,"NEW",3},{p,"outside",7,"NEW",3}};
     rejected(FsBatchReplace(root,b,2),base,"batch-replace");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,p},"outside",7,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn-plan";g_ops++;
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK,"txn plan accepted a generated path");
     ck(!plan.before&&!plan.effects.items,"failed txn plan left allocations");
     same(base,"txn plan changed the tree");}
    g_what="recover";
    ck(FsBatchRecover(root)==FS_READ_OK,"recover after rejection");
    same(base,"recover changed the tree");
}
static int path_ok(const char *p)
{
    const char *c=p;
    if(!p||!*p||*p=='/'||*p=='\\')return 0;
    for(const char *q=p;;q++){
        if(*q=='/'||!*q){
            size_t n=(size_t)(q-c);
            if(!n||(n==1&&c[0]=='.')||(n==2&&c[0]=='.'&&c[1]=='.'))return 0;
            if(c==p&&n>=4&&!strncmp(c,"link",4))return 0; /* root symlinks */
            c=q+1;if(!*q)break;
        }else if(*q=='\\'||*q==':'||(unsigned char)*q<32)return 0;
    }
    return 1;
}
static void manifest_table(FS_READ_ROOT *root,const char *base)
{
    char longp[1100];FS_MANIFEST m;FS_OP_REQUEST r[2];
    memset(longp,'a',sizeof(longp)-1);longp[sizeof(longp)-1]=0;
#define BAD(reqs,n,why) do{g_what=why;memset(&m,0,sizeof(m));\
    ck(FsManifestPlan(root,reqs,n,&m)!=FS_READ_OK,why);\
    ck(!m.items&&!m.count,why " left items");same(base,why " changed disk");}while(0)
    g_what="manifest";
    r[0]=(FS_OP_REQUEST){FS_OP_CREATE,NULL,"x"};
    BAD(NULL,1,"null requests");BAD(r,0,"zero requests");BAD(r,65,"65 requests");
    ck(FsManifestPlan(NULL,r,1,&m)!=FS_READ_OK,"null root");
    ck(FsManifestPlan(root,r,1,NULL)!=FS_READ_OK,"null out");
    {FS_OP_REQUEST q[1]={{(FS_OP_KIND)0,NULL,"x"}};BAD(q,1,"kind 0");}
    {FS_OP_REQUEST q[1]={{(FS_OP_KIND)6,NULL,"x"}};BAD(q,1,"kind 6");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,"safe","x"}};BAD(q,1,"create with source");}
    {FS_OP_REQUEST q[1]={{FS_OP_REMOVE,"safe","x"}};BAD(q,1,"remove with target");}
    {FS_OP_REQUEST q[1]={{FS_OP_MOVE,NULL,"x"}};BAD(q,1,"move without source");}
    {FS_OP_REQUEST q[1]={{FS_OP_COPY,"safe",NULL}};BAD(q,1,"copy without target");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,""}};BAD(q,1,"empty path");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"/abs"}};BAD(q,1,"absolute");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"c\001x"}};BAD(q,1,"control byte");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"a\\b"}};BAD(q,1,"backslash");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"C:x"}};BAD(q,1,"colon");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"sub/../safe2"}};BAD(q,1,"dotdot component");}
    {FS_OP_REQUEST q[2]={{FS_OP_CREATE,NULL,"n1"},{FS_OP_CREATE,NULL,"n1"}};BAD(q,2,"same path twice");}
    {FS_OP_REQUEST q[2]={{FS_OP_REMOVE,"sub",NULL},{FS_OP_REMOVE,"sub/inner",NULL}};BAD(q,2,"prefix overlap");}
    {FS_OP_REQUEST q[1]={{FS_OP_MOVE,"safe","safe"}};BAD(q,1,"move onto itself");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"nodir/x"}};BAD(q,1,"missing parent");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"safe/x"}};BAD(q,1,"file as parent");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"linkdir/x"}};BAD(q,1,"symlink dir component");}
    {FS_OP_REQUEST q[1]={{FS_OP_REPLACE,NULL,"linkfile"}};BAD(q,1,"symlink leaf");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,longp}};BAD(q,1,"path over 1024");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,"safe"},NULL,0,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn no expected";
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK&&!plan.before,"txn without expected image");
     same(base,"txn no expected changed disk");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,"safe"},"WRONG",5,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn mismatch";
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK&&!plan.before,"txn with wrong image");
     same(base,"txn mismatch changed disk");}
}
static void manifest_random(FS_READ_ROOT *root,const char *base,int iters)
{
    static const char *valid[]={"safe","sub/inner","fresh","sub/fresh"};
    for(int k=0;k<iters;k++){
        FS_OP_REQUEST r[4];char paths[8][96];unsigned n=1+rnd()%4,used=0;FS_MANIFEST m={0};
        FS_READ_STATUS s;int skipped;
        g_iter=(unsigned)k;g_what="manifest-random";
        for(unsigned i=0;i<n;i++){
            FS_OP_KIND kind=(FS_OP_KIND)(1+rnd()%5);
            const char *src=NULL,*dst=NULL;
            int need_src=kind==FS_OP_MOVE||kind==FS_OP_COPY||kind==FS_OP_REMOVE;
            int need_dst=kind!=FS_OP_REMOVE;
            if(rnd()%8==0)need_src=!need_src;       /* wrong arity on purpose */
            if(rnd()%8==0)need_dst=!need_dst;
            if(need_src){
                if(rnd()%2)src=valid[rnd()%4];
                else{gen_path(paths[used],sizeof(paths[used]),&skipped);src=paths[used++];}
            }
            if(need_dst){
                if(rnd()%2)dst=valid[rnd()%4];
                else{gen_path(paths[used],sizeof(paths[used]),&skipped);dst=paths[used++];}
            }
            r[i]=(FS_OP_REQUEST){kind,src,dst};
        }
        s=FsManifestPlan(root,r,n,&m);
        if(s!=FS_READ_OK)ck(!m.items&&!m.count,"failed plan left items");
        else for(size_t e=0;e<m.count;e++){
            ck(path_ok(m.items[e].path),"accepted plan names an unsafe path");
            if(m.items[e].other)ck(path_ok(m.items[e].other),"accepted plan names an unsafe target");
        }
        FsManifestFree(&m);
        same(base,"manifest plan changed disk");
    }
}
int main(void)
{
    FS_READ_ROOT *root;char *base,*changed;unsigned iters=400;
    clock_t t0=clock();
    const char *e=getenv("FS_FUZZ_SEED");if(e)g_seed=(uint32_t)strtoul(e,NULL,10);
    e=getenv("FS_FUZZ_ITERS");if(e)iters=(unsigned)strtoul(e,NULL,10);
    rs=g_seed;
    printf("fuzz seed=%u iterations=%u\n",(unsigned)g_seed,iters);
    fixtures();
    ck(FsReadOpen(SCRATCH,&root)==FS_READ_OK,"open");
    base=snap();
    /* Positive control: the change detector sees real writes. */
    g_what="control";
    ck(FsReplaceFile(root,"safe","SAFE",4,"SAFF",4)==FS_READ_OK,"control replace");
    changed=snap();ck(strcmp(base,changed),"detector missed a replace");free(changed);
    ck(FsReplaceFile(root,"safe","SAFF",4,"SAFE",4)==FS_READ_OK,"control restore");
    {FS_BATCH_REPLACE b[2]={{"safe","SAFE",4,"SAFF",4},{"sub/inner","INNER",5,"INNEX",5}};
     ck(FsBatchReplace(root,b,2)==FS_READ_OK,"control batch");
     changed=snap();ck(strcmp(base,changed),"detector missed a batch");free(changed);
     {FS_BATCH_REPLACE r[2]={{"safe","SAFF",4,"SAFE",4},{"sub/inner","INNEX",5,"INNER",5}};
      ck(FsBatchReplace(root,r,2)==FS_READ_OK,"control batch restore");}}
    free(base);base=snap();
    {char *check=snap();ck(!strcmp(base,check),"control restore not exact");free(check);}
    for(unsigned k=0;k<iters;k++){
        char p[160];int skip;
        g_iter=k;gen_path(p,sizeof(p),&skip);
        if(skip)continue;
        path_ops(root,p,base);
    }
    manifest_table(root,base);
    manifest_random(root,base,300);
    FsReadClose(root);free(base);
    ck(system("rm -rf " SCRATCH " " OUTDIR)==0,"teardown");
    printf("fuzz ok: %u rejected operations, %.2f s\n",g_ops,
           (double)(clock()-t0)/CLOCKS_PER_SEC);
    return 0;
}
#else
/* Windows port of the confinement fuzz (M1 criterion 5), first slice.
   Same assertions as the POSIX branch: a rejected operation leaves the
   workspace tree, a directory outside it and a file outside it unchanged
   (names, kind, read-only attribute, size, bytes), and leaves no journal,
   stage or marker behind. Reproduce with FS_FUZZ_SEED / FS_FUZZ_ITERS.
   Differences from the POSIX branch, all deliberate:
   - Modes are the read-only attribute only (0444 or 0644).
   - The directory link fixture is a junction made with `mklink /J`; there
     is no symlink fixture and no "linkfile".
   - FsBatchCreate and FsBatchRecover are real on Windows (W2), and so is
     FsBatchReplace (W4); a generated bad path must be rejected with the
     tree unchanged.
   - Default iterations are lower (150 and 100) until the CI cost is known;
     the elapsed time is printed. FS_FUZZ_ITERS raises the first. */
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_batch.h"
#include "fs_manifest.h"
#include "fs_txn_plan.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#define SCRATCH "test_fs_fuzz_ops_scratch"
#define OUTDIR "test_fs_fuzz_ops_outside_dir"
#define OUTFILE OUTDIR "/target"
static uint32_t g_seed=0x20d1854u;
static unsigned g_iter=0;
static const char *g_what="setup";
static const char *g_path="";
static void ck(int x,const char *m)
{
    if(!x){
        fprintf(stderr,"FAIL %s (seed=%u iter=%u op=%s path=[%s] gle=%lu)\n",m,
                (unsigned)g_seed,g_iter,g_what,g_path,GetLastError());
        exit(1);
    }
}
static uint32_t rs;
static uint32_t rnd(void){rs=rs*1664525u+1013904223u;return rs;}
static void put(const char *p,const char *v,int readonly)
{
    FILE *f=fopen(p,"wb");
    ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");
    if(readonly)ck(SetFileAttributesA(p,FILE_ATTRIBUTE_READONLY),"fixture attribute");
}
typedef struct {char *b;size_t n,cap;} BUF;
static void bput(BUF *o,const void *p,size_t n)
{
    if(o->n+n+1>o->cap){o->cap=(o->n+n+1)*2;o->b=realloc(o->b,o->cap);ck(o->b!=NULL,"oom");}
    memcpy(o->b+o->n,p,n);o->n+=n;o->b[o->n]=0;
}
static int cmpname(const void *a,const void *b){return strcmp(*(char*const*)a,*(char*const*)b);}
static void walk(BUF *o,const char *dir,const char *rel)
{
    WIN32_FIND_DATAA fd;char pat[512];HANDLE h;char *names[256];DWORD attrs[256];
    unsigned long long sizes[256];size_t n=0;
    snprintf(pat,sizeof(pat),"%s/*",dir);
    h=FindFirstFileA(pat,&fd);ck(h!=INVALID_HANDLE_VALUE,"walk open");
    do{
        if(!strcmp(fd.cFileName,".")||!strcmp(fd.cFileName,".."))continue;
        if(!strcmp(fd.cFileName,".fstxn.lock"))continue;
        ck(n<256,"walk size");
        names[n]=_strdup(fd.cFileName);attrs[n]=fd.dwFileAttributes;
        sizes[n]=((unsigned long long)fd.nFileSizeHigh<<32)|fd.nFileSizeLow;n++;
    }while(FindNextFileA(h,&fd));
    ck(GetLastError()==ERROR_NO_MORE_FILES,"walk enumerate");
    FindClose(h);
    /* Sort names only; look attributes and sizes up again by name. */
    {char *sorted[256];size_t i,j;
     for(i=0;i<n;i++)sorted[i]=names[i];
     qsort(sorted,n,sizeof(*sorted),cmpname);
     for(i=0;i<n;i++){
        char path[512],line[640];DWORD a=0;unsigned long long sz=0;
        for(j=0;j<n;j++)if(names[j]==sorted[i]){a=attrs[j];sz=sizes[j];break;}
        snprintf(path,sizeof(path),"%s/%s",dir,sorted[i]);
        if(a&FILE_ATTRIBUTE_REPARSE_POINT){
            /* Never followed: a junction is recorded as a link by name. */
            snprintf(line,sizeof(line),"L %s/%s\n",rel,sorted[i]);bput(o,line,strlen(line));
        }else if(a&FILE_ATTRIBUTE_DIRECTORY){
            snprintf(line,sizeof(line),"D %s/%s\n",rel,sorted[i]);bput(o,line,strlen(line));
            {char sub[640];snprintf(sub,sizeof(sub),"%s/%s",rel,sorted[i]);walk(o,path,sub);}
        }else{
            FILE *f=fopen(path,"rb");char data[512];size_t got;
            ck(f!=NULL,"walk read");got=fread(data,1,sizeof(data),f);fclose(f);
            snprintf(line,sizeof(line),"F %s/%s %s %llu %zu ",rel,sorted[i],
                     (a&FILE_ATTRIBUTE_READONLY)?"ro":"rw",sz,got);
            bput(o,line,strlen(line));bput(o,data,got);bput(o,"\n",1);
        }
     }
     for(i=0;i<n;i++)free(names[i]);}
}
static char *snap(void)
{
    BUF o={0};
    bput(&o,"S\n",2);walk(&o,SCRATCH,"s");
    bput(&o,"O\n",2);walk(&o,OUTDIR,"o");
    return o.b;
}
static void same(const char *before,const char *what)
{
    char *now=snap();
    ck(!strcmp(before,now),what);free(now);
}
static void clean(void)
{
    /* A junction is removed as a link, never followed. */
    ck(system("if exist " SCRATCH "\\linkdir rmdir " SCRATCH "\\linkdir")==0,"junction clean");
    ck(system("if exist " SCRATCH " rmdir /s /q " SCRATCH)==0,"scratch clean");
    ck(system("if exist " OUTDIR " rmdir /s /q " OUTDIR)==0,"outside clean");
}
static void fixtures(void)
{
    clean();
    ck(CreateDirectoryA(SCRATCH,NULL)&&CreateDirectoryA(SCRATCH "/sub",NULL)&&
       CreateDirectoryA(OUTDIR,NULL),"dirs");
    put(SCRATCH "/safe","SAFE",0);
    put(SCRATCH "/sub/inner","INNER",0);
    put(OUTFILE,"outside",0);
    ck(system("mklink /J " SCRATCH "\\linkdir " OUTDIR " >nul")==0,"junction fixture");
}
static const char *POOL[]={"..",".","","safe","sub","\\","C:","linkdir",
    "target",".fsrp-0123456789abcdef0123456789abcdef",
    ".fsrb-0123456789abcdef0123456789abcdef",".fstxn.batch",".fstxn.commit",
    ".fstxn.lock",".fstxn.intent",".fstxn.remove",".fstxn.move",".fstxn.rcommit",
    ".fstxn.mcommit",".fst-stage",".fsrm-stage",".fsmv-stage",
    "ctl\001x","a\\b","/","NUL","con.txt","trail.","space "};
#define NPOOL (sizeof(POOL)/sizeof(*POOL))
/* A path that names a plain new leaf in the workspace or in "sub" is a legal
   target, so a writer may accept it; those are skipped. Everything else
   must be rejected. Win32 aliases (device names, trailing dot or space) are
   not in the skipped set: the writers are expected to refuse them. */
static int innocuous(unsigned count,const char *seg[])
{
    const char *last=seg[count-1];
    for(unsigned k=0;k+1<count;k++)if(strcmp(seg[k],"sub"))return 0;
    if(count==1)return !strcmp(last,"safe")||!strcmp(last,"sub")||!strcmp(last,"target");
    return strcmp(last,"..")&&strcmp(last,".")&&*last&&strcmp(last,"\\")&&
           strcmp(last,"C:")&&strcmp(last,"/")&&strcmp(last,"a\\b")&&
           strcmp(last,"ctl\001x")&&strcmp(last,"NUL")&&strcmp(last,"con.txt")&&
           strcmp(last,"trail.")&&strcmp(last,"space ");
}
static void gen_path(char *out,size_t cap,int *skip)
{
    const char *seg[3];unsigned n=1+rnd()%3;
    for(unsigned k=0;k<n;k++)seg[k]=POOL[rnd()%NPOOL];
    *skip=innocuous(n,seg);
    out[0]=0;
    for(unsigned k=0;k<n;k++){
        if(k)strncat(out,"/",cap-strlen(out)-1);
        strncat(out,seg[k],cap-strlen(out)-1);
    }
}
static unsigned g_ops=0;
static void rejected(FS_READ_STATUS s,const char *base,const char *what)
{
    g_what=what;g_ops++;
    ck(s!=FS_READ_OK,"operation with a generated path was accepted");
    same(base,"rejected operation changed the tree or the outside");
}
static void path_ops(FS_READ_ROOT *root,const char *p,const char *base)
{
    static const char *exp[]={"outside","SAFE","INNER","x"};
    g_path=p;
    for(int e=0;e<4;e++){
        const char *x=exp[e];size_t xl=strlen(x);
        rejected(FsReplaceFile(root,p,x,xl,"NEW",3),base,"replace");
        rejected(FsRemoveFile(root,p,x,xl),base,"remove");
    }
    rejected(FsCreateFile(root,p,"x",1,0600),base,"create");
    rejected(FsMoveFile(root,p,"moved","SAFE",4),base,"move-from");
    rejected(FsMoveFile(root,"safe",p,"SAFE",4),base,"move-to");
    rejected(FsCopyFile(root,p,"copied","SAFE",4),base,"copy-from");
    rejected(FsCopyFile(root,"safe",p,"SAFE",4),base,"copy-to");
    {FS_BATCH_CREATE b[2]={{"fresh-a","x",1,0600},{p,"x",1,0600}};
     rejected(FsBatchCreate(root,b,2),base,"batch-create");}
    {FS_BATCH_REPLACE b[2]={{"safe","SAFE",4,"NEW",3},{p,"outside",7,"NEW",3}};
     rejected(FsBatchReplace(root,b,2),base,"batch-replace");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,p},"outside",7,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn-plan";g_ops++;
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK,"txn plan accepted a generated path");
     ck(!plan.before&&!plan.effects.items,"failed txn plan left allocations");
     same(base,"txn plan changed the tree");}
    g_what="recover";
    ck(FsBatchRecover(root)==FS_READ_OK,"batch recover on a clean tree");
    same(base,"recover changed the tree");
}
static int path_ok(const char *p)
{
    const char *c=p;
    if(!p||!*p||*p=='/'||*p=='\\')return 0;
    for(const char *q=p;;q++){
        if(*q=='/'||!*q){
            size_t n=(size_t)(q-c);
            if(!n||(n==1&&c[0]=='.')||(n==2&&c[0]=='.'&&c[1]=='.'))return 0;
            if(c==p&&n>=4&&!strncmp(c,"link",4))return 0; /* root junction */
            c=q+1;if(!*q)break;
        }else if(*q=='\\'||*q==':'||(unsigned char)*q<32)return 0;
    }
    return 1;
}
static void manifest_table(FS_READ_ROOT *root,const char *base)
{
    char longp[1100];FS_MANIFEST m;FS_OP_REQUEST r[2];
    memset(longp,'a',sizeof(longp)-1);longp[sizeof(longp)-1]=0;
#define BAD(reqs,n,why) do{g_what=why;memset(&m,0,sizeof(m));\
    ck(FsManifestPlan(root,reqs,n,&m)!=FS_READ_OK,why);\
    ck(!m.items&&!m.count,why " left items");same(base,why " changed disk");}while(0)
    g_what="manifest";
    r[0]=(FS_OP_REQUEST){FS_OP_CREATE,NULL,"x"};
    BAD(NULL,1,"null requests");BAD(r,0,"zero requests");BAD(r,65,"65 requests");
    ck(FsManifestPlan(NULL,r,1,&m)!=FS_READ_OK,"null root");
    ck(FsManifestPlan(root,r,1,NULL)!=FS_READ_OK,"null out");
    {FS_OP_REQUEST q[1]={{(FS_OP_KIND)0,NULL,"x"}};BAD(q,1,"kind 0");}
    {FS_OP_REQUEST q[1]={{(FS_OP_KIND)6,NULL,"x"}};BAD(q,1,"kind 6");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,"safe","x"}};BAD(q,1,"create with source");}
    {FS_OP_REQUEST q[1]={{FS_OP_REMOVE,"safe","x"}};BAD(q,1,"remove with target");}
    {FS_OP_REQUEST q[1]={{FS_OP_MOVE,NULL,"x"}};BAD(q,1,"move without source");}
    {FS_OP_REQUEST q[1]={{FS_OP_COPY,"safe",NULL}};BAD(q,1,"copy without target");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,""}};BAD(q,1,"empty path");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"/abs"}};BAD(q,1,"absolute");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"c\001x"}};BAD(q,1,"control byte");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"a\\b"}};BAD(q,1,"backslash");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"C:x"}};BAD(q,1,"colon");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"sub/../safe2"}};BAD(q,1,"dotdot component");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"NUL"}};BAD(q,1,"device name");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"trail."}};BAD(q,1,"trailing dot");}
    {FS_OP_REQUEST q[2]={{FS_OP_CREATE,NULL,"n1"},{FS_OP_CREATE,NULL,"n1"}};BAD(q,2,"same path twice");}
    {FS_OP_REQUEST q[2]={{FS_OP_CREATE,NULL,"Same"},{FS_OP_CREATE,NULL,"SAME"}};BAD(q,2,"same path, case only");}
    {FS_OP_REQUEST q[2]={{FS_OP_REMOVE,"sub",NULL},{FS_OP_REMOVE,"sub/inner",NULL}};BAD(q,2,"prefix overlap");}
    {FS_OP_REQUEST q[1]={{FS_OP_MOVE,"safe","safe"}};BAD(q,1,"move onto itself");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"nodir/x"}};BAD(q,1,"missing parent");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"safe/x"}};BAD(q,1,"file as parent");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,"linkdir/x"}};BAD(q,1,"junction dir component");}
    {FS_OP_REQUEST q[1]={{FS_OP_CREATE,NULL,longp}};BAD(q,1,"path over 1024");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,"safe"},NULL,0,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn no expected";
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK&&!plan.before,"txn without expected image");
     same(base,"txn no expected changed disk");}
    {FS_TXN_REQUEST t[1]={{{FS_OP_REPLACE,NULL,"safe"},"WRONG",5,"x",1}};FS_TXN_PLAN plan={0};
     g_what="txn mismatch";
     ck(FsTxnPlan(root,t,1,&plan)!=FS_READ_OK&&!plan.before,"txn with wrong image");
     same(base,"txn mismatch changed disk");}
}
static void manifest_random(FS_READ_ROOT *root,const char *base,int iters)
{
    static const char *valid[]={"safe","sub/inner","fresh","sub/fresh"};
    for(int k=0;k<iters;k++){
        FS_OP_REQUEST r[4];char paths[8][96];unsigned n=1+rnd()%4,used=0;FS_MANIFEST m={0};
        FS_READ_STATUS s;int skipped;
        g_iter=(unsigned)k;g_what="manifest-random";
        for(unsigned i=0;i<n;i++){
            FS_OP_KIND kind=(FS_OP_KIND)(1+rnd()%5);
            const char *src=NULL,*dst=NULL;
            int need_src=kind==FS_OP_MOVE||kind==FS_OP_COPY||kind==FS_OP_REMOVE;
            int need_dst=kind!=FS_OP_REMOVE;
            if(rnd()%8==0)need_src=!need_src;
            if(rnd()%8==0)need_dst=!need_dst;
            if(need_src){
                if(rnd()%2)src=valid[rnd()%4];
                else{gen_path(paths[used],sizeof(paths[used]),&skipped);src=paths[used++];}
            }
            if(need_dst){
                if(rnd()%2)dst=valid[rnd()%4];
                else{gen_path(paths[used],sizeof(paths[used]),&skipped);dst=paths[used++];}
            }
            r[i]=(FS_OP_REQUEST){kind,src,dst};
        }
        s=FsManifestPlan(root,r,n,&m);
        if(s!=FS_READ_OK)ck(!m.items&&!m.count,"failed plan left items");
        else for(size_t e=0;e<m.count;e++){
            ck(path_ok(m.items[e].path),"accepted plan names an unsafe path");
            if(m.items[e].other)ck(path_ok(m.items[e].other),"accepted plan names an unsafe target");
        }
        FsManifestFree(&m);
        same(base,"manifest plan changed disk");
    }
}
int main(void)
{
    FS_READ_ROOT *root;char *base,*changed;unsigned iters=150;
    clock_t t0=clock();
    const char *e=getenv("FS_FUZZ_SEED");if(e)g_seed=(uint32_t)strtoul(e,NULL,10);
    e=getenv("FS_FUZZ_ITERS");if(e)iters=(unsigned)strtoul(e,NULL,10);
    rs=g_seed;
    printf("fuzz (windows) seed=%u iterations=%u\n",(unsigned)g_seed,iters);
    fixtures();
    ck(FsReadOpen(SCRATCH,&root)==FS_READ_OK,"open");
    base=snap();
    /* Positive control: the change detector sees real writes. */
    g_what="control";
    ck(FsReplaceFile(root,"safe","SAFE",4,"SAFF",4)==FS_READ_OK,"control replace");
    changed=snap();ck(strcmp(base,changed),"detector missed a replace");free(changed);
    ck(FsReplaceFile(root,"safe","SAFF",4,"SAFE",4)==FS_READ_OK,"control restore");
    free(base);base=snap();
    {char *check=snap();ck(!strcmp(base,check),"control restore not exact");free(check);}
    for(unsigned k=0;k<iters;k++){
        char p[160];int skip;
        g_iter=k;gen_path(p,sizeof(p),&skip);
        if(skip)continue;
        path_ops(root,p,base);
    }
    manifest_table(root,base);
    manifest_random(root,base,100);
    FsReadClose(root);free(base);
    clean();
    printf("fuzz ok (windows): %u rejected operations, %.2f s\n",g_ops,
           (double)(clock()-t0)/CLOCKS_PER_SEC);
    return 0;
}
#endif
