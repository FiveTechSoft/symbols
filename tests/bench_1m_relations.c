/* bench_1m_relations.c - 1M relations hash benchmark (M3 C5 contract).
   Emits per-block SAMPLE lines for tools/bench_runner.py:
     SAMPLE insert <ns>   (one per block of inserts)
     SAMPLE query  <ns>   (one per block of queries)
   then BENCH_DONE and exit 0. Human lines are informational only.
   Memory high-water is measured externally by the runner; this program
   prints no memory figure. Usage: bench_1m [num_relations] [num_queries]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static double GetTimeSeconds(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
static double GetTimeSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

#include "symbol.h"
#include "relation.h"

#define DEFAULT_RELATIONS 1000000u
#define DEFAULT_QUERIES   100000u
#define BENCH_SEED_INSERT 42u
#define BENCH_SEED_QUERY  99999u
#define NUM_BLOCKS        100u

static long long SecondsToNs(double seconds)
{
    return (long long)(seconds * 1e9);
}

int main(int argc, char **argv)
{
    uint32_t num_relations = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : DEFAULT_RELATIONS;
    uint32_t num_queries   = (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 10) : DEFAULT_QUERIES;
    uint32_t insert_block  = num_relations / NUM_BLOCKS;
    uint32_t query_block   = num_queries / NUM_BLOCKS;

    if (num_relations < NUM_BLOCKS || num_queries < NUM_BLOCKS)
    {
        fprintf(stderr, "sizes below %u blocks are not measurable\n", NUM_BLOCKS);
        return EXIT_FAILURE;
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== bench_1m_relations ===\n");
    printf("config: relations=%u queries=%u seed_insert=%u seed_query=%u blocks=%u\n",
           num_relations, num_queries, BENCH_SEED_INSERT, BENCH_SEED_QUERY, NUM_BLOCKS);

    /* 1. Symbol table setup (not part of the measured phases). */
    SYMBOL_TABLE *symbols = SymbolTableCreate(4096);
    for (uint32_t i = 0; i < 10000; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "SYM_%05u", i);
        SymbolAdd(symbols, name);
    }
    printf("symbols: %u\n", SymbolCount(symbols));

    RELATION_TABLE *relations = RelationTableCreate(num_relations + 1000);
    if (relations == NULL)
    {
        fprintf(stderr, "could not create relation table\n");
        return EXIT_FAILURE;
    }

    /* 2. Insert phase: one SAMPLE per block. */
    srand(BENCH_SEED_INSERT);
    double t0 = GetTimeSeconds();
    for (uint32_t i = 0; i < num_relations; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;
        (void)RelationAdd(relations, s, p, o);
        if ((i + 1) % insert_block == 0)
        {
            double t1 = GetTimeSeconds();
            printf("SAMPLE insert %lld\n", SecondsToNs(t1 - t0));
            t0 = t1;
        }
    }

    /* Counts are honest: attempts vs unique edges actually in the table. */
    uint32_t unique_count = RelationCount(relations);
    printf("insert: attempts=%u unique=%u duplicates=%u\n",
           num_relations, unique_count, num_relations - unique_count);

    /* 3. Query phase: one SAMPLE per block. */
    srand(BENCH_SEED_QUERY);
    uint32_t found_count = 0;
    t0 = GetTimeSeconds();
    for (uint32_t i = 0; i < num_queries; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;
        RELATION *r = RelationFind(relations, s, p, o);
        if (r != NULL)
            found_count++;
        if ((i + 1) % query_block == 0)
        {
            double t1 = GetTimeSeconds();
            printf("SAMPLE query %lld\n", SecondsToNs(t1 - t0));
            t0 = t1;
        }
    }
    printf("query: found=%u of %u\n", found_count, num_queries);

    /* 4. Integrity check on a fixed sample. */
    srand(BENCH_SEED_INSERT);
    int errors = 0;
    for (uint32_t i = 0; i < 1000; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;
        RELATION *r = RelationFind(relations, s, p, o);
        if (r != NULL && (r->subject != s || r->relation != p || r->object != o))
            errors++;
    }
    printf("verify: samples=1000 errors=%d\n", errors);
    if (errors != 0)
    {
        RelationTableDestroy(relations);
        SymbolTableDestroy(symbols);
        return EXIT_FAILURE;
    }

    /* 5. Summary while the tables are still alive (the previous harness
       printed RelationCount after RelationTableDestroy: use-after-free). */
    printf("summary: unique=%u attempts=%u found=%u\n",
           unique_count, num_relations, found_count);

    RelationTableDestroy(relations);
    SymbolTableDestroy(symbols);

    printf("BENCH_DONE\n");
    return EXIT_SUCCESS;
}
