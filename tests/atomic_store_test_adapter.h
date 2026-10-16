#ifndef ATOMIC_STORE_TEST_ADAPTER_H
#define ATOMIC_STORE_TEST_ADAPTER_H
/* Private test executable only: observe actual OS calls, not returned enums. */
#ifdef _WIN32
BOOL AtomicStoreTestCloseHandle(HANDLE);
#define CloseHandle AtomicStoreTestCloseHandle
BOOL AtomicStoreTestFlushFileBuffers(HANDLE);
BOOL AtomicStoreTestMoveFileExA(LPCSTR,LPCSTR,DWORD);
HANDLE AtomicStoreTestCreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
#define FlushFileBuffers AtomicStoreTestFlushFileBuffers
#define MoveFileExA AtomicStoreTestMoveFileExA
#define CreateFileA AtomicStoreTestCreateFileA
#else
int AtomicStoreTestClose(int);
#define close AtomicStoreTestClose
int AtomicStoreTestFsync(int);
int AtomicStoreTestRename(const char *,const char *);
int AtomicStoreTestOpen(const char *,int,...);
ssize_t AtomicStoreTestWrite(int,const void *,size_t);
#define fsync AtomicStoreTestFsync
#define rename AtomicStoreTestRename
#define open AtomicStoreTestOpen
#define write AtomicStoreTestWrite
#endif
#endif
