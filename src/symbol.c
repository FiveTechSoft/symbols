#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "learning.h"

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

static inline uint32_t HashString(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
    {
        hash = ((hash << 5) + hash) ^ (uint32_t)c;
    }
    return hash;
}

static void RehashBuckets(SYMBOL_TABLE *table, uint32_t new_cap)
{
    uint32_t *new_buckets = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
    if (new_buckets == NULL)
        return;

    SYMBOL *new_items = (SYMBOL *)realloc(
        table->items, new_cap * sizeof(SYMBOL));
    if (new_items == NULL)
    {
        free(new_buckets);
        return;
    }
    memset(new_items + table->capacity, 0,
           (new_cap - table->capacity) * sizeof(SYMBOL));
    table->items = new_items;

    uint32_t mask = new_cap - 1;
    for (uint32_t i = 0; i < new_cap; i++)
        new_buckets[i] = EMPTY_BUCKET;

    for (uint32_t i = 0; i < table->count; i++)
    {
        uint32_t h = HashString(table->items[i].name) & mask;
        while (new_buckets[h] != EMPTY_BUCKET)
            h = (h + 1) & mask;
        new_buckets[h] = i;
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_cap;
    table->mask = mask;
}

static void RehashNormBuckets(SYMBOL_TABLE *table, uint32_t new_cap)
{
    uint32_t *new_nb = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
    if (new_nb == NULL)
        return;

    uint32_t mask = new_cap - 1;
    for (uint32_t i = 0; i < new_cap; i++)
        new_nb[i] = EMPTY_BUCKET;

    for (uint32_t i = 0; i < table->count; i++)
    {
        char norm[256];
        strncpy(norm, table->items[i].name, sizeof(norm) - 1);
        norm[sizeof(norm) - 1] = '\0';
        NormalizeDiacritics(norm);

        uint32_t h = HashString(norm) & mask;
        while (new_nb[h] != EMPTY_BUCKET)
            h = (h + 1) & mask;
        new_nb[h] = table->items[i].id;
    }

    free(table->norm_buckets);
    table->norm_buckets = new_nb;
    table->norm_capacity = new_cap;
    table->norm_mask = mask;
}

static void NormIndexInsert(SYMBOL_TABLE *table, const char *normalized, SYMBOL_ID id)
{
    if (table->norm_capacity == 0)
        return;

    /* Rehash at 70% load */
    /* Count entries: we approximate by checking how many non-empty */
    uint32_t used = 0;
    for (uint32_t i = 0; i < table->norm_capacity; i++)
        if (table->norm_buckets[i] != EMPTY_BUCKET)
            used++;
    if (used >= table->norm_capacity)
        return;
    if (used * HASH_LOAD_FACTOR_DEN >= table->norm_capacity * HASH_LOAD_FACTOR_NUM)
        RehashNormBuckets(table, table->norm_capacity * 2);

    uint32_t mask = table->norm_mask;
    uint32_t h = HashString(normalized) & mask;
    while (table->norm_buckets[h] != EMPTY_BUCKET)
        h = (h + 1) & mask;
    table->norm_buckets[h] = id;
}

SYMBOL_TABLE *SymbolTableCreate(uint32_t capacity)
{
    capacity = NextPowerOfTwo(capacity);

    SYMBOL_TABLE *table = (SYMBOL_TABLE *)malloc(sizeof(SYMBOL_TABLE));
    if (table == NULL)
        return NULL;

    SymbolTableInit(table, capacity);
    if (table->items == NULL)
    {
        free(table);
        return NULL;
    }
    return table;
}

void SymbolTableInit(SYMBOL_TABLE *table, uint32_t capacity)
{
    if (table == NULL)
        return;

    capacity = NextPowerOfTwo(capacity);

    table->items = (SYMBOL *)calloc(capacity, sizeof(SYMBOL));
    table->buckets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    if (table->items == NULL || table->buckets == NULL)
    {
        free(table->items);
        free(table->buckets);
        memset(table, 0, sizeof(*table));
        return;
    }
    table->count = 0;
    table->capacity = capacity;
    table->mask = capacity - 1;

    for (uint32_t i = 0; i < capacity; i++)
        table->buckets[i] = EMPTY_BUCKET;

    /* Diacritics index: same capacity as main table */
    table->norm_capacity = capacity;
    table->norm_mask = capacity - 1;
    table->norm_buckets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    if (table->norm_buckets == NULL)
    {
        table->norm_capacity = 0;
        table->norm_mask = 0;
        return;
    }
    for (uint32_t i = 0; i < capacity; i++)
        table->norm_buckets[i] = EMPTY_BUCKET;
}

void SymbolTableDestroy(SYMBOL_TABLE *table)
{
    if (table == NULL)
        return;

    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].name)
            free(table->items[i].name);
    }
    free(table->items);
    free(table->buckets);
    free(table->norm_buckets);
}

SYMBOL_ID SymbolAdd(SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL)
        return SYMBOL_INVALID;
    if (table->items == NULL || table->buckets == NULL || table->capacity == 0)
        return SYMBOL_INVALID;

    /* Try exact match first */
    SYMBOL_ID existing = SymbolFind(table, name);
    if (existing != SYMBOL_INVALID)
        return existing;

    /* Rehash at 70% load */
    if (table->count * HASH_LOAD_FACTOR_DEN >= table->capacity * HASH_LOAD_FACTOR_NUM)
    {
        RehashBuckets(table, table->capacity * 2);
        RehashNormBuckets(table, table->norm_capacity * 2);
    }

    if (table->count >= table->capacity)
        return SYMBOL_INVALID;

    uint32_t idx = table->count;
    SYMBOL *sym = &table->items[idx];
    sym->id = idx + 1;
    sym->name = strdup(name);
    if (sym->name == NULL)
        return SYMBOL_INVALID;
    sym->frequency = 1;
    table->count++;

    /* Insert into main hash table (exact name) */
    uint32_t h = HashString(name) & table->mask;
    while (table->buckets[h] != EMPTY_BUCKET)
        h = (h + 1) & table->mask;
    table->buckets[h] = idx;

    /* Insert into diacritics index (normalized name) */
    char norm[256];
    strncpy(norm, name, sizeof(norm) - 1);
    norm[sizeof(norm) - 1] = '\0';
    NormalizeDiacritics(norm);
    NormIndexInsert(table, norm, sym->id);

    return sym->id;
}

SYMBOL_ID SymbolFind(const SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL || table->count == 0)
        return SYMBOL_INVALID;

    /* 1. Exact match (fast path) */
    uint32_t h = HashString(name) & table->mask;
    while (table->buckets[h] != EMPTY_BUCKET)
    {
        uint32_t idx = table->buckets[h];
        if (strcmp(table->items[idx].name, name) == 0)
            return table->items[idx].id;
        h = (h + 1) & table->mask;
    }

    /* 2. Diacritics fallback: normalize input, look up in norm index */
    if (table->norm_capacity == 0 || table->norm_buckets == NULL)
        return SYMBOL_INVALID;
    char norm[256];
    strncpy(norm, name, sizeof(norm) - 1);
    norm[sizeof(norm) - 1] = '\0';
    NormalizeDiacritics(norm);

    uint32_t nh = HashString(norm) & table->norm_mask;
    while (table->norm_buckets[nh] != EMPTY_BUCKET)
    {
        SYMBOL_ID sid = table->norm_buckets[nh];
        /* Verify by comparing normalized stored name */
        const SYMBOL *sym = SymbolGet(table, sid);
        if (sym)
        {
            char snorm[256];
            strncpy(snorm, sym->name, sizeof(snorm) - 1);
            snorm[sizeof(snorm) - 1] = '\0';
            NormalizeDiacritics(snorm);
            if (strcmp(snorm, norm) == 0)
                return sid;
        }
        nh = (nh + 1) & table->norm_mask;
    }

    return SYMBOL_INVALID;
}

const SYMBOL *SymbolGet(const SYMBOL_TABLE *table, SYMBOL_ID id)
{
    if (table == NULL || id == SYMBOL_INVALID || id > table->count)
        return NULL;
    return &table->items[id - 1];
}

void SymbolIncrementFrequency(SYMBOL_TABLE *table, SYMBOL_ID id)
{
    if (table == NULL || id == SYMBOL_INVALID || id > table->count)
        return;
    table->items[id - 1].frequency++;
}

uint32_t SymbolCount(const SYMBOL_TABLE *table)
{
    return table ? table->count : 0;
}
