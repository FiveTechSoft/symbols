#ifndef PRUNE_H
#define PRUNE_H

#include <stdint.h>
#include "graph.h"

typedef struct
{
    uint32_t relations_before;
    uint32_t relations_after;
    uint32_t relations_removed;
} PRUNE_STATS;

/*
 * Remove all relations with count < min_count.
 * This filters noise and rare/erroneous triples.
 * Returns stats about what was pruned.
 */
PRUNE_STATS PruneByMinCount(GRAPH *graph, uint32_t min_count);

/*
 * Remove relations with weight below threshold.
 */
PRUNE_STATS PruneByMinWeight(GRAPH *graph, float min_weight);

/*
 * Remove orphan symbols (no relations) from the symbol table.
 * Returns number of symbols removed.
 */
uint32_t PruneOrphanSymbols(GRAPH *graph);

#endif
