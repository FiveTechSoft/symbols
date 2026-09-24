#include <stdio.h>
#include <string.h>
#include "symbol.h"
#include "relation.h"
#include "embedding.h"
#include "graph.h"
#include "model.h"

int main(void)
{
    MODEL *m = ModelLoad("data/wiki_model.bin");
    if (!m || !m->graph)
    {
        printf("Notice: wiki_model.bin not found, creating synthetic model for attention test...\n");
        m = ModelCreate(64, 64);
        if (!m || !m->graph) { printf("FAIL\n"); return 1; }
        const char *sample_names[] = {"CAPITAL", "MONEDA", "IDIOMA", "GOBIERNO",
                                      "FRANCIA", "PARIS", "EURO", "ALEMÁN", "BERLÍN", NULL};
        for (int i = 0; sample_names[i]; i++)
        {
            SYMBOL_ID sid = GraphAddSymbol(m->graph, sample_names[i]);
            float vec[EMBEDDING_DIM];
            EmbeddingRandomInit(vec, 100 + i);
            EmbeddingSetVector(m->embeddings, sid, vec);
        }
        SYMBOL_ID fr = SymbolFind(m->graph->symbols, "FRANCIA");
        SYMBOL_ID cap = SymbolFind(m->graph->symbols, "CAPITAL");
        SYMBOL_ID pa = SymbolFind(m->graph->symbols, "PARIS");
        GraphAddRelation(m->graph, fr, cap, pa);
    }
    GRAPH *g = m->graph;

    /* Check embeddings of key symbols */
    const char *names[] = {"CAPITAL", "MONEDA", "IDIOMA", "GOBIERNO",
                           "FRANCIA", "PARIS", "EURO", "ALEMÁN", "BERLÍN", NULL};

    printf("=== Embedding cosine matrix ===\n");
    printf("%-12s", "");
    for (int j = 0; names[j]; j++) printf("%-12s", names[j]);
    printf("\n");

    for (int i = 0; names[i]; i++)
    {
        SYMBOL_ID si = SymbolFind(g->symbols, names[i]);
        const float *vi = (si != SYMBOL_INVALID) ? EmbeddingGetVector(g->embeddings, si) : NULL;
        printf("%-12s", names[i]);
        for (int j = 0; names[j]; j++)
        {
            SYMBOL_ID sj = SymbolFind(g->symbols, names[j]);
            const float *vj = (sj != SYMBOL_INVALID) ? EmbeddingGetVector(g->embeddings, sj) : NULL;
            if (vi && vj)
                printf("%-12.3f", EmbeddingCosineSimilarity(vi, vj));
            else
                printf("%-12s", "N/A");
        }
        printf("\n");
    }

    /* Test: what does GraphEmbedQuery produce for "la capital de Francia es"? */
    printf("\n=== Query embedding test ===\n");
    float q_vec[EMBEDDING_DIM];
    SYMBOL_ID matched[16];
    uint32_t mc = 0;
    int ok = GraphEmbedQuery(g, "la capital de Francia es", q_vec, matched, &mc);
    printf("GraphEmbedQuery returned %d, matched %u tokens\n", ok, mc);
    for (uint32_t i = 0; i < mc; i++)
    {
        const SYMBOL *s = SymbolGet(g->symbols, matched[i]);
        printf("  token %u: %s\n", i, s ? s->name : "?");
    }

    /* Score top 5 relations by this query */
    printf("\nTop 10 relations for 'la capital de Francia es':\n");
    RELATION *rels[10];
    float scores[10];
    uint32_t n = GraphQueryByEmbedding(g, q_vec, rels, scores, 10);
    for (uint32_t i = 0; i < n; i++)
    {
        const SYMBOL *s = SymbolGet(g->symbols, rels[i]->subject);
        const SYMBOL *p = SymbolGet(g->symbols, rels[i]->relation);
        const SYMBOL *o = SymbolGet(g->symbols, rels[i]->object);
        printf("  %.3f  %s -> %s -> %s\n", scores[i],
            s ? s->name : "?", p ? p->name : "?", o ? o->name : "?");
    }

    ModelDestroy(m);
    return 0;
}
