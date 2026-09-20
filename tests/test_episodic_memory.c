/* test_episodic_memory.c: Unit tests for persistent continuous episodic memory.
   Verifies initialization, persistence, deduplication, reloading, and clearance. */

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
