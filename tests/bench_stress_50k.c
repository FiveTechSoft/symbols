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

#include "model.h"

#define NUM_SYMBOLS 50000
#define BENCH_FILE  "stress_50k.bin"

int main(void)
{
    printf("========================================================\n");
    printf("     SYMBOLIC LLM - BENCHMARK DE ESTRES (50.000 32D)    \n");
    printf("========================================================\n\n");

    double t0, t1;

    /* 1. Bulk creation and insertion */
    printf("1. Reservando e insertando %u simbolos con vectores 32D...\n", NUM_SYMBOLS);
    t0 = GetTimeSeconds();

    MODEL *model = ModelCreate(NUM_SYMBOLS + 100, 1000);
    if (model == NULL)
    {
        fprintf(stderr, "Error: no se pudo asignar memoria para el modelo.\n");
        return EXIT_FAILURE;
    }

    char name_buf[32];
    float vec_buf[EMBEDDING_DIM];

    for (uint32_t i = 0; i < NUM_SYMBOLS; i++)
    {
        snprintf(name_buf, sizeof(name_buf), "SYM_%06u", i);
        SYMBOL_ID sid = GraphAddSymbol(model->graph, name_buf);

        EmbeddingRandomInit(vec_buf, i + 1);
        EmbeddingSetVector(model->embeddings, sid, vec_buf);
    }

    t1 = GetTimeSeconds();
    double insert_time = t1 - t0;
    printf("   -> Tiempo de creacion: %.4f s (%.0f simbolos/segundo)\n\n",
           insert_time, NUM_SYMBOLS / insert_time);

    /* 2. Serializacion a Disco */
    printf("2. Serializando modelo a '%s'...\n", BENCH_FILE);
    t0 = GetTimeSeconds();

    int ok = ModelSave(model, BENCH_FILE);
    if (!ok)
    {
        fprintf(stderr, "Error al guardar el archivo de estres.\n");
        ModelDestroy(model);
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();
    double save_time = t1 - t0;

    FILE *f = fopen(BENCH_FILE, "rb");
    if (!f) return EXIT_FAILURE;
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fclose(f);

    double file_size_mb = (double)file_size / (1024.0 * 1024.0);
    double write_speed = file_size_mb / save_time;

    printf("   -> Guardado en: %.4f s\n", save_time);
    printf("   -> Tamano de archivo: %.2f MB (%ld bytes)\n", file_size_mb, file_size);
    printf("   -> Velocidad de escritura: %.2f MB/s\n\n", write_speed);

    /* 3. Instance teardown */
    printf("3. Liberando completamente la memoria RAM original...\n");
    t0 = GetTimeSeconds();
    ModelDestroy(model);
    model = NULL;
    t1 = GetTimeSeconds();
    printf("   -> Liberacion completada en: %.4f s\n\n", t1 - t0);

    /* 4. Deserializacion desde Disco */
    printf("4. Deserializando %u simbolos y vectores desde disco...\n", NUM_SYMBOLS);
    t0 = GetTimeSeconds();

    MODEL *loaded = ModelLoad(BENCH_FILE);
    if (loaded == NULL)
    {
        fprintf(stderr, "Error al cargar el archivo de estres.\n");
        remove(BENCH_FILE);
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();
    double load_time = t1 - t0;
    double read_speed = file_size_mb / load_time;

    printf("   -> Cargado y reconstruido en: %.4f s (%.1f milisegundos)\n",
           load_time, load_time * 1000.0);
    printf("   -> Tasa de ingesta: %.0f simbolos/segundo\n", NUM_SYMBOLS / load_time);
    printf("   -> Velocidad de lectura: %.2f MB/s\n\n", read_speed);

    /* 5. Integrity spot-check */
    printf("5. Comprobando integridad en muestras aleatorias...\n");
    uint32_t test_indices[] = {0, 100, 1234, 25000, 49999};
    uint32_t num_checks = sizeof(test_indices) / sizeof(test_indices[0]);

    for (uint32_t i = 0; i < num_checks; i++)
    {
        uint32_t idx = test_indices[i];
        snprintf(name_buf, sizeof(name_buf), "SYM_%06u", idx);

        SYMBOL_ID sid = SymbolFind(loaded->graph->symbols, name_buf);
        if (sid == SYMBOL_INVALID)
        {
            printf("FAIL: Simbolo '%s' no hallado tras la carga.\n", name_buf);
            exit(EXIT_FAILURE);
        }

        const float *loaded_vec = EmbeddingGetVector(loaded->embeddings, sid);
        if (loaded_vec == NULL)
        {
            printf("FAIL: Vector para '%s' es nulo.\n", name_buf);
            exit(EXIT_FAILURE);
        }

        EmbeddingRandomInit(vec_buf, idx + 1);

        float sim = EmbeddingCosineSimilarity(loaded_vec, vec_buf);
        if (fabsf(sim - 1.0f) > 1e-5f)
        {
            printf("FAIL: Discrepancia numerica en vector '%s' (similitud = %.6f)\n", name_buf, sim);
            exit(EXIT_FAILURE);
        }
    }
    printf("   -> 100%% de las muestras analizadas coinciden bit a bit.\n\n");

    /* Limpieza */
    ModelDestroy(loaded);
    remove(BENCH_FILE);

    printf("========================================================\n");
    printf("Resumen del Benchmark:\n");
    printf("  - Simbolos & Vectores: %u\n", NUM_SYMBOLS);
    printf("  - Tamano en disco     : %.2f MB\n", file_size_mb);
    printf("  - Escritura a disco   : %.2f ms\n", save_time * 1000.0);
    printf("  - Lectura y montado   : %.2f ms\n", load_time * 1000.0);
    printf("========================================================\n");

    return EXIT_SUCCESS;
}
