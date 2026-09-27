#ifndef FS_WRITE_H
#define FS_WRITE_H
#include "fs_read.h"
/* Create a new regular file only; never replace an existing name. The caller
   owns the bytes. A successful publish is reported as success even if a
   best-effort directory flush fails. On POSIX, cooperating writers use the
   workspace lock. External processes ignoring or replacing that advisory lock
   are outside scope. No crash-durability or multi-file atomicity guarantee. */
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *root,const char *relative,
                            const void *bytes,size_t len,unsigned mode);
#endif
