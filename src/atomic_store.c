#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "atomic_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
#define MKDIR(d) mkdir(d, 0755)
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#endif

/* Only the private fault-test executable defines this callback. No production
   environment variable, exported failpoint or mutable test state exists. */
#ifdef ATOMIC_STORE_PRIVATE_TEST
#include "../tests/atomic_store_test_adapter.h"
extern int AtomicStoreTestHook(const char *stage, const char *path);
#define FAIL(stage,path) AtomicStoreTestHook(stage,path)
#else
#define FAIL(stage,path) 0
#endif

#ifndef _WIN32
static int sync_fd(int fd)
{
    int rc;
    do { rc = fsync(fd); } while (rc < 0 && errno == EINTR);
    return rc == 0;
}
static int parent_name(const char *path, char *out, size_t cap)
{
    const char *slash = strrchr(path, '/');
    size_t n = slash ? (slash == path ? 1 : (size_t)(slash-path)) : 1;
    if (n >= cap) return 0;
    if (!slash) out[0] = '.'; else memcpy(out,path,n);
    out[n] = 0;
    return 1;
}
static int sync_directory_entry(const char *dir, int created)
{
    (void)created; /* Relevant to private fault adapters only. */
    char parent[512];
    if (!parent_name(dir,parent,sizeof(parent))) return 0;
    int fd=open(parent,O_RDONLY|O_CLOEXEC);
    if(fd<0) return 0;
    int ok=!FAIL(created?"mkdir_parent_sync":"existing_parent_sync",dir) && sync_fd(fd);
    int saved=errno;
    (void)close(fd); errno=saved;
    return ok;
}
#endif

int AtomicStorePrepareParent(const char *filepath)
{
    char path[512];
    if (!filepath) return 0;
    size_t n = strlen(filepath);
    if (!n || n >= sizeof(path)) return 0;
    memcpy(path, filepath, n + 1);
    for (char *p = path; *p; ++p)
        if (*p == '/' || *p == '\\') {
            char save = *p; *p = 0;
            if (*path) {
                if (MKDIR(path) == 0) {
#ifndef _WIN32
                    if (!sync_directory_entry(path,1)) { *p=save; return 0; }
#endif
                } else {
#ifdef _WIN32
                    DWORD attrs = GetFileAttributesA(path);
                    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) { *p=save; return 0; }
#else
                    struct stat st;
                    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) { *p=save; return 0; }
                    /* Also confirm existing ancestor entries: a prior failed
                       invocation may have created them without a successful sync. */
                    if (!sync_directory_entry(path,0)) { *p=save; return 0; }
#endif
                }
            }
            *p = save;
        }
    return 1;
}

ATOMIC_STORE_RESULT AtomicStoreReplaceDetailed(const char *path, const void *data, size_t len)
{
    if (!path || (!data && len) || !*path || strlen(path)>506 ||
        !AtomicStorePrepareParent(path)) return ATOMIC_STORE_UNCHANGED;
    char tmp[600];
#ifdef _WIN32
    static volatile LONG counter;
    HANDLE f=INVALID_HANDLE_VALUE;
    for (unsigned attempt=0;attempt<128;++attempt) {
        unsigned long id=(unsigned long)InterlockedIncrement(&counter);
        int n=snprintf(tmp,sizeof(tmp),"%s.tmp.%lu.%lu",path,(unsigned long)GetCurrentProcessId(),id);
        if(n<0 || (size_t)n>=sizeof(tmp) || FAIL("create",tmp)) return ATOMIC_STORE_UNCHANGED;
        f=CreateFileA(tmp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(f!=INVALID_HANDLE_VALUE) break;
        DWORD err=GetLastError();
        if(err!=ERROR_FILE_EXISTS && err!=ERROR_ALREADY_EXISTS) return ATOMIC_STORE_UNCHANGED;
    }
    if(f==INVALID_HANDLE_VALUE) return ATOMIC_STORE_UNCHANGED;
    int ok=!FAIL("temp_created",tmp);
    const unsigned char *p=(const unsigned char *)data;
    size_t left=len;
    if(FAIL("write",tmp)) ok=0;
    while(ok && left) {
        DWORD chunk=left>0x40000000u?0x40000000u:(DWORD)left,written=0;
        if(!WriteFile(f,p,chunk,&written,NULL) || !written) { ok=0; break; }
        p+=written;left-=written;
    }
    if(ok && (FAIL("file_sync",tmp) || !FlushFileBuffers(f))) ok=0;
    int close_fault=FAIL("file_close",tmp);
    if(!CloseHandle(f) || close_fault) ok=0;
    if(ok && !FAIL("rename",tmp) && MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        return ATOMIC_STORE_DURABLE;
    DWORD saved=GetLastError();
    (void)FAIL("cleanup",tmp); DeleteFileA(tmp);SetLastError(saved);
    return ATOMIC_STORE_UNCHANGED;
#else
    char parent[512];
    if(!parent_name(path,parent,sizeof(parent))) return ATOMIC_STORE_UNCHANGED;
    int dir=open(parent,O_RDONLY|O_CLOEXEC);
    if(dir<0) return ATOMIC_STORE_UNCHANGED;
    /* Preserve existing permission bits best effort, as separately reviewed.
       New files keep 0666 under the caller's umask. No ownership/ACL promise. */
    mode_t mode=0; int have_mode=0;
    int old=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(old>=0) {
        struct stat st;
        if(fstat(old,&st)==0 && S_ISREG(st.st_mode)) {mode=st.st_mode&07777;have_mode=1;}
        (void)close(old);
    }
    static atomic_ulong counter;
    int fd=-1;
    for(unsigned attempt=0;attempt<128;++attempt) {
        unsigned long id=atomic_fetch_add(&counter,1)+1;
        int n=snprintf(tmp,sizeof(tmp),"%s.tmp.%lu.%lu",path,(unsigned long)getpid(),id);
        if(n<0 || (size_t)n>=sizeof(tmp) || FAIL("create",tmp)) break;
        fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0666);
        if(fd>=0 || errno!=EEXIST) break;
    }
    if(fd<0) {int saved=errno;(void)close(dir);errno=saved;return ATOMIC_STORE_UNCHANGED;}
    int ok=!FAIL("temp_created",tmp);
    const unsigned char *p=(const unsigned char *)data; size_t left=len;
    if(FAIL("write",tmp)) ok=0;
    while(ok && left) {
        size_t chunk=left>0x40000000u?0x40000000u:left;
        ssize_t wrote=write(fd,p,chunk);
        if(wrote<0 && errno==EINTR) continue;
        if(wrote<=0) {ok=0;break;}
        p+=(size_t)wrote;left-=(size_t)wrote;
    }
    if(have_mode && !FAIL("chmod",tmp)) (void)fchmod(fd,mode);
    if(ok && (FAIL("file_sync",tmp) || !sync_fd(fd))) ok=0;
    int close_fault=FAIL("file_close",tmp);
    if(close(fd)!=0 || close_fault) ok=0; /* Never retry close after an error. */
    if(!ok || FAIL("rename",tmp) || rename(tmp,path)!=0) {
        int saved=errno;(void)FAIL("cleanup",tmp);(void)unlink(tmp);(void)close(dir);errno=saved;
        return ATOMIC_STORE_UNCHANGED;
    }
    int durable=!FAIL("parent_sync",path) && sync_fd(dir);
    int saved=errno;(void)close(dir);errno=saved;
    return durable?ATOMIC_STORE_DURABLE:ATOMIC_STORE_COMMITTED_DURABILITY_UNKNOWN;
#endif
}

int AtomicStoreReplace(const char *path, const void *data, size_t len)
{
    return AtomicStoreReplaceDetailed(path,data,len)==ATOMIC_STORE_DURABLE;
}
