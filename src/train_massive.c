#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static double GetTimeSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

#include "model.h"
#include "ingest.h"
#include "prune.h"

#define BUFFER_SIZE 65536

static void PrintUsage(const char *prog)
{
    fprintf(stderr, "Usage: %s <input.tsv> [model.bin] [min_count]\n", prog);
}

/* Direct TSV ingestion with 64KB I/O buffer */
static uint64_t TrainFromTSV(MODEL *model, const char *tsv_path)
{
    FILE *f = fopen(tsv_path, "r");
    if (!f)
    {
        fprintf(stderr, "Error abriendo: %s\n", tsv_path);
        return 0;
    }

    char file_buf[BUFFER_SIZE];
    setvbuf(f, file_buf, _IOFBF, sizeof(file_buf));

    INGEST_STATS stats = IngestTSVStreamSrc(model->graph, f, tsv_path);

    fclose(f);
    return stats.relations_inserted + stats.relations_updated;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        PrintUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_path = argv[1];
    const char *output_path = argc > 2 ? argv[2] : "model.bin";
    uint32_t min_count = argc > 3 ? (uint32_t)atoi(argv[3]) : 3;

    printf("============================================================\n");
    printf("  SYMBOLIC LLM - MASSIVE CORPUS INGEST (Streaming)         \n");
    printf("============================================================\n\n");

    double t0, t1;

    /* 1. Open input */
    printf("1. Abriendo corpus: %s\n", input_path);
    FILE *f = fopen(input_path, "r");
    if (f == NULL)
    {
        fprintf(stderr, "Error: no se pudo abrir '%s'\n", input_path);
        return EXIT_FAILURE;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("   Tamano: %.2f MB\n\n", (double)file_size / (1024.0 * 1024.0));
    fclose(f);

    /* 2. Create model */
    printf("2. Inicializando modelo...\n");
    t0 = GetTimeSeconds();

    MODEL *model = ModelCreate(65536, 65536);
    if (model == NULL)
    {
        fprintf(stderr, "Error: no se pudo crear el modelo.\n");
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();
    printf("   Modelo creado en %.4f s\n\n", t1 - t0);

    /* 3. Stream ingest with 64KB buffer */
    printf("3. Ingestando corpus en streaming (buffer=64KB)...\n");
    t0 = GetTimeSeconds();

    uint64_t lines = TrainFromTSV(model, input_path);

    t1 = GetTimeSeconds();
    double ingest_time = t1 - t0;

    printf("   Lineas procesadas: %llu\n", (unsigned long long)lines);
    printf("   Tiempo de ingest : %.4f s\n", ingest_time);
    printf("   Velocidad        : %.0f lineas/seg\n\n", lines / ingest_time);

    printf("   Estado post-ingest:\n");
    printf("     Simbolos  : %u\n", SymbolCount(model->graph->symbols));
    printf("     Relaciones: %u\n\n", RelationCount(model->graph->relations));

    /* 4. Prune */
    printf("4. Podando relaciones con count < %u...\n", min_count);
    t0 = GetTimeSeconds();

    PRUNE_STATS prune = PruneByMinCount(model->graph, min_count);

    t1 = GetTimeSeconds();

    printf("   Antes     : %u\n", prune.relations_before);
    printf("   Despues   : %u\n", prune.relations_after);
    printf("   Eliminadas: %u (%.1f%%)\n",
           prune.relations_removed,
           prune.relations_before > 0 ?
               100.0 * prune.relations_removed / prune.relations_before : 0.0);
    printf("   Tiempo de poda: %.4f s\n\n", t1 - t0);

    /* 5. Save */
    printf("5. Guardando modelo: %s\n", output_path);
    t0 = GetTimeSeconds();

    if (!ModelSave(model, output_path))
    {
        fprintf(stderr, "Error al guardar.\n");
        ModelDestroy(model);
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();

    FILE *fout = fopen(output_path, "rb");
    fseek(fout, 0, SEEK_END);
    long out_size = ftell(fout);
    fclose(fout);

    printf("   Tamano: %.2f MB (%ld bytes)\n",
           (double)out_size / (1024.0 * 1024.0), out_size);
    printf("   Tiempo: %.4f s\n\n", t1 - t0);

    /* 6. Summary */
    printf("============================================================\n");
    printf("Resumen:\n");
    printf("  Lineas procesadas: %llu\n", (unsigned long long)lines);
    printf("  Relaciones finales: %u\n", prune.relations_after);
    printf("  Simbolos        : %u\n", SymbolCount(model->graph->symbols));
    printf("  Tamano en disco : %.2f MB\n", (double)out_size / (1024.0 * 1024.0));
    printf("  Modelo          : %s\n", output_path);
    printf("============================================================\n");

    ModelDestroy(model);
    return EXIT_SUCCESS;
}
