#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include "model.h"

typedef struct
{
    uint32_t total_symbols;
    uint32_t total_relations;
    uint64_t total_evidence_observations;
    uint32_t total_embeddings_32d;
    float    average_degree;
    size_t   memory_footprint_bytes;
    uint32_t max_out_degree;
    char     most_connected_symbol[64];
} MODEL_STATS;

MODEL_STATS ModelGetStats(const MODEL *model);
void ModelPrintReport(const MODEL *model);

#endif
