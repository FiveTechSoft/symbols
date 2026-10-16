#ifndef ATOMIC_STORE_H
#define ATOMIC_STORE_H
#include <stddef.h>
typedef enum {
    ATOMIC_STORE_UNCHANGED = 0,
    ATOMIC_STORE_COMMITTED_DURABILITY_UNKNOWN = 1,
    ATOMIC_STORE_DURABLE = 2
} ATOMIC_STORE_RESULT;
/* Prepare directories. POSIX synchronizes traversed directory entries, including existing ones
   so retries after a failed creation do not assume ancestor durability.
   A failure may leave created directories. Requires a stable trusted namespace. */
int AtomicStorePrepareParent(const char *path);
/* Replace one complete image using an exclusive per-operation sibling temp.
   UNCHANGED means this invocation did not publish (other writers may change it).
   COMMITTED_DURABILITY_UNKNOWN means the new image is visible but the POSIX
   parent fsync failed: never infer rollback. DURABLE means file+parent fsync on
   POSIX, or FlushFileBuffers+MoveFileEx WRITE_THROUGH on Windows, succeeded.
   This is an OS/filesystem API result, not proof against every physical failure.
   Unpublished temp cleanup is best effort; crashes can leave orphan temps.
   POSIX new mode is 0666 under umask; existing mode copied best effort using
   fstat/fchmod. Ownership/ACLs are not preserved. No whole-cycle writer lock. */
ATOMIC_STORE_RESULT AtomicStoreReplaceDetailed(const char *path, const void *data, size_t len);
/* Compatibility wrapper: 1 only for DURABLE. A 0 after successful publication
   may have changed disk; it is NOT a promise that the old target is intact. */
int AtomicStoreReplace(const char *path, const void *data, size_t len);
#endif
