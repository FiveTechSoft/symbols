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

/* Append a new record. If auto_save is non-zero, immediately flushes to disk. Returns 1 on success, 0 on failure/dup */
int EpisodicStoreAppend(EPISODIC_STORE *store, const char *subject, const char *relation, const char *object, const char *source);

/* Save all records to disk */
int EpisodicStoreSave(const EPISODIC_STORE *store);

/* Clear all records in memory and truncate on-disk file */
int EpisodicStoreClear(EPISODIC_STORE *store);

/* Get number of records */
uint32_t EpisodicStoreCount(const EPISODIC_STORE *store);

/* Get record by index */
const EPISODIC_RECORD *EpisodicStoreGet(const EPISODIC_STORE *store, uint32_t idx);

#endif /* EPISODIC_MEMORY_H */
