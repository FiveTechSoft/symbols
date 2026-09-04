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
#include "shard.h"

#define BUFFER_SIZE 65536

static void PrintUsage(const char *prog)
{
    fprintf(stderr, "Usage: %s <input.tsv> [model.bin] [min_count] [num_shards]\n", prog);
}

static uint64_t TrainShardFromTSV(MODEL *model, const char *tsv_path)
{
    FILE *f = fopen(tsv_path, "r");
    if (!f)
        return 0;

    char file_buf[BUFFER_SIZE];
    setvbuf(f, file_buf, _IOFBF, sizeof(file_buf));

    char line[512];
    uint64_t lines = 0;
    char s[128], p[128], o[128];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (sscanf(line, "%127[^\t]\t%127[^\t]\t%127[^\n]", s, p, o) == 3)
        {
            SYMBOL_ID sid = GraphAddSymbol(model->graph, s);
            SYMBOL_ID pid = GraphAddSymbol(model->graph, p);
            SYMBOL_ID oid = GraphAddSymbol(model->graph, o);

            if (sid != SYMBOL_INVALID && pid != SYMBOL_INVALID &&
                oid != SYMBOL_INVALID)
            {
                RelationAdd(model->graph->relations, sid, pid, oid);
                lines++;
            }
        }
    }

    fclose(f);
    return lines;
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
    uint32_t num_shards = argc > 4 ? (uint32_t)atoi(argv[4]) : 4;

    printf("============================================================\n");
    printf("  SYMBOLIC LLM - MASSIVE INGEST (Sharded + Pruned)         \n");
    printf("============================================================\n\n");

    double t_total = GetTimeSeconds();
    double t0, t1;

    /* 1. Split corpus */
    printf("1. Particionando corpus en %u shards...\n", num_shards);
    t0 = GetTimeSeconds();

    SHARD_RESULT split = ShardSplit(input_path, num_shards, ".");

    t1 = GetTimeSeconds();
    printf("   Triples leidos: %llu\n", (unsigned long long)split.total_lines);
    printf("   Shards creados: %u\n", split.shards_created);
    printf("   Tiempo: %.4f s\n\n", t1 - t0);

    /* 2. Train each shard (sequential for now, ready for threading) */
    printf("2. Entrenando shards secuencialmente...\n");
    t0 = GetTimeSeconds();

    char shard_model_paths[MAX_SHARDS][64];
    const char *shard_model_ptrs[MAX_SHARDS];

    for (uint32_t i = 0; i < num_shards; i++)
    {
        snprintf(shard_model_paths[i], 64, "shard_%u.bin", i);
        shard_model_ptrs[i] = shard_model_paths[i];

        char shard_tsv[64];
        snprintf(shard_tsv, 64, "shard_%u.tsv", i);

        printf("  Entrenando shard %u/%u...\n", i + 1, num_shards);

        MODEL *shard_model = ModelCreate(16384, 16384);
        if (shard_model == NULL)
        {
            fprintf(stderr, "  Error creando modelo para shard %u\n", i);
            continue;
        }

        uint64_t lines = TrainShardFromTSV(shard_model, shard_tsv);

        if (!ModelSave(shard_model, shard_model_paths[i]))
        {
            fprintf(stderr, "  Error guardando shard %u\n", i);
        }
        else
        {
            printf("  Shard %u: %llu triples -> %s\n",
                   i, (unsigned long long)lines, shard_model_paths[i]);
        }

        ModelDestroy(shard_model);
    }

    t1 = GetTimeSeconds();
    printf("   Tiempo de entrenamiento: %.4f s\n\n", t1 - t0);

    /* 3. Merge all shards */
    printf("3. Fusionando %u shards...\n", num_shards);
    t0 = GetTimeSeconds();

    MODEL *merged = ShardMerge(shard_model_ptrs, num_shards, 65536, 65536);
    if (merged == NULL)
    {
        fprintf(stderr, "Error en la fusion.\n");
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();
    printf("   Tiempo de fusion: %.4f s\n\n", t1 - t0);

    /* 4. Prune */
    printf("4. Podando relaciones con count < %u...\n", min_count);
    t0 = GetTimeSeconds();

    PRUNE_STATS prune = PruneByMinCount(merged->graph, min_count);

    t1 = GetTimeSeconds();
    printf("   Antes     : %u\n", prune.relations_before);
    printf("   Despues   : %u\n", prune.relations_after);
    printf("   Eliminadas: %u (%.1f%%)\n",
           prune.relations_removed,
           prune.relations_before > 0 ?
               100.0 * prune.relations_removed / prune.relations_before : 0.0);
    printf("   Tiempo: %.4f s\n\n", t1 - t0);

    /* 5. Save final model */
    printf("5. Guardando modelo final: %s\n", output_path);
    t0 = GetTimeSeconds();

    if (!ModelSave(merged, output_path))
    {
        fprintf(stderr, "Error al guardar.\n");
        ModelDestroy(merged);
        return EXIT_FAILURE;
    }

    t1 = GetTimeSeconds();

    FILE *fout = fopen(output_path, "rb");
    fseek(fout, 0, SEEK_END);
    long out_size = ftell(fout);
    fclose(fout);

    printf("   Tamano: %.2f MB\n", (double)out_size / (1024.0 * 1024.0));
    printf("   Tiempo: %.4f s\n\n", t1 - t0);

    double t_final = GetTimeSeconds() - t_total;

    /* 6. Summary */
    printf("============================================================\n");
    printf("Resumen:\n");
    printf("  Corpus original   : %s\n", input_path);
    printf("  Triples procesados: %llu\n", (unsigned long long)split.total_lines);
    printf("  Shards            : %u\n", num_shards);
    printf("  Relaciones finales: %u\n", prune.relations_after);
    printf("  Simbolos          : %u\n", SymbolCount(merged->graph->symbols));
    printf("  Tamano en disco   : %.2f MB\n", (double)out_size / (1024.0 * 1024.0));
    printf("  Tiempo total      : %.4f s\n", t_final);
    printf("  Velocidad efectiva: %.0f triples/seg\n",
           split.total_lines / t_final);
    printf("  Modelo            : %s\n", output_path);
    printf("============================================================\n");

    ModelDestroy(merged);

    /* Cleanup shard files */
    for (uint32_t i = 0; i < num_shards; i++)
    {
        char path[64];
        snprintf(path, 64, "shard_%u.tsv", i);
        remove(path);
        remove(shard_model_paths[i]);
    }

    return EXIT_SUCCESS;
}
