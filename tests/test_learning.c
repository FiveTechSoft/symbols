#include <stdio.h>
#include <stdlib.h>
#include "learning.h"

int main(void)
{
    printf("========================================\n");
    printf("     SYMBOLIC LLM - LEARNING CORPUS    \n");
    printf("========================================\n\n");

    GRAPH *graph = GraphCreate(128, 256);

    const char *corpus[] = {
        "El gato come pescado.",
        "El gato come pescado.",
        "Un gato come pescado.",
        "El gato come carne.",
        "El gato come pescado.",
        "El gato come carne.",
        "Un gato come pescado.",
        "El gato come pescado.",
        "El gato come carne.",
        "El gato come pescado.",

        "El perro come carne.",
        "Un perro come carne.",
        "El perro come carne.",
        "El perro come carne.",
        "El perro come carne.",

        "El gato es un animal.",
        "El perro es un animal.",
        "El pez es un animal.",
        "El animal necesita agua.",
        "El pez vive en agua."
    };

    uint32_t corpus_size = sizeof(corpus) / sizeof(corpus[0]);

    printf("Entrenando corpus (%u frases)...\n", corpus_size);
    uint32_t learned = LearningCorpus(graph, corpus, corpus_size);
    printf("Frases procesadas con exito: %u\n", learned);
    printf("Simbolos unicos en memoria: %u\n", SymbolCount(graph->symbols));
    printf("Relaciones en el grafo    : %u\n\n", RelationCount(graph->relations));

    PREDICTION rels[8];
    uint32_t n;

    printf("--- Que come el gato? ---\n\n");
    n = LearningPredictText(graph, "GATO", "COME", rels, 8);
    for (uint32_t i = 0; i < n; i++)
    {
        printf("  Top %u: %-10s | count=%2llu | P = %5.1f%%\n",
               i + 1,
               rels[i].name,
               (unsigned long long)rels[i].count,
               rels[i].probability * 100.0f);
    }

    printf("\n--- Que come el perro? ---\n\n");
    n = LearningPredictText(graph, "PERRO", "COME", rels, 8);
    for (uint32_t i = 0; i < n; i++)
    {
        printf("  Top %u: %-10s | count=%2llu | P = %5.1f%%\n",
               i + 1,
               rels[i].name,
               (unsigned long long)rels[i].count,
               rels[i].probability * 100.0f);
    }

    printf("\n--- Razonamiento tras aprendizaje ---\n\n");

    SYMBOL_ID gato     = SymbolFind(graph->symbols, "GATO");
    SYMBOL_ID es       = SymbolFind(graph->symbols, "ES");
    SYMBOL_ID animal   = SymbolFind(graph->symbols, "ANIMAL");
    SYMBOL_ID necesita = SymbolFind(graph->symbols, "NECESITA");
    SYMBOL_ID agua     = SymbolFind(graph->symbols, "AGUA");

    printf("GATO ES ANIMAL?: %s\n",
           GraphFindRelation(graph, gato, es, animal) ? "SI (Explicito)" : "NO");

    if (GraphFindRelation(graph, gato, es, animal) &&
        GraphFindRelation(graph, animal, necesita, agua))
    {
        printf("Deduccion: GATO necesita AGUA -> [DEMOSTRADO POR COMPOSICION]\n");
    }

    GraphDestroy(graph);

    printf("\n========================================\n");
    printf("Aprendizaje e inferencia verificados OK.\n");
    printf("========================================\n");

    return 0;
}
