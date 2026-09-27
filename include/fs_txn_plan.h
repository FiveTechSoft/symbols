#ifndef FS_TXN_PLAN_H
#define FS_TXN_PLAN_H
#include "fs_manifest.h"
/* Read-only transaction planning. The returned journal image is in memory,
   not durable and NOT permission to execute. All participating writers must
   acquire one workspace lock, revalidate byte-exact expected images while
   holding it, durably journal old images and effects, then publish/rollback.
   The existing create primitive is not yet a participant and must be brought
   under the same lock before this protocol can claim cooperative isolation.
   An outside process ignoring the lock may still change the namespace.
   No transaction lock, durable journal, recovery, or executable transition
   is implemented here. This is only a snapshot/preflight data contract. */
typedef enum { FS_TXN_PLANNED=1, FS_TXN_JOURNALED, FS_TXN_APPLYING,
               FS_TXN_COMMITTED, FS_TXN_ROLLING_BACK, FS_TXN_RESTORED } FS_TXN_STATE;
typedef struct {
    FS_OP_REQUEST operation;
    /* Existing images are mandatory, including a non-NULL pointer for zero
       bytes. The planner checks byte equality, not inode/version identity. */
    const void *expected_source;size_t expected_source_len;
    const void *expected_target;size_t expected_target_len;
} FS_TXN_REQUEST;
typedef struct {
    char *path;unsigned char *bytes;size_t len;unsigned mode;
    int existed;
} FS_TXN_IMAGE;
typedef struct {
    FS_TXN_STATE state;
    FS_MANIFEST effects;
    FS_TXN_IMAGE *before;size_t count;
} FS_TXN_PLAN;
FS_READ_STATUS FsTxnPlan(const FS_READ_ROOT *root,const FS_TXN_REQUEST *requests,
                          size_t count,FS_TXN_PLAN *out);
void FsTxnFree(FS_TXN_PLAN *plan);
#endif
