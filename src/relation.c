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

/* MurmurMix64 for (subject, predicate, object) triplet */
static inline uint32_t HashTriplet(SYMBOL_ID s, SYMBOL_ID p, SYMBOL_ID o)
{
    uint64_t k = ((uint64_t)s) ^ ((uint64_t)p << 21) ^ ((uint64_t)o << 42);
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return (uint32_t)k;
}

static void RehashBuckets(RELATION_TABLE *table, uint32_t new_cap)
{
    new_cap = NextPowerOfTwo(new_cap);

    /* Grow items array to match */
    RELATION *new_items = (RELATION *)realloc(
        table->items, new_cap * sizeof(RELATION));
    if (new_items == NULL)
        return;
    memset(new_items + table->capacity, 0,
           (new_cap - table->capacity) * sizeof(RELATION));
    table->items = new_items;

    /* Rebuild buckets */
    uint32_t *new_buckets = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
    if (new_buckets == NULL)
        return;

    uint32_t new_mask = new_cap - 1;
    for (uint32_t i = 0; i < new_cap; i++)
        new_buckets[i] = EMPTY_BUCKET;

    for (uint32_t i = 0; i < table->count; i++)
    {
        RELATION *r = &table->items[i];
        uint32_t h = HashTriplet(r->subject, r->predicate, r->object) & new_mask;
        while (new_buckets[h] != EMPTY_BUCKET)
            h = (h + 1) & new_mask;
        new_buckets[h] = i;
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_cap;
    table->mask = new_mask;
}

/* ============================================================
   Create / Init / Destroy
   ============================================================ */

RELATION_TABLE *RelationTableCreate(uint32_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    RELATION_TABLE *table = (RELATION_TABLE *)malloc(sizeof(RELATION_TABLE));
    if (table == NULL)
        return NULL;

    RelationTableInit(table, capacity);
    return table;
}

void RelationTableInit(RELATION_TABLE *table, uint32_t capacity)
{
    if (table == NULL)
        return;

    if (capacity == 0)
        capacity = 16;

    capacity = NextPowerOfTwo(capacity);

    table->items = (RELATION *)calloc(capacity, sizeof(RELATION));
    table->buckets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    table->count = 0;
    table->capacity = capacity;
    table->mask = capacity - 1;

    for (uint32_t i = 0; i < capacity; i++)
        table->buckets[i] = EMPTY_BUCKET;
}

void RelationTableDestroy(RELATION_TABLE *table)
{
    if (table == NULL)
        return;

    free(table->items);
    free(table->buckets);
}

/* ============================================================
   Find O(1) average via hash
   ============================================================ */

RELATION *RelationFind(RELATION_TABLE *table,
                       SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object)
{
    if (table == NULL || table->count == 0)
        return NULL;

    uint32_t h = HashTriplet(subject, predicate, object) & table->mask;

    while (table->buckets[h] != EMPTY_BUCKET)
    {
        uint32_t idx = table->buckets[h];
        RELATION *r = &table->items[idx];
        if (r->subject == subject &&
            r->predicate == predicate &&
            r->object == object)
        {
            return r;
        }
        h = (h + 1) & table->mask;
    }

    return NULL;
}

/* ============================================================
   Strengthen
   ============================================================ */

void RelationStrengthen(RELATION *relation, float amount)
{
    if (relation == NULL)
        return;

    relation->weight += amount;
    if (relation->weight > 1.0e9f)
        relation->weight = 1.0e9f;
}

/* ============================================================
   Add (with auto-rehash at 70% load)
   ============================================================ */

int RelationAdd(RELATION_TABLE *table,
                SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object)
{
    if (table == NULL)
        return 0;

    if (subject == SYMBOL_INVALID ||
        predicate == SYMBOL_INVALID ||
        object == SYMBOL_INVALID)
    {
        return 0;
    }

    /* Check if already exists */
    RELATION *existing = RelationFind(table, subject, predicate, object);
    if (existing != NULL)
    {
        existing->count++;
        RelationStrengthen(existing, 0.01f);
        return 1;
    }

    /* Rehash at 70% load (grows both items and buckets) */
    if (table->count * HASH_LOAD_FACTOR_DEN >= table->capacity * HASH_LOAD_FACTOR_NUM)
    {
        RehashBuckets(table, table->capacity * 2);
    }

    /* Insert into dense array */
    uint32_t idx = table->count;
    RELATION *r = &table->items[idx];
    r->subject = subject;
    r->predicate = predicate;
    r->object = object;
    r->count = 1;
    r->weight = 1.0f;
    table->count++;

    /* Insert into hash table */
    uint32_t h = HashTriplet(subject, predicate, object) & table->mask;
    while (table->buckets[h] != EMPTY_BUCKET)
        h = (h + 1) & table->mask;
    table->buckets[h] = idx;

    return 1;
}

/* ============================================================
   Query by subject (scans dense array - unavoidable for partial keys)
   ============================================================ */

uint32_t RelationFindBySubject(const RELATION_TABLE *table, SYMBOL_ID subject,
                               RELATION **results, uint32_t max_results)
{
    if (table == NULL || results == NULL || max_results == 0)
        return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject)
        {
            results[found++] = &table->items[i];
            if (found >= max_results)
                break;
        }
    }
    return found;
}

uint32_t RelationFindBySubjectPredicate(const RELATION_TABLE *table,
                                        SYMBOL_ID subject, SYMBOL_ID predicate,
                                        RELATION **results, uint32_t max_results)
{
    if (table == NULL || results == NULL || max_results == 0)
        return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject &&
            table->items[i].predicate == predicate)
        {
            results[found++] = &table->items[i];
            if (found >= max_results)
                break;
        }
    }
    return found;
}

uint32_t RelationFindByObject(const RELATION_TABLE *table, SYMBOL_ID object,
                              RELATION **results, uint32_t max_results)
{
    if (table == NULL || results == NULL || max_results == 0)
        return 0;

    uint32_t found = 0;
    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].object == object)
        {
            results[found++] = &table->items[i];
            if (found >= max_results)
                break;
        }
    }
    return found;
}

const RELATION *RelationGet(const RELATION_TABLE *table, uint32_t index)
{
    if (table == NULL || index >= table->count)
        return NULL;
    return &table->items[index];
}

uint32_t RelationCount(const RELATION_TABLE *table)
{
    return table ? table->count : 0;
}
