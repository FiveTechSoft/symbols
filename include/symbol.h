#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdint.h>

typedef uint32_t SYMBOL_ID;

#define SYMBOL_INVALID 0
#define SYMBOL_SLOT_EMPTY UINT32_MAX

typedef struct
{
    SYMBOL_ID id;
    char     *name;
    uint64_t  frequency;
} SYMBOL;

typedef struct
{
    SYMBOL    *items;
    uint32_t  *buckets;
    uint32_t   count;
    uint32_t   capacity;
    uint32_t   mask;
} SYMBOL_TABLE;

SYMBOL_TABLE *SymbolTableCreate(uint32_t capacity);
void SymbolTableInit(SYMBOL_TABLE *table, uint32_t capacity);
void SymbolTableDestroy(SYMBOL_TABLE *table);

SYMBOL_ID SymbolAdd(SYMBOL_TABLE *table, const char *name);
SYMBOL_ID SymbolFind(const SYMBOL_TABLE *table, const char *name);
const SYMBOL *SymbolGet(const SYMBOL_TABLE *table, SYMBOL_ID id);
void SymbolIncrementFrequency(SYMBOL_TABLE *table, SYMBOL_ID id);
uint32_t SymbolCount(const SYMBOL_TABLE *table);

#endif
