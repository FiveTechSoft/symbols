#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "relation.h"

#define HASH_LOAD_FACTOR_NUM 7
#define HASH_LOAD_FACTOR_DEN 10
#define EMPTY_BUCKET 0xFFFFFFFF

static uint32_t NextPowerOfTwo(uint32_t n)
{
    if (n < 16) n = 16;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

static inline uint32_t HashTripletPolar(SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o, RELATION_POLARITY pol)
{
    uint64_t k = ((uint64_t)s) ^ ((uint64_t)p << 21) ^ ((uint64_t)o << 42) ^ ((uint64_t)pol << 63);
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return (uint32_t)k;
}

typedef struct
{
    uint32_t *buckets;
    uint32_t  capacity;
    uint32_t  mask;
} RELATION_INDEX;

static RELATION_INDEX *g_rel_idx = NULL;

static RELATION_INDEX *RelationIndexCreate(uint32_t capacity)
{
    capacity = NextPowerOfTwo(capacity * 2);
    RELATION_INDEX *idx = (RELATION_INDEX *)malloc(sizeof(RELATION_INDEX));
    if (!idx) return NULL;

    idx->buckets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    if (!idx->buckets)
    {
        free(idx);
        return NULL;
    }

    memset(idx->buckets, 0xFF, capacity * sizeof(uint32_t));
    idx->capacity = capacity;
    idx->mask = capacity - 1;
    return idx;
}

static void RelationIndexDestroy(RELATION_INDEX *idx)
{
    if (idx)
    {
        free(idx->buckets);
        free(idx);
    }
}

static void RelationIndexRehash(RELATION_INDEX *idx, const RELATION *items,
                                uint32_t count, uint32_t new_cap)
{
    new_cap = NextPowerOfTwo(new_cap);
    uint32_t *new_buckets = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
    if (!new_buckets) return;

    memset(new_buckets, 0xFF, new_cap * sizeof(uint32_t));
    uint32_t new_mask = new_cap - 1;

    for (uint32_t i = 0; i < count; i++)
    {
        const RELATION *r = &items[i];
        uint32_t h = HashTripletPolar(r->subject, r->predicate, r->object, r->polarity) & new_mask;
        while (new_buckets[h] != EMPTY_BUCKET)
        {
            h = (h + 1) & new_mask;
        }
        new_buckets[h] = i;
    }

    free(idx->buckets);
    idx->buckets = new_buckets;
    idx->capacity = new_cap;
    idx->mask = new_mask;
}

RELATION_TABLE *RelationTableCreate(uint32_t capacity)
{
    capacity = NextPowerOfTwo(capacity);
    RELATION_TABLE *table = (RELATION_TABLE *)malloc(sizeof(RELATION_TABLE));
    if (!table) return NULL;

    RelationTableInit(table, capacity);
    return table;
}

void RelationTableInit(RELATION_TABLE *table, uint32_t capacity)
{
    if (!table) return;

    capacity = NextPowerOfTwo(capacity);
    table->items = (RELATION *)calloc(capacity, sizeof(RELATION));
    table->count = 0;
    table->capacity = capacity;

    if (g_rel_idx)
        RelationIndexDestroy(g_rel_idx);
    g_rel_idx = RelationIndexCreate(capacity);
}

void RelationTableDestroy(RELATION_TABLE *table)
{
    if (!table) return;

    if (g_rel_idx)
    {
        RelationIndexDestroy(g_rel_idx);
        g_rel_idx = NULL;
    }
    free(table->items);
    free(table);
}

RELATION *RelationFindPolar(RELATION_TABLE *table, SYMBOL_ID subject,
                            SYMBOL_ID predicate, SYMBOL_ID object,
                            RELATION_POLARITY polarity)
{
    if (!table || table->count == 0 || !g_rel_idx)
        return NULL;

    uint32_t mask = g_rel_idx->mask;
    uint32_t h = HashTripletPolar(subject, predicate, object, polarity) & mask;

    while (1)
    {
        uint32_t item_idx = g_rel_idx->buckets[h];
        if (item_idx == EMPTY_BUCKET)
            return NULL;

        RELATION *r = &table->items[item_idx];
        if (r->subject == subject && r->predicate == predicate &&
            r->object == object && r->polarity == polarity)
        {
            return r;
        }
        h = (h + 1) & mask;
    }
}

RELATION *RelationFind(RELATION_TABLE *table, SYMBOL_ID subject,
                       SYMBOL_ID predicate, SYMBOL_ID object)
{
    return RelationFindPolar(table, subject, predicate, object, POLARITY_POSITIVE);
}

RELATION *RelationFindOpposite(RELATION_TABLE *table, SYMBOL_ID subject,
                               SYMBOL_ID predicate, SYMBOL_ID object,
                               RELATION_POLARITY polarity)
{
    RELATION_POLARITY opp = (polarity == POLARITY_POSITIVE) ? POLARITY_NEGATIVE : POLARITY_POSITIVE;
    return RelationFindPolar(table, subject, predicate, object, opp);
}

void RelationStrengthen(RELATION *relation, float amount)
{
    if (!relation) return;
    relation->weight += amount;
    if (relation->weight > 1.0e9f)
        relation->weight = 1.0e9f;
}

int RelationAddPolar(RELATION_TABLE *table, SYMBOL_ID subject,
                     SYMBOL_ID predicate, SYMBOL_ID object,
                     RELATION_POLARITY polarity)
{
    if (!table || subject == SYMBOL_INVALID || predicate == SYMBOL_INVALID || object == SYMBOL_INVALID)
        return 0;

    if (!g_rel_idx)
        g_rel_idx = RelationIndexCreate(table->capacity);

    RELATION *existing = RelationFindPolar(table, subject, predicate, object, polarity);
    if (existing != NULL)
    {
        existing->count++;
        RelationStrengthen(existing, 0.01f);
        return 1;
    }

    if (table->count >= table->capacity)
    {
        uint32_t new_cap = table->capacity * 2;
        RELATION *new_items = (RELATION *)realloc(table->items, new_cap * sizeof(RELATION));
        if (!new_items) return 0;

        memset(new_items + table->capacity, 0, (new_cap - table->capacity) * sizeof(RELATION));
        table->items = new_items;
        table->capacity = new_cap;
    }

    if (table->count * HASH_LOAD_FACTOR_DEN >= g_rel_idx->capacity * HASH_LOAD_FACTOR_NUM)
    {
        RelationIndexRehash(g_rel_idx, table->items, table->count, g_rel_idx->capacity * 2);
    }

    uint32_t new_idx = table->count;
    RELATION *r = &table->items[new_idx];
    r->subject = subject;
    r->predicate = predicate;
    r->object = object;
    r->polarity = polarity;
    r->count = 1;
    r->weight = 1.0f;
    table->count++;

    uint32_t mask = g_rel_idx->mask;
    uint32_t h = HashTripletPolar(subject, predicate, object, polarity) & mask;
    while (g_rel_idx->buckets[h] != EMPTY_BUCKET)
    {
        h = (h + 1) & mask;
    }
    g_rel_idx->buckets[h] = new_idx;

    return 1;
}

int RelationAdd(RELATION_TABLE *table, SYMBOL_ID subject,
                SYMBOL_ID predicate, SYMBOL_ID object)
{
    return RelationAddPolar(table, subject, predicate, object, POLARITY_POSITIVE);
}

uint32_t RelationFindBySubject(const RELATION_TABLE *table, SYMBOL_ID subject,
                               RELATION **results, uint32_t max_results)
{
    if (!table || !results || max_results == 0) return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject)
        {
            results[found++] = &table->items[i];
            if (found >= max_results) break;
        }
    }
    return found;
}

uint32_t RelationFindBySubjectPredicate(const RELATION_TABLE *table,
                                        SYMBOL_ID subject, SYMBOL_ID predicate,
                                        RELATION **results, uint32_t max_results)
{
    if (!table || !results || max_results == 0) return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject && table->items[i].predicate == predicate)
        {
            results[found++] = &table->items[i];
            if (found >= max_results) break;
        }
    }
    return found;
}

uint32_t RelationFindByObject(const RELATION_TABLE *table, SYMBOL_ID object,
                              RELATION **results, uint32_t max_results)
{
    if (!table || !results || max_results == 0) return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].object == object)
        {
            results[found++] = &table->items[i];
            if (found >= max_results) break;
        }
    }
    return found;
}

const RELATION *RelationGet(const RELATION_TABLE *table, uint32_t index)
{
    if (!table || index >= table->count) return NULL;
    return &table->items[index];
}

uint32_t RelationCount(const RELATION_TABLE *table)
{
    return table ? table->count : 0;
}

void RelationIndexRebuild(RELATION_TABLE *table)
{
    if (!table || !g_rel_idx) return;
    uint32_t cap = g_rel_idx->capacity;
    while (cap > 16 && table->count * 10 < cap * 7)
        cap /= 2;
    if (cap < 16) cap = 16;
    RelationIndexRehash(g_rel_idx, table->items, table->count, cap);
}
