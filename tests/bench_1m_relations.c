#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
static double GetTimeSeconds(void)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
static double GetTimeSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

#include "symbol.h"
#include "relation.h"

#define NUM_RELATIONS 1000000
#define NUM_QUERIES   100000
#define BENCH_SEED    42

int main(void)
{
    printf("=========================================================\n");
    printf("  SYMBOLIC LLM - BENCHMARK 1M RELATIONS (HASH O(1))      \n");
    printf("=========================================================\n\n");

    double t0, t1;

    /* --------------------------------------------------------
       1. Crear tabla de simbolos y poblarla
       -------------------------------------------------------- */
    printf("1. Creando tabla de simbolos...\n");
    t0 = GetTimeSeconds();

    SYMBOL_TABLE *symbols = SymbolTableCreate(4096);
    for (uint32_t i = 0; i < 10000; i++)
    {
        char name[32];
        snprintf(name, sizeof(name), "SYM_%05u", i);
        SymbolAdd(symbols, name);
    }

    t1 = GetTimeSeconds();
    printf("   Simbolos: %u (en %.4f s)\n\n", SymbolCount(symbols), t1 - t0);

    /* --------------------------------------------------------
       2. Insertar 1M relaciones
       -------------------------------------------------------- */
    printf("2. Insertando %u relaciones unicas...\n", NUM_RELATIONS);
    t0 = GetTimeSeconds();

    RELATION_TABLE *relations = RelationTableCreate(NUM_RELATIONS + 1000);
    if (relations == NULL)
    {
        fprintf(stderr, "Error: no se pudo crear la tabla de relaciones.\n");
        return EXIT_FAILURE;
    }

    srand(BENCH_SEED);
    uint32_t inserted = 0;
    uint32_t duplicates = 0;

    for (uint32_t i = 0; i < NUM_RELATIONS; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;

        int ok = RelationAdd(relations, s, p, o);
        if (ok)
        {
            inserted++;
            /* RelationAdd returns 1 for both new and existing (incremented) */
        }
    }

    t1 = GetTimeSeconds();
    double insert_time = t1 - t0;

    printf("   Relaciones en tabla: %u\n", RelationCount(relations));
    printf("   Tiempo de insercion: %.4f s\n", insert_time);
    printf("   Tasa de insercion: %.0f rel/seg\n\n",
           RelationCount(relations) / insert_time);

    /* --------------------------------------------------------
       3. Benchmark de busqueda O(1)
       -------------------------------------------------------- */
    printf("3. Ejecutando %u consultas aleatorias...\n", NUM_QUERIES);
    t0 = GetTimeSeconds();

    srand(99999);
    uint32_t found_count = 0;

    for (uint32_t i = 0; i < NUM_QUERIES; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;

        RELATION *r = RelationFind(relations, s, p, o);
        if (r != NULL)
            found_count++;
    }

    t1 = GetTimeSeconds();
    double query_time = t1 - t0;

    printf("   Encontradas: %u / %u\n", found_count, NUM_QUERIES);
    printf("   Tiempo de busqueda: %.4f s\n", query_time);
    printf("   Latencia media: %.3f us/query\n\n",
           query_time * 1e6 / NUM_QUERIES);

    /* --------------------------------------------------------
       4. Medicion de memoria
       -------------------------------------------------------- */
    printf("4. Uso de memoria:\n");
    size_t sym_mem = (size_t)symbols->capacity * sizeof(SYMBOL) +
                     (size_t)symbols->capacity * sizeof(uint32_t);
    size_t rel_mem = (size_t)relations->capacity * sizeof(RELATION) +
                     (size_t)relations->capacity * sizeof(uint32_t);
    size_t str_mem = 0;
    for (uint32_t i = 0; i < symbols->count; i++)
    {
        if (symbols->items[i].name)
            str_mem += strlen(symbols->items[i].name) + 1;
    }

    printf("   Simbolos: %.2f MB (items + buckets + strings)\n",
           (double)(sym_mem + str_mem) / (1024.0 * 1024.0));
    printf("   Relaciones: %.2f MB (items + buckets)\n",
           (double)rel_mem / (1024.0 * 1024.0));
    printf("   Total estimado: %.2f MB\n\n",
           (double)(sym_mem + str_mem + rel_mem) / (1024.0 * 1024.0));

    /* --------------------------------------------------------
       5. Verificar integridad
       -------------------------------------------------------- */
    printf("5. Verificando integridad...\n");
    srand(BENCH_SEED);
    int errors = 0;

    for (uint32_t i = 0; i < 1000; i++)
    {
        SYMBOL_ID s = (rand() % 10000) + 1;
        SYMBOL_ID p = (rand() % 100) + 1;
        SYMBOL_ID o = (rand() % 10000) + 1;

        RELATION *r = RelationFind(relations, s, p, o);
        if (r != NULL)
        {
            if (r->subject != s || r->relation != p || r->object != o)
            {
                printf("   ERROR: tripleta corrupta en indice %u\n", i);
                errors++;
            }
        }
    }

    printf("   Muestras verificadas: 1000 | Errores: %d\n\n", errors);

    /* Limpieza */
    RelationTableDestroy(relations);
    SymbolTableDestroy(symbols);

    printf("=========================================================\n");
    printf("Resumen Benchmark 1M:\n");
    printf("  Relaciones      : %u\n", RelationCount(relations));
    printf("  Insercion       : %.2f ms (%.0f rel/seg)\n",
           insert_time * 1000.0, RelationCount(relations) / insert_time);
    printf("  Busquedas O(1)  : %.3f us/query\n",
           query_time * 1e6 / NUM_QUERIES);
    printf("  Memoria total   : %.2f MB\n",
           (double)(sym_mem + str_mem + rel_mem) / (1024.0 * 1024.0));
    printf("=========================================================\n");

    return EXIT_SUCCESS;
}
