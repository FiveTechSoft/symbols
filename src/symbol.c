#include <stdlib.h>
#include <string.h>
#include "symbol.h"

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
    /* Grow items array to match new capacity */
    SYMBOL *new_items = (SYMBOL *)realloc(
        table->items, new_cap * sizeof(SYMBOL));
    if (new_items == NULL)
        return;
    memset(new_items + table->capacity, 0,
           (new_cap - table->capacity) * sizeof(SYMBOL));
    table->items = new_items;

    /* Rebuild buckets */
    uint32_t *new_buckets = (uint32_t *)malloc(new_cap * sizeof(uint32_t));
    if (new_buckets == NULL)
        return;

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

SYMBOL_TABLE *SymbolTableCreate(uint32_t capacity)
{
    capacity = NextPowerOfTwo(capacity);

    SYMBOL_TABLE *table = (SYMBOL_TABLE *)malloc(sizeof(SYMBOL_TABLE));
    if (table == NULL)
        return NULL;

    SymbolTableInit(table, capacity);
    return table;
}

void SymbolTableInit(SYMBOL_TABLE *table, uint32_t capacity)
{
    if (table == NULL)
        return;

    capacity = NextPowerOfTwo(capacity);

    table->items = (SYMBOL *)calloc(capacity, sizeof(SYMBOL));
    table->buckets = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    table->count = 0;
    table->capacity = capacity;
    table->mask = capacity - 1;

    for (uint32_t i = 0; i < capacity; i++)
        table->buckets[i] = EMPTY_BUCKET;
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
}

SYMBOL_ID SymbolAdd(SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL)
        return SYMBOL_INVALID;

    SYMBOL_ID existing = SymbolFind(table, name);
    if (existing != SYMBOL_INVALID)
        return existing;

    /* Rehash at 70% load */
    if (table->count * HASH_LOAD_FACTOR_DEN >= table->capacity * HASH_LOAD_FACTOR_NUM)
    {
        RehashBuckets(table, table->capacity * 2);
    }

    uint32_t idx = table->count;
    SYMBOL *sym = &table->items[idx];
    sym->id = idx + 1;
    sym->name = strdup(name);
    sym->frequency = 1;
    table->count++;

    /* Insert into hash table */
    uint32_t h = HashString(name) & table->mask;
    while (table->buckets[h] != EMPTY_BUCKET)
        h = (h + 1) & table->mask;
    table->buckets[h] = idx;

    return sym->id;
}

SYMBOL_ID SymbolFind(const SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL || table->count == 0)
        return SYMBOL_INVALID;

    uint32_t h = HashString(name) & table->mask;

    while (table->buckets[h] != EMPTY_BUCKET)
    {
        uint32_t idx = table->buckets[h];
        if (strcmp(table->items[idx].name, name) == 0)
            return table->items[idx].id;
        h = (h + 1) & table->mask;
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
