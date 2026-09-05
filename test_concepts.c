#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "graph.h"
#include "symbol.h"
#include "embedding.h"
#include "model.h"

int main(void)
{
    MODEL *m = ModelLoad("wiki_model.bin");
    if (!m || !m->graph) { printf("FAIL: no model\n"); return 1; }
    GRAPH *graph = m->graph;

    /* Check what relations exist for FRANCIA */
    const char *concepts[] = {"FRANCIA", "PARIS", "BERLIN", "ALEMANIA", "TIERRA", NULL};
    for (int i = 0; concepts[i]; i++)
    {
        SYMBOL_ID sid = SymbolFind(graph->symbols, concepts[i]);
        if (sid != SYMBOL_INVALID)
        {
            printf("=== %s (id=%u) ===\n", concepts[i], sid);
            RELATION *rels[32];
            uint32_t n = GraphQuerySubject(graph, sid, rels, 32);
            for (uint32_t j = 0; j < n; j++)
            {
                const SYMBOL *pred = SymbolGet(graph->symbols, rels[j]->predicate);
                const SYMBOL *obj = SymbolGet(graph->symbols, rels[j]->object);
                if (pred && obj)
                    printf("  %s -> %s\n", pred->name, obj->name);
            }
            if (n == 0) printf("  (no relations)\n");
            printf("\n");
        }
        else
            printf("=== %s: NOT FOUND ===\n\n", concepts[i]);
    }

    ModelDestroy(m);
    return 0;
}
