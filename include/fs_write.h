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
   power-loss persistence remains filesystem-dependent. */
FS_READ_STATUS FsCreateRecover(const FS_READ_ROOT *root);
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *root,const char *relative,
                            const void *bytes,size_t len,unsigned mode);
#endif
