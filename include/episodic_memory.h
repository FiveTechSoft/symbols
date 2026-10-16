#ifndef EPISODIC_MEMORY_H
#define EPISODIC_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "atomic_store.h"

#define EPISODIC_STR_MAX 64
#define EPISODIC_INITIAL_CAP 256

typedef struct {
    char subject[EPISODIC_STR_MAX];
    char relation[EPISODIC_STR_MAX];
    char object[EPISODIC_STR_MAX];
    char source[EPISODIC_STR_MAX];
    uint64_t timestamp;
} EPISODIC_RECORD;

typedef struct {
    char filepath[512];
    EPISODIC_RECORD *records;
    uint32_t count;
    uint32_t capacity;
    int auto_save; /* 1 = automatically save on append */
} EPISODIC_STORE;

/* Initialize store with target file path (e.g. data/memory/episodic.tsv) */
int EpisodicStoreInit(EPISODIC_STORE *store, const char *filepath);

/* Free dynamic resources */
void EpisodicStoreDestroy(EPISODIC_STORE *store);

/* Load records from disk (deduplicating). Returns count of newly loaded records. */
uint32_t EpisodicStoreLoad(EPISODIC_STORE *store);

/* Check if a triple already exists in episodic memory */
int EpisodicStoreExists(const EPISODIC_STORE *store, const char *subject, const char *relation, const char *object);

/* Legacy operation results: append/forget 1 success, 2 duplicate/absent,
   0 failure; save/clear 1 success, 0 failure. A post-publication durability
   failure can change disk and RAM while returning 0. Only UNCHANGED failures
   roll RAM back. auto_save=0 append/forget succeed without a disk operation. */
int EpisodicStoreAppend(EPISODIC_STORE *, const char *, const char *, const char *, const char *);
int EpisodicStoreForget(EPISODIC_STORE *, const char *, const char *, const char *);
int EpisodicStoreSave(const EPISODIC_STORE *);
int EpisodicStoreClear(EPISODIC_STORE *);
/* Ex retains legacy return codes and initializes optional status on every path.
   No publication attempted (invalid/no-op/auto_save=0) yields UNCHANGED even
   when the operation succeeds in RAM or returns duplicate/absent. */
int EpisodicStoreAppendEx(EPISODIC_STORE *, const char *, const char *, const char *, const char *, ATOMIC_STORE_RESULT *);
int EpisodicStoreForgetEx(EPISODIC_STORE *, const char *, const char *, const char *, ATOMIC_STORE_RESULT *);
int EpisodicStoreSaveEx(const EPISODIC_STORE *, ATOMIC_STORE_RESULT *);
int EpisodicStoreClearEx(EPISODIC_STORE *, ATOMIC_STORE_RESULT *);

/* Get number of records */
uint32_t EpisodicStoreCount(const EPISODIC_STORE *store);

/* Get record by index */
const EPISODIC_RECORD *EpisodicStoreGet(const EPISODIC_STORE *store, uint32_t idx);

#endif /* EPISODIC_MEMORY_H */
