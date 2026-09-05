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
#include "generator.h"
#include "dialog.h"
#include "model.h"
#include "stats.h"
#include "transfer.h"
#include "parser.h"
#include "stem.h"
#include "export.h"
#include "ingest.h"
#include "i18n.h"

/* " [SOURCE]" suffix for a relation with provenance, "" without. */
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
    printf("\n=== Symbol Map ===\n");
    printf("Symbols loaded     : %u\n", SymbolCount(graph->symbols));
    printf("Relations in memory: %u\n\n", RelationCount(graph->relations));

    for (uint32_t i = 0; i < RelationCount(graph->relations); i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (!r) continue;

        const SYMBOL *s = SymbolGet(graph->symbols, r->subject);
        const SYMBOL *p = SymbolGet(graph->symbols, r->relation);
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
    printf("Active entities: %u (decay rate: %.2f)\n\n",
           ctx->count, ctx->decay_rate);

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        const CONTEXT_ENTITY *e = &ctx->entities[i];
        printf("  - %-12s | Activation: %5.1f%% | %u turns ago | Role: %s\n",
               e->name, e->activation * 100.0f, e->turns_ago,
               e->was_subject ? "Subject" : "Object");
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
        fprintf(stderr, "Critical error: could not initialize structures.\n");
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
                printf("AI > Model loaded from 'wiki_model.bin'.\n\n");
            }
        }
    }

    printf("========================================================\n");
    printf("     SYMBOLIC LLM\n");
    printf("  Natural conversation without backpropagation or GPUs.\n");
    printf("  Commands: /graph, /context, /stats, /query <word>,\n");
  printf("            /find S P O (*), /area SYMBOL, /export <f.dot|f.ttl>,\n");
  printf("            /synonyms, /alias, /analogy A B,\n");
  printf("            /learn S P O, /save, /load, /lang, /clear, /exit\n");
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
                printf("\nAI > Goodbye! All learned knowledge is saved.\n");
            else
                printf("\nAI > Goodbye! (Warning: could not save wiki_model.bin).\n");
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
                printf("AI > Error: out of memory, keeping previous graph.\n\n");
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
            printf("AI > Memory cleared. Starting fresh.\n\n");
            continue;
        }

        /* Language switch: /lang EN|ES|FR (case-insensitive). */
        if (strcmp(input, "/lang") == 0 || strncmp(input, "/lang ", 6) == 0)
        {
            if (strncmp(input, "/lang ", 6) == 0)
            {
                char code[8] = {0};
                if (sscanf(input + 6, "%7s", code) == 1)
                {
                    LANG_ID target = LangFindByCode(code);
                    if (target < LANG_COUNT)
                    {
                        LangSet(target);
                        printf("AI > Language set to %s (%s).\n\n",
                               LangShortName(target), LangName(target));
                    }
                    else
                    {
                        printf("AI > Unknown language '%s'. Use EN, ES or FR.\n\n",
                               code);
                    }
                }
            }
            else
            {
                printf("AI > Language: %s (%s). Switch with /lang EN|ES|FR.\n\n",
                       LangShortName(LangGet()), LangName(LangGet()));
            }
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
                printf("AI > I don't know '%s'.\n\n", term);
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
                        const SYMBOL *rel = SymbolGet(graph->symbols, att_rel[i]->relation);
                        const SYMBOL *obj  = SymbolGet(graph->symbols, att_rel[i]->object);
                        if (rel && obj)
                            printf("    [%5.1f%%] --%s--> %s\n",
                                   att_scores[i] * 100.0f, rel->name, obj->name);
                    }
                }

                /* Relations as object */
                RELATION *as_obj[64];
                uint32_t n_obj = GraphQueryObject(graph, sid, as_obj, 64);

                printf("  As OBJECT (%u):\n", n_obj);
                for (uint32_t i = 0; i < n_obj && i < 30; i++)
                {
                    const SYMBOL *subj = SymbolGet(graph->symbols, as_obj[i]->subject);
                    const SYMBOL *rel = SymbolGet(graph->symbols, as_obj[i]->relation);
                    if (subj && rel)
                        printf("    %s --%s-->\n", subj->name, rel->name);
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
                printf("AI > No known concepts found in your query.\n\n");
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
                    const SYMBOL *p = SymbolGet(graph->symbols, att_rel[i]->relation);
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
                    printf("AI > I don't know one of those concepts.\n\n");
                }
                else
                {
                    float sim = TransferSimilarity(graph, id_a, id_b);
                    printf("  Structural similarity: %.2f\n\n", sim);

                    TRANSFER_RESULT results[8];

                    /* Analogical transfer */
                    uint32_t n = TransferAnalogy(graph, id_a, id_b, results, 8);
                    if (n > 0)
                    {
                        printf("  Analogy %s -> %s:\n", ent_a, ent_b);
                        TransferPrintResults(graph, results, n);
                    }
                    else
                        printf("  No analogical transfer found.\n\n");
                }
            }
            else
            {
                    printf("AI > Usage: /analogy ENTITY_A ENTITY_B\n\n");
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
                    printf("AI > Alias: '%s' ~ '%s' (similarity: %.1f%%).\n\n",
                       new_term, base_term, sim * 100.0f);
            }
            else
            {
                printf("Usage: /alias NEW EXISTING\n\n");
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
                    printf("AI > '%s' has no embedding assigned.\n\n", target);
            }
            else
            {
                EMBEDDING_MATCH matches[8];
                uint32_t m_count = EmbeddingFindSimilar(embeds, tid, matches, 8);
                printf("\nSynonyms of '%s':\n", target);
                for (uint32_t i = 0; i < m_count; i++)
                {
                    const SYMBOL *sym = SymbolGet(graph->symbols, matches[i].id);
                    printf("  - %-12s | cosine=%.1f%%\n",
                           sym ? sym->name : "?", matches[i].score * 100.0f);
                }
                if (m_count == 0)
                    printf("  (none close enough)\n");
                printf("\n");
            }
            continue;
        }

        /* Raw triple ingest: /learn S P O. No patterns, no verb lists:
           any three tokens become a relation, uppercased by ingest.
           This is how new relations and depth words enter the live
           map (e.g. /learn TRASABUELO HOPS 5). */
        if (strncmp(input, "/learn ", 7) == 0)
        {
            char s[64], p[64], o[64];
            if (sscanf(input + 7, "%63s %63s %63s", s, p, o) == 3)
            {
                int rc = IngestTripleSource(graph, s, p, o, "CLI");
                if (rc == 1)
                    printf("AI > Learned: %s --%s--> %s.\n\n", s, p, o);
                else if (rc == 2)
                    printf("AI > Known already, strengthened: %s --%s--> %s.\n\n",
                           s, p, o);
                else
                    printf("AI > Could not learn that triple.\n\n");
            }
            else
                printf("AI > Usage: /learn SUBJECT RELATION OBJECT\n\n");
            continue;
        }

        /* Full-text ingest: /ingest FILE. Two passes over the file
           (bootstrap with positional roots, then re-parse with the
           grown vocabulary). Lines are sentences. Whole texts go
           through the same tree as single inputs. */
        if (strncmp(input, "/ingest ", 8) == 0)
        {
            const char *path = input + 8;
            while (*path == ' ') path++;
            FILE *f = fopen(path, "r");
            if (f == NULL)
            {
                printf("AI > Cannot open '%s'.\n\n", path);
                continue;
            }
            uint32_t base = RelationCount(graph->relations);
            uint32_t lines = 0;
            char line[2048];
            for (int pass = 0; pass < 2; pass++)
            {
                rewind(f);
                while (fgets(line, sizeof(line), f) != NULL)
                {
                    size_t L = strlen(line);
                    while (L > 0 && (line[L - 1] == '\n' ||
                                    line[L - 1] == '\r'))
                        line[--L] = '\0';
                    if (L == 0)
                        continue;
                    if (pass == 0)
                        lines++;
                    if (lines > 20000)
                        break;
                    ParserIngestSentenceCtx(graph, ctx, line);
                }
            }
            fclose(f);
            uint32_t added = RelationCount(graph->relations) - base;
            printf("AI > Ingested %u lines, %u new relations (2 passes).\n\n",
                   lines, added);
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
                printf("AI > Model saved to '%s'.\n\n", path);
            else
                printf("AI > Error saving to '%s'.\n\n", path);
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

                printf("AI > Model loaded from '%s'.\n\n", path);
            }
            else
            {
                printf("AI > Error loading '%s'.\n\n", path);
            }
            continue;
        }

        /* Semantic area: extractive neighborhood report. Relations
           touching the symbol, ordered by area coherence (composed
           vectors). No summaries invented: only stored triples. */
        if (strncmp(input, "/area ", 6) == 0)
        {
            char target[64] = {0};
            if (sscanf(input + 6, "%63s", target) == 1)
            {
                SYMBOL_ID tid = StemFindSymbol(graph->symbols, target);
                if (tid == SYMBOL_INVALID)
                {
                    printf("AI > Unknown symbol '%s'.\n\n", target);
                    continue;
                }
                RELATION *rels[64];
                uint32_t n = 0;
                n += GraphQuerySubject(graph, tid, rels + n, 64 - n);
                {
                    RELATION *objs[64];
                    uint32_t m = GraphQueryObject(graph, tid, objs, 64);
                    for (uint32_t i = 0; i < m && n < 64; i++)
                    {
                        int dup = 0;
                        for (uint32_t k = 0; k < n; k++)
                            if (rels[k] == objs[i]) { dup = 1; break; }
                        if (!dup)
                            rels[n++] = objs[i];
                    }
                }
                ParserRankByArea(graph, tid, rels, n);
                uint32_t shown = (n > 12) ? 12 : n;
                printf("  Area of '%s': %u relations (showing %u)\n",
                       target, n, shown);
                for (uint32_t i = 0; i < shown; i++)
                {
                    const SYMBOL *ss = SymbolGet(graph->symbols,
                                                 rels[i]->subject);
                    const SYMBOL *pp = SymbolGet(graph->symbols,
                                                 rels[i]->relation);
                    const SYMBOL *oo = SymbolGet(graph->symbols,
                                                 rels[i]->object);
                    if (ss && pp && oo)
                        printf("  %s --%s--> %s\n",
                               ss->name, pp->name, oo->name);
                }
                printf("\n");
            }
            else
                printf("AI > Usage: /area SYMBOL\n\n");
            continue;
        }

        /* Map topics: relation symbols by stored mass, with one example
           each. Global questions start here: what the map is about, by
           count of stored triples. Statistics, not semantics. */
        if (strcmp(input, "/about") == 0)
        {
            typedef struct { SYMBOL_ID rid; uint32_t count; } TOP;
            TOP tops[1024];
            uint32_t ntops = 0;
            uint32_t total = RelationCount(graph->relations);
            for (uint32_t i = 0; i < total; i++)
            {
                const RELATION *r = RelationGet(graph->relations, i);
                if (!r || r->relation == SYMBOL_INVALID)
                    continue;
                uint32_t k = 0;
                while (k < ntops && tops[k].rid != r->relation)
                    k++;
                if (k >= ntops)
                {
                    if (ntops >= 1024)
                        continue;
                    tops[ntops].rid = r->relation;
                    tops[ntops].count = 0;
                    ntops++;
                }
                tops[k].count++;
            }
            for (uint32_t i = 1; i < ntops; i++)
            {
                TOP t = tops[i];
                uint32_t j = i;
                while (j > 0 && tops[j - 1].count < t.count)
                {
                    tops[j] = tops[j - 1];
                    j--;
                }
                tops[j] = t;
            }
            uint32_t shown = (ntops > 8) ? 8 : ntops;
            printf("  Map topics: %u relation kinds\n", ntops);
            for (uint32_t i = 0; i < shown; i++)
            {
                const SYMBOL *p = SymbolGet(graph->symbols, tops[i].rid);
                printf("  %s (%u)\n", p ? p->name : "?",
                       tops[i].count);
            }
            printf("\n");
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
                    SYMBOL_ID trid = StemFindSymbol(graph->symbols, p);
                    const SYMBOL *ps = SymbolGet(graph->symbols, trid);
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
                        SymbolGet(graph->symbols, r->relation) &&
                        strcmp(SymbolGet(graph->symbols, r->relation)->name, pname) != 0)
                        continue;
                    if (strcmp(o, "*") != 0 &&
                        SymbolGet(graph->symbols, r->object) &&
                        strcmp(SymbolGet(graph->symbols, r->object)->name, oname) != 0)
                        continue;
                    const SYMBOL *ss = SymbolGet(graph->symbols, r->subject);
                    const SYMBOL *pp = SymbolGet(graph->symbols, r->relation);
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
                printf("AI > %u matching relation(s).\n\n", shown);
            }
            else
                printf("AI > Usage: /find SUBJECT RELATION OBJECT  ('*' = any)\n\n");
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
            printf("AI > %s '%s'.\n\n", ok ? "Exported to" :
                   "Export failed for", path);
            continue;
        }

        /* Structured questions bypass the attention scorer: it would
           hijack them with a 1-hop guess. Valid parses have exact
           answers from the symbol map. */
        {
            QUESTION kq = ParserDetectQuestion(graph, input);
            if (kq.valid)
            {
                DialogGenerateResponse(graph, ctx, input,
                                       response, sizeof(response));
                printf("AI > %s\n\n", response);
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
            uint32_t best_rel = SYMBOL_INVALID;
            uint32_t best_obj  = SYMBOL_INVALID;

            /* For each query token, check if it matches a relation name */
            for (uint32_t t = 0; t < toks.count; t++)
            {
                SYMBOL_ID rel_id = SymbolFind(graph->symbols, toks.tokens[t]);
                if (rel_id == SYMBOL_INVALID) continue;

                /* Check if this symbol is actually used as a relation */
                int used_as_rel = 0;
                for (uint32_t r = 0; r < RelationCount(graph->relations); r++)
                {
                    const RELATION *rel = RelationGet(graph->relations, r);
                    if (rel && rel->relation == rel_id) { used_as_rel = 1; break; }
                }
                if (!used_as_rel) continue;

                /* Found relation match! Score each relation with this relation */
                for (uint32_t r = 0; r < RelationCount(graph->relations); r++)
                {
                    const RELATION *rel = RelationGet(graph->relations, r);
                    if (rel == NULL || rel->relation != rel_id) continue;

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
                        best_rel = rel->relation;
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
                    const SYMBOL *p = SymbolGet(graph->symbols, best_rel);
                    if (s && p)
                    {
                        char src[144];
                        RELATION *br = GraphFindRelation(graph, best_subj,
                                                         best_rel, best_obj);
                        SourceSuffix(graph, br, src, sizeof(src));
                        printf("AI > %s (because %s --%s--> %s%s)\n\n",
                               o->name, s->name, p->name, o->name, src);
                    }
                    else
                        printf("AI > %s\n\n", o->name);
                }
            }
            else
            {
                /* Social input never reaches the attention scorer */
                DIALOG_INTENT di = DialogClassify(input);
                if (di.act == SPEECH_ACT_SOCIAL && di.is_social_only)
                {
                    DialogGenerateResponse(graph, ctx, input,
                                           response, sizeof(response));
                    printf("AI > %s\n\n", response);
                    continue;
                }

                uint32_t base = RelationCount(graph->relations);
                ParserIngestSentenceCtx(graph, ctx, input);
                uint32_t added = RelationCount(graph->relations) - base;
                if (added > 0)
                    printf("AI > Learned %u new %s.\n\n", added, added == 1 ? "fact" : "facts");
                else
                    printf("AI > I don't know. Teach me and I'll remember.\n\n");
            }
            continue;
        }

        /* Dialog engine */
        if (DialogGenerateResponse(graph, ctx, input, response, sizeof(response)))
        {
            printf("AI > %s\n\n", response);
        }
        else
        {
            /* Syntax tree ingest: input → tree → symbols → relations */
            uint32_t base = RelationCount(graph->relations);
            ParserIngestSentenceCtx(graph, ctx, input);
            uint32_t added = RelationCount(graph->relations) - base;
            if (added > 0)
                printf("AI > Learned %u new %s.\n\n", added, added == 1 ? "fact" : "facts");
            else
                printf("AI > Could not understand. Please rephrase.\n\n");
        }
    }

    EmbeddingTableDestroy(embeds);
    ContextDestroy(ctx);
    GraphDestroy(graph);

    return EXIT_SUCCESS;
}
