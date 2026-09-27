#ifndef FS_REPLACE_H
#define FS_REPLACE_H
#include "fs_read.h"
/* POSIX cooperating-writer conditional replace. Mandatory exact old bytes,
   held-root inode revalidation, durable old/new images and intent, then rename
   publish. Recovery checks exact inode identity before rollback or commit
   cleanup; ambiguity fails closed. Advisory lock excludes only cooperating
   writers: an external writer can swap the name between check and rename.
   A self-contained commit marker permits marker-only recovery. A published but
   not durably committed replacement reports FS_READ_PENDING; call recovery,
   never blindly retry. External in-place writes to the same inode are excluded
   even if their bytes change while the lock is held. External actors that
   ignore or replace the lock may change a target between identity checks and
   rename. The guarantee applies only to cooperating writers. No universal
   power-loss guarantee or Windows write support. */
FS_READ_STATUS FsReplaceFile(const FS_READ_ROOT *root,const char *path,
 const void *expected,size_t expected_len,const void *replacement,size_t replacement_len);
FS_READ_STATUS FsReplaceRecover(const FS_READ_ROOT *root);
#endif
