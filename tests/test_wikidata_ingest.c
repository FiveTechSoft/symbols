#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "ingest.h"
#include "prune.h"
#include "model.h"
#include "embedding.h"

int main(void)
{
    printf("=== WIKIDATA INFOBOX INGEST + INFERENCE ===\n\n");

    GRAPH *graph = GraphCreate(4096, 64);
    if (!graph) { printf("FAIL: GraphCreate\n"); return 1; }

    /* Attach embeddings for co-occurrence learning during ingestion */
    EMBEDDING_TABLE *embeddings = EmbeddingTableCreate(16384);
    GraphSetEmbeddingTable(graph, embeddings);

    /* 1. Ingest infobox triples */
    printf("1. Ingesting wikidata_clean.tsv (infoboxes)...\n");
    INGEST_STATS ir1 = IngestTSV(graph, "data/samples/wikidata_clean.tsv");
    printf("   Infoboxes: %llu triples, %u symbols\n",
           ir1.relations_inserted, SymbolCount(graph->symbols));

    /* 2. Ingest Wikipedia text triples */
    printf("2. Ingesting wiki_corpus.tsv (text extraction)...\n");
    INGEST_STATS ir2 = IngestTSV(graph, "wiki_corpus.tsv");
    printf("   Text:      %llu triples, %u symbols\n",
           ir2.relations_inserted, SymbolCount(graph->symbols));

    /* 3. Ingest C knowledge */
    printf("3. Ingesting c_knowledge.tsv (C programming)...\n");
    INGEST_STATS ir3 = IngestTSV(graph, "data/samples/c_knowledge.tsv");
    printf("   C code:    %llu triples, %u symbols\n",
           ir3.relations_inserted, SymbolCount(graph->symbols));

    /* 4. Ingest Psalms knowledge */
    printf("4. Ingesting psalms_knowledge.tsv (Bible Psalms)...\n");
    INGEST_STATS ir4 = IngestTSV(graph, "data/samples/psalms_knowledge.tsv");
    printf("   Psalms:    %llu triples, %u symbols\n",
           ir4.relations_inserted, SymbolCount(graph->symbols));

    /* 5. Ingest full Bible knowledge */
    printf("5. Ingesting bible_knowledge.tsv (full Bible)...\n");
    INGEST_STATS ir5 = IngestTSV(graph, "data/bible/bible_knowledge.tsv");
    printf("   Bible:     %llu triples, %u symbols\n",
           ir5.relations_inserted, SymbolCount(graph->symbols));

    /* 6. Ingest love knowledge */
    printf("6. Ingesting love_knowledge.tsv (love/amor)...\n");
    INGEST_STATS ir6 = IngestTSV(graph, "data/samples/love_knowledge.tsv");
    printf("   Love:      %llu triples, %u symbols\n",
           ir6.relations_inserted, SymbolCount(graph->symbols));

    /* 7. Ingest geographic knowledge */
    printf("7. Ingesting geo_knowledge.tsv (capitals, languages, currencies)...\n");
    INGEST_STATS ir7 = IngestTSV(graph, "data/samples/geo_knowledge.tsv");
    printf("   Geo:       %llu triples, %u symbols\n",
           ir7.relations_inserted, SymbolCount(graph->symbols));

    /* 8. Ingest Bible kinship (deterministic patterns, lint-gated) */
    printf("8. Ingesting bible_relations.tsv (kinship)...\n");
    INGEST_STATS ir8 = IngestTSV(graph, "data/bible/bible_relations.tsv");
    printf("   Kinship:   %llu triples, %u symbols\n",
           ir8.relations_inserted, SymbolCount(graph->symbols));

    printf("\n   TOTAL: %u relations, %u symbols\n",
           RelationCount(graph->relations), SymbolCount(graph->symbols));

    /* 8. Show sample relations */
    printf("\n8. Sample relations:\n");
    uint32_t shown = 0;
    for (uint32_t i = 0; i < RelationCount(graph->relations) && shown < 15; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r)
        {
            const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
            const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
            const SYMBOL *o = SymbolGet(graph->symbols, r->object);
            if (s && p && o)
            {
                printf("   %s --%s--> %s\n", s->name, p->name, o->name);
                shown++;
            }
        }
    }

    /* 9. Save model */
    printf("\n9. Saving model...\n");
    printf("   DEBUG: embeddings=%p count=%u\n", (void*)embeddings, embeddings ? embeddings->count : 0);
    fflush(stdout);
    {
        /* Debug: count initialized embeddings */
        int emb_init = 0;
        uint32_t emb_count = 0;
        if (embeddings) {
            emb_count = embeddings->count;
            for (uint32_t i = 0; i < embeddings->count; i++)
                if (embeddings->items[i].initialized) emb_init++;
        }
        printf("   Embedding table: %u items, %d initialized\n", emb_count, emb_init);
        fflush(stdout);

        /* Build MODEL manually to preserve the populated embedding table */
        MODEL model;
        model.graph = graph;
        model.embeddings = embeddings;
        model.config = LearningConfigDefault();
        int ok = ModelSave(&model, "wiki_model.bin");
        printf("   %s\n", ok ? "OK" : "FAIL");
        model.graph = NULL;
        model.embeddings = NULL;
    }

    /* 10. Load and test queries */
    printf("\n10. Loading and querying...\n");
    MODEL *m2 = ModelLoad("wiki_model.bin");
    if (m2 && m2->graph)
    {
        GRAPH *g2 = m2->graph;
        printf("   Loaded: %u symbols, %u relations\n",
               SymbolCount(g2->symbols), RelationCount(g2->relations));

        /* Query some known entities */
        const char *queries[] = {
            "ESPAÑA", "FRANCIA", "ALEMANIA", "MADRID", "BARCELONA",
            "ALBERT_EINSTEIN", "HARBOUR", "DAVID", "JESUS", "MOSES", NULL
        };

        for (int qi = 0; queries[qi]; qi++)
        {
            SYMBOL_ID sid = SymbolFind(g2->symbols, queries[qi]);
            if (sid == SYMBOL_INVALID) continue;

            printf("\n   Query: %s\n", queries[qi]);
            RELATION *results[16];
            uint32_t n = GraphQuerySubject(g2, sid, results, 16);
            for (uint32_t i = 0; i < n && i < 8; i++)
            {
                const SYMBOL *pred = SymbolGet(g2->symbols, results[i]->predicate);
                const SYMBOL *obj = SymbolGet(g2->symbols, results[i]->object);
                if (pred && obj)
                    printf("     --%s--> %s\n", pred->name, obj->name);
            }
            if (n == 0) printf("     (no relations)\n");
        }

        ModelDestroy(m2);
    }

    GraphDestroy(graph);
    printf("\n=== COMPLETE ===\n");
    return 0;
}
