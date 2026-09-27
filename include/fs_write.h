#ifndef FS_WRITE_H
#define FS_WRITE_H
#include "fs_read.h"
/* Create a new regular file only; never replace an existing name. The caller
   owns the bytes. On POSIX, cooperating writers hold a workspace lock and
   use a durable, single-intent create protocol. Success means the target name
   was published; if a post-publication flush or cleanup failed, an intent may
   remain and the caller must explicitly invoke FsCreateRecover. New creates
   refuse a pending intent. Recovery refuses ambiguous identities and never
   replaces or removes an unrelated target. External writers ignoring or
   replacing the advisory lock are outside scope. This does not offer
   multi-file atomicity or a durable replace primitive.
   A crash before intent publication or during final cleanup may leave
   untracked random stage/record files; they cannot be safely removed by
   automatic replay. Crash-point tests model process termination; actual
   power-loss persistence remains filesystem-dependent.
   Windows create-only is deliberately NOT atomic or journaled: FILE_CREATE
   publishes the name before content is written. After publication, a short
   write or flush failure returns FS_READ_PENDING; a partial file may remain.
   Inspect and clean it manually, never blindly retry. FsCreateRecover is
   unsupported on Windows. A future journal layer owns atomicity. */
FS_READ_STATUS FsCreateRecover(const FS_READ_ROOT *root);
/* Copy a byte-exact expected source to a new destination, no replace. The
   expected pointer is mandatory even for zero bytes. POSIX only; participates
   in the workspace lock and create intent recovery. */
FS_READ_STATUS FsCopyFile(const FS_READ_ROOT *root,const char *source,
                          const char *target,const void *expected,size_t expected_len);
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *root,const char *relative,
                            const void *bytes,size_t len,unsigned mode);
#endif
