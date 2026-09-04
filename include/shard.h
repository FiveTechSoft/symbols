#ifndef SHARD_H
#define SHARD_H

#include "model.h"
#include <stdint.h>

#define MAX_SHARDS 64

typedef struct
{
    uint32_t shard_id;
    uint32_t total_shards;
    uint64_t lines_processed;
    uint64_t lines_written;
    char input_path[512];
    char output_path[512];
} SHARD_JOB;

typedef struct
{
    uint32_t shards_created;
    uint64_t total_lines;
    double   elapsed_seconds;
} SHARD_RESULT;

/* Split a TSV file into N shard files using deterministic hashing */
SHARD_RESULT ShardSplit(const char *input_path, uint32_t num_shards,
                        const char *output_dir);

/* Merge N submodel files into one final model by summing relation counts */
MODEL *ShardMerge(const char **shard_paths, uint32_t num_shards,
                  uint32_t initial_symbols, uint32_t initial_relations);

#endif
