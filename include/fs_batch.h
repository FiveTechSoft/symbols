#ifndef FS_BATCH_H
#define FS_BATCH_H
#include "fs_read.h"
/* Bounded create-new batch; all target names are absent at the locked check.
   Fail-closed on unrelated target or malformed journal. A pending batch blocks
   new creates until explicit recovery. Before a synced commit marker, recovery
   rolls back exact matching published targets. With a marker it keeps all
   targets. This is cooperative POSIX process-crash recovery, not atomic
   visibility to readers, multi-file replace, or a cross-filesystem/power-loss
   guarantee. An orphan stage before journal publication may remain. A crash
   between journal and marker retirement leaves a marker-only state that blocks
   writes pending manual inspection; automatic recovery cannot verify targets
   without the journal. Windows writes fail closed. */
typedef struct { const char *target; const void *bytes; size_t len; unsigned mode; } FS_BATCH_CREATE;
FS_READ_STATUS FsBatchCreate(const FS_READ_ROOT *root,const FS_BATCH_CREATE *entries,size_t count);
FS_READ_STATUS FsBatchRecover(const FS_READ_ROOT *root);
/* Bounded multi-file replace (2..8 distinct existing regular files, single
   hard link each, byte-exact expected image per file). All expected images are
   verified and all new images are durably staged before any target changes. A
   durable journal pins every old inode; publication is a rename per target; a
   synced commit marker ends rollback. A crash or in-process failure before the
   marker restores the original bytes of every target (FsBatchRecover, or
   immediately in-process); after the marker every target keeps its new bytes.
   Uses the create batch journal name, so any pending batch (create or replace)
   blocks all other writers until FsBatchRecover. A crash between journal
   retirement and marker retirement leaves a marker-only state that fails closed
   for manual inspection, as for create. Cooperating writers only (workspace
   lock); not atomic visibility to readers, not a power-loss guarantee, not a
   protection against an outside process that ignores the lock. Inodes change
   on replace; permission bits are kept. Windows fails closed
   (FS_READ_UNSUPPORTED); there is no Windows multi-file replace. */
typedef struct { const char *target; const void *expected; size_t expected_len;
                 const void *replacement; size_t replacement_len; } FS_BATCH_REPLACE;
FS_READ_STATUS FsBatchReplace(const FS_READ_ROOT *root,const FS_BATCH_REPLACE *entries,size_t count);
#endif
