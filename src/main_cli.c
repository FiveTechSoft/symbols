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
    printf("Simbolos registrados  : %u\n", SymbolCount(graph->symbols));
    printf("Relaciones en memoria : %u\n\n", RelationCount(graph->relations));

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
    printf("\n=== Memoria de Corto Plazo (Contexto) ===\n");
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

    printf("========================================================\n");
    printf("     SYMBOLIC LLM - ASISTENTE CONVERSACIONAL            \n");
    printf("  Conversacion natural sin backpropagation ni GPUs.     \n");
    printf("  Comandos: /graph, /context, /stats, /synonyms,        \n");
    printf("            /alias, /save, /load, /clear, /exit          \n");
    printf("========================================================\n\n");

    char input[CLI_BUFFER_SIZE];
    char response[CLI_BUFFER_SIZE];

    while (1)
    {
        printf("Tu > ");
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        CleanString(input);
        if (strlen(input) == 0)
            continue;

        if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0)
        {
            printf("\nIA > Hasta luego! Todo el conocimiento aprendido queda listo.\n");
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
            printf("IA > Memoria reiniciada. Comencemos desde cero.\n\n");
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

        if (strncmp(input, "/analogy ", 9) == 0)
        {
            char ent_a[64], ent_b[64];
            if (sscanf(input + 9, "%63s %63s", ent_a, ent_b) == 2)
            {
                SYMBOL_ID id_a = SymbolFind(graph->symbols, ent_a);
                SYMBOL_ID id_b = SymbolFind(graph->symbols, ent_b);
                if (id_a == SYMBOL_INVALID || id_b == SYMBOL_INVALID)
                {
                    printf("IA > No conozco uno de esos conceptos.\n\n");
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
                        printf("  No se encontraron transferencias.\n\n");
                }
            }
            else
            {
                printf("IA > Uso: /analogy ENTIDAD_A ENTIDAD_B\n\n");
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
                printf("IA > Alias: '%s' ~ '%s' (similitud: %.1f%%).\n\n",
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
                printf("IA > '%s' no tiene vector asignado.\n\n", target);
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
                printf("IA > Modelo guardado en '%s'.\n\n", path);
            else
                printf("IA > Error al guardar en '%s'.\n\n", path);
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

                printf("IA > Modelo cargado desde '%s'.\n\n", path);
            }
            else
            {
                printf("IA > Error al cargar '%s'.\n\n", path);
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
            printf("IA > No logre comprender. Reformula la oracion.\n\n");
        }
    }

    EmbeddingTableDestroy(embeds);
    ContextDestroy(ctx);
    GraphDestroy(graph);

    return EXIT_SUCCESS;
}
