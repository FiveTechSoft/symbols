#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "atomic_store.h"
#include "episodic_memory.h"
#include "engineering_episode.h"
#include "chat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define UNLINK _unlink
#define TLS __declspec(thread)
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#define UNLINK unlink
#define TLS _Thread_local
#endif
/* Real OS adapters are defined here without the macro header. */
typedef struct {
    const char *fault;
    char trace[32768],tmp[600],foreign[600];
    int collision,write_interrupt,write_short,pause;
#ifdef _WIN32
    HANDLE ready,go,temp_handle;
#else
    int temp_fd;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready,go;
#endif
} TEST_STATE;
static TEST_STATE states[3];
static TLS int actor;
static void trace(const char *s)
{
    TEST_STATE *t=&states[actor];size_t n=strlen(t->trace),m=strlen(s);
    assert(n+m+2<sizeof(t->trace));memcpy(t->trace+n,s,m);t->trace[n+m]=' ';t->trace[n+m+1]=0;
}
int AtomicStoreTestHook(const char *stage,const char *path)
{
    TEST_STATE *t=&states[actor];
    if(!strcmp(stage,"create") && t->collision) {
        t->collision=0;snprintf(t->foreign,sizeof(t->foreign),"%s",path);
        FILE *f=fopen(path,"wb");assert(f);assert(fwrite("foreign",1,7,f)==7);assert(!fclose(f));
    }
    if(!strcmp(stage,"temp_created")) {
        snprintf(t->tmp,sizeof(t->tmp),"%s",path);
        if(t->pause) {
#ifdef _WIN32
            assert(SetEvent(t->ready));assert(WaitForSingleObject(t->go,10000)==WAIT_OBJECT_0);
#else
            assert(!pthread_mutex_lock(&t->mutex));t->ready=1;assert(!pthread_cond_signal(&t->cond));
            while(!t->go) assert(!pthread_cond_wait(&t->cond,&t->mutex));
            assert(!pthread_mutex_unlock(&t->mutex));
#endif
        }
    }
    if(t->fault && !strcmp(t->fault,stage)) {errno=EIO;return 1;}
    return 0;
}
#ifdef _WIN32
HANDLE AtomicStoreTestCreateFileA(LPCSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sec,DWORD create,DWORD flags,HANDLE h)
{ assert(create==CREATE_NEW);trace("exclusive_create");HANDLE f=CreateFileA(p,a,s,sec,create,flags,h);states[actor].temp_handle=f;return f; }
BOOL AtomicStoreTestCloseHandle(HANDLE h)
{if(h==states[actor].temp_handle)trace("file_close");return CloseHandle(h);}
BOOL AtomicStoreTestFlushFileBuffers(HANDLE h)
{trace("file_sync");return FlushFileBuffers(h);}
BOOL AtomicStoreTestMoveFileExA(LPCSTR a,LPCSTR b,DWORD flags)
{
#ifdef ATOMIC_STORE_PRIVATE_DROP_WRITE_THROUGH
    flags &= ~MOVEFILE_WRITE_THROUGH;
#endif
    if((flags&(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))!=(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        fputs("ATOMIC_STORE_FLAGS_MUTANT missing_write_through=1\n",stderr);fflush(stderr);exit(42);
    }
    trace("rename");return MoveFileExA(a,b,flags);
}
#else
int AtomicStoreTestOpen(const char *p,int flags,...)
{
    if(flags&O_CREAT) {
        va_list ap;va_start(ap,flags);mode_t mode=(mode_t)va_arg(ap,int);va_end(ap);
        assert(flags&O_EXCL);assert(mode==0666);trace("exclusive_create");int fd=open(p,flags,mode);states[actor].temp_fd=fd;return fd;
    }
    return open(p,flags);
}
int AtomicStoreTestClose(int fd)
{if(fd==states[actor].temp_fd && fd>0){trace("file_close");states[actor].temp_fd=-1;}return close(fd);}
int AtomicStoreTestFsync(int fd)
{
    struct stat st;assert(!fstat(fd,&st));trace(S_ISDIR(st.st_mode)?"parent_sync":"file_sync");return fsync(fd);
}
int AtomicStoreTestRename(const char *a,const char *b)
{trace("rename");return rename(a,b);}
ssize_t AtomicStoreTestWrite(int fd,const void *p,size_t n)
{
    TEST_STATE *t=&states[actor];
    if(t->write_interrupt) {t->write_interrupt=0;errno=EINTR;return -1;}
    if(t->write_short && n>1) {t->write_short=0;return write(fd,p,n/2);}
    return write(fd,p,n);
}
#endif
static int exists(const char *p)
{
#ifdef _WIN32
    return GetFileAttributesA(p)!=INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;return lstat(p,&st)==0;
#endif
}
static void bytes(const char *path,const char *want)
{
    char b[4096];FILE *f=fopen(path,"rb");assert(f);size_t n=fread(b,1,sizeof(b),f);assert(!ferror(f));assert(!fclose(f));assert(n==strlen(want));assert(!memcmp(b,want,n));
}
static const char *file="build/atomic-durability-private/image";
static void reset(void)
{assert(actor==0);memset(&states[0],0,sizeof(states[0]));}
static void atomic_cases(void)
{
    assert(AtomicStorePrepareParent(file));
    const char *faults[]={"create","write","file_sync","file_close","rename"};
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) {
        reset();assert(AtomicStoreReplace(file,"old",3));reset();states[0].fault=faults[i];
        assert(AtomicStoreReplaceDetailed(file,"new",3)==ATOMIC_STORE_UNCHANGED);bytes(file,"old");
        assert(!states[0].tmp[0] || !exists(states[0].tmp));
        assert(!AtomicStoreReplace(file,"new",3));bytes(file,"old");
        reset();assert(!UNLINK(file));states[0].fault=faults[i];
        assert(AtomicStoreReplaceDetailed(file,"new",3)==ATOMIC_STORE_UNCHANGED);assert(!exists(file));
        assert(!states[0].tmp[0] || !exists(states[0].tmp));
    }
    reset();assert(AtomicStoreReplace(file,"old",3));reset();
#ifndef _WIN32
    states[0].fault="parent_sync";
    assert(AtomicStoreReplaceDetailed(file,"new",3)==ATOMIC_STORE_COMMITTED_DURABILITY_UNKNOWN);bytes(file,"new");
    assert(!AtomicStoreReplace(file,"next",4));bytes(file,"next");
    reset();assert(!UNLINK(file));states[0].fault="parent_sync";
    assert(AtomicStoreReplaceDetailed(file,"new",3)==ATOMIC_STORE_COMMITTED_DURABILITY_UNKNOWN);bytes(file,"new");
    reset();states[0].fault="mkdir_parent_sync";
    /* Each run gets a new directory name, so the fault cannot be bypassed by
       directories left by an earlier interrupted run. */
    char nested[180];snprintf(nested,sizeof(nested),"build/atomic-durability-private/new-%lu/a/image",(unsigned long)getpid());
    assert(AtomicStoreReplaceDetailed(nested,"x",1)==ATOMIC_STORE_UNCHANGED);assert(!exists(nested));
#endif
    reset();states[0].collision=1;assert(AtomicStoreReplace(file,"new",3));bytes(states[0].foreign,"foreign");assert(!UNLINK(states[0].foreign));
    reset();states[0].write_interrupt=1;states[0].write_short=1;
    assert(AtomicStoreReplaceDetailed(file,"complete-image",14)==ATOMIC_STORE_DURABLE);bytes(file,"complete-image");
#ifdef _WIN32
    assert(!strcmp(strstr(states[0].trace,"exclusive_create"),"exclusive_create file_sync file_close rename "));
#else
    assert(!strcmp(strstr(states[0].trace,"exclusive_create"),"exclusive_create file_sync file_close rename parent_sync "));
    struct stat st;assert(!chmod(file,0640));reset();assert(AtomicStoreReplace(file,"mode",4));assert(!stat(file,&st) && (st.st_mode&0777)==0640);
    reset();states[0].fault="chmod";assert(AtomicStoreReplace(file,"best-effort",11));bytes(file,"best-effort");
    reset();assert(!UNLINK(file));mode_t before=umask(0027);assert(AtomicStoreReplace(file,"mode",4));umask(before);assert(!stat(file,&st) && (st.st_mode&0777)==0640);
#endif
#ifndef _WIN32
    reset();assert(!UNLINK(file));assert(!mkfifo(file,0600));assert(AtomicStoreReplace(file,"fifo-replaced",13));bytes(file,"fifo-replaced");
#endif
    reset();char foreign[600];snprintf(foreign,sizeof(foreign),"%s.tmp",file);FILE *f=fopen(foreign,"wb");assert(f);fputs("foreign-fixed",f);assert(!fclose(f));assert(AtomicStoreReplace(file,"new",3));bytes(foreign,"foreign-fixed");assert(!UNLINK(foreign));
    assert(AtomicStoreReplace("leaf-image","leaf",4));bytes("leaf-image","leaf");assert(!UNLINK("leaf-image"));
    assert(AtomicStoreReplace("build/atomic-durability-private/space dir/image","space",5));bytes("build/atomic-durability-private/space dir/image","space");
    char absolute[600];
#ifdef _WIN32
    assert(GetFullPathNameA(file,(DWORD)sizeof(absolute),absolute,NULL)>0);
#else
    char cwd[512];assert(getcwd(cwd,sizeof(cwd)));assert(snprintf(absolute,sizeof(absolute),"%s/%s",cwd,file)>0);
#endif
    assert(AtomicStoreReplace(absolute,"absolute",8));bytes(absolute,"absolute");
    assert(AtomicStoreReplace(file,NULL,0));bytes(file,"");assert(AtomicStoreReplaceDetailed(file,NULL,1)==ATOMIC_STORE_UNCHANGED);assert(AtomicStoreReplaceDetailed(NULL,"x",1)==ATOMIC_STORE_UNCHANGED);assert(!AtomicStoreReplace("","x",1));
    char longpath[600];memset(longpath,'x',sizeof(longpath)-1);longpath[sizeof(longpath)-1]=0;assert(!AtomicStoreReplace(longpath,"x",1));
    puts("atomic_durability private atomic faults/order/mode checks passed");
}
static void episodic_cases(void)
{
    const char *path="build/atomic-durability-private/episodic.tsv";
    const char *faults[]={"rename",
#ifndef _WIN32
    "parent_sync",
#endif
    NULL};
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) for(int op=0;op<4;++op) {
        reset();UNLINK(path);EPISODIC_STORE st,loaded;assert(EpisodicStoreInit(&st,path));assert(EpisodicStoreAppend(&st,"old","is","fact","test")==1);
        states[0].fault=faults[i];ATOMIC_STORE_RESULT status=ATOMIC_STORE_DURABLE;int rc;
        if(op==0) rc=EpisodicStoreAppendEx(&st,"new","is","fact","test",&status);
        else if(op==1) rc=EpisodicStoreForgetEx(&st,"old","is","fact",&status);
        else if(op==2) rc=EpisodicStoreClearEx(&st,&status);
        else { st.auto_save=0;assert(EpisodicStoreAppend(&st,"new","is","fact","test")==1);rc=EpisodicStoreSaveEx(&st,&status); }
        int published=!faults[i] || strcmp(faults[i],"rename");
        assert(rc==(!faults[i]?1:0));assert(status==(!faults[i]?ATOMIC_STORE_DURABLE:published?ATOMIC_STORE_COMMITTED_DURABILITY_UNKNOWN:ATOMIC_STORE_UNCHANGED));
        assert(EpisodicStoreInit(&loaded,path));EpisodicStoreLoad(&loaded);
        unsigned expected=op==0||op==3?(published?2:1):(published?0:1);
        assert(loaded.count==expected);assert(st.count==(op==3?2:expected));
        EpisodicStoreDestroy(&loaded);EpisodicStoreDestroy(&st);
    }
    reset();EPISODIC_STORE st;ATOMIC_STORE_RESULT status=ATOMIC_STORE_DURABLE;assert(!EpisodicStoreSaveEx(NULL,&status) && status==ATOMIC_STORE_UNCHANGED);assert(EpisodicStoreInit(&st,path));st.auto_save=0;
    assert(EpisodicStoreAppendEx(&st,"one","is","fact","test",&status)==1 && status==ATOMIC_STORE_UNCHANGED);
    assert(EpisodicStoreAppendEx(&st,"one","is","fact","test",&status)==2 && status==ATOMIC_STORE_UNCHANGED);
    assert(EpisodicStoreForgetEx(&st,"absent","is","fact",&status)==2 && status==ATOMIC_STORE_UNCHANGED);EpisodicStoreDestroy(&st);
    puts("atomic_durability private episodic append/forget/clear/save state checks passed");
}
static void chat_cases(void)
{
    const char *faults[]={"rename",
#ifndef _WIN32
    "parent_sync",
#endif
    NULL};
    const char *path="build/atomic-durability-private/chat.tsv";
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) for(int op=0;op<3;++op) {
        reset();UNLINK(path);CHAT *ch=(CHAT *)calloc(1,sizeof(*ch));assert(ch);ChatInit(ch,"corpus.tsv");
        EpisodicStoreDestroy(&ch->episodic);assert(EpisodicStoreInit(&ch->episodic,path));
        assert(ChatLearnTriple(ch,"Ada","hermano_de","Babbage","test")==1);
        states[0].fault=faults[i];int rc;
        if(op==0) rc=ChatLearnTriple(ch,"Bela","hermano_de","Turing","test");
        else if(op==1) rc=ChatForgetTriple(ch,"Ada","hermano_de","Babbage");
        else rc=ChatEpisodicClear(ch);
        int published=!faults[i] || strcmp(faults[i],"rename");
        assert(rc==(!faults[i]?1:op==1?-1:0));assert(ch->episodic.count==(op==0?(published?2:1):(published?0:1)));
        /* Query the exact same live-session route as the existing session test. */
        char answer[4096]={0};
        ChatHandleToBuf(ch,op==0?"es bela hermano de turing?":"es ada hermano de babbage?",answer,sizeof(answer));
        int should_answer=op==0?published:!published;
        assert((strstr(answer,"Si,")!=NULL)==should_answer);
        EPISODIC_STORE loaded;assert(EpisodicStoreInit(&loaded,path));EpisodicStoreLoad(&loaded);
        assert(loaded.count==ch->episodic.count);EpisodicStoreDestroy(&loaded);
        reset();ChatDestroy(ch);free(ch);
    }
    puts("atomic_durability private chat publication-state checks passed");
}
static void episode_cases(void)
{
    const char *path="build/atomic-durability-private/engineering.v1";
    const char *faults[]={"rename",
#ifndef _WIN32
    "parent_sync",
#endif
    NULL};
    for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);++i) {
        reset();UNLINK(path);ENGINEERING_EPISODE row={0};
        strcpy(row.episode_id,"one");strcpy(row.run_id,"run");strcpy(row.attempt_id,"one");
        strcpy(row.outcome,"abstained");strcpy(row.rollback,"not_needed");
        states[0].fault=faults[i];assert(EpisodeAppend(path,&row)==(!faults[i]?1:0));
        EPISODE_STORE *loaded=(EPISODE_STORE *)calloc(1,sizeof(*loaded));assert(loaded);
        int published=!faults[i] || strcmp(faults[i],"rename");
        assert(EpisodeLoad(path,loaded)==(published?1:0));assert(loaded->count==(published?1:0));free(loaded);
        char lock[600];snprintf(lock,sizeof(lock),"%s.lock",path);assert(!exists(lock));
    }
    puts("atomic_durability private engineering publication/lock checks passed");
}
#ifdef _WIN32
static DWORD WINAPI writer(LPVOID arg)
#else
static void *writer(void *arg)
#endif
{
    actor=(int)(size_t)arg;int ok=AtomicStoreReplace(file,actor==1?"AAAAAAAA":"BBBBBBBB",8);
    assert(ok==(states[actor].fault?0:1));
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
static void concurrency_cases(void)
{
    for(int fail=0;fail<2;++fail) {
        reset();assert(AtomicStoreReplace(file,"original",8));
#ifdef _WIN32
        HANDLE threads[2];
#else
        pthread_t threads[2];
#endif
        for(int i=1;i<=2;++i) {
            memset(&states[i],0,sizeof(states[i]));states[i].pause=1;if(i==1&&fail)states[i].fault="rename";
#ifdef _WIN32
            states[i].ready=CreateEventA(NULL,TRUE,FALSE,NULL);states[i].go=CreateEventA(NULL,TRUE,FALSE,NULL);assert(states[i].ready && states[i].go);
            threads[i-1]=CreateThread(NULL,0,writer,(LPVOID)(size_t)i,0,NULL);assert(threads[i-1]);assert(WaitForSingleObject(states[i].ready,10000)==WAIT_OBJECT_0);
#else
            assert(!pthread_mutex_init(&states[i].mutex,NULL));assert(!pthread_cond_init(&states[i].cond,NULL));assert(!pthread_create(&threads[i-1],NULL,writer,(void *)(size_t)i));
            assert(!pthread_mutex_lock(&states[i].mutex));while(!states[i].ready)assert(!pthread_cond_wait(&states[i].cond,&states[i].mutex));assert(!pthread_mutex_unlock(&states[i].mutex));
#endif
        }
        assert(strcmp(states[1].tmp,states[2].tmp));assert(exists(states[1].tmp)&&exists(states[2].tmp));
        for(int i=1;i<=2;++i) {
#ifdef _WIN32
            assert(SetEvent(states[i].go));assert(WaitForSingleObject(threads[i-1],10000)==WAIT_OBJECT_0);CloseHandle(threads[i-1]);CloseHandle(states[i].go);CloseHandle(states[i].ready);
#else
            assert(!pthread_mutex_lock(&states[i].mutex));states[i].go=1;assert(!pthread_cond_signal(&states[i].cond));assert(!pthread_mutex_unlock(&states[i].mutex));assert(!pthread_join(threads[i-1],NULL));pthread_cond_destroy(&states[i].cond);pthread_mutex_destroy(&states[i].mutex);
#endif
            if(i==1) {assert(exists(states[2].tmp));bytes(file,fail?"original":"AAAAAAAA");}
        }
        bytes(file,"BBBBBBBB");assert(!exists(states[1].tmp)&&!exists(states[2].tmp));
    }
    puts("atomic_durability private two-writer unique-temp/cleanup checks passed");
}
int main(void)
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#endif
    FILE *corpus=fopen("corpus.tsv","wb");assert(corpus);fputs("paris\tIN\tfrance\n",corpus);assert(!fclose(corpus));
    atomic_cases();episodic_cases();chat_cases();episode_cases();concurrency_cases();reset();UNLINK(file);
    puts("ATOMIC_DURABILITY complete=1");return 0;
}
