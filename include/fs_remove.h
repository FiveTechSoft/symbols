#ifndef FS_REMOVE_H
#define FS_REMOVE_H
#include "fs_read.h"
/* POSIX cooperative removal with byte-exact expected source. Pending intent
   blocks writes until explicit recovery. Before a commit marker recovery
   restores the original inode; after commit it retains removal. A marker-only
   state fails closed for manual inspection. This is not atomic visibility to
   outside readers and power-loss behavior depends on filesystem durability.
   A non-cooperating external writer can replace a checked name before the
   following unlink; the workspace lock does not protect against that writer.
   The hard identity guarantee covers participating writers only. On Windows,
   NTFS process-crash replay uses a full marker and a pinned original FileId;
   source files with preexisting hard links are refused. A crash after the
   committed marker is removed but before pin deletion can leave an untracked
   pin in the journal area containing the bytes the caller believed removed.
   That pin is never automatically deleted; inspect and remove it manually.
   Directory flushing is diagnostic only, not a power-loss guarantee. */
FS_READ_STATUS FsRemoveFile(const FS_READ_ROOT *root,const char *path,
                            const void *expected,size_t expected_len);
FS_READ_STATUS FsRemoveRecover(const FS_READ_ROOT *root);
#endif
