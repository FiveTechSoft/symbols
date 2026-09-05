#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stats.h"
#include "symbol.h"
#include "relation.h"
#include "embedding.h"
#include "graph.h"
#include "model.h"

MODEL_STATS ModelGetStats(const MODEL *model)
{
    MODEL_STATS st;
    memset(&st, 0, sizeof(st));

    if (!model || !model->graph) return st;

    st.total_symbols   = SymbolCount(model->graph->symbols);
    st.total_relations = RelationCount(model->graph->relations);

    /* Approximate RAM footprint */
    size_t sym_mem = sizeof(SYMBOL_TABLE);
    if (model->graph->symbols)
        sym_mem += (size_t)model->graph->symbols->capacity * sizeof(SYMBOL);

    size_t rel_mem = sizeof(RELATION_TABLE);
    if (model->graph->relations)
        rel_mem += (size_t)model->graph->relations->capacity * sizeof(RELATION);

    size_t emb_mem = 0;
    if (model->embeddings)
    {
        emb_mem = sizeof(EMBEDDING_TABLE)
                + (size_t)model->embeddings->capacity * sizeof(SYMBOL_EMBEDDING);
    }

    st.memory_footprint_bytes = sizeof(MODEL) + sizeof(GRAPH)
                              + sym_mem + rel_mem + emb_mem;

    /* Count observations and find most connected symbol */
    uint32_t *out_degrees = (uint32_t *)calloc(st.total_symbols + 1, sizeof(uint32_t));
    if (!out_degrees) return st;

    for (uint32_t i = 0; i < st.total_relations; i++)
    {
        const RELATION *r = RelationGet(model->graph->relations, i);
        if (!r) continue;

        st.total_evidence_observations += r->count;

        if (r->subject <= st.total_symbols)
        {
            out_degrees[r->subject]++;
            if (out_degrees[r->subject] > st.max_out_degree)
            {
                st.max_out_degree = out_degrees[r->subject];
                const SYMBOL *s = SymbolGet(model->graph->symbols, r->subject);
                if (s && s->name)
                    strncpy(st.most_connected_symbol, s->name, 63);
            }
        }
    }
    free(out_degrees);

    /* Average degree */
    if (st.total_symbols > 0)
        st.average_degree = (float)st.total_relations / (float)st.total_symbols;

    /* Count initialized embeddings */
    if (model->embeddings)
    {
        for (uint32_t i = 0; i < model->embeddings->count; i++)
        {
            if (model->embeddings->items[i].initialized)
                st.total_embeddings_32d++;
        }
    }

    return st;
}


void ModelPrintReport(const MODEL *model)
{
    MODEL_STATS st = ModelGetStats(model);

    printf("========================================================\n");
    printf("        MODEL CAPACITY AND STATE REPORT        \n");
    printf("========================================================\n");

    printf("\n  1. Knowledge volume:\n");
    printf("     - Vocabulary (unique symbols) : %u\n", st.total_symbols);
    printf("     - Facts (unique relations)    : %u\n", st.total_relations);
    printf("     - Total observed evidence     : %llu samples\n",
           (unsigned long long)st.total_evidence_observations);
    printf("     - 32D semantic vectors        : %u\n", st.total_embeddings_32d);

    printf("\n  2. Graph topology:\n");
    printf("     - Mean connection density     : %.2f relations/symbol\n",
           st.average_degree);
    printf("     - Central concept ('Hub')     : %s (%u connections)\n",
           st.most_connected_symbol[0] ? st.most_connected_symbol : "N/A",
           st.max_out_degree);

    printf("\n  3. Compute efficiency:\n");
    printf("     - Hot RAM usage               : %.2f KB (%.4f MB)\n",
           (double)st.memory_footprint_bytes / 1024.0,
           (double)st.memory_footprint_bytes / (1024.0 * 1024.0));

    printf("\n  4. Knowledge quality:\n");
    if (st.total_symbols > 0 && st.average_degree >= 3.0f)
        printf("     - State: DENSE (coherent semantic areas)\n");
    else if (st.total_symbols > 0 && st.average_degree >= 1.0f)
        printf("     - State: MODERATE (sparse areas)\n");
    else
        printf("     - State: SCARCE (knowledge fragmented in islands)\n");

    if (st.total_evidence_observations > 0 && st.total_relations > 0)
    {
        double avg_count = (double)st.total_evidence_observations
                         / (double)st.total_relations;
        printf("     - Mean observations/fact: %.1f\n", avg_count);
    }

    printf("========================================================\n\n");
}
