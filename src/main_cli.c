#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "symbol.h"
#include "relation.h"
#include "embedding.h"
#include "graph.h"
#include "learning.h"
#include "context.h"
#include "inference.h"
#include "generator.h"
#include "dialog.h"
#include "model.h"
#include "stats.h"
#include "transfer.h"
#include "parser.h"

#define CLI_BUFFER_SIZE 512

static void CleanString(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
        len--;
    }
}

static void DumpGraph(const GRAPH *graph)
{
    printf("\n=== Estado del Grafo de Conocimiento ===\n");
    printf("Symbols loaded     : %u\n", SymbolCount(graph->symbols));
    printf("Relations in memory: %u\n\n", RelationCount(graph->relations));

    for (uint32_t i = 0; i < RelationCount(graph->relations); i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (!r) continue;

        const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
        const SYMBOL *p = SymbolGet(graph->symbols, r->predicate);
        const SYMBOL *o = SymbolGet(graph->symbols, r->object);

        if (s && p && o)
        {
            const char *pol = (r->polarity == POLARITY_NEGATIVE) ? "[NO] " : "";
            printf("  [%02u] %-12s --%s%-10s--> %-12s (obs=%llu, peso=%.2f)\n",
                   i + 1, s->name, pol, p->name, o->name,
                   (unsigned long long)r->count, r->weight);
        }
    }
    printf("========================================\n\n");
}

static void DumpContext(const CONTEXT *ctx)
{
    printf("\n=== Working Memory (Context) ===\n");
    printf("Entidades activas: %u (Tasa decaimiento: %.2f)\n\n",
           ctx->count, ctx->decay_rate);

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        const CONTEXT_ENTITY *e = &ctx->entities[i];
        printf("  - %-12s | Activacion: %5.1f%% | Hace %u turnos | Rol: %s\n",
               e->name, e->activation * 100.0f, e->turns_ago,
               e->was_subject ? "Sujeto" : "Objeto");
    }
    printf("=========================================\n\n");
}

int main(void)
{
    srand((unsigned int)time(NULL));

    GRAPH *graph = GraphCreate(128, 256);
    CONTEXT *ctx = ContextCreate();
    EMBEDDING_TABLE *embeds = EmbeddingTableCreate(128);

    if (!graph || !ctx || !embeds)
    {
        fprintf(stderr, "Error critico: no se pudieron inicializar las estructuras.\n");
        return EXIT_FAILURE;
    }

    GraphSetEmbeddingTable(graph, embeds);

    /* Auto-load default model if it exists */
    {
        FILE *ftest = fopen("wiki_model.bin", "rb");
        if (ftest != NULL)
        {
            fclose(ftest);
            MODEL *loaded = ModelLoad("wiki_model.bin");
            if (loaded != NULL)
            {
                GraphDestroy(graph);
                EmbeddingTableDestroy(embeds);
                graph = loaded->graph;
                embeds = loaded->embeddings;
                GraphSetEmbeddingTable(graph, embeds);
                ctx = ContextCreate();
                printf("IA > Model loaded from 'wiki_model.bin'.\n\n");
            }
        }
    }

    printf("========================================================\n");
    printf("     SYMBOLIC LLM\n");
    printf("  Natural conversation without backpropagation or GPUs.\n");
    printf("  Commands: /graph, /context, /stats, /query <word>,\n");
    printf("            /synonyms, /alias, /analogy A B,\n");
    printf("            /save, /load, /clear, /exit                  \n");
    printf("========================================================\n\n");

    char input[CLI_BUFFER_SIZE];
    char response[CLI_BUFFER_SIZE];

    while (1)
    {
        printf("You> ");
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        CleanString(input);
        if (strlen(input) == 0)
            continue;

        if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0)
        {
            printf("\nIA > Goodbye! All learned knowledge is saved.\n");
            break;
        }

        if (strcmp(input, "/graph") == 0)
        {
            DumpGraph(graph);
            continue;
        }

        if (strcmp(input, "/context") == 0)
        {
            DumpContext(ctx);
            continue;
        }

        if (strcmp(input, "/clear") == 0)
        {
            GraphDestroy(graph);
            ContextReset(ctx);
            EmbeddingTableDestroy(embeds);

            graph = GraphCreate(128, 256);
            embeds = EmbeddingTableCreate(128);
            GraphSetEmbeddingTable(graph, embeds);
            printf("IA > Memory cleared. Starting fresh.\n\n");
            continue;
        }

        if (strcmp(input, "/stats") == 0 || strcmp(input, "/info") == 0)
        {
            MODEL tmp;
            memset(&tmp, 0, sizeof(tmp));
            tmp.graph = graph;
            tmp.embeddings = embeds;
            ModelPrintReport(&tmp);
            continue;
        }

        if (strncmp(input, "/query ", 7) == 0)
        {
            const char *term = input + 7;
            SYMBOL_ID sid = SymbolFind(graph->symbols, term);
            if (sid == SYMBOL_INVALID)
            {
                printf("IA > I don't know '%s'.\n\n", term);
            }
            else
            {
                printf("\n=== Relations of '%s' ===\n", term);

                /* Attended relations: sorted by embedding relevance */
                RELATION *att_rel[64];
                float att_scores[64];
                uint32_t n_att = GraphQueryAttended(graph, sid, att_rel, att_scores, 64);

                if (n_att > 0)
                {
                    printf("  Attended (by relevance):\n");
                    for (uint32_t i = 0; i < n_att && i < 20; i++)
                    {
                        const SYMBOL *pred = SymbolGet(graph->symbols, att_rel[i]->predicate);
                        const SYMBOL *obj  = SymbolGet(graph->symbols, att_rel[i]->object);
                        if (pred && obj)
                            printf("    [%5.1f%%] --%s--> %s\n",
                                   att_scores[i] * 100.0f, pred->name, obj->name);
                    }
                }

                /* Relations as object */
                RELATION *as_obj[64];
                uint32_t n_obj = GraphQueryObject(graph, sid, as_obj, 64);

                printf("  As OBJECT (%u):\n", n_obj);
                for (uint32_t i = 0; i < n_obj && i < 30; i++)
                {
                    const SYMBOL *subj = SymbolGet(graph->symbols, as_obj[i]->subject);
                    const SYMBOL *pred = SymbolGet(graph->symbols, as_obj[i]->predicate);
                    if (subj && pred)
                        printf("    %s --%s-->\n", subj->name, pred->name);
                }

                /* Semantic similar words */
                EMBEDDING_MATCH matches[8];
                uint32_t n_sim = EmbeddingFindSimilar(graph->embeddings, sid, matches, 8);
                if (n_sim > 0)
                {
                    printf("  Similar words (vectorial):\n");
                    for (uint32_t i = 0; i < n_sim; i++)
                    {
                        const SYMBOL *s = SymbolGet(graph->symbols, matches[i].id);
                        if (s)
                            printf("    %s (%.2f)\n", s->name, matches[i].score);
                    }
                }

                printf("\n  Total: %u attended, %u as object, %u similar\n\n",
                       n_att, n_obj, n_sim);
            }
            continue;
        }

        /* /ask <query> — embed query, attend over ALL relations */
        if (strncmp(input, "/ask ", 5) == 0)
        {
            const char *query = input + 5;

            float qvec[EMBEDDING_DIM];
            SYMBOL_ID matched[32];
            uint32_t n_matched = 0;

            if (!GraphEmbedQuery(graph, query, qvec, matched, &n_matched))
            {
                printf("IA > No known concepts found in your query.\n\n");
                continue;
            }

            printf("\n=== Asking: \"%s\" ===\n", query);
            printf("  Matched %u concepts: ", n_matched);
            for (uint32_t i = 0; i < n_matched && i < 8; i++)
            {
                const SYMBOL *s = SymbolGet(graph->symbols, matched[i]);
                if (s) printf("%s ", s->name);
            }
            printf("\n\n");

            /* Full-graph attention */
            RELATION *att_rel[32];
            float att_scores[32];
            uint32_t n_att = GraphQueryByEmbedding(
                graph, qvec, att_rel, att_scores, 32);

            if (n_att > 0)
            {
                printf("  Most relevant facts:\n");
                for (uint32_t i = 0; i < n_att && i < 15; i++)
                {
                    const SYMBOL *s = SymbolGet(graph->symbols, att_rel[i]->subject);
                    const SYMBOL *p = SymbolGet(graph->symbols, att_rel[i]->predicate);
                    const SYMBOL *o = SymbolGet(graph->symbols, att_rel[i]->object);
                    if (s && p && o)
                        printf("    [%5.1f%%] %s --%s--> %s\n",
                               att_scores[i] * 100.0f,
                               s->name, p->name, o->name);
                }
            }
            else
            {
                printf("  No relevant facts found.\n");
            }
            printf("\n");
            continue;
        }

        if (strncmp(input, "/analogy ", 9) == 0)
        {
            char ent_a[64], ent_b[64];
            if (sscanf(input + 9, "%63s %63s", ent_a, ent_b) == 2)
            {
                SYMBOL_ID id_a = SymbolFind(graph->symbols, ent_a);
                SYMBOL_ID id_b = SymbolFind(graph->symbols, ent_b);
                if (id_a == SYMBOL_INVALID || id_b == SYMBOL_INVALID)
                {
                    printf("IA > I don't know one of those concepts.\n\n");
                }
                else
                {
                    float sim = TransferSimilarity(graph, id_a, id_b);
                    printf("  Similitud estructural: %.2f\n\n", sim);

                    TRANSFER_RESULT results[8];

                    /* Apply rules to target */
                    uint32_t n = TransferApply(graph, id_b, results, 8);
                    if (n > 0)
                    {
                        printf("  Reglas aplicadas a %s:\n", ent_b);
                        TransferPrintResults(graph, results, n);
                    }

                    /* Analogical transfer */
                    n = TransferAnalogy(graph, id_a, id_b, results, 8);
                    if (n > 0)
                    {
                        printf("  Analoga %s -> %s:\n", ent_a, ent_b);
                        TransferPrintResults(graph, results, n);
                    }

                    if (n == 0 && TransferApply(graph, id_b, results, 8) == 0)
                        printf("  No transfer rules found.\n\n");
                }
            }
            else
            {
                    printf("IA > Usage: /analogy ENTITY_A ENTITY_B\n\n");
            }
            continue;
        }

        if (strncmp(input, "/alias ", 7) == 0)
        {
            char new_term[64], base_term[64];
            if (sscanf(input + 7, "%63s %63s", new_term, base_term) == 2)
            {
                SYMBOL_ID id_new = GraphAddSymbol(graph, new_term);
                SYMBOL_ID id_base = GraphAddSymbol(graph, base_term);

                if (!EmbeddingGetVector(embeds, id_base))
                {
                    float v[EMBEDDING_DIM];
                    EmbeddingRandomInit(v, 42);
                    EmbeddingSetVector(embeds, id_base, v);
                }

                const float *base_v = EmbeddingGetVector(embeds, id_base);
                float clone_v[EMBEDDING_DIM];
                memcpy(clone_v, base_v, sizeof(clone_v));
                clone_v[0] += 0.02f;
                EmbeddingNormalize(clone_v);
                EmbeddingSetVector(embeds, id_new, clone_v);

                float sim = EmbeddingCosineSimilarity(base_v, clone_v);
                    printf("IA > Alias: '%s' ~ '%s' (similarity: %.1f%%).\n\n",
                       new_term, base_term, sim * 100.0f);
            }
            else
            {
                printf("Uso: /alias [nuevo] [existente]\n\n");
            }
            continue;
        }

        if (strncmp(input, "/synonyms ", 10) == 0)
        {
            char target[64];
            sscanf(input + 10, "%63s", target);
            SYMBOL_ID tid = SymbolFind(graph->symbols, target);
            if (tid == SYMBOL_INVALID || !EmbeddingGetVector(embeds, tid))
            {
                    printf("IA > '%s' has no embedding assigned.\n\n", target);
            }
            else
            {
                EMBEDDING_MATCH matches[8];
                uint32_t m_count = EmbeddingFindSimilar(embeds, tid, matches, 8);
                printf("\nSinonimos de '%s':\n", target);
                for (uint32_t i = 0; i < m_count; i++)
                {
                    const SYMBOL *sym = SymbolGet(graph->symbols, matches[i].id);
                    printf("  - %-12s | coseno=%.1f%%\n",
                           sym ? sym->name : "?", matches[i].score * 100.0f);
                }
                if (m_count == 0)
                    printf("  (ninguno cercano)\n");
                printf("\n");
            }
            continue;
        }

        if (strncmp(input, "/save ", 6) == 0)
        {
            const char *path = input + 6;
            MODEL temp;
            temp.graph = graph;
            temp.embeddings = embeds;
            temp.config = LearningConfigDefault();

            if (ModelSave(&temp, path))
                printf("IA > Model saved to '%s'.\n\n", path);
            else
                printf("IA > Error saving to '%s'.\n\n", path);
            continue;
        }

        if (strncmp(input, "/load ", 6) == 0)
        {
            const char *path = input + 6;
            MODEL *loaded = ModelLoad(path);
            if (loaded != NULL)
            {
                EmbeddingTableDestroy(embeds);
                GraphDestroy(graph);

                graph = loaded->graph;
                embeds = loaded->embeddings;
                GraphSetEmbeddingTable(graph, embeds);
                ContextReset(ctx);
                free(loaded);

                printf("IA > Model loaded from '%s'.\n\n", path);
            }
            else
            {
                printf("IA > Error loading '%s'.\n\n", path);
            }
            continue;
        }

        /* Dialog engine */
        if (DialogGenerateResponse(graph, ctx, input, response, sizeof(response)))
        {
            printf("IA > %s\n\n", response);

            /* Also try to extract structured S-P-O for better knowledge */
            int parsed = ParserIngestSentence(graph, input);
            if (parsed)
            {
                PARSED_SENTENCE toks;
                ParserTokenize(input, &toks);
                PARSE_RESULT spo = ParserExtractSPO(&toks);
                if (spo.valid)
                    printf("   Parsed: %s --%s--> %s\n\n",
                           spo.subject, spo.predicate, spo.object);
            }
        }
        else
        {
            /* Try natural language parser */
            int parsed = ParserIngestSentence(graph, input);
            if (parsed)
            {
                PARSED_SENTENCE toks;
                ParserTokenize(input, &toks);
                PARSE_RESULT spo = ParserExtractSPO(&toks);
                if (spo.valid)
                    printf("IA > Learned: %s --%s--> %s\n\n",
                           spo.subject, spo.predicate, spo.object);
                else
                    printf("IA > Noted. I'll remember that.\n\n");
            }
            else
            {
                printf("IA > Could not understand. Please rephrase.\n\n");
            }
        }
    }

    EmbeddingTableDestroy(embeds);
    ContextDestroy(ctx);
    GraphDestroy(graph);

    return EXIT_SUCCESS;
}
