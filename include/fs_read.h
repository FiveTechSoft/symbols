#ifndef FS_READ_H
#define FS_READ_H
#include <stddef.h>
#include <stdint.h>

/* Read-only workspace capability. POSIX children are opened relative to a
   held directory with no-follow semantics. Windows child paths are opened component-wise relative to held directory
   handles through NtCreateFile; missing API support fails closed. */
typedef struct FS_READ_ROOT FS_READ_ROOT;
typedef enum { FS_READ_OK=0, FS_READ_INVALID, FS_READ_MISSING,
               FS_READ_DENIED, FS_READ_UNSUPPORTED, FS_READ_IO, FS_READ_PENDING } FS_READ_STATUS;
typedef enum { FS_KIND_FILE=1, FS_KIND_DIR=2 } FS_READ_KIND;
typedef enum { FS_NEWLINE_NONE=0, FS_NEWLINE_LF, FS_NEWLINE_CRLF,
               FS_NEWLINE_MIXED } FS_READ_NEWLINE;
typedef struct {
    FS_READ_KIND kind;
    uint64_t size;
    unsigned mode; /* POSIX permission bits, or Windows readonly 0444/0644 */
    int binary; /* -1 until read, otherwise NUL-byte detection */
    FS_READ_NEWLINE newline; /* known only after read */
} FS_READ_META;
typedef struct { char *name; FS_READ_KIND kind; } FS_READ_ENTRY;

FS_READ_STATUS FsReadOpen(const char *root, FS_READ_ROOT **out);
void FsReadClose(FS_READ_ROOT *root);
FS_READ_STATUS FsReadStat(const FS_READ_ROOT *root, const char *relative,
                          FS_READ_META *meta);
/* At most 1 MiB. Returns owned bytes (not a C string); free with free(). */
FS_READ_STATUS FsReadFile(const FS_READ_ROOT *root, const char *relative,
                          unsigned char **bytes, size_t *len, FS_READ_META *meta);
/* Direct children, sorted by name. Free with FsReadFreeList. */
FS_READ_STATUS FsReadList(const FS_READ_ROOT *root, const char *relative,
                          FS_READ_ENTRY **entries, size_t *count);
void FsReadFreeList(FS_READ_ENTRY *entries, size_t count);
#endif
