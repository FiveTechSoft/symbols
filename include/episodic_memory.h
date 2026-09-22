#ifndef EPISODIC_MEMORY_H
#define EPISODIC_MEMORY_H

#include <stdint.h>
#include <stddef.h>

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

/* Append a new record. If auto_save is non-zero, immediately persists to
   disk through the atomic save; a persistence failure rolls the in-memory
   append back and reports failure, so RAM never claims more than disk.
   Returns 1 on success, 2 when the triple already exists, 0 on failure. */
int EpisodicStoreAppend(EPISODIC_STORE *store, const char *subject, const char *relation, const char *object, const char *source);

/* Forget one triple: removes every matching record (same case-insensitive
   match as EpisodicStoreExists) and persists the reduced store atomically.
   A persistence failure restores the in-memory records, so a reported
   forget always means forgotten on disk too.
   Returns 1 on success, 2 when the triple was not present, 0 on failure. */
int EpisodicStoreForget(EPISODIC_STORE *store, const char *subject, const char *relation, const char *object);

/* Save all records to disk atomically: the full store is written to a
   sibling temporary file and renamed over the target, so a crash or I/O
   error can never leave a truncated store behind. Any write, flush,
   close or rename failure returns 0 and leaves the previous file
   untouched. */
int EpisodicStoreSave(const EPISODIC_STORE *store);

/* Clear all records: the empty store is persisted atomically first and
   in-memory records are dropped only after the write is confirmed.
   Returns 1 on success, 0 on persistence failure (nothing changed). */
int EpisodicStoreClear(EPISODIC_STORE *store);

/* Get number of records */
uint32_t EpisodicStoreCount(const EPISODIC_STORE *store);

/* Get record by index */
const EPISODIC_RECORD *EpisodicStoreGet(const EPISODIC_STORE *store, uint32_t idx);

#endif /* EPISODIC_MEMORY_H */
