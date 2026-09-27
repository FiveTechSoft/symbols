#ifndef FS_MOVE_H
#define FS_MOVE_H
#include "fs_read.h"
/* POSIX regular-file move, byte-exact expected source and no-replace target.
   It stages the original inode and uses a durable intent plus commit marker.
   Recovery rolls back before commit or retains the target after commit;
   foreign names and marker-only state fail closed. Cooperative process-crash
   safety, not atomic two-name visibility or a universal power-loss guarantee.
   A non-cooperating external writer can replace a checked name before the
   following unlink; the workspace lock does not protect against that writer.
   The hard identity guarantee covers participating writers only. On Windows,
   NTFS process-crash replay uses a full marker and a pinned original FileId;
   source files with preexisting hard links are refused. A crash after marker
   retirement but before pin deletion can leave an untracked pin, never
   automatically deleted. Directory flushing is diagnostic only, not a
   power-loss guarantee. */
FS_READ_STATUS FsMoveFile(const FS_READ_ROOT *root,const char *source,
                          const char *target,const void *expected,size_t expected_len);
FS_READ_STATUS FsMoveRecover(const FS_READ_ROOT *root);
#endif
