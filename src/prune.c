#include <stdlib.h>
#include <string.h>

#include "prune.h"

/* ============================================================
   Prune by minimum observation count
   ============================================================ */

PRUNE_STATS PruneByMinCount(GRAPH *graph, uint32_t min_count)
{
    PRUNE_STATS stats;
    memset(&stats, 0, sizeof(stats));

    if (graph == NULL || graph->relations == NULL)
        return stats;

    stats.relations_before = RelationCount(graph->relations);

    /* Compact in-place: keep relations with count >= min_count */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < graph->relations->count; i++)
    {
        RELATION *r = &graph->relations->items[i];
        if (r->count >= min_count)
        {
            if (write_idx != i)
                graph->relations->items[write_idx] = *r;
            write_idx++;
        }
    }

    /* Zero out the rest */
    uint32_t removed = graph->relations->count - write_idx;
    memset(&graph->relations->items[write_idx], 0,
           removed * sizeof(RELATION));
    graph->relations->count = write_idx;

    stats.relations_after = write_idx;
    stats.relations_removed = removed;

    /* Rebuild hash table for remaining relations */
    if (removed > 0 && write_idx > 0)
    {
        /* Force rehash by resetting capacity trigger */
        uint32_t new_cap = graph->relations->capacity;
        while (new_cap > 16 &&
               write_idx * 10 < new_cap * 7)
        {
            new_cap /= 2;
        }
        if (new_cap < 16) new_cap = 16;

        /* Rebuild buckets from scratch */
        uint32_t *new_buckets = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
        if (new_buckets != NULL)
        {
            uint32_t mask = new_cap - 1;
            for (uint32_t i = 0; i < new_cap; i++)
                new_buckets[i] = 0xFFFFFFFF;

            for (uint32_t i = 0; i < write_idx; i++)
            {
                RELATION *r = &graph->relations->items[i];
                /* Simple hash for rebuild */
                uint64_t k = ((uint64_t)r->subject) ^
                             ((uint64_t)r->predicate << 21) ^
                             ((uint64_t)r->object << 42);
                k ^= k >> 33;
                k *= 0xff51afd7ed558ccdULL;
                k ^= k >> 33;
                k *= 0xc4ceb9fe1a85ec53ULL;
                k ^= k >> 33;
                uint32_t h = (uint32_t)k & mask;
                while (new_buckets[h] != 0xFFFFFFFF)
                    h = (h + 1) & mask;
                new_buckets[h] = i;
            }

            free(graph->relations->buckets);
            graph->relations->buckets = new_buckets;
            graph->relations->capacity = new_cap;
            graph->relations->mask = mask;
        }
    }

    return stats;
}

/* ============================================================
   Prune by minimum weight
   ============================================================ */

PRUNE_STATS PruneByMinWeight(GRAPH *graph, float min_weight)
{
    PRUNE_STATS stats;
    memset(&stats, 0, sizeof(stats));

    if (graph == NULL || graph->relations == NULL)
        return stats;

    stats.relations_before = RelationCount(graph->relations);

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < graph->relations->count; i++)
    {
        RELATION *r = &graph->relations->items[i];
        if (r->weight >= min_weight)
        {
            if (write_idx != i)
                graph->relations->items[write_idx] = *r;
            write_idx++;
        }
    }

    uint32_t removed = graph->relations->count - write_idx;
    memset(&graph->relations->items[write_idx], 0,
           removed * sizeof(RELATION));
    graph->relations->count = write_idx;

    stats.relations_after = write_idx;
    stats.relations_removed = removed;

    return stats;
}

/* ============================================================
   Remove orphan symbols
   ============================================================ */

uint32_t PruneOrphanSymbols(GRAPH *graph)
{
    if (graph == NULL || graph->symbols == NULL)
        return 0;

    uint32_t removed = 0;
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < graph->symbols->count; i++)
    {
        SYMBOL_ID id = graph->symbols->items[i].id;
        RELATION *dummy[1];
        uint32_t has_relations = RelationFindBySubject(
            graph->relations, id, dummy, 1);

        if (has_relations > 0)
        {
            if (write_idx != i)
                graph->symbols->items[write_idx] = graph->symbols->items[i];
            write_idx++;
        }
        else
        {
            free(graph->symbols->items[i].name);
            removed++;
        }
    }

    memset(&graph->symbols->items[write_idx], 0,
           removed * sizeof(SYMBOL));
    graph->symbols->count = write_idx;

    return removed;
}
