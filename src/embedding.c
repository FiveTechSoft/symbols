#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "embedding.h"

EMBEDDING_TABLE *EmbeddingTableCreate(uint32_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    EMBEDDING_TABLE *table = (EMBEDDING_TABLE *)malloc(sizeof(EMBEDDING_TABLE));
    if (table == NULL)
        return NULL;

    table->items = (SYMBOL_EMBEDDING *)calloc(capacity, sizeof(SYMBOL_EMBEDDING));
    if (table->items == NULL)
    {
        free(table);
        return NULL;
    }

    table->count = 0;
    table->capacity = capacity;
    return table;
}

void EmbeddingTableDestroy(EMBEDDING_TABLE *table)
{
    if (table == NULL)
        return;

    free(table->items);
    free(table);
}

static SYMBOL_EMBEDDING *FindOrAlloc(EMBEDDING_TABLE *table, SYMBOL_ID id)
{
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].id == id)
            return &table->items[i];
    }

    if (table->count >= table->capacity)
    {
        uint32_t new_cap = table->capacity * 2;
        SYMBOL_EMBEDDING *new_items = (SYMBOL_EMBEDDING *)realloc(
            table->items, new_cap * sizeof(SYMBOL_EMBEDDING));
        if (new_items == NULL)
            return NULL;

        memset(new_items + table->capacity, 0,
               (new_cap - table->capacity) * sizeof(SYMBOL_EMBEDDING));
        table->items = new_items;
        table->capacity = new_cap;
    }

    SYMBOL_EMBEDDING *item = &table->items[table->count++];
    item->id = id;
    item->initialized = 0;
    memset(item->vector, 0, sizeof(item->vector));
    return item;
}

int EmbeddingSetVector(EMBEDDING_TABLE *table, SYMBOL_ID id, const float *vec)
{
    if (table == NULL || vec == NULL || id == SYMBOL_INVALID)
        return 0;

    SYMBOL_EMBEDDING *item = FindOrAlloc(table, id);
    if (item == NULL)
        return 0;

    memcpy(item->vector, vec, sizeof(item->vector));
    item->initialized = 1;
    return 1;
}

const float *EmbeddingGetVector(const EMBEDDING_TABLE *table, SYMBOL_ID id)
{
    if (table == NULL || id == SYMBOL_INVALID)
        return NULL;

    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].id == id && table->items[i].initialized)
            return table->items[i].vector;
    }
    return NULL;
}

/* ============================================================
   Matematicas Vectoriales (32 dimensiones)
   ============================================================ */

void EmbeddingRandomInit(float *vec, uint32_t seed)
{
    if (vec == NULL)
        return;

    srand(seed);
    for (int i = 0; i < EMBEDDING_DIM; i++)
    {
        int r = rand() % 10;
        if (r == 0)      vec[i] =  1.0f;
        else if (r == 1) vec[i] = -1.0f;
        else             vec[i] =  0.0f;
    }
    EmbeddingNormalize(vec);
}

void EmbeddingCooccur(float *target, const float *context, float learning_rate)
{
    if (target == NULL || context == NULL)
        return;

    for (int i = 0; i < EMBEDDING_DIM; i++)
        target[i] += context[i] * learning_rate;

    EmbeddingNormalize(target);
}

void EmbeddingNormalize(float *vec)
{
    if (vec == NULL)
        return;

    float sum_sq = 0.0f;
    for (int i = 0; i < EMBEDDING_DIM; i++)
        sum_sq += vec[i] * vec[i];

    float norm = sqrtf(sum_sq);
    if (norm > 1e-8f)
    {
        for (int i = 0; i < EMBEDDING_DIM; i++)
            vec[i] /= norm;
    }
}

float EmbeddingCosineSimilarity(const float *v1, const float *v2)
{
    if (v1 == NULL || v2 == NULL)
        return 0.0f;

    float dot = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (int i = 0; i < EMBEDDING_DIM; i++)
    {
        dot   += v1[i] * v2[i];
        norm1 += v1[i] * v1[i];
        norm2 += v2[i] * v2[i];
    }

    float denom = sqrtf(norm1) * sqrtf(norm2);
    if (denom < 1e-8f)
        return 0.0f;

    float similarity = dot / denom;
    if (similarity > 1.0f)  return 1.0f;
    if (similarity < -1.0f) return -1.0f;
    return similarity;
}

/* ============================================================
   Busqueda de Similitud
   ============================================================ */

static int CompareMatches(const void *a, const void *b)
{
    const EMBEDDING_MATCH *m1 = (const EMBEDDING_MATCH *)a;
    const EMBEDDING_MATCH *m2 = (const EMBEDDING_MATCH *)b;
    if (m2->score > m1->score) return  1;
    if (m2->score < m1->score) return -1;
    return 0;
}

uint32_t EmbeddingFindSimilar(
    const EMBEDDING_TABLE *table,
    SYMBOL_ID query_id,
    EMBEDDING_MATCH *out_matches,
    uint32_t max_matches)
{
    if (table == NULL || out_matches == NULL || max_matches == 0)
        return 0;

    const float *q_vec = EmbeddingGetVector(table, query_id);
    if (q_vec == NULL)
        return 0;

    uint32_t found = 0;

    for (uint32_t i = 0; i < table->count; i++)
    {
        if (!table->items[i].initialized || table->items[i].id == query_id)
            continue;

        float score = EmbeddingCosineSimilarity(q_vec, table->items[i].vector);

        if (score > 0.1f)
        {
            out_matches[found].id = table->items[i].id;
            out_matches[found].score = score;
            found++;
            if (found >= max_matches)
                break;
        }
    }

    if (found > 1)
        qsort(out_matches, found, sizeof(EMBEDDING_MATCH), CompareMatches);

    return found;
}
