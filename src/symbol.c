#include <stdlib.h>
#include <string.h>

#include "symbol.h"


SYMBOL_TABLE *SymbolTableCreate(uint32_t capacity)
{
    SYMBOL_TABLE *table;

    if (capacity == 0)
        capacity = 16;

    table = (SYMBOL_TABLE *)malloc(sizeof(SYMBOL_TABLE));
    if (table == NULL)
        return NULL;

    SymbolTableInit(table, capacity);
    return table;
}

void SymbolTableInit(SYMBOL_TABLE *table, uint32_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    table->items = (SYMBOL *)calloc(capacity, sizeof(SYMBOL));
    table->count = 0;
    table->capacity = capacity;
}

void SymbolTableDestroy(SYMBOL_TABLE *table)
{
    if (table == NULL)
        return;

    for (uint32_t i = 0; i < table->count; i++)
    {
        free(table->items[i].name);
    }
    free(table->items);
}

SYMBOL_ID SymbolAdd(SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL)
        return SYMBOL_INVALID;

    SYMBOL_ID existing = SymbolFind(table, name);
    if (existing != SYMBOL_INVALID)
        return existing;

    if (table->count >= table->capacity)
    {
        uint32_t new_cap = table->capacity * 2;
        SYMBOL *new_items = (SYMBOL *)realloc(
            table->items, new_cap * sizeof(SYMBOL));
        if (new_items == NULL)
            return SYMBOL_INVALID;

        memset(new_items + table->capacity, 0,
               (new_cap - table->capacity) * sizeof(SYMBOL));
        table->items = new_items;
        table->capacity = new_cap;
    }

    SYMBOL *sym = &table->items[table->count];
    sym->id = table->count + 1;
    sym->name = strdup(name);
    sym->frequency = 1;

    table->count++;
    return sym->id;
}

SYMBOL_ID SymbolFind(const SYMBOL_TABLE *table, const char *name)
{
    if (table == NULL || name == NULL)
        return SYMBOL_INVALID;

    for (uint32_t i = 0; i < table->count; i++)
    {
        if (table->items[i].name &&
            strcmp(table->items[i].name, name) == 0)
        {
            return table->items[i].id;
        }
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
