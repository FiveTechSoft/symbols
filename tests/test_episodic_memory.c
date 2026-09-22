/* test_episodic_memory.c: Unit tests for persistent continuous episodic memory.
   Verifies initialization, persistence, deduplication, reloading, clearance,
   selective forgetting, atomic save behavior, and persistence-failure
   propagation (a failed write must never be reported as learned). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "episodic_memory.h"

#ifdef _WIN32
#include <io.h>
#define UNLINK _unlink
#else
#include <unistd.h>
#include <sys/stat.h>
#define UNLINK unlink
#endif

int main(void)
{
    printf("=== RUNNING CONTINUOUS EPISODIC MEMORY TESTS ===\n\n");
    const char *test_path = "build-gcc/test_episodic_scratch.tsv";
    UNLINK(test_path);

    /* --- Test 1: Initialization --- */
    printf("--- Test 1: Store Initialization ---\n");
    EPISODIC_STORE store;
    int rc = EpisodicStoreInit(&store, test_path);
    assert(rc == 1);
    assert(EpisodicStoreCount(&store) == 0);
    assert(strcmp(store.filepath, test_path) == 0);
    printf("  [PASS] Store initialized with custom path\n");

    /* --- Test 2: Appending Triples & Deduplication --- */
    printf("\n--- Test 2: Appending Triples & Deduplication ---\n");
    rc = EpisodicStoreAppend(&store, "Socrates", "IS_A", "Philosopher", "test");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store) == 1);
    assert(EpisodicStoreExists(&store, "socrates", "is_a", "philosopher") == 1);
    printf("  [PASS] Appended first episodic fact\n");

    /* Attempt duplicate */
    rc = EpisodicStoreAppend(&store, "socrates", "IS_A", "philosopher", "test");
    assert(rc == 2); /* Already exists */
    assert(EpisodicStoreCount(&store) == 1);
    printf("  [PASS] Duplicate rejected idempotently\n");

    /* Append second and third facts */
    rc = EpisodicStoreAppend(&store, "Juan", "hermano_de", "Pedro", "user");
    assert(rc == 1);
    rc = EpisodicStoreAppend(&store, "Alicia", "esposa_de", "Pedro", "user");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store) == 3);
    printf("  [PASS] Added multi-domain relationships (count = 3)\n");

    /* --- Test 3: Disk Persistence & Reloading --- */
    printf("\n--- Test 3: Disk Persistence & Cross-Session Reloading ---\n");
    rc = EpisodicStoreSave(&store);
    assert(rc == 1);

    /* Create completely new store instance from same file */
    EPISODIC_STORE store2;
    EpisodicStoreInit(&store2, test_path);
    assert(EpisodicStoreCount(&store2) == 0);

    uint32_t loaded = EpisodicStoreLoad(&store2);
    assert(loaded == 3);
    assert(EpisodicStoreCount(&store2) == 3);
    assert(EpisodicStoreExists(&store2, "socrates", "is_a", "philosopher") == 1);
    assert(EpisodicStoreExists(&store2, "juan", "hermano_de", "pedro") == 1);
    assert(EpisodicStoreExists(&store2, "alicia", "esposa_de", "pedro") == 1);
    printf("  [PASS] Reloaded 3 facts across sessions with full fidelity\n");

    const EPISODIC_RECORD *rec0 = EpisodicStoreGet(&store2, 0);
    assert(rec0 != NULL);
    assert(strcmp(rec0->subject, "Socrates") == 0);
    assert(strcmp(rec0->relation, "IS_A") == 0);
    assert(strcmp(rec0->object, "Philosopher") == 0);
    assert(rec0->timestamp > 0);
    printf("  [PASS] Record attributes and timestamps verified\n");

    /* --- Test 4: Incremental Updates --- */
    printf("\n--- Test 4: Incremental Fact Accumulation ---\n");
    rc = EpisodicStoreAppend(&store2, "Plato", "disciple_of", "Socrates", "book");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store2) == 4);

    /* Reload into another store */
    EPISODIC_STORE store3;
    EpisodicStoreInit(&store3, test_path);
    loaded = EpisodicStoreLoad(&store3);
    assert(loaded == 4);
    assert(EpisodicStoreExists(&store3, "plato", "disciple_of", "socrates") == 1);
    printf("  [PASS] Incremental accumulation verified\n");

    /* --- Test 5: Store Clearance --- */
    printf("\n--- Test 5: Clearance & Reset ---\n");
    rc = EpisodicStoreClear(&store3);
    assert(rc == 1);
    assert(EpisodicStoreCount(&store3) == 0);

    /* Verify on-disk file was truncated */
    EPISODIC_STORE store4;
    EpisodicStoreInit(&store4, test_path);
    loaded = EpisodicStoreLoad(&store4);
    assert(loaded == 0);
    assert(EpisodicStoreCount(&store4) == 0);
    printf("  [PASS] Memory clear emptied both RAM and disk\n");

    /* --- Test 6: Selective Forgetting --- */
    printf("\n--- Test 6: Selective Forgetting ---\n");
    EPISODIC_STORE store5;
    EpisodicStoreInit(&store5, test_path);
    rc = EpisodicStoreAppend(&store5, "Karl", "hermano_de", "Franz", "user");
    assert(rc == 1);
    rc = EpisodicStoreAppend(&store5, "Karl", "esposa_de", "Marta", "user");
    assert(rc == 1);
    rc = EpisodicStoreAppend(&store5, "Irene", "hija_de", "Marta", "user");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store5) == 3);

    /* Forget one fact: gone from RAM and, after reload, from disk */
    rc = EpisodicStoreForget(&store5, "karl", "HERMANO_DE", "franz");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store5) == 2);
    assert(EpisodicStoreExists(&store5, "karl", "hermano_de", "franz") == 0);
    assert(EpisodicStoreExists(&store5, "karl", "esposa_de", "marta") == 1);

    EPISODIC_STORE store6;
    EpisodicStoreInit(&store6, test_path);
    loaded = EpisodicStoreLoad(&store6);
    assert(loaded == 2);
    assert(EpisodicStoreExists(&store6, "karl", "hermano_de", "franz") == 0);
    assert(EpisodicStoreExists(&store6, "karl", "esposa_de", "marta") == 1);
    assert(EpisodicStoreExists(&store6, "irene", "hija_de", "marta") == 1);
    printf("  [PASS] One fact forgotten; siblings survive on disk\n");

    /* Forgetting an unknown fact reports not-found, changes nothing */
    rc = EpisodicStoreForget(&store5, "karl", "hermano_de", "franz");
    assert(rc == 2);
    assert(EpisodicStoreCount(&store5) == 2);
    printf("  [PASS] Unknown fact forget reported as not-found\n");

    EpisodicStoreDestroy(&store5);
    EpisodicStoreDestroy(&store6);

#ifndef _WIN32
    /* --- Test 7: Persistence Failure Propagation & Atomicity (POSIX) --- */
    printf("\n--- Test 7: Persistence Failure Propagation & Atomicity ---\n");
    if (geteuid() == 0)
    {
        /* chmod 0555 does not block root: the failure path cannot be
           exercised, so report the skip instead of a false pass. */
        printf("  [SKIP] running as root; permission-based failure not enforceable\n");
    }
    else
    {
    const char *ro_dir = "build-gcc/test_episodic_ro";
    const char *ro_path = "build-gcc/test_episodic_ro/episodic.tsv";
    mkdir("build-gcc/test_episodic_ro", 0755);

    EPISODIC_STORE store7;
    EpisodicStoreInit(&store7, ro_path);
    rc = EpisodicStoreAppend(&store7, "Marie", "esposa_de", "Pierre", "user");
    assert(rc == 1);

    /* Snapshot the good on-disk image */
    char before[4096];
    size_t before_len = 0;
    FILE *snap = fopen(ro_path, "rb");
    assert(snap != NULL);
    before_len = fread(before, 1, sizeof(before), snap);
    fclose(snap);
    assert(before_len > 0);

    /* Make the directory unwritable: every later write must fail honestly */
    assert(chmod(ro_dir, 0555) == 0);

    /* Append reports failure and rolls the record back */
    rc = EpisodicStoreAppend(&store7, "Pierre", "hermano_de", "Jacques", "user");
    assert(rc == 0);
    assert(EpisodicStoreCount(&store7) == 1);
    printf("  [PASS] Failed save reported; append rolled back\n");

    /* Forget reports failure and keeps the record */
    rc = EpisodicStoreForget(&store7, "marie", "esposa_de", "pierre");
    assert(rc == 0);
    assert(EpisodicStoreExists(&store7, "marie", "esposa_de", "pierre") == 1);
    printf("  [PASS] Failed forget reported; record restored\n");

    /* Clear reports failure and keeps the records */
    rc = EpisodicStoreClear(&store7);
    assert(rc == 0);
    assert(EpisodicStoreCount(&store7) == 1);
    printf("  [PASS] Failed clear reported; store untouched\n");

    /* The previous on-disk image is byte-identical: no truncation, no
       half-written store, no temporary file left behind */
    char after[4096];
    size_t after_len = 0;
    snap = fopen(ro_path, "rb");
    assert(snap != NULL);
    after_len = fread(after, 1, sizeof(after), snap);
    fclose(snap);
    assert(after_len == before_len);
    assert(memcmp(before, after, before_len) == 0);
    assert(access("build-gcc/test_episodic_ro/episodic.tsv.tmp", F_OK) != 0);
    printf("  [PASS] Failed writes left the store byte-identical (atomic)\n");

    /* Restore writability: the store works again */
    assert(chmod(ro_dir, 0755) == 0);
    rc = EpisodicStoreAppend(&store7, "Pierre", "hermano_de", "Jacques", "user");
    assert(rc == 1);
    assert(EpisodicStoreCount(&store7) == 2);
    printf("  [PASS] Store recovers once the path is writable again\n");

    EpisodicStoreDestroy(&store7);
    }
#endif

    /* Clean up stores and scratch file */
    EpisodicStoreDestroy(&store);
    EpisodicStoreDestroy(&store2);
    EpisodicStoreDestroy(&store3);
    EpisodicStoreDestroy(&store4);
    UNLINK(test_path);

    printf("\n=======================================================\n");
    printf("EPISODIC MEMORY SUMMARY: ALL TESTS PASSED\n");
    printf("=======================================================\n");
    return 0;
}
