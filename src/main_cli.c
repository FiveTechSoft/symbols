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
#include "stem.h"
#include "export.h"

/* Sufijo " [FUENTE]" para una relacion con procedencia, "" si no. */
static void SourceSuffix(const GRAPH *graph, const RELATION *r,
                         char *out, size_t size)
{
    if (out == NULL || size == 0)
        return;
    out[0] = '\0';
    if (graph == NULL || r == NULL || r->source == SYMBOL_INVALID)
        return;
    const SYMBOL *s = SymbolGet(graph->symbols, r->source);
    if (s != NULL && s->name != NULL)
        snprintf(out, size, " [%s]", s->name);
}

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
                ContextReset(ctx);
                printf("IA > Model loaded from 'wiki_model.bin'.\n\n");
            }
        }
    }

    printf("========================================================\n");
    printf("     SYMBOLIC LLM\n");
    printf("  Natural conversation without backpropagation or GPUs.\n");
    printf("  Commands: /graph, /context, /stats, /query <word>,\n");
  printf("            /find S P O (*), /why S P O, /export <f.dot|f.ttl>,\n");
  printf("            /synonyms, /alias, /analogy A B, /infer,\n");
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
            MODEL temp;
            temp.graph = graph;
            temp.embeddings = embeds;
            temp.config = LearningConfigDefault();
            if (ModelSave(&temp, "wiki_model.bin"))
                printf("\nIA > Goodbye! All learned knowledge is saved.\n");
            else
                printf("\nIA > Goodbye! (Warning: could not save wiki_model.bin).\n");
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
            GRAPH *new_graph = GraphCreate(128, 256);
            EMBEDDING_TABLE *new_embeds = EmbeddingTableCreate(128);

            if (!new_graph || !new_embeds)
            {
                printf("IA > Error: out of memory, keeping previous graph.\n\n");
                GraphDestroy(new_graph);
                EmbeddingTableDestroy(new_embeds);
                continue;
            }

            GraphDestroy(graph);
            EmbeddingTableDestroy(embeds);
            graph = new_graph;
            embeds = new_embeds;
            GraphSetEmbeddingTable(graph, embeds);
            ContextReset(ctx);
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
            SYMBOL_ID sid = StemFindSymbol(graph->symbols, term);
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

        /* /ask <query> â€” embed query, attend over ALL relations */
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

        /* Inference dry-run: /infer (read-only, nothing is written).
           Applies the compositional rules and lists what WOULD be written. */
        if (strcmp(input, "/infer") == 0)
        {
            static const struct { const char *first, *second, *result; } RULES[] = {
                { "ES", "TIENE", "TIENE" },       /* property inheritance */
                { "ES", "ES", "ES" },             /* transitive taxonomy */
                { "ES", "NECESITA", "NECESITA" }, /* needs inheritance */
            };
            INFERENCE_CONFIG cfg = InferenceConfigDefault();
            INFERRED_TRIPLE out[64];
            uint32_t total = 0;
            printf("\n/infer dry-run (0 written):\n");
            for (size_t ri = 0; ri < sizeof(RULES) / sizeof(RULES[0]); ri++)
            {
                SYMBOL_ID f = SymbolFind(graph->symbols, RULES[ri].first);
                SYMBOL_ID s = SymbolFind(graph->symbols, RULES[ri].second);
                SYMBOL_ID r = SymbolFind(graph->symbols, RULES[ri].result);
                if (f == SYMBOL_INVALID || s == SYMBOL_INVALID ||
                    r == SYMBOL_INVALID)
                    continue;
                COMPOSITION_RULE rule = { f, s, r, 0.95f };
                uint32_t n = InferenceDryRun(graph, &rule, &cfg, out, 64);
                for (uint32_t i = 0; i < n && total < 64; i++, total++)
                {
                    const SYMBOL *ss = SymbolGet(graph->symbols, out[i].subject);
                    const SYMBOL *pp = SymbolGet(graph->symbols, out[i].predicate);
                    const SYMBOL *oo = SymbolGet(graph->symbols, out[i].object);
                    if (total < 32)
                        printf("  %s --%s--> %s  [conf %.0f%% | %s+%s=>%s]\n",
                               ss ? ss->name : "?", pp ? pp->name : "?",
                               oo ? oo->name : "?",
                               out[i].confidence * 100.0f,
                               RULES[ri].first, RULES[ri].second,
                               RULES[ri].result);
                }
            }
            if (total > 32)
                printf("  ... and %u more (showing 32)\n", total - 32);
            printf("IA > %u candidates, 0 written (dry-run).\n\n", total);
            continue;
        }

        /* Multi-hop inference trace: /why S P O */
        if (strncmp(input, "/why ", 5) == 0)
        {
            char s[64] = {0}, p[64] = {0}, o[64] = {0};
            if (sscanf(input + 5, "%63s %63s %63s", s, p, o) == 3)
            {
                SYMBOL_ID sid = StemFindSymbol(graph->symbols, s);
                SYMBOL_ID pid = StemFindSymbol(graph->symbols, p);
                SYMBOL_ID oid = StemFindSymbol(graph->symbols, o);
                if (sid != SYMBOL_INVALID && pid != SYMBOL_INVALID &&
                    oid != SYMBOL_INVALID)
                {
                    INFERENCE_PATH path;
                    if (InferenceProve(graph, sid, pid, oid, NULL, &path))
                        InferencePrintExplanation(graph, &path);
                    else
                        printf("IA > No proof found: %s -/-> %s.\n\n", s, o);
                }
                else
                    printf("IA > Unknown symbol in /why arguments.\n\n");
            }
            else
                printf("IA > Usage: /why SUBJECT PREDICATE OBJECT\n\n");
            continue;
        }

        /* Wildcard triple search: /find S P O ('*' allowed) */
        if (strncmp(input, "/find ", 6) == 0)
        {
            char s[64] = {0}, p[64] = {0}, o[64] = {0};
            if (sscanf(input + 6, "%63s %63s %63s", s, p, o) == 3)
            {
                SYMBOL_ID sid = (strcmp(s, "*") == 0) ?
                    SYMBOL_INVALID : StemFindSymbol(graph->symbols, s);
                /* Canonicalize P/O filters through the stemmer so
                   inflected forms match stored names */
                const char *pname = p;
                const char *oname = o;
                char pcanon[64] = {0}, ocanon[64] = {0};
                if (strcmp(p, "*") != 0)
                {
                    SYMBOL_ID tpid = StemFindSymbol(graph->symbols, p);
                    const SYMBOL *ps = SymbolGet(graph->symbols, tpid);
                    if (ps && ps->name)
                    {
                        strncpy(pcanon, ps->name, sizeof(pcanon) - 1);
                        pname = pcanon;
                    }
                }
                if (strcmp(o, "*") != 0)
                {
                    SYMBOL_ID toid = StemFindSymbol(graph->symbols, o);
                    const SYMBOL *os = SymbolGet(graph->symbols, toid);
                    if (os && os->name)
                    {
                        strncpy(ocanon, os->name, sizeof(ocanon) - 1);
                        oname = ocanon;
                    }
                }
                RELATION *rels[64];
                uint32_t n = (sid != SYMBOL_INVALID) ?
                    GraphQuerySubject(graph, sid, rels, 64) :
                    RelationCount(graph->relations);
                uint32_t shown = 0;
                for (uint32_t i = 0; i < n && shown < 32; i++)
                {
                    const RELATION *r = (sid != SYMBOL_INVALID) ? rels[i] :
                        RelationGet(graph->relations, i);
                    if (!r) continue;
                    if (strcmp(p, "*") != 0 &&
                        SymbolGet(graph->symbols, r->predicate) &&
                        strcmp(SymbolGet(graph->symbols, r->predicate)->name, pname) != 0)
                        continue;
                    if (strcmp(o, "*") != 0 &&
                        SymbolGet(graph->symbols, r->object) &&
                        strcmp(SymbolGet(graph->symbols, r->object)->name, oname) != 0)
                        continue;
                    const SYMBOL *ss = SymbolGet(graph->symbols, r->subject);
                    const SYMBOL *pp = SymbolGet(graph->symbols, r->predicate);
                    const SYMBOL *oo = SymbolGet(graph->symbols, r->object);
                    if (ss && pp && oo)
                    {
                        char src[144];
                        SourceSuffix(graph, r, src, sizeof(src));
                        printf("  %s --%s--> %s%s\n",
                               ss->name, pp->name, oo->name, src);
                    }
                    shown++;
                }
                printf("IA > %u matching relation(s).\n\n", shown);
            }
            else
                printf("IA > Usage: /find SUBJECT PREDICATE OBJECT  ('*' = any)\n\n");
            continue;
        }

        /* Graph export: /export file.dot | file.ttl */
        if (strncmp(input, "/export ", 8) == 0)
        {
            const char *path = input + 8;
            size_t plen = strlen(path);
            int ok = 0;
            if (plen > 4 && strcmp(path + plen - 4, ".ttl") == 0)
                ok = GraphExportTurtle(graph, path);
            else
                ok = GraphExportDot(graph, path);
            printf("IA > %s '%s'.\n\n", ok ? "Exported to" :
                   "Export failed for", path);
            continue;
        }

        /* Kinship/count shapes bypass the attention scorer: ES/TIENE
           always match there and would hijack the question with a
           1-hop guess. These shapes have exact 2-hop/count answers. */
        {
            QUESTION kq = ParserDetectQuestion(input);
            if (kq.valid && (strcmp(kq.predicate, "ABUELO") == 0 ||
                             strncmp(kq.predicate, "CUENTA", 6) == 0))
            {
                DialogGenerateResponse(graph, ctx, input,
                                       response, sizeof(response));
                printf("IA > %s\n\n", response);
                continue;
            }
        }

        /* Attention + token-match hybrid query */
        if (graph != NULL && embeds != NULL)
        {
            PARSED_SENTENCE toks;
            ParserTokenize(input, &toks);

            float best_score = 0.0f;
            uint32_t best_subj = SYMBOL_INVALID;
            uint32_t best_pred = SYMBOL_INVALID;
            uint32_t best_obj  = SYMBOL_INVALID;

            /* For each query token, check if it matches a predicate name */
            for (uint32_t t = 0; t < toks.count; t++)
            {
                SYMBOL_ID pred_id = SymbolFind(graph->symbols, toks.tokens[t]);
                if (pred_id == SYMBOL_INVALID) continue;

                /* Check if this symbol is actually used as a predicate */
                int used_as_pred = 0;
                for (uint32_t r = 0; r < RelationCount(graph->relations); r++)
                {
                    const RELATION *rel = RelationGet(graph->relations, r);
                    if (rel && rel->predicate == pred_id) { used_as_pred = 1; break; }
                }
                if (!used_as_pred) continue;

                /* Found predicate match! Score each relation with this predicate */
                for (uint32_t r = 0; r < RelationCount(graph->relations); r++)
                {
                    const RELATION *rel = RelationGet(graph->relations, r);
                    if (rel == NULL || rel->predicate != pred_id) continue;

                    /* Score entity match against other query tokens */
                    float entity_score = 0.0f;
                    for (uint32_t u = 0; u < toks.count; u++)
                    {
                        if (u == t) continue;
                        SYMBOL_ID token_id = SymbolFind(graph->symbols, toks.tokens[u]);
                        if (token_id == SYMBOL_INVALID) continue;

                        const float *t_vec = EmbeddingGetVector(embeds, token_id);
                        const float *o_vec = EmbeddingGetVector(embeds, rel->object);
                        const float *s_vec = EmbeddingGetVector(embeds, rel->subject);

                        if (t_vec && o_vec)
                        {
                            float sim = EmbeddingCosineSimilarity(t_vec, o_vec);
                            if (sim > entity_score) entity_score = sim;
                        }
                        if (t_vec && s_vec)
                        {
                            float sim = EmbeddingCosineSimilarity(t_vec, s_vec);
                            if (sim > entity_score) entity_score = sim;
                        }
                    }

                    float total = 0.5f + entity_score * 0.5f;
                    if (total > best_score)
                    {
                        best_score = total;
                        best_subj = rel->subject;
                        best_pred = rel->predicate;
                        best_obj  = rel->object;
                    }
                }
            }

            if (best_score > 0.5f && best_obj != SYMBOL_INVALID)
            {
                const SYMBOL *o = SymbolGet(graph->symbols, best_obj);
                if (o)
                {
                    const SYMBOL *s = SymbolGet(graph->symbols, best_subj);
                    const SYMBOL *p = SymbolGet(graph->symbols, best_pred);
                    if (s && p)
                    {
                        char src[144];
                        RELATION *br = GraphFindRelation(graph, best_subj,
                                                         best_pred, best_obj);
                        SourceSuffix(graph, br, src, sizeof(src));
                        printf("IA > %s (because %s --%s--> %s%s)\n\n",
                               o->name, s->name, p->name, o->name, src);
                    }
                    else
                        printf("IA > %s\n\n", o->name);
                }
            }
            else
            {
                /* Social / identity acts never reach the attention scorer */
                DIALOG_INTENT di = DialogClassify(input);
                if (di.act == SPEECH_ACT_IDENTITY ||
                    di.act == SPEECH_ACT_CAPABILITY ||
                    ((di.act == SPEECH_ACT_GREETING ||
                      di.act == SPEECH_ACT_GRATITUDE ||
                      di.act == SPEECH_ACT_FAREWELL) && di.is_social_only))
                {
                    DialogGenerateResponse(graph, ctx, input,
                                           response, sizeof(response));
                    printf("IA > %s\n\n", response);
                    continue;
                }

                uint32_t base = RelationCount(graph->relations);
                ParserIngestSentence(graph, input);
                uint32_t added = RelationCount(graph->relations) - base;
                if (added > 0)
                    printf("IA > Learned %u new %s.\n\n", added, added == 1 ? "fact" : "facts");
                else
                    printf("IA > I don't know. Teach me and I'll remember.\n\n");
            }
            continue;
        }

        /* Dialog engine */
        if (DialogGenerateResponse(graph, ctx, input, response, sizeof(response)))
        {
            printf("IA > %s\n\n", response);
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
