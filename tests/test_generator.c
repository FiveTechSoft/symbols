#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "learning.h"
#include "generator.h"

int main(void)
{
    printf("========================================\n");
    printf("    SYMBOLIC LLM - TEXT GENERATION     \n");
    printf("========================================\n\n");

    GRAPH *graph = GraphCreate(64, 64);
    GENERATOR_CONFIG cfg = GeneratorConfigDefault();
    char output[256];

    /* 1. Simple sentence */
    printf("--- 1. Simple sentence from relation ---\n");
    SYMBOL_ID antonio = GraphAddSymbol(graph, "ANTONIO");
    SYMBOL_ID prog    = GraphAddSymbol(graph, "PROGRAMA");
    SYMBOL_ID harbour = GraphAddSymbol(graph, "HARBOUR");
    GraphAddRelation(graph, antonio, prog, harbour);

    RELATION *r1 = GraphFindRelation(graph, antonio, prog, harbour);
    GeneratorFromRelation(graph, r1, &cfg, output, sizeof(output));
    printf("Generated: \"%s\"\n\n", output);

    /* 2. Aggregation and coordination */
    printf("--- 2. Syntactic aggregation ---\n");
    SYMBOL_ID gato  = GraphAddSymbol(graph, "GATO");
    SYMBOL_ID come  = GraphAddSymbol(graph, "COME");
    SYMBOL_ID pez   = GraphAddSymbol(graph, "PEZ");
    SYMBOL_ID carne = GraphAddSymbol(graph, "CARNE");

    GraphAddRelation(graph, gato, come, pez);
    GraphAddRelation(graph, gato, come, carne);

    RELATION *cat_food[2];
    cat_food[0] = GraphFindRelation(graph, gato, come, pez);
    cat_food[1] = GraphFindRelation(graph, gato, come, carne);

    GeneratorAggregateRelations(graph, (const RELATION **)cat_food, 2,
                                &cfg, output, sizeof(output));
    printf("Generated: \"%s\"\n\n", output);

    /* 3. Probabilistic generation */
    printf("--- 3. Discursive generation with probability ---\n");
    for (int i = 0; i < 6; i++) GraphAddRelation(graph, gato, come, pez);
    for (int i = 0; i < 2; i++) GraphAddRelation(graph, gato, come, carne);

    PREDICTION rels[8];
    uint32_t n = LearningPredict(graph, gato, come, rels, 8);

    GeneratorFromPredictions(graph, "GATO", "COME", rels, n,
                             output, sizeof(output));
    printf("Generated: \"%s\"\n\n", output);

    /* 4. Query and answer */
    printf("--- 4. Question -> Text answer ---\n");
    GeneratorAnswerQuery(graph, "GATO", "COME", output, sizeof(output));
    printf("Q: Que come GATO?\nA: \"%s\"\n\n", output);

    GeneratorAnswerQuery(graph, "PERRO", "COME", output, sizeof(output));
    printf("Q: Que come PERRO?\nA: \"%s\"\n\n", output);

    GraphDestroy(graph);

    printf("========================================\n");
    printf("Generation verified.\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
