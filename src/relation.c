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

typedef struct RELATION_INDEX
{
    uint32_t *buckets;
    uint32_t  capacity;
    uint32_t  mask;
} RELATION_INDEX;

static void SubjectIndexRebuild(RELATION_TABLE *table);
static int SubjectIndexInsert(RELATION_TABLE *table, uint32_t item_idx);
static uint32_t SubjectIndexCollect(const RELATION_TABLE *table,
                                     SYMBOL_ID subject,
                                     RELATION **results,
                                     uint32_t max_results);

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
    if (table->items == NULL)
    {
        free(table);
        return NULL;
    }
    return table;
}

void RelationTableInit(RELATION_TABLE *table, uint32_t capacity)
{
    if (!table) return;

    capacity = NextPowerOfTwo(capacity);
    table->items = (RELATION *)calloc(capacity, sizeof(RELATION));
    table->count = 0;
    table->capacity = capacity;
    table->idx = NULL;
    table->subj_heads = NULL;
    table->subj_next = NULL;
    table->subj_capacity = 0;
    table->subj_mask = 0;

    if (table->items == NULL)
    {
        table->capacity = 0;
        return;
    }

    table->idx = RelationIndexCreate(capacity);
}

void RelationTableDestroy(RELATION_TABLE *table)
{
    if (!table) return;

    if (table->idx)
    {
        RelationIndexDestroy(table->idx);
        table->idx = NULL;
    }
    free(table->subj_heads);
    free(table->subj_next);
    table->subj_heads = NULL;
    table->subj_next = NULL;
    table->subj_capacity = 0;
    table->subj_mask = 0;
    free(table->items);
    free(table);
}

RELATION *RelationFindPolar(RELATION_TABLE *table, SYMBOL_ID subject,
                            SYMBOL_ID predicate, SYMBOL_ID object,
                            RELATION_POLARITY polarity)
{
    if (!table || table->count == 0 || !table->idx)
        return NULL;

    RELATION_INDEX *idx = table->idx;
    uint32_t mask = idx->mask;
    uint32_t h = HashTripletPolar(subject, predicate, object, polarity) & mask;

    uint32_t probes = 0;
    while (probes < idx->capacity)
    {
        uint32_t item_idx = idx->buckets[h];
        if (item_idx == EMPTY_BUCKET)
            return NULL;

        RELATION *r = &table->items[item_idx];
        if (r->subject == subject && r->predicate == predicate &&
            r->object == object && r->polarity == polarity)
        {
            return r;
        }
        h = (h + 1) & mask;
        probes++;
    }
    return NULL;
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

void RelationSetSource(RELATION *relation, SYMBOL_ID source)
{
    if (!relation) return;
    relation->source = source;
}

int RelationAddPolar(RELATION_TABLE *table, SYMBOL_ID subject,
                     SYMBOL_ID predicate, SYMBOL_ID object,
                     RELATION_POLARITY polarity)
{
    if (!table || !table->items || table->capacity == 0)
        return 0;
    if (subject == SYMBOL_INVALID || predicate == SYMBOL_INVALID || object == SYMBOL_INVALID)
        return 0;

    if (!table->idx)
    {
        table->idx = RelationIndexCreate(table->capacity);
        if (!table->idx)
            return 0;
    }

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

    if (table->count * HASH_LOAD_FACTOR_DEN >= table->idx->capacity * HASH_LOAD_FACTOR_NUM)
    {
        RelationIndexRehash(table->idx, table->items, table->count, table->idx->capacity * 2);
    }

    if (table->count >= table->idx->capacity)
        return 0;

    uint32_t new_idx = table->count;
    RELATION *r = &table->items[new_idx];
    r->subject = subject;
    r->predicate = predicate;
    r->object = object;
    r->polarity = polarity;
    r->count = 1;
    r->weight = 1.0f;
    table->count++;

    uint32_t mask = table->idx->mask;
    uint32_t h = HashTripletPolar(subject, predicate, object, polarity) & mask;
    while (table->idx->buckets[h] != EMPTY_BUCKET)
    {
        h = (h + 1) & mask;
    }
    table->idx->buckets[h] = new_idx;
    SubjectIndexInsert(table, new_idx);

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

    if (table->subj_capacity > 0 && subject != SYMBOL_INVALID)
    {
        uint32_t n = SubjectIndexCollect(table, subject, results, max_results);
        if (n != 0xFFFFFFFF)
            return n;
    }

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject)
        {
            results[found++] = (RELATION *)&table->items[i];
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

    /* Use the subject chain to limit the scan to this subject's relations */
    if (table->subj_capacity > 0 && subject != SYMBOL_INVALID)
    {
        RELATION *chain[64];
        uint32_t n = SubjectIndexCollect(table, subject, chain, 64);
        if (n != 0xFFFFFFFF)
        {
            uint32_t found = 0;
            for (uint32_t i = 0; i < n && found < max_results; i++)
            {
                if (chain[i]->predicate == predicate)
                    results[found++] = chain[i];
            }
            return found;
        }
    }

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject && table->items[i].predicate == predicate)
        {
            results[found++] = (RELATION *)&table->items[i];
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

/* ============================================================
   Subject chain index: hash(subject) -> chain of item indices.
   Chains are LIFO on insert; lookups reverse them so results
   keep insertion order (same semantics as the old linear scan).
   ============================================================ */

static void SubjectIndexRebuild(RELATION_TABLE *table)
{
    uint32_t cap = NextPowerOfTwo((table->capacity > table->count ? table->capacity : table->count) * 2);
    if (cap < 16) cap = 16;

    uint32_t *heads = (uint32_t *)malloc(cap * sizeof(uint32_t));
    uint32_t *next = (uint32_t *)realloc(table->subj_next, cap * sizeof(uint32_t));
    if (heads == NULL || next == NULL)
    {
        free(heads);
        free(table->subj_heads); /* keep next: realloc keeps old block on fail */
        table->subj_heads = NULL;
        table->subj_next = next;
        table->subj_capacity = 0;
        table->subj_mask = 0;
        return;
    }

    memset(heads, 0xFF, cap * sizeof(uint32_t));
    table->subj_heads = heads;
    table->subj_next = next;
    table->subj_capacity = cap;
    table->subj_mask = cap - 1;

    for (uint32_t i = 0; i < table->count; i++)
    {
        uint32_t h = table->items[i].subject & table->subj_mask;
        table->subj_next[i] = table->subj_heads[h];
        table->subj_heads[h] = i;
    }
}

static int SubjectIndexInsert(RELATION_TABLE *table, uint32_t item_idx)
{
    if (table->subj_capacity == 0)
        SubjectIndexRebuild(table);
    if (table->subj_capacity == 0)
        return 0;

    if ((table->count + 1) * HASH_LOAD_FACTOR_DEN >=
        table->subj_capacity * HASH_LOAD_FACTOR_NUM)
    {
        SubjectIndexRebuild(table);
        if (table->subj_capacity == 0)
            return 0;
    }

    uint32_t h = table->items[item_idx].subject & table->subj_mask;
    table->subj_next[item_idx] = table->subj_heads[h];
    table->subj_heads[h] = item_idx;
    return 1;
}

/* Collect all items with matching subject, oldest first (insertion order).
   Returns found count, or 0xFFFFFFFF to signal "too long, use linear". */
static uint32_t SubjectIndexCollect(const RELATION_TABLE *table,
                                    SYMBOL_ID subject,
                                    RELATION **results,
                                    uint32_t max_results)
{
    if (table->subj_capacity == 0)
        return 0xFFFFFFFF;

    uint32_t chain[64];
    uint32_t n = 0;
    uint32_t i = table->subj_heads[subject & table->subj_mask];
    while (i != EMPTY_BUCKET)
    {
        if (n >= 64) return 0xFFFFFFFF;
        chain[n++] = i;
        i = table->subj_next[i];
    }

    /* chain[] is newest-first; emit oldest-first */
    uint32_t found = 0;
    for (uint32_t k = n; k > 0 && found < max_results; k--)
    {
        uint32_t idx = chain[k - 1];
        if (table->items[idx].subject == subject)
            results[found++] = (RELATION *)&table->items[idx];
    }
    return found;
}

void RelationIndexRebuild(RELATION_TABLE *table)
{
    if (!table || !table->idx || !table->items) return;
    uint32_t cap = table->idx->capacity;
    while (cap > 16 && table->count * 10 < cap * 7)
        cap /= 2;
    if (cap < 16) cap = 16;
    RelationIndexRehash(table->idx, table->items, table->count, cap);
    SubjectIndexRebuild(table);
}
