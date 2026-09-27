#ifndef FS_MANIFEST_H
#define FS_MANIFEST_H
#include "fs_read.h"
/* A dry-run plan only. It neither reserves paths nor authorizes a later write.
   Revalidate against held handles when an eventual mutation is attempted. */
typedef enum { FS_OP_CREATE=1, FS_OP_REPLACE, FS_OP_MOVE, FS_OP_COPY, FS_OP_REMOVE } FS_OP_KIND;
typedef enum { FS_EFFECT_READ=1, FS_EFFECT_WRITE, FS_EFFECT_RENAME, FS_EFFECT_DELETE } FS_EFFECT_KIND;
typedef struct { FS_OP_KIND kind; const char *source; const char *target; } FS_OP_REQUEST;
typedef struct { FS_EFFECT_KIND kind; char *path; char *other; } FS_EFFECT;
typedef struct { FS_EFFECT *items; size_t count; } FS_MANIFEST;
FS_READ_STATUS FsManifestPlan(const FS_READ_ROOT *root,const FS_OP_REQUEST *requests,
                              size_t request_count,FS_MANIFEST *out);
void FsManifestFree(FS_MANIFEST *manifest);
#endif
