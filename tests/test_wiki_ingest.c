#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "ingest.h"
#include "prune.h"
#include "model.h"

int main(void)
{
    printf("=== WIKIPEDIA INGEST + INFERENCE TEST ===\n\n");

    GRAPH *graph = GraphCreate(1024, 32);
    if (!graph) { printf("FAIL: GraphCreate\n"); return 1; }

    printf("1. Ingesting wiki_test.tsv...\n");
    INGEST_STATS ir = IngestTSV(graph, "wiki_corpus.tsv");
    printf("   Lines read:    %llu\n", ir.lines_read);
    printf("   Lines parsed:  %llu\n", ir.lines_parsed);
    printf("   Relations new: %llu\n", ir.relations_inserted);
    printf("   Relations upd: %llu\n", ir.relations_updated);
    printf("   Symbols:       %u\n", SymbolCount(graph->symbols));
    printf("   Relations:     %u\n", RelationCount(graph->relations));

    printf("\n2. Sample relations:\n");
    uint32_t shown = 0;
    for (uint32_t i = 0; i < RelationCount(graph->relations) && shown < 10; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r)
        {
            const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
            const SYMBOL *p = SymbolGet(graph->symbols, r->relation);
            const SYMBOL *o = SymbolGet(graph->symbols, r->object);
            if (s && p && o)
            {
                printf("   %s --%s--> %s (count=%llu)\n",
                       s->name, p->name, o->name, r->count);
                shown++;
            }
        }
    }

    printf("\n3. Pruning (min_count=1)...\n");
    PRUNE_STATS ps = PruneByMinCount(graph, 1);
    printf("   Before: %u, After: %u, Removed: %u\n",
           ps.relations_before, ps.relations_after, ps.relations_removed);

    printf("\n4. Saving model...\n");
    MODEL *model = ModelCreate(256, 256);
    if (model)
    {
        GraphDestroy(model->graph);
        model->graph = graph;
        if (ModelSave(model, "wiki_model_ingest_test.bin"))
            printf("   OK\n");
        else
            printf("   FAIL\n");
        model->graph = NULL;
        ModelDestroy(model);
    }

    printf("\n5. Loading model and testing inference...\n");
    MODEL *m2 = ModelLoad("wiki_model_ingest_test.bin");
    if (m2 && m2->graph)
    {
        GRAPH *g2 = m2->graph;
        printf("   Loaded: %u symbols, %u relations\n",
               SymbolCount(g2->symbols), RelationCount(g2->relations));

        SYMBOL_ID spain = SymbolFind(g2->symbols, "ESPAÑA");
        if (spain != SYMBOL_INVALID)
        {
            printf("\n   Querying: ESPAÑA ?\n");
            RELATION *results[16];
            uint32_t n = GraphQuerySubject(g2, spain, results, 16);
            printf("   Found %u direct relations:\n", n);
            for (uint32_t i = 0; i < n; i++)
            {
                const SYMBOL *rel = SymbolGet(g2->symbols, results[i]->relation);
                const SYMBOL *obj = SymbolGet(g2->symbols, results[i]->object);
                if (rel && obj)
                    printf("     ESPAÑA --%s--> %s\n", rel->name, obj->name);
            }
        }
        else
        {
            printf("   ESPAÑA not found. First symbols: ");
            for (uint32_t i = 0; i < 10 && i < SymbolCount(g2->symbols); i++)
            {
                const SYMBOL *s = SymbolGet(g2->symbols, (SYMBOL_ID)i);
                if (s) printf("%s ", s->name);
            }
            printf("\n");
        }

        ModelDestroy(m2);
    }

    GraphDestroy(graph);
    remove("wiki_model_ingest_test.bin");
    printf("\n=== TEST COMPLETE ===\n");
    return 0;
}
