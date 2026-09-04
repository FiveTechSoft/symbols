#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "symbol.h"
#include "relation.h"
#include "embedding.h"
#include "graph.h"
#include "learning.h"
#include "context.h"
#include "generator.h"
#include "model.h"

#define CLI_BUFFER_SIZE 512
#define CLI_FUZZY_THRESHOLD 0.70f

/* ============================================================
   Forward declarations
   ============================================================ */
static void LearnEmbeddingForPair(GRAPH *graph, EMBEDDING_TABLE *embeds,
                                  const char *s_name, const char *o_name);

/* ============================================================
   Utilidades del CLI
   ============================================================ */

static void StrToUpper(const char *src, char *dst, size_t max_len)
{
    size_t i = 0;
    while (src[i] && i < max_len - 1)
    {
        dst[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static void CleanString(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
        len--;
    }
}

static int IsQuestion(const char *text)
{
    if (strchr(text, '?') != NULL || strstr(text, "\xc2\xbf") != NULL)
        return 1;

    char upper[CLI_BUFFER_SIZE];
    StrToUpper(text, upper, sizeof(upper));

    if (strncmp(upper, "QUE ", 4) == 0 || strncmp(upper, "QUIEN ", 6) == 0 ||
        strncmp(upper, "DONDE ", 6) == 0 || strncmp(upper, "CUAL ", 5) == 0)
    {
        return 1;
    }

    return 0;
}

/* ============================================================
   Parser de Preguntas en Lenguaje Natural
   ============================================================ */
static int ParseQuery(const char *query_text, char *out_subj, char *out_pred)
{
    char clean[CLI_BUFFER_SIZE];
    strncpy(clean, query_text, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';

    for (size_t i = 0; i < strlen(clean); i++)
    {
        if (clean[i] == '?' || clean[i] == '.' || clean[i] == ',' || clean[i] == '!')
            clean[i] = ' ';
    }

    char words[16][64];
    uint32_t word_count = 0;

    char *tok = strtok(clean, " \t\r\n");
    while (tok != NULL && word_count < 16)
    {
        char upper[64];
        StrToUpper(tok, upper, sizeof(upper));

        if (strcmp(upper, "\xc2\xbf") != 0 && strcmp(upper, "QUE") != 0 &&
            strcmp(upper, "QUIEN") != 0 && strcmp(upper, "DONDE") != 0 &&
            strcmp(upper, "EL") != 0 && strcmp(upper, "LA") != 0 &&
            strcmp(upper, "LOS") != 0 && strcmp(upper, "LAS") != 0 &&
            strcmp(upper, "UN") != 0 && strcmp(upper, "UNA") != 0)
        {
            strncpy(words[word_count], upper, 63);
            words[word_count][63] = '\0';
            word_count++;
        }
        tok = strtok(NULL, " \t\r\n");
    }

    if (word_count < 2)
        return 0;

    strncpy(out_pred, words[0], 63);
    strncpy(out_subj, words[1], 63);

    return 1;
}

/* ============================================================
   Gestor de Embeddings durante el Aprendizaje
   ============================================================ */
static void EnsureVector(EMBEDDING_TABLE *embeds, SYMBOL_ID id, uint32_t seed)
{
    if (EmbeddingGetVector(embeds, id) == NULL)
    {
        float vec[EMBEDDING_DIM];
        EmbeddingRandomInit(vec, seed + (uint32_t)id * 101);
        EmbeddingSetVector(embeds, id, vec);
    }
}

/* ============================================================
   Inspeccion de Estado y Diagnostico
   ============================================================ */

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
            printf("  [%02u] %-12s --%-10s--> %-12s (obs=%llu, peso=%.2f)\n",
                   i + 1, s->name, p->name, o->name,
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

static void CmdHelp(void)
{
    printf("\nComandos disponibles:\n");
    printf("  /graph             Ver el grafo de conocimiento\n");
    printf("  /context           Ver la memoria de contexto\n");
    printf("  /synonyms PAL      Ver sinonimos cercanos\n");
    printf("  /synonyms A B      Ver similitud entre A y B\n");
    printf("  /alias [n] [b]     Definir alias manual\n");
    printf("  /save <archivo>    Guardar modelo V2 a disco\n");
    printf("  /load <archivo>    Cargar modelo desde disco\n");
    printf("  /clear             Limpiar contexto\n");
    printf("  /help              Mostrar esta ayuda\n");
    printf("  /exit              Salir\n\n");
}

/* ============================================================
   BUCLE PRINCIPAL (REPL)
   ============================================================ */

int main(void)
{
    MODEL *model = ModelCreate(256, 1024);
    CONTEXT *ctx = ContextCreate();

    if (model == NULL || ctx == NULL)
    {
        fprintf(stderr, "Error critico: no se pudieron inicializar las estructuras.\n");
        return EXIT_FAILURE;
    }

    GRAPH *graph = model->graph;
    EMBEDDING_TABLE *embeds = model->embeddings;

    /* Corpus inicial */
    const char *bootstrap[] = {
        "El gato come pescado.",
        "El gato come carne.",
        "El perro come carne.",
        "El gato es animal.",
        "El perro es animal.",
        "El animal es ser vivo.",
    };

    for (uint32_t i = 0; i < 6; i++)
    {
        LearningSentence(graph, bootstrap[i]);

        char buf[CLI_BUFFER_SIZE];
        strncpy(buf, bootstrap[i], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        for (size_t j = 0; buf[j]; j++)
            if (buf[j] == '.' || buf[j] == ',')
                buf[j] = ' ';

        char t[16][64];
        uint32_t tc = 0;
        char *tok = strtok(buf, " \t\r");
        while (tok && tc < 16)
        {
            char upper[64];
            size_t k = 0;
            while (tok[k] && k < 63)
            {
                upper[k] = (char)toupper((unsigned char)tok[k]);
                k++;
            }
            upper[k] = '\0';
            strncpy(t[tc], upper, 63);
            t[tc][63] = '\0';
            tc++;
            tok = strtok(NULL, " \t\r");
        }

        if (tc >= 3)
            LearnEmbeddingForPair(graph, embeds, t[0], t[2]);
    }

    printf("========================================================\n");
    printf("       LLM SIMBOLICO LIGERO EN C - REPL v2.0 (HIBRIDO)  \n");
    printf("  Escribe frases para ensenar o preguntas para consultar.\n");
    printf("  Busqueda semantica activa con embeddings de 32D.      \n");
    printf("  /help para ver comandos disponibles.\n");
    printf("========================================================\n\n");
    printf("Corpus inicial: 6 frases, %u relaciones, %u embeddings.\n\n",
           RelationCount(graph->relations), embeds->count);

    char input[CLI_BUFFER_SIZE];
    char resolved_input[CLI_BUFFER_SIZE];
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
            printf("\nCerrando sesion del modelo simbolico.\n");
            break;
        }

        if (strcmp(input, "/help") == 0)
        {
            CmdHelp();
            continue;
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
            ContextReset(ctx);
            printf("Contexto limpiado.\n\n");
            continue;
        }

        /* /synonyms PALABRA */
        if (strncmp(input, "/synonyms ", 10) == 0)
        {
            char words[2][64];
            uint32_t wc = 0;
            char tmp[CLI_BUFFER_SIZE];
            strncpy(tmp, input + 10, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';

            char *t = strtok(tmp, " \t");
            while (t && wc < 2)
            {
                StrToUpper(t, words[wc], sizeof(words[wc]));
                wc++;
                t = strtok(NULL, " \t");
            }

            if (wc == 1)
            {
                SYMBOL_ID tid = SymbolFind(graph->symbols, words[0]);
                if (tid == SYMBOL_INVALID || EmbeddingGetVector(embeds, tid) == NULL)
                {
                    printf("  '%s' no tiene vector asignado.\n\n", words[0]);
                }
                else
                {
                    EMBEDDING_MATCH matches[8];
                    uint32_t m = EmbeddingFindSimilar(embeds, tid, matches, 8);
                    printf("\nSinonimos cercanos a '%s':\n", words[0]);
                    for (uint32_t i = 0; i < m; i++)
                    {
                        const SYMBOL *sym = SymbolGet(graph->symbols, matches[i].id);
                        printf("  %-12s  coseno=%.3f\n",
                               sym ? sym->name : "?", matches[i].score);
                    }
                    if (m == 0)
                        printf("  (ninguno cercano)\n");
                    printf("\n");
                }
            }
            else if (wc == 2)
            {
                SYMBOL_ID id1 = SymbolFind(graph->symbols, words[0]);
                SYMBOL_ID id2 = SymbolFind(graph->symbols, words[1]);
                if (id1 == SYMBOL_INVALID || id2 == SYMBOL_INVALID)
                {
                    printf("  Ambos simbolos deben estar registrados.\n\n");
                }
                else
                {
                    const float *v1 = EmbeddingGetVector(embeds, id1);
                    const float *v2 = EmbeddingGetVector(embeds, id2);
                    if (!v1 || !v2)
                    {
                        printf("  Falta vector para uno de los simbolos.\n\n");
                    }
                    else
                    {
                        printf("  Coseno(%s, %s) = %.3f\n\n",
                               words[0], words[1],
                               EmbeddingCosineSimilarity(v1, v2));
                    }
                }
            }
            else
            {
                printf("  Uso: /synonyms PALABRA  o  /synonyms A B\n\n");
            }
            continue;
        }

        /* /alias NUEVO BASE */
        if (strncmp(input, "/alias ", 7) == 0)
        {
            char new_term[64], base_term[64];
            if (sscanf(input + 7, "%63s %63s", new_term, base_term) == 2)
            {
                char u_new[64], u_base[64];
                StrToUpper(new_term, u_new, sizeof(u_new));
                StrToUpper(base_term, u_base, sizeof(u_base));

                SYMBOL_ID id_new = GraphAddSymbol(graph, u_new);
                SYMBOL_ID id_base = GraphAddSymbol(graph, u_base);

                EnsureVector(embeds, id_base, 42);
                const float *base_v = EmbeddingGetVector(embeds, id_base);

                float clone_v[EMBEDDING_DIM];
                memcpy(clone_v, base_v, sizeof(clone_v));
                clone_v[0] += 0.02f;
                EmbeddingNormalize(clone_v);
                EmbeddingSetVector(embeds, id_new, clone_v);

                float sim = EmbeddingCosineSimilarity(base_v, clone_v);
                printf("  Alias: '%s' ~ '%s' (similitud: %.1f%%)\n\n",
                       u_new, u_base, sim * 100.0f);
            }
            else
            {
                printf("  Uso: /alias [nuevo] [existente]\n\n");
            }
            continue;
        }

        /* /save PATH */
        if (strncmp(input, "/save ", 6) == 0)
        {
            const char *path = input + 6;
            while (*path == ' ')
                path++;

            if (ModelSave(model, path))
                printf("  Modelo V2 guardado en '%s'.\n\n", path);
            else
                printf("  Error al guardar en '%s'.\n\n", path);
            continue;
        }

        /* /load PATH */
        if (strncmp(input, "/load ", 6) == 0)
        {
            const char *path = input + 6;
            while (*path == ' ')
                path++;

            MODEL *loaded = ModelLoad(path);
            if (loaded != NULL)
            {
                ModelDestroy(model);
                model = loaded;
                graph = model->graph;
                embeds = model->embeddings;
                ContextReset(ctx);
                printf("  Modelo cargado desde '%s'. (%u simbolos, %u relaciones, %u embeddings)\n\n",
                       path,
                       SymbolCount(graph->symbols),
                       RelationCount(graph->relations),
                       embeds->count);
            }
            else
            {
                printf("  Error: no se pudo abrir o formato invalido en '%s'.\n\n", path);
            }
            continue;
        }

        /* ----------------------------------------------------
           MODO PREGUNTA (Symbolico + Fallback Fuzzy Vectorial)
           ---------------------------------------------------- */
        if (IsQuestion(input))
        {
            char subj_str[64] = "";
            char pred_str[64] = "";

            if (ParseQuery(input, subj_str, pred_str))
            {
                SYMBOL_ID s_id = SymbolFind(graph->symbols, subj_str);
                SYMBOL_ID p_id = SymbolFind(graph->symbols, pred_str);

                if (s_id == SYMBOL_INVALID)
                    s_id = GraphAddSymbol(graph, subj_str);
                if (p_id == SYMBOL_INVALID)
                    p_id = GraphAddSymbol(graph, pred_str);

                RELATION *results[16];
                SYMBOL_ID resolved_subject = s_id;

                uint32_t found = GraphQuerySubjectPredicateFuzzy(
                    graph, s_id, p_id, results, 16,
                    CLI_FUZZY_THRESHOLD, &resolved_subject);

                if (found > 0)
                {
                    if (resolved_subject != s_id)
                    {
                        const SYMBOL *s_orig = SymbolGet(graph->symbols, s_id);
                        const SYMBOL *s_res  = SymbolGet(graph->symbols, resolved_subject);
                        const float *v1 = EmbeddingGetVector(embeds, s_id);
                        const float *v2 = EmbeddingGetVector(embeds, resolved_subject);
                        float sim = (v1 && v2) ? EmbeddingCosineSimilarity(v1, v2) : 0.0f;

                        printf("  [sinonimo] '%s' se aproxima a '%s' (similitud: %.1f%%)\n",
                               s_orig ? s_orig->name : "?",
                               s_res ? s_res->name : "?",
                               sim * 100.0f);
                    }

                    GENERATOR_CONFIG gcfg = GeneratorConfigDefault();
                    GeneratorAggregateRelations(
                        graph,
                        (const RELATION **)results,
                        found,
                        &gcfg,
                        response,
                        sizeof(response));

                    printf("IA > %s\n\n", response);
                }
                else
                {
                    printf("IA > No poseo suficiente informacion sobre que %s %s.\n\n",
                           pred_str, subj_str);
                }
            }
            else
            {
                printf("IA > No logre identificar el sujeto y la accion.\n\n");
            }
            continue;
        }

        /* ----------------------------------------------------
           MODO APRENDIZAJE
           ---------------------------------------------------- */
        ContextPreprocessSentence(ctx, input, resolved_input, sizeof(resolved_input));

        if (strcmp(input, resolved_input) != 0)
            printf("  [anafora] \"%s\" -> \"%s\"\n", input, resolved_input);

        int ok = LearningSentence(graph, resolved_input);
        if (ok)
        {
            char temp[CLI_BUFFER_SIZE];
            strncpy(temp, resolved_input, sizeof(temp));
            char *s_name = strtok(temp, " \t\r\n");
            char *p_name = strtok(NULL, " \t\r\n");
            char *o_name = strtok(NULL, " \t\r\n");

            if (s_name && o_name)
            {
                char upper_s[64], upper_o[64];
                StrToUpper(s_name, upper_s, sizeof(upper_s));
                StrToUpper(o_name, upper_o, sizeof(upper_o));

                SYMBOL_ID s_id = SymbolFind(graph->symbols, upper_s);
                SYMBOL_ID o_id = SymbolFind(graph->symbols, upper_o);

                if (s_id != SYMBOL_INVALID)
                    ContextPushEntity(ctx, s_id, upper_s,
                                       GENDER_MASCULINE, NUMBER_SINGULAR,
                                       ENTITY_TYPE_PERSON, 1);

                EnsureVector(embeds, s_id, 100);
                EnsureVector(embeds, o_id, 200);

                float vec_s_buf[EMBEDDING_DIM];
                const float *src_s = EmbeddingGetVector(embeds, s_id);
                const float *vec_o = EmbeddingGetVector(embeds, o_id);
                if (src_s && vec_o)
                {
                    memcpy(vec_s_buf, src_s, sizeof(vec_s_buf));
                    EmbeddingCooccur(vec_s_buf, vec_o, 0.15f);
                    EmbeddingSetVector(embeds, s_id, vec_s_buf);
                }
            }

            printf("  Aprendido. (%u relaciones)\n\n",
                   RelationCount(graph->relations));
        }
        else
        {
            printf("  No pude extraer una tripleta.\n\n");
        }
    }

    ModelDestroy(model);
    ContextDestroy(ctx);
    return EXIT_SUCCESS;
}

/* Helper used by bootstrap to learn embeddings for pairs */
static void LearnEmbeddingForPair(GRAPH *graph, EMBEDDING_TABLE *embeds,
                                  const char *s_name, const char *o_name)
{
    SYMBOL_ID s_id = SymbolFind(graph->symbols, s_name);
    SYMBOL_ID o_id = SymbolFind(graph->symbols, o_name);
    if (s_id == SYMBOL_INVALID || o_id == SYMBOL_INVALID)
        return;

    EnsureVector(embeds, s_id, 100);
    EnsureVector(embeds, o_id, 200);

    float vec_s_buf[EMBEDDING_DIM];
    const float *src_s = EmbeddingGetVector(embeds, s_id);
    const float *vec_o = EmbeddingGetVector(embeds, o_id);
    if (src_s && vec_o)
    {
        memcpy(vec_s_buf, src_s, sizeof(vec_s_buf));
        EmbeddingCooccur(vec_s_buf, vec_o, 0.15f);
        EmbeddingSetVector(embeds, s_id, vec_s_buf);
    }
}
