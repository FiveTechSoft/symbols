#ifdef NDEBUG
#undef NDEBUG
#endif
#include "atomic_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define UNLINK _unlink
#else
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#define UNLINK unlink
#endif
static void image(const char *path,char *buf,size_t n)
{
    FILE *f=fopen(path,"rb");assert(f);assert(fread(buf,1,n,f)==n);assert(fgetc(f)==EOF);fclose(f);
}
int main(void)
{
    const char *path="build/atomic-store-test/subdir/store";
    assert(AtomicStoreReplace(path,"original",8));
    char b[8];image(path,b,8);assert(!memcmp(b,"original",8));
    assert(AtomicStoreReplace(path,"updated!",8));image(path,b,8);assert(!memcmp(b,"updated!",8));
#ifdef _WIN32
    assert(!AtomicStoreReplace("build/atomic-store-test/subdir/store/child","x",1));
    /* Windows MoveFileExA must overwrite an existing file. */
    assert(AtomicStoreReplace(path,"again!!!",8));image(path,b,8);assert(!memcmp(b,"again!!!",8));
#else
    assert(!AtomicStoreReplace("build/atomic-store-test/subdir/store/child","x",1));
    /* Blocking the temporary file forces failure before rename. */
    assert(mkdir("build/atomic-store-test/subdir/store.tmp",0700)==0);
    assert(!AtomicStoreReplace(path,"bad-data",8));
    image(path,b,8);assert(!memcmp(b,"updated!",8));
    assert(rmdir("build/atomic-store-test/subdir/store.tmp")==0);
    /* /dev/full accepts a buffered write, then rejects flush: target survives. */
    if (access("/dev/full", F_OK)==0) {
        assert(symlink("/dev/full", "build/atomic-store-test/subdir/store.tmp")==0);
        assert(!AtomicStoreReplace(path,"bad-data",8));
        image(path,b,8);assert(!memcmp(b,"updated!",8));
        assert(access("build/atomic-store-test/subdir/store.tmp",F_OK)!=0);
    }
    /* A directory target cannot be replaced, original still exists. */
    assert(mkdir("build/atomic-store-test/target",0700)==0);
    assert(!AtomicStoreReplace("build/atomic-store-test/target","bad",3));
    assert(rmdir("build/atomic-store-test/target")==0);
#endif
    assert(!AtomicStoreReplace("", "x",1));
    UNLINK(path);puts("atomic-store cross-platform replacement checks passed");return 0;
}
