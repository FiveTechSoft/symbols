#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "relation.h"
#include "graph.h"
#include "learning.h"
#include "embedding.h"

static void Assert(int cond, const char *msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    printf("========================================\n");
    printf("  SYMBOLIC LLM - FUZZY QUERY (H4)      \n");
    printf("========================================\n\n");

    /* 1. Graph with only GATO */
    GRAPH *graph = GraphCreate(64, 64);
    Assert(graph != NULL, "GraphCreate");

    LearningSentence(graph, "El gato come pescado.");
    LearningSentence(graph, "El gato es animal.");

    printf("Grafo inicial: %u relaciones\n", RelationCount(graph->relations));

    SYMBOL_ID s_gato = SymbolFind(graph->symbols, "GATO");
    SYMBOL_ID s_felino = SymbolAdd(graph->symbols, "FELINO");

    Assert(s_gato != SYMBOL_INVALID, "GATO registrado");
    Assert(s_felino != SYMBOL_INVALID, "FELINO registrado");

    /* 2. Create embedding table and attach it to the graph */
    EMBEDDING_TABLE *embeds = EmbeddingTableCreate(64);
    GraphSetEmbeddingTable(graph, embeds);

    /* Vectores iniciales aleatorios */
    float v_gato[EMBEDDING_DIM];
    float v_felino[EMBEDDING_DIM];
    EmbeddingRandomInit(v_gato, 42);
    EmbeddingRandomInit(v_felino, 99);
    EmbeddingSetVector(embeds, s_gato, v_gato);
    EmbeddingSetVector(embeds, s_felino, v_felino);

    /* 3. Train co-occurrence so FELINO ~ GATO */
    printf("Entrenando coocurrencia FELINO ~ GATO...\n");
    for (int i = 0; i < 10; i++)
    {
        float buf_gato[EMBEDDING_DIM];
        float buf_felino[EMBEDDING_DIM];
        const float *src_g = EmbeddingGetVector(embeds, s_gato);
        const float *src_f = EmbeddingGetVector(embeds, s_felino);
        memcpy(buf_gato, src_g, sizeof(buf_gato));
        memcpy(buf_felino, src_f, sizeof(buf_felino));
        EmbeddingCooccur(buf_gato, src_g, 0.2f);
        EmbeddingCooccur(buf_felino, src_g, 0.2f);
        EmbeddingSetVector(embeds, s_gato, buf_gato);
        EmbeddingSetVector(embeds, s_felino, buf_felino);
    }

    float sim = EmbeddingCosineSimilarity(
        EmbeddingGetVector(embeds, s_gato),
        EmbeddingGetVector(embeds, s_felino));
    printf("Coseno(FELINO, GATO) = %.3f\n\n", sim);
    Assert(sim > 0.70f, "FELINO y GATO deben ser similares");

    /* 4. Consulta exacta: GATO + COME -> PEZ */
    RELATION *results[16];
    uint32_t n = GraphQuerySubjectPredicate(graph, s_gato,
        SymbolFind(graph->symbols, "COME"), results, 16);

    printf("Consulta exacta GATO+COME: %u resultados\n", n);
    Assert(n > 0, "GATO debe tener relaciones COME");

    const SYMBOL *obj = SymbolGet(graph->symbols, results[0]->object);
    printf("  GATO --COME--> %s\n\n", obj->name);

    /* 5. Consulta fuzzy: FELINO + COME -> debe resolver a GATO */
    SYMBOL_ID pred_come = SymbolFind(graph->symbols, "COME");
    SYMBOL_ID resolved = SYMBOL_INVALID;

    n = GraphQuerySubjectPredicateFuzzy(
        graph, s_felino, pred_come, results, 16,
        SIMILARITY_THRESHOLD_DEFAULT, &resolved);

    printf("Consulta fuzzy FELINO+COME:\n");
    printf("  Resolved a: %s\n",
           SymbolGet(graph->symbols, resolved)->name);
    printf("  Resultados: %u\n", n);
    Assert(n > 0, "FELINO+COME debe resolver via embedding");
    Assert(resolved == s_gato, "Debe resolver a GATO");

    obj = SymbolGet(graph->symbols, results[0]->object);
    printf("  FELINO --COME--> %s\n\n", obj->name);
    Assert(strcmp(obj->name, "PESCADO") == 0, "Objeto debe ser PESCADO");

    /* 6. AUTOMOVIL must not resolve */
    SYMBOL_ID s_auto = SymbolAdd(graph->symbols, "AUTOMOVIL");
    float v_auto[EMBEDDING_DIM];
    EmbeddingRandomInit(v_auto, 777);
    EmbeddingSetVector(embeds, s_auto, v_auto);

    n = GraphQuerySubjectPredicateFuzzy(
        graph, s_auto, pred_come, results, 16,
        SIMILARITY_THRESHOLD_DEFAULT, &resolved);

    printf("Consulta fuzzy AUTOMOVIL+COME: %u resultados\n", n);
    Assert(n == 0, "AUTOMOVIL no debe resolver a nada");

    /* Limpieza */
    EmbeddingTableDestroy(embeds);
    GraphDestroy(graph);

    printf("\n========================================\n");
    printf("Consulta hibrida simbolico-vectorial OK.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
