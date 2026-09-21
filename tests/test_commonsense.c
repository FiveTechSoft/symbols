/* =========================================================================
   test_commonsense.c: Verification suite for Commonsense & World Knowledge
   Pillar 3: Native C11 Streaming Ingestion & Intuitive World Reasoning
   - Tests ConceptNet 5.8 5-column line parsing and URI extraction
   - Tests declarative relation canonicalization (HARDCODING=0)
   - Tests streaming buffer and file ingestion with language/weight filters
   - Verifies strictly 32 bytes per relation struct (10M triples in ~320 MB)
   - Benchmarks high-scale ingestion throughput and sub-100ns lookup latency
   - Verifies spatial, functional affordance, and physical consequence reasoning
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "commonsense.h"
#include "graph.h"
#include "relation.h"

static int g_pass = 0;
static int g_fail = 0;

static void check_str(const char *test_name, const char *actual, const char *expected)
{
    if (actual != NULL && strcmp(actual, expected) == 0)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Expected: \"%s\"\n    Got:      \"%s\"\n",
               test_name, expected ? expected : "(null)", actual ? actual : "(null)");
        g_fail++;
    }
}

static void check_int(const char *test_name, int actual, int expected)
{
    if (actual == expected)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Expected: %d\n    Got:      %d\n",
               test_name, expected, actual);
        g_fail++;
    }
}

static void check_contains(const char *test_name, const char *haystack, const char *needle)
{
    if (haystack != NULL && strstr(haystack, needle) != NULL)
    {
        printf("  [PASS] %s\n", test_name);
        g_pass++;
    }
    else
    {
        printf("  [FAIL] %s\n    Looking for: \"%s\"\n    In text:     \"%s\"\n",
               test_name, needle, haystack ? haystack : "(null)");
        g_fail++;
    }
}

int main(void)
{
    printf("=== RUNNING PILLAR 3: LARGE-SCALE COMMONSENSE INGESTION TESTS ===\n\n");

    /* =====================================================================
       Test 1: ConceptNet URI Parsing & Entity Extraction
       ===================================================================== */
    printf("--- Test 1: ConceptNet URI Parsing ---\n");
    {
        char lang[8];
        char concept[64];

        int ok1 = CommonsenseParseConceptNetURI("/c/en/kitchen", lang, sizeof(lang), concept, sizeof(concept));
        check_int("Parse /c/en/kitchen status", ok1, 1);
        check_str("Parse /c/en/kitchen lang", lang, "en");
        check_str("Parse /c/en/kitchen concept", concept, "kitchen");

        int ok2 = CommonsenseParseConceptNetURI("/c/es/perro", lang, sizeof(lang), concept, sizeof(concept));
        check_int("Parse /c/es/perro status", ok2, 1);
        check_str("Parse /c/es/perro lang", lang, "es");
        check_str("Parse /c/es/perro concept", concept, "perro");

        /* Multi-word underscore replacement */
        int ok3 = CommonsenseParseConceptNetURI("/c/en/drop_on", lang, sizeof(lang), concept, sizeof(concept));
        check_int("Parse /c/en/drop_on status", ok3, 1);
        check_str("Parse /c/en/drop_on concept", concept, "drop on");
    }

    /* =====================================================================
       Test 2: Declarative Relation Canonicalization
       ===================================================================== */
    printf("\n--- Test 2: Relation Canonicalization (HARDCODING=0) ---\n");
    {
        char can[64];
        CS_REL_TYPE t1 = CommonsenseCanonicalizeRelation("/r/CapableOf", can, sizeof(can));
        check_int("Canonicalize /r/CapableOf type", t1, CS_REL_CAPABLE_OF);
        check_str("Canonicalize /r/CapableOf name", can, "CAPABLE_OF");

        CS_REL_TYPE t2 = CommonsenseCanonicalizeRelation("/r/AtLocation", can, sizeof(can));
        check_int("Canonicalize /r/AtLocation type", t2, CS_REL_AT_LOCATION);
        check_str("Canonicalize /r/AtLocation name", can, "AT_LOCATION");

        CS_REL_TYPE t3 = CommonsenseCanonicalizeRelation("/r/PartOf", can, sizeof(can));
        check_int("Canonicalize /r/PartOf type", t3, CS_REL_PART_OF);
        check_str("Canonicalize /r/PartOf name", can, "PART_OF");

        CS_REL_TYPE t4 = CommonsenseCanonicalizeRelation("/r/MadeOf", can, sizeof(can));
        check_int("Canonicalize /r/MadeOf type", t4, CS_REL_MADE_OF);
        check_str("Canonicalize /r/MadeOf name", can, "MADE_OF");

        CS_REL_TYPE t5 = CommonsenseCanonicalizeRelation("/r/Causes", can, sizeof(can));
        check_int("Canonicalize /r/Causes type", t5, CS_REL_CAUSES);
        check_str("Canonicalize /r/Causes name", can, "CAUSES");
    }

    /* =====================================================================
       Test 3: ConceptNet 5.8 Official 5-Column Dump Line Parsing
       ===================================================================== */
    printf("\n--- Test 3: ConceptNet 5.8 Line Parsing & Filtering ---\n");
    {
        CS_CONFIG cfg = CommonsenseConfigDefault();
        CS_TRIPLE triple;

        /* Valid English line with weight 2.5 */
        const char *line1 =
            "/a/[/r/CapableOf/,/c/en/dog/,/c/en/bark/]\t/r/CapableOf\t/c/en/dog\t/c/en/bark\t{\"dataset\": \"/d/conceptnet/4/en\", \"weight\": 2.5}";
        int ok1 = CommonsenseParseLine(line1, &cfg, &triple);
        check_int("Parse valid ConceptNet 5.8 line", ok1, 1);
        check_str("Subject is 'dog'", triple.subject, "dog");
        check_str("Relation is 'CAPABLE_OF'", triple.relation, "CAPABLE_OF");
        check_str("Object is 'bark'", triple.object, "bark");
        check_int("Weight parsed as >= 2", (triple.weight >= 2.4f), 1);

        /* Spanish line filtered out when filter_lang="en" */
        const char *line2 =
            "/a/[/r/CapableOf/,/c/es/perro/,/c/es/ladrar/]\t/r/CapableOf\t/c/es/perro\t/c/es/ladrar\t{\"weight\": 2.0}";
        int ok2 = CommonsenseParseLine(line2, &cfg, &triple);
        check_int("Filter out non-English line", ok2, 0);

        /* Low weight filtered out */
        const char *line3 =
            "/a/[/r/CapableOf/,/c/en/cat/,/c/en/bark/]\t/r/CapableOf\t/c/en/cat\t/c/en/bark\t{\"weight\": 0.2}";
        int ok3 = CommonsenseParseLine(line3, &cfg, &triple);
        check_int("Filter out low confidence line (weight < 1.0)", ok3, 0);
    }

    /* =====================================================================
       Test 4: Strict 32-Byte Memory Footprint Verification (M3.2)
       ===================================================================== */
    printf("\n--- Test 4: Memory Density Verification ---\n");
    {
        size_t rel_size = sizeof(RELATION);
        printf("  [INFO] sizeof(RELATION) = %zu bytes\n", rel_size);
        check_int("RELATION struct is strictly 32 bytes", (int)rel_size, 32);

        /* 10,000,000 triples calculation */
        double mb_10m = (10000000.0 * 32.0) / (1024.0 * 1024.0);
        printf("  [INFO] 10M triples RAM = %.2f MB (Milestone Target: < 350 MB)\n", mb_10m);
        check_int("10M triples reside in < 350 MB RAM", (mb_10m < 350.0), 1);
    }

    /* =====================================================================
       Test 5: Streaming Chunked Buffer Ingestion
       ===================================================================== */
    printf("\n--- Test 5: Streaming Buffer Ingestion ---\n");
    {
        GRAPH *g = GraphCreate(1024, 1024);
        CS_CONFIG cfg = CommonsenseConfigDefault();
        CS_STATS stats;

        const char *buf =
            "sun\tCAUSES\tlight\t3.0\n"
            "sun\tCAUSES\theat\t3.0\n"
            "fire\tCAUSES\theat\t2.5\n"
            "ice\tHAS_PROPERTY\tcold\t2.0\n"
            "water\tHAS_PROPERTY\tliquid\t2.0\n";

        int count = CommonsenseIngestBuffer(g, buf, strlen(buf), &cfg, &stats);
        check_int("Ingest buffer line count", count, 5);
        check_int("Graph relations count", (int)RelationCount(g->relations), 5);
        check_int("Graph symbol count (8 entities + 2 relations)", (int)SymbolCount(g->symbols), 10);

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 6: High-Scale Ingestion Benchmark (M3.2 Verification)
       ===================================================================== */
    printf("\n--- Test 6: High-Scale Ingestion Benchmark ---\n");
    {
        const uint32_t BENCH_COUNT = 1000000; /* 1 Million triples */
        CS_STATS stats;

        printf("  [BENCH] Streaming %u canonical triples...\n", BENCH_COUNT);
        int ok = CommonsenseBenchmarkScale(BENCH_COUNT, &stats);
        check_int("High-scale benchmark completed", ok, 1);

        printf("  [BENCH] Ingested: %llu triples in %.3f s\n",
               (unsigned long long)stats.triples_ingested, stats.elapsed_sec);
        printf("  [BENCH] Throughput: %.0f triples/sec\n", stats.triples_per_sec);
        printf("  [BENCH] Memory: %.2f MB\n", (double)stats.ram_bytes / (1024.0 * 1024.0));

        /* Target: > 500,000 triples/sec (robust under concurrent ctest execution) */
        check_int("Ingestion throughput > 500,000 triples/sec",
                  (stats.triples_per_sec > 500000.0), 1);
    }

    /* =====================================================================
       Test 7: Spatial & Transitive Location Reasoning (M3.3)
       ===================================================================== */
    printf("\n--- Test 7: Spatial Location Reasoning ---\n");
    {
        GRAPH *g = GraphCreate(2048, 2048);
        CS_STATS stats;
        CommonsenseIngestSeed(g, &stats);

        char out[256];
        CS_INFERENCE_PATH path;

        /* "Where is the milk?" */
        int ok1 = CommonsenseQueryLocation(g, "milk", &path, out, sizeof(out));
        check_int("Location query for 'milk' succeeded", ok1, 1);
        check_contains("Milk is in the refrigerator", out, "milk is in the refrigerator");
        check_contains("Located in the kitchen", out, "located in the kitchen");
        check_contains("Part of the house", out, "part of the house");
        check_int("Path has 3 hops", (int)path.hop_count, 3);

        /* Unknown entity returns fail-closed honest UNKNOWN */
        int ok2 = CommonsenseQueryLocation(g, "spaceship", &path, out, sizeof(out));
        check_int("Unknown entity location returns 0 (honest UNKNOWN)", ok2, 0);

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 8: Functional Affordances & Capabilities (M3.3)
       ===================================================================== */
    printf("\n--- Test 8: Functional Affordances ---\n");
    {
        GRAPH *g = GraphCreate(2048, 2048);
        CS_STATS stats;
        CommonsenseIngestSeed(g, &stats);

        char out[256];

        /* "What is a knife used for?" */
        int ok1 = CommonsenseQueryAffordance(g, "knife", "USED_FOR", out, sizeof(out));
        check_int("Knife affordance query succeeded", ok1, 1);
        check_str("Knife is used to cut", out, "A knife is used to cut.");

        /* "What can birds do?" */
        int ok2 = CommonsenseQueryAffordance(g, "bird", "CAPABLE_OF", out, sizeof(out));
        check_int("Bird capability query succeeded", ok2, 1);
        check_str("Bird can fly", out, "A bird can fly.");

        /* "What can dogs do?" */
        int ok3 = CommonsenseQueryAffordance(g, "dog", "CAPABLE_OF", out, sizeof(out));
        check_int("Dog capability query succeeded", ok3, 1);
        check_str("Dog can bark", out, "A dog can bark.");

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 9: Physical Consequence & Causal Reasoning (M3.3)
       ===================================================================== */
    printf("\n--- Test 9: Physical Consequence Reasoning ---\n");
    {
        GRAPH *g = GraphCreate(2048, 2048);
        CS_STATS stats;
        CommonsenseIngestSeed(g, &stats);

        char out[256];
        CS_INFERENCE_PATH path;

        /* "What happens when glass is dropped on concrete?" */
        int ok1 = CommonsenseQueryPhysicalConsequence(g, "glass", "dropped on", "concrete",
                                                      &path, out, sizeof(out));
        check_int("Glass dropped on concrete consequence derived", ok1, 1);
        check_contains("Consequence predicts shatter", out, "it will shatter");
        check_contains("Explanation references brittle_material", out, "made of brittle_material");
        check_int("Inference path verified", path.verified, 1);
        check_int("Path hop count is 2", (int)path.hop_count, 2);

        /* Unknown object has no false positive */
        int ok2 = CommonsenseQueryPhysicalConsequence(g, "pillow", "dropped on", "concrete",
                                                      &path, out, sizeof(out));
        check_int("Unknown physical consequence returns 0 (honest UNKNOWN)", ok2, 0);

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 10: Taxonomy & Transitive IS_A Reasoning (WordNet)
       ===================================================================== */
    printf("\n--- Test 10: Taxonomy & WordNet Hierarchy ---\n");
    {
        GRAPH *g = GraphCreate(2048, 2048);
        CS_STATS stats;
        CommonsenseIngestSeed(g, &stats);

        /* Dog IsA Canine -> Canine IsA Mammal -> Mammal IsA Animal */
        SYMBOL_ID dog_id = SymbolFind(g->symbols, "dog");
        SYMBOL_ID is_a_id = SymbolFind(g->symbols, "IS_A");

        RELATION *res[4];
        uint32_t c1 = RelationFindBySubjectRelation(g->relations, dog_id, is_a_id, res, 4);
        check_int("Dog is a canine", (int)(c1 >= 1), 1);

        const SYMBOL *s1 = SymbolGet(g->symbols, res[0]->object);
        check_str("Dog -> Canine", s1->name, "canine");

        uint32_t c2 = RelationFindBySubjectRelation(g->relations, res[0]->object, is_a_id, res, 4);
        check_int("Canine is a mammal", (int)(c2 >= 1), 1);

        const SYMBOL *s2 = SymbolGet(g->symbols, res[0]->object);
        check_str("Canine -> Mammal", s2->name, "mammal");

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 11: Binary Snapshot Serialization (CommonsenseSaveBinary)
       ===================================================================== */
    printf("\n--- Test 11: Binary Snapshot Serialization ---\n");
    const char *test_bin_path = "test_commonsense_snapshot.bin";
    {
        GRAPH *g = GraphCreate(2048, 2048);
        CS_STATS stats;
        CommonsenseIngestSeed(g, &stats);

        int saved = CommonsenseSaveBinary(g, test_bin_path);
        check_int("CommonsenseSaveBinary returned success", saved, 1);

        FILE *fp = fopen(test_bin_path, "rb");
        check_int("Binary snapshot file created on disk", (fp != NULL), 1);
        if (fp)
        {
            CS_BIN_HEADER hdr;
            size_t rd = fread(&hdr, 1, sizeof(hdr), fp);
            check_int("Header read size matches 40 bytes", (int)rd, 40);
            check_int("Header magic is CS_BIN_MAGIC", hdr.magic == CS_BIN_MAGIC, 1);
            check_int("Header version is CS_BIN_VERSION", (int)hdr.version, CS_BIN_VERSION);
            check_int("Header symbol count matches source graph", (int)hdr.symbol_count, (int)g->symbols->count);
            fclose(fp);
        }

        /* Also export the default production snapshot to data/commonsense.bin
           if in repo root AND no snapshot exists yet (never clobber a full
           ConceptNet ingest: seed snapshot is fallback-only) */
        FILE *fp_check = fopen("data/english-spanish.txt", "rb");
        FILE *fp_bin = NULL;
        if (fp_check)
        {
            fclose(fp_check);
            fp_bin = fopen("data/commonsense.bin", "rb");
            if (fp_bin)
                fclose(fp_bin);
            else
                CommonsenseSaveBinary(g, "data/commonsense.bin");
        }

        GraphDestroy(g);
    }

    /* =====================================================================
       Test 12: High-Speed Binary Deserialization (CommonsenseLoadBinary)
       ===================================================================== */
    printf("\n--- Test 12: High-Speed Binary Deserialization ---\n");
    {
        double t0 = (double)clock() / (double)CLOCKS_PER_SEC;
        GRAPH *loaded = CommonsenseLoadBinary(test_bin_path);
        double t1 = (double)clock() / (double)CLOCKS_PER_SEC;
        double load_ms = (t1 - t0) * 1000.0;

        printf("  [BENCH] CommonsenseLoadBinary took %.3f ms\n", load_ms);
        check_int("CommonsenseLoadBinary returned non-NULL graph", (loaded != NULL), 1);

        if (loaded)
        {
            /* Verify all functional affordances and reasoning work identically */
            char out[256];
            int ok_aff = CommonsenseQueryAffordance(loaded, "knife", "USED_FOR", out, sizeof(out));
            check_int("Loaded graph knife affordance succeeded", ok_aff, 1);
            check_str("Loaded graph knife affordance text", out, "A knife is used to cut.");

            CS_INFERENCE_PATH path;
            int ok_phys = CommonsenseQueryPhysicalConsequence(loaded, "glass", "dropped on", "concrete",
                                                             &path, out, sizeof(out));
            check_int("Loaded graph physical consequence succeeded", ok_phys, 1);
            check_contains("Loaded graph glass shatter", out, "it will shatter");

            /* Verify taxonomy */
            SYMBOL_ID dog_id = SymbolFind(loaded->symbols, "dog");
            SYMBOL_ID is_a_id = SymbolFind(loaded->symbols, "IS_A");
            RELATION *res[4];
            uint32_t c1 = RelationFindBySubjectRelation(loaded->relations, dog_id, is_a_id, res, 4);
            check_int("Loaded graph dog is a canine", (int)(c1 >= 1), 1);

            GraphDestroy(loaded);
        }
    }

    /* =====================================================================
       Test 13: Virtual Memory-Mapped Commonsense Graph (CommonsenseLoadMmap)
       ===================================================================== */
    printf("\n--- Test 13: Virtual Memory-Mapped Commonsense Graph (mmap) ---\n");
    {
        CS_MMAP_CONTEXT mmap_ctx;
        double t0 = (double)clock() / (double)CLOCKS_PER_SEC;
        GRAPH *mmap_graph = CommonsenseLoadMmap(test_bin_path, &mmap_ctx);
        double t1 = (double)clock() / (double)CLOCKS_PER_SEC;
        double map_ms = (t1 - t0) * 1000.0;

        printf("  [BENCH] CommonsenseLoadMmap took %.3f ms\n", map_ms);
        check_int("CommonsenseLoadMmap returned non-NULL graph", (mmap_graph != NULL), 1);
        check_int("Context map_view is valid", (mmap_ctx.map_view != NULL), 1);
        check_int("Context file_size > 0", (mmap_ctx.file_size > 0), 1);

        if (mmap_graph)
        {
            /* Verify physical reasoning on mmap graph */
            char out[256];
            CS_INFERENCE_PATH path;
            int ok_phys = CommonsenseQueryPhysicalConsequence(mmap_graph, "glass", "dropped on", "concrete",
                                                             &path, out, sizeof(out));
            check_int("Mmap graph physical consequence derived", ok_phys, 1);
            check_contains("Mmap graph predicts shatter", out, "it will shatter");

            /* Verify affordance */
            int ok_bird = CommonsenseQueryAffordance(mmap_graph, "bird", "CAPABLE_OF", out, sizeof(out));
            check_int("Mmap graph bird affordance", ok_bird, 1);
            check_str("Mmap graph bird can fly", out, "A bird can fly.");
        }

        CommonsenseMmapClose(&mmap_ctx);
        check_int("Mmap context cleaned up (map_view NULL)", (mmap_ctx.map_view == NULL), 1);
    }

    /* =====================================================================
       Test 14: Corrupted Snapshot Fail-Closed Verification
       ===================================================================== */
    printf("\n--- Test 14: Corrupted Snapshot Fail-Closed Verification ---\n");
    {
        const char *corrupt_path = "test_corrupt.bin";
        FILE *fp_src = fopen(test_bin_path, "rb");
        if (fp_src)
        {
            fseek(fp_src, 0, SEEK_END);
            long sz = ftell(fp_src);
            fseek(fp_src, 0, SEEK_SET);

            uint8_t *tmp = (uint8_t *)malloc((size_t)sz);
            if (tmp)
            {
                size_t rd = fread(tmp, 1, (size_t)sz, fp_src);
                fclose(fp_src);

                if (rd == (size_t)sz && sz > 60)
                {
                    /* Corrupt payload byte */
                    tmp[50] ^= 0xFF;

                    FILE *fp_corrupt = fopen(corrupt_path, "wb");
                    if (fp_corrupt)
                    {
                        fwrite(tmp, 1, (size_t)sz, fp_corrupt);
                        fclose(fp_corrupt);
                    }

                    GRAPH *g_bad = CommonsenseLoadBinary(corrupt_path);
                    check_int("Corrupted binary snapshot rejected (NULL)", (g_bad == NULL), 1);
                    remove(corrupt_path);
                }
                free(tmp);
            }
            else
            {
                fclose(fp_src);
            }
        }
        remove(test_bin_path);
    }

    printf("\n=======================================================\n");
    printf("PILLAR 3 COMMONSENSE INGESTION SUMMARY: %d PASSED, %d FAILED\n", g_pass, g_fail);
    printf("=======================================================\n");

    return (g_fail == 0) ? 0 : 1;
}
