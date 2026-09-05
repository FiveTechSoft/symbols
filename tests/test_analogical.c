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
        const SYMBOL *p = SymbolGet(g->symbols, results[i]->predicate);
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

    printf("=== TRANSFER RULES TEST ===\n\n");

    /* 1. Test TransferApply on MALLOC */
    printf("1. TransferApply on MALLOC:\n");
    SYMBOL_ID sid = SymbolFind(g->symbols, "MALLOC");
    if (sid != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferApply(g, sid, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* 2. Test TransferApply on STRCPY */
    printf("2. TransferApply on STRCPY:\n");
    sid = SymbolFind(g->symbols, "STRCPY");
    if (sid != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferApply(g, sid, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* 3. Test TransferSimilarity MALLOC vs STRCPY */
    printf("3. Structural similarity MALLOC <-> STRCPY:\n");
    SYMBOL_ID mid = SymbolFind(g->symbols, "MALLOC");
    SYMBOL_ID sid2 = SymbolFind(g->symbols, "STRCPY");
    if (mid != SYMBOL_INVALID && sid2 != SYMBOL_INVALID)
    {
        float sim = TransferSimilarity(g, mid, sid2);
        printf("   Similarity: %.2f\n\n", sim);
    }

    /* 4. Test TransferAnalogy: what MALLOC knows that STRCPY doesn't */
    printf("4. Analogy: MALLOC -> STRCPY (transfer knowledge):\n");
    if (mid != SYMBOL_INVALID && sid2 != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferAnalogy(g, mid, sid2, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* 5. Test TransferApply on CALLOC (should also need FREE) */
    printf("5. TransferApply on CALLOC:\n");
    sid = SymbolFind(g->symbols, "CALLOC");
    if (sid != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferApply(g, sid, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* 6. Test TransferApply on FOPEN */
    printf("6. TransferApply on FOPEN:\n");
    sid = SymbolFind(g->symbols, "FOPEN");
    if (sid != SYMBOL_INVALID)
    {
        TRANSFER_RESULT results[8];
        uint32_t n = TransferApply(g, sid, results, 8);
        TransferPrintResults(g, results, n);
    }

    /* 7. Analogical chain: MALLOC -> REALLOC */
    printf("7. Analogy: MALLOC -> REALLOC:\n");
    sid = SymbolFind(g->symbols, "REALLOC");
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
