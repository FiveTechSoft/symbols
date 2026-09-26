#ifndef ATOMIC_STORE_H
#define ATOMIC_STORE_H
#include <stddef.h>
/* Replace one complete image. Returns 0 without replacing the target on an
   ordinary write/flush/close/rename failure. Not a crash-durability guarantee. */
int AtomicStorePrepareParent(const char *path);
int AtomicStoreReplace(const char *path, const void *data, size_t len);
#endif
