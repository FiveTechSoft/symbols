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

static void test_arena_growth(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(8);
    char name[64];

    /* Force multiple arena reallocs with pointer fixups */
    for (uint32_t i = 0; i < 5000; i++)
    {
        sprintf(name, "ARENA_TEST_SYMBOL_%08u_%s", i,
                (i % 2) ? "padding_extension_longer" : "short");
        SYMBOL_ID id = SymbolAdd(t, name);
        assert(id != SYMBOL_INVALID);
    }
    assert(t->count == 5000);
    assert(t->arena != NULL);
    assert(t->arena_used > 0);

    /* All names readable and correct after realloc fixups */
    for (uint32_t i = 0; i < 5000; i++)
    {
        sprintf(name, "ARENA_TEST_SYMBOL_%08u_%s", i,
                (i % 2) ? "padding_extension_longer" : "short");
        assert(strcmp(SymbolGet(t, i + 1)->name, name) == 0);
    }

    /* Contiguity: last name's NUL is the final arena byte */
    const SYMBOL *last = SymbolGet(t, t->count);
    assert(last->name + strlen(last->name) + 1 ==
           t->arena + t->arena_used);

    SymbolTableDestroy(t);
    printf("  PASS test_arena_growth\n");
}

static void test_arena_no_leak(void)
{
    /* create -> intern -> destroy, 100 rounds; heap corruption or
       double-free would crash under ASan/gcc heap checking */
    for (int round = 0; round < 100; round++)
    {
        SYMBOL_TABLE *t = SymbolTableCreate(16);
        assert(t != NULL);
        for (uint32_t i = 0; i < 200; i++)
        {
            char name[32];
            sprintf(name, "LEAK_%u_%d", i, round);
            assert(SymbolAdd(t, name) != SYMBOL_INVALID);
        }
        SymbolTableDestroy(t);
    }
    printf("  PASS test_arena_no_leak\n");
}

static void test_intern_idempotent_1m(void)
{
    SYMBOL_TABLE *t = SymbolTableCreate(1024);
    char name[32];
    uint32_t unique = 1000;

    for (uint32_t i = 0; i < unique; i++)
    {
        sprintf(name, "SYM_%u", i);
        assert(SymbolAdd(t, name) == i + 1);
    }

    /* 1M idempotent lookups: every hit must map to the same ID */
    for (uint32_t round = 0; round < 1000; round++)
    {
        for (uint32_t i = 0; i < unique; i++)
        {
            sprintf(name, "SYM_%u", i);
            assert(SymbolFind(t, name) == i + 1);
        }
    }
    assert(t->count == 1000);

    /* Re-intern via Add: no new symbols, no arena growth */
    size_t used_before = t->arena_used;
    for (uint32_t round = 0; round < 10; round++)
    {
        for (uint32_t i = 0; i < unique; i++)
        {
            sprintf(name, "SYM_%u", i);
            assert(SymbolAdd(t, name) == i + 1);
        }
    }
    assert(t->count == 1000);
    assert(t->arena_used == used_before);

    SymbolTableDestroy(t);
    printf("  PASS test_intern_idempotent_1m\n");
}

int main(void)
{
    printf("=== test_symbol ===\n");
    test_create_destroy();
    test_add_find();
    test_frequency();
    test_bulk();
    test_arena_growth();
    test_arena_no_leak();
    test_intern_idempotent_1m();
    printf("All symbol tests passed.\n");
    return 0;
}
