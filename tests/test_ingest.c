#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "ingest.h"
#include "prune.h"
#include "model.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

static void WriteTestCorpus(const char *filepath)
{
    FILE *f = fopen(filepath, "w");
    if (f == NULL) return;

    /* Known relations (will be repeated to build count) */
    for (int i = 0; i < 10; i++)
    {
        fprintf(f, "GATO\tCOME\tPESCADO\n");
        fprintf(f, "GATO\tCOME\tCARNE\n");
        fprintf(f, "PERRO\tCOME\tCARNE\n");
        fprintf(f, "GATO\tES\tANIMAL\n");
        fprintf(f, "PERRO\tES\tANIMAL\n");
    }

    /* Rare relations (count=1, should be pruned) */
    fprintf(f, "GATO\tDURME\tSILLA\n");
    fprintf(f, "PEZ\tNADA\tAGUA\n");

    /* Comments and empty lines (should be skipped) */
    fprintf(f, "# This is a comment\n");
    fprintf(f, "\n");
    fprintf(f, "  \n");
    fprintf(f, "ANTONIO\tPROGRAMA\tHARBOUR\n");
    fprintf(f, "ANTONIO\tCOMPILA\tHBMK2\n");

    fclose(f);
}

int main(void)
{
    printf("==================================================\n");
    printf("  SYMBOLIC LLM - INGEST + PRUNE PIPELINE         \n");
    printf("==================================================\n\n");

    const char *corpus = "test_corpus.tsv";

    /* 1. Write test corpus */
    printf("1. Generando corpus TSV de prueba...\n");
    WriteTestCorpus(corpus);
    printf("   Archivo: %s\n\n", corpus);

    /* 2. Create graph and ingest */
    printf("2. Ingestando corpus en el grafo...\n");
    GRAPH *graph = GraphCreate(256, 256);
    Assert(graph != NULL, "GraphCreate");

    INGEST_STATS stats = IngestTSV(graph, corpus);

    printf("   Lineas leidas    : %llu\n", (unsigned long long)stats.lines_read);
    printf("   Lineas parseadas : %llu\n", (unsigned long long)stats.lines_parsed);
    printf("   Lineas fallidas  : %llu\n", (unsigned long long)stats.lines_failed);
    printf("   Relaciones nuevas: %llu\n", (unsigned long long)stats.relations_inserted);
    printf("   Relaciones actualizadas: %llu\n", (unsigned long long)stats.relations_updated);
    printf("   Simbolos: %u | Relaciones: %u\n\n",
           SymbolCount(graph->symbols), RelationCount(graph->relations));

    Assert(stats.lines_read > 0, "Debe leer lineas");
    Assert(stats.relations_inserted > 0, "Debe insertar relaciones");
    Assert(RelationCount(graph->relations) > 0, "Grafo no vacio");

    /* 3. Verify known relations */
    printf("3. Verificando relaciones conocidas...\n");
    SYMBOL_ID gato = SymbolFind(graph->symbols, "GATO");
    SYMBOL_ID come = SymbolFind(graph->symbols, "COME");
    SYMBOL_ID pez = SymbolFind(graph->symbols, "PESCADO");

    Assert(gato != SYMBOL_INVALID, "GATO registrado");
    Assert(come != SYMBOL_INVALID, "COME registrado");
    Assert(pez != SYMBOL_INVALID, "PESCADO registrado");

    RELATION *r = GraphFindRelation(graph, gato, come, pez);
    Assert(r != NULL, "GATO-COME-PESCADO existe");
    printf("   GATO-COME-PESCADO: count=%llu, weight=%.3f\n",
           (unsigned long long)r->count, r->weight);
    Assert(r->count >= 10, "count >= 10 (repeated 10 times)\n");

    /* 4. Prune by min count */
    printf("\n4. Podando relaciones con count < 3...\n");
    PRUNE_STATS prune = PruneByMinCount(graph, 3);

    printf("   Antes     : %u\n", prune.relations_before);
    printf("   Despues   : %u\n", prune.relations_after);
    printf("   Eliminadas: %u\n\n", prune.relations_removed);

    Assert(prune.relations_removed > 0, "Debe eliminar relaciones raras");

    /* Verify GATO-COME-PESCADO survived pruning */
    r = GraphFindRelation(graph, gato, come, pez);
    Assert(r != NULL, "GATO-COME-PESCADO sobrevive poda");
    printf("   GATO-COME-PESCADO post-poda: count=%llu OK\n",
           (unsigned long long)r->count);

    /* Verify rare relations were removed */
    SYMBOL_ID durme = SymbolFind(graph->symbols, "DURME");
    SYMBOL_ID silla = SymbolFind(graph->symbols, "SILLA");
    if (durme != SYMBOL_INVALID && silla != SYMBOL_INVALID)
    {
        RELATION *rare = GraphFindRelation(graph, durme, SymbolFind(graph->symbols, "DURME"), silla);
        Assert(rare == NULL, "GATO-DURME-SILLA eliminada por poda");
        printf("   GATO-DURME-SILLA: eliminada (count=1) OK\n");
    }

    /* 5. Save and reload */
    printf("\n5. Guardando modelo podado...\n");
    MODEL *model = (MODEL *)malloc(sizeof(MODEL));
    model->graph = graph;
    model->embeddings = EmbeddingTableCreate(64);
    model->config = LearningConfigDefault();
    GraphSetEmbeddingTable(graph, model->embeddings);

    Assert(ModelSave(model, "test_ingest.bin") == 1, "ModelSave");

    FILE *f = fopen("test_ingest.bin", "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fclose(f);
    printf("   Tamano: %ld bytes\n", fsize);

    ModelDestroy(model);

    /* 6. Reload and verify */
    printf("\n6. Recargando modelo...\n");
    MODEL *loaded = ModelLoad("test_ingest.bin");
    Assert(loaded != NULL, "ModelLoad");

    printf("   Simbolos  : %u\n", SymbolCount(loaded->graph->symbols));
    printf("   Relaciones: %u\n", RelationCount(loaded->graph->relations));

    r = GraphFindRelation(loaded->graph,
                          SymbolFind(loaded->graph->symbols, "GATO"),
                          SymbolFind(loaded->graph->symbols, "COME"),
                          SymbolFind(loaded->graph->symbols, "PESCADO"));
    Assert(r != NULL, "GATO-COME-PESCADO post-reload");
    printf("   GATO-COME-PESCADO: count=%llu OK\n",
           (unsigned long long)r->count);

    /* Cleanup */
    ModelDestroy(loaded);
    remove("test_ingest.bin");
    remove(corpus);

    printf("\n==================================================\n");
    printf("Pipeline ingest + prune + persist verificado OK.\n");
    printf("==================================================\n");

    return EXIT_SUCCESS;
}
