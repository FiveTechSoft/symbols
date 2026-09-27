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
#endif
