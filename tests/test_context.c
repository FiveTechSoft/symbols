#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"
#include "learning.h"
#include "context.h"

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
    printf("========================================\n");
    printf("     SYMBOLIC LLM - CONTEXT & ANAPHORA  \n");
    printf("========================================\n\n");

    GRAPH *graph = GraphCreate(64, 64);
    CONTEXT *ctx = ContextCreate();
    Assert(graph != NULL && ctx != NULL, "Initialization");

    /* Frase 1: "Antonio programa en Harbour." */
    printf("1. Procesando: \"Antonio programa en Harbour.\"\n");
    LearningSentence(graph, "Antonio programa en Harbour.");

    SYMBOL_ID antonio = GraphAddSymbol(graph, "ANTONIO");
    SYMBOL_ID harbour = GraphAddSymbol(graph, "HARBOUR");

    ContextPushEntity(ctx, antonio, "ANTONIO", 1);
    ContextPushEntity(ctx, harbour, "HARBOUR", 0);

    /* Frase 2: "El compila con hbmk2." */
    printf("\n2. Frase con pronombre: \"El compila con hbmk2.\"\n");

    char resolved[256];
    ContextPreprocessSentence(ctx, graph, "El compila con hbmk2.",
                              resolved, sizeof(resolved));
    printf("   Frase resuelta: \"%s\"\n", resolved);
    Assert(strstr(resolved, "ANTONIO") != NULL,
           "El debe resolver a ANTONIO");

    LearningSentence(graph, resolved);

    ContextStepTurn(ctx);
    SYMBOL_ID hbmk2 = GraphAddSymbol(graph, "HBMK2");
    ContextPushEntity(ctx, antonio, "ANTONIO", 1);
    ContextPushEntity(ctx, hbmk2, "HBMK2", 0);

    /* Sentence 3: elided subject */
    printf("\n3. Sujeto eliptico:\n");
    SYMBOL_ID implicit_subj = ContextResolveImplicitSubject(ctx);
    const SYMBOL *s = SymbolGet(graph->symbols, implicit_subj);
    printf("   Sujeto tacito: %s\n", s ? s->name : "?");
    Assert(implicit_subj == antonio,
           "El sujeto tacito debe ser ANTONIO");

    /* Check against the graph */
    printf("\n--- Relaciones aprendidas ---\n");

    RELATION *results[16];
    uint32_t n = GraphQuerySubject(graph, antonio, results, 16);

    for (uint32_t i = 0; i < n; i++)
    {
        const SYMBOL *sub = SymbolGet(graph->symbols, results[i]->subject);
        const SYMBOL *prd = SymbolGet(graph->symbols, results[i]->predicate);
        const SYMBOL *obj = SymbolGet(graph->symbols, results[i]->object);

        printf("  %s --%s--> %s\n", sub->name, prd->name, obj->name);
    }

    Assert(n >= 2, "ANTONIO tiene al menos 2 relaciones");

    ContextDestroy(ctx);
    GraphDestroy(graph);

    printf("\n========================================\n");
    printf("Context and anaphora tests passed!\n");
    printf("========================================\n");

    return EXIT_SUCCESS;
}
