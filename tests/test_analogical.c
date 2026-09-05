#include <stdio.h>
#include <string.h>
#include "model.h"
#include "transfer.h"

void query_subject(GRAPH *g, const char *name)
{
    SYMBOL_ID sid = SymbolFind(g->symbols, name);
    if (sid == SYMBOL_INVALID) { printf("   %s not found\n", name); return; }
    RELATION *results[16];
    uint32_t n = GraphQuerySubject(g, sid, results, 16);
    for (uint32_t i = 0; i < n; i++)
    {
        const SYMBOL *p = SymbolGet(g->symbols, results[i]->relation);
        const SYMBOL *o = SymbolGet(g->symbols, results[i]->object);
        if (p && o) printf("   %s --%s--> %s\n", name, p->name, o->name);
    }
    if (n == 0) printf("   (no relations)\n");
}

int main(void)
{
    MODEL *model = ModelLoad("wiki_model.bin");
    if (!model || !model->graph) { printf("FAIL\n"); return 1; }
    GRAPH *g = model->graph;

    printf("=== STRUCTURAL ANALOGY TEST ===\n\n");

    /* Structural similarity MALLOC <-> STRCPY */
    printf("1. Structural similarity MALLOC <-> STRCPY:\n");
    SYMBOL_ID mid = SymbolFind(g->symbols, "MALLOC");
    SYMBOL_ID sid2 = SymbolFind(g->symbols, "STRCPY");
    if (mid != SYMBOL_INVALID && sid2 != SYMBOL_INVALID)
    {
        float sim = TransferSimilarity(g, mid, sid2);
        printf("   Similarity: %.2f\n\n", sim);
    }

    /* Analogy: what MALLOC knows that STRCPY doesn't */
    printf("2. Analogy: MALLOC -> STRCPY (transfer knowledge):\n");
    if (mid != SYMBOL_INVALID && sid2 != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferAnalogy(g, mid, sid2, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* Analogical chain: MALLOC -> REALLOC */
    printf("3. Analogy: MALLOC -> REALLOC:\n");
    SYMBOL_ID sid = SymbolFind(g->symbols, "REALLOC");
    if (mid != SYMBOL_INVALID && sid != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferAnalogy(g, mid, sid, results, 8);
        TransferPrintResults(g, results, n);
    }

    ModelDestroy(model);
    printf("=== DONE ===\n");
    return 0;
}
