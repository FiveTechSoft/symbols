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

    /* Sentence 1: canonical shape (no medial preposition) so the
       syntax tree roots PROGRAMA on a virgin map. */
    printf("1. Processing: \"Antonio usa Pascal.\"\n");
    LearningSentence(graph, "Antonio usa Pascal.");

    SYMBOL_ID antonio = GraphAddSymbol(graph, "ANTONIO");
    SYMBOL_ID pascal = GraphAddSymbol(graph, "PASCAL");

    ContextPushEntity(ctx, antonio, "ANTONIO", 1);
    ContextPushEntity(ctx, pascal, "PASCAL", 0);

    /* Sentence 2: pronoun lead resolves structurally (unknown "El"
       takes the topicalized subject). Canonical shape, no preposition. */
    printf("\n2. Pronoun sentence: \"El compila fpc.\"\n");

    char resolved[256];
    ContextPreprocessSentence(ctx, graph, "El compila fpc.",
                              resolved, sizeof(resolved));
    printf("   Resolved sentence: \"%s\"\n", resolved);
    Assert(strstr(resolved, "ANTONIO") != NULL,
           "El must resolve to ANTONIO");

    LearningSentence(graph, resolved);

    ContextStepTurn(ctx);
    SYMBOL_ID fpc = GraphAddSymbol(graph, "FPC");
    ContextPushEntity(ctx, antonio, "ANTONIO", 1);
    ContextPushEntity(ctx, fpc, "FPC", 0);

    /* Sentence 3: elided subject */
    printf("\n3. Elided subject:\n");
    SYMBOL_ID implicit_subj = ContextResolveImplicitSubject(ctx);
    const SYMBOL *s = SymbolGet(graph->symbols, implicit_subj);
    printf("   Tacit subject: %s\n", s ? s->name : "?");
    Assert(implicit_subj == antonio,
           "Tacit subject must be ANTONIO");

    /* Check against the graph */
    printf("\n--- Relaciones aprendidas ---\n");

    RELATION *results[16];
    uint32_t n = GraphQuerySubject(graph, antonio, results, 16);

    for (uint32_t i = 0; i < n; i++)
    {
        const SYMBOL *sub = SymbolGet(graph->symbols, results[i]->subject);
        const SYMBOL *prd = SymbolGet(graph->symbols, results[i]->relation);
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
