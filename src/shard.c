#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
static double GetTimeSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

#include "shard.h"
#include "model.h"

/* DJB2a hash for deterministic shard assignment */
static uint32_t ShardHash(const char *key)
{
    uint32_t hash = 5381;
    int c;
    while ((c = (unsigned char)*key++))
    {
        hash = ((hash << 5) + hash) ^ (uint32_t)c;
    }
    return hash;
}

SHARD_RESULT ShardSplit(const char *input_path, uint32_t num_shards,
                        const char *output_dir)
{
    SHARD_RESULT result = {0};
    double t0 = GetTimeSeconds();

    if (num_shards > MAX_SHARDS)
        num_shards = MAX_SHARDS;

    /* Open input */
    FILE *fin = fopen(input_path, "r");
    if (fin == NULL)
    {
        fprintf(stderr, "ShardSplit: cannot open %s\n", input_path);
        return result;
    }

    /* Open shard output files */
    FILE *shards[MAX_SHARDS] = {0};
    char path[640];
    for (uint32_t i = 0; i < num_shards; i++)
    {
        snprintf(path, sizeof(path), "%s/shard_%u.tsv", output_dir, i);
        shards[i] = fopen(path, "w");
        if (shards[i] == NULL)
        {
            fprintf(stderr, "ShardSplit: cannot create %s\n", path);
            for (uint32_t j = 0; j < i; j++)
                fclose(shards[j]);
            fclose(fin);
            return result;
        }
    }

    /* Stream and distribute */
    char line[512];
    while (fgets(line, sizeof(line), fin) != NULL)
    {
        result.total_lines++;

        /* Hash first column to assign shard deterministically */
        char *tab = strchr(line, '\t');
        if (tab == NULL)
            continue;
        *tab = '\0';
        uint32_t shard_id = ShardHash(line) % num_shards;
        *tab = '\t';

        fputs(line, shards[shard_id]);
    }

    fclose(fin);
    for (uint32_t i = 0; i < num_shards; i++)
        fclose(shards[i]);

    result.shards_created = num_shards;
    result.elapsed_seconds = GetTimeSeconds() - t0;

    return result;
}

MODEL *ShardMerge(const char **shard_paths, uint32_t num_shards,
                  uint32_t initial_symbols, uint32_t initial_relations)
{
    double t0 = GetTimeSeconds();

    MODEL *merged = ModelCreate(initial_symbols, initial_relations);
    if (merged == NULL)
        return NULL;

    uint64_t total_triples = 0;

    for (uint32_t s = 0; s < num_shards; s++)
    {
        /* Load binary model */
        MODEL *shard = ModelLoad(shard_paths[s]);
        if (shard == NULL)
        {
            fprintf(stderr, "ShardMerge: cannot load %s\n", shard_paths[s]);
            continue;
        }

        printf("  Shard %u: %s cargado (%u relaciones, %u simbolos)\n",
               s, shard_paths[s],
               RelationCount(shard->graph->relations),
               SymbolCount(shard->graph->symbols));

        /* Copy every relation from shard into merged, summing counts */
        RELATION_TABLE *src = shard->graph->relations;
        for (uint32_t i = 0; i < src->capacity; i++)
        {
            const RELATION *r = &src->items[i];
            if (r->subject == SYMBOL_INVALID)
                continue;

            SYMBOL_ID mid = GraphAddSymbol(merged->graph,
                                           SymbolGet(shard->graph->symbols, r->subject)->name);
            SYMBOL_ID pid = GraphAddSymbol(merged->graph,
                                           SymbolGet(shard->graph->symbols, r->predicate)->name);
            SYMBOL_ID oid = GraphAddSymbol(merged->graph,
                                           SymbolGet(shard->graph->symbols, r->object)->name);

            if (mid == SYMBOL_INVALID || pid == SYMBOL_INVALID ||
                oid == SYMBOL_INVALID)
                continue;

            RELATION *existing = RelationFind(merged->graph->relations, mid, pid, oid);
            if (existing != NULL)
            {
                existing->count += r->count;
                existing->weight += r->weight;
            }
            else
            {
                RelationAdd(merged->graph->relations, mid, pid, oid);
                RELATION *added = RelationFind(merged->graph->relations, mid, pid, oid);
                if (added != NULL)
                {
                    added->count = r->count;
                    added->weight = r->weight;
                }
            }
            total_triples++;
        }

        /* Copy shard embeddings into merged, mapping symbol IDs by name
           (first shard with the symbol wins) */
        if (shard->embeddings != NULL && merged->embeddings != NULL)
        {
            for (uint32_t i = 0; i < shard->embeddings->count; i++)
            {
                const SYMBOL_EMBEDDING *e = &shard->embeddings->items[i];
                if (!e->initialized)
                    continue;

                const SYMBOL *sym = SymbolGet(shard->graph->symbols, e->id);
                if (sym == NULL || sym->name == NULL)
                    continue;

                SYMBOL_ID mid = SymbolFind(merged->graph->symbols, sym->name);
                if (mid == SYMBOL_INVALID)
                    continue;

                if (EmbeddingGetVector(merged->embeddings, mid) == NULL)
                    EmbeddingSetVector(merged->embeddings, mid, e->vector);
            }
        }

        ModelDestroy(shard);
    }

    double elapsed = GetTimeSeconds() - t0;

    printf("\n  Merge completado:\n");
    printf("    Shards fusionados : %u\n", num_shards);
    printf("    Triples totales   : %llu\n", (unsigned long long)total_triples);
    printf("    Simbolos          : %u\n", SymbolCount(merged->graph->symbols));
    printf("    Relaciones        : %u\n", RelationCount(merged->graph->relations));
    printf("    Tiempo de merge   : %.4f s\n", elapsed);

    return merged;
}
