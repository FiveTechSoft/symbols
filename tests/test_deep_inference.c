#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "inference.h"

static void Assert(int condition, const char *msg)
{
    if (!condition)
    {
        printf("FAIL: %s\n", msg);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    printf("========================================================\n");
    printf("     SYMBOLIC LLM - RAZONAMIENTO PROFUNDO (CHAINING)    \n");
    printf("========================================================\n\n");

    GRAPH *graph = GraphCreate(64, 128);

    SYMBOL_ID siames   = GraphAddSymbol(graph, "SIAMES");
    SYMBOL_ID gato     = GraphAddSymbol(graph, "GATO");
    SYMBOL_ID felino   = GraphAddSymbol(graph, "FELINO");
    SYMBOL_ID mamifero = GraphAddSymbol(graph, "MAMIFERO");
    SYMBOL_ID animal   = GraphAddSymbol(graph, "ANIMAL");

    SYMBOL_ID es       = GraphAddSymbol(graph, "ES");
    SYMBOL_ID tiene    = GraphAddSymbol(graph, "TIENE");
    SYMBOL_ID necesita = GraphAddSymbol(graph, "NECESITA");

    SYMBOL_ID pulmones = GraphAddSymbol(graph, "PULMONES");
    SYMBOL_ID agua     = GraphAddSymbol(graph, "AGUA");

    GraphAddRelation(graph, siames, es, gato);
    GraphAddRelation(graph, gato, es, felino);
    GraphAddRelation(graph, felino, es, mamifero);
    GraphAddRelation(graph, mamifero, es, animal);

    GraphAddRelation(graph, mamifero, tiene, pulmones);
    GraphAddRelation(graph, animal, necesita, agua);

    printf("Base de conocimiento inicial:\n");
    printf("  SIAMES   --ES--> GATO\n");
    printf("  GATO     --ES--> FELINO\n");
    printf("  FELINO   --ES--> MAMIFERO\n");
    printf("  MAMIFERO --ES--> ANIMAL\n");
    printf("  MAMIFERO --TIENE--> PULMONES\n");
    printf("  ANIMAL   --NECESITA--> AGUA\n\n");

    Assert(GraphFindRelation(graph, siames, tiene, pulmones) == NULL,
           "No debe existir relacion explicita inicial");

    printf("--- Test 1: Demostrar si SIAMES TIENE PULMONES (4 saltos) ---\n");
    INFERENCE_PATH path;
    INFERENCE_CONFIG cfg = InferenceConfigDefault();
    cfg.max_depth = 5;

    int proven = InferenceProve(graph, siames, tiene, pulmones, &cfg, &path);
    Assert(proven == 1, "Debe demostrar que SIAMES TIENE PULMONES");

    InferencePrintExplanation(graph, &path);
    printf("\n");

    printf("--- Test 2: Demostrar si SIAMES NECESITA AGUA (5 saltos) ---\n");
    proven = InferenceProve(graph, siames, necesita, agua, &cfg, &path);
    Assert(proven == 1, "Debe demostrar que SIAMES NECESITA AGUA");

    InferencePrintExplanation(graph, &path);
    printf("\n");

    printf("--- Test 3: Materializar composiciones ES + TIENE => TIENE ---\n");
    COMPOSITION_RULE rule_tiene;
    rule_tiene.pred_first = es;
    rule_tiene.pred_second = tiene;
    rule_tiene.pred_result = tiene;
    rule_tiene.rule_weight = 0.95f;

    uint32_t nuevas = InferenceApplyCompositionRule(graph, &rule_tiene, &cfg);
    printf("Relaciones de propiedad deducidas e incorporadas: %u\n", nuevas);

    RELATION *felino_pulm = GraphFindRelation(graph, felino, tiene, pulmones);
    Assert(felino_pulm != NULL, "FELINO TIENE PULMONES debe haberse materializado");
    printf("  Verificado: FELINO --TIENE--> PULMONES (peso: %.3f)\n", felino_pulm->weight);

    printf("\n--- Test 4: Consulta negativa (SIAMES TIENE ALAS) ---\n");
    SYMBOL_ID alas = GraphAddSymbol(graph, "ALAS");
    proven = InferenceProve(graph, siames, tiene, alas, &cfg, &path);
    Assert(proven == 0, "NO debe poder demostrar que SIAMES TIENE ALAS");
    printf("  Resultado: No deducible en el grafo (correcto, evita alucinacion).\n");

    GraphDestroy(graph);

    printf("\n========================================================\n");
    printf("Razonamiento profundo e inferencia validados con exito.\n");
    printf("========================================================\n");

    return EXIT_SUCCESS;
}
