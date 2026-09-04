#include "symbol.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_create_destroy(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(16);
    assert(t != NULL);
    assert(t->count == 0);
    assert(t->capacity == 16);
    SymbolTableDestroy(t);
    printf("  PASS test_create_destroy\n");
}

static void test_add_find(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(16);

    SYMBOL_ID gato = SymbolAdd(t, "GATO");
    assert(gato != SYMBOL_INVALID);
    assert(gato == 1);

    SYMBOL_ID perro = SymbolAdd(t, "PERRO");
    assert(perro != SYMBOL_INVALID);
    assert(perro == 2);

    SYMBOL_ID gato2 = SymbolAdd(t, "GATO");
    assert(gato2 == gato);

    assert(SymbolFind(t, "GATO") == gato);
    assert(SymbolFind(t, "PERRO") == perro);
    assert(SymbolFind(t, "PEZ") == SYMBOL_INVALID);

    SymbolTableDestroy(t);
    printf("  PASS test_add_find\n");
}

static void test_frequency(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(16);
    SYMBOL_ID id = SymbolAdd(t, "TEST");
    assert(id != SYMBOL_INVALID);

    const SYMBOL *s = SymbolGet(t, id);
    assert(s->frequency == 1);

    SymbolIncrementFrequency(t, id);
    s = SymbolGet(t, id);
    assert(s->frequency == 2);

    SymbolTableDestroy(t);
    printf("  PASS test_frequency\n");
}

static void test_bulk(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(8);
    char name[32];

    for (uint32_t i = 0; i < 1000; i++)
    {
        sprintf(name, "SYM_%u", i);
        SYMBOL_ID id = SymbolAdd(t, name);
        assert(id != SYMBOL_INVALID);
    }

    assert(t->count == 1000);

    for (uint32_t i = 0; i < 1000; i++)
    {
        sprintf(name, "SYM_%u", i);
        assert(SymbolFind(t, name) == i + 1);
    }

    SymbolTableDestroy(t);
    printf("  PASS test_bulk\n");
}

int main(void)
{
    printf("=== test_symbol ===\n");
    test_create_destroy();
    test_add_find();
    test_frequency();
    test_bulk();
    printf("All symbol tests passed.\n");
    return 0;
}
