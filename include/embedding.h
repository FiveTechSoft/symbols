#ifndef EMBEDDING_H
#define EMBEDDING_H

#include <stdint.h>
#include "symbol.h"

#define EMBEDDING_DIM 32

typedef struct
{
    SYMBOL_ID id;
    float     vector[EMBEDDING_DIM];
    int       initialized;
} SYMBOL_EMBEDDING;

typedef struct
{
    SYMBOL_EMBEDDING *items;
    uint32_t          count;
    uint32_t          capacity;
    /* Direct id->item index: by_id[id] = item_index + 1 (0 = absent).
       SYMBOL_IDs are dense (1..N), so this is O(1) without hashing. */
    uint32_t         *by_id;
    uint32_t          id_capacity;
} EMBEDDING_TABLE;

EMBEDDING_TABLE *EmbeddingTableCreate(uint32_t capacity);
void EmbeddingTableDestroy(EMBEDDING_TABLE *table);

int EmbeddingSetVector(EMBEDDING_TABLE *table, SYMBOL_ID id, const float *vec);
const float *EmbeddingGetVector(const EMBEDDING_TABLE *table, SYMBOL_ID id);

void EmbeddingRandomInit(float *vec, uint32_t seed);
void EmbeddingCooccur(float *target, const float *context, float learning_rate);
void EmbeddingNormalize(float *vec);
float EmbeddingCosineSimilarity(const float *v1, const float *v2);

typedef struct
{
    SYMBOL_ID id;
    float     score;
} EMBEDDING_MATCH;

uint32_t EmbeddingFindSimilar(
    const EMBEDDING_TABLE *table,
    SYMBOL_ID query_id,
    EMBEDDING_MATCH *out_matches,
    uint32_t max_matches
);

#endif
