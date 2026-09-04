#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "nlg.h"

static const char *CONNECTORS_POSITIVE[] = {
    "Segun lo que he aprendido, ",
    "De los datos que poseo, ",
    "Segun el grafo de conocimiento, ",
    "La evidencia almacenada indica que ",
    "He encontrado que ",
    "Deduzco que "
};
#define NUM_CONNECTORS_POS 6

static const char *CONNECTORS_INFERENCE[] = {
    "Razonando por encadenamiento, ",
    "Por deduccion transitiva, ",
    "Siguiendo la cadena logica, ",
    "Mediante razonamiento profundo, "
};
#define NUM_CONNECTORS_INF 4

static const char *CONNECTORS_SUGGEST[] = {
    "Podrias preguntar tambien sobre ",
    "Te sugiero consultar sobre ",
    "Quizas te interese saber sobre "
};
#define NUM_CONNECTORS_SUG 3

static const char *NO_RESULTS_PREFIX[] = {
    "No poseo informacion directa sobre eso. ",
    "Ese hecho no esta registrado en el grafo. ",
    "No tengo evidencia almacenada al respecto. "
};
#define NUM_NO_RES 3

static uint32_t PickIndex(uint32_t range, const char *seed)
{
    if (range == 0) return 0;
    uint32_t h = 0;
    for (const char *p = seed; *p; p++)
        h = h * 31 + (unsigned char)*p;
    return h % range;
}

static void AppendStr(char *out, uint32_t size, const char *s)
{
    size_t len = strlen(out);
    size_t slen = strlen(s);
    if (len + slen < size)
        memcpy(out + len, s, slen + 1);
}

static void AppendUInt64(char *out, uint32_t size, uint64_t v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
    AppendStr(out, size, buf);
}

static void AppendFloat1(char *out, uint32_t size, float v)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", v * 100.0f);
    AppendStr(out, size, buf);
}

uint32_t NLGGenerateDirect(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t count,
    char *out,
    uint32_t out_size)
{
    out[0] = '\0';

    if (count == 0)
        return 0;

    const SYMBOL *subj_sym = SymbolGet(graph->symbols, subject);
    const SYMBOL *pred_sym = SymbolGet(graph->symbols, predicate);
    const char *subj_name = subj_sym ? subj_sym->name : "?";
    const char *pred_name = pred_sym ? pred_sym->name : "?";

    /* Connector */
    uint32_t idx = PickIndex(NUM_CONNECTORS_POS, subj_name);
    AppendStr(out, out_size, CONNECTORS_POSITIVE[idx]);

    /* Subject */
    AppendStr(out, out_size, subj_name);
    AppendStr(out, out_size, " ");

    /* Lowercase first letter of predicate for grammar */
    if (pred_name[0])
    {
        char lc[128];
        lc[0] = (char)tolower((unsigned char)pred_name[0]);
        strncpy(lc + 1, pred_name + 1, sizeof(lc) - 2);
        lc[sizeof(lc) - 1] = '\0';
        AppendStr(out, out_size, lc);
        AppendStr(out, out_size, " ");
    }

    /* Compute total weight for percentages */
    float total_weight = 0.0f;
    for (uint32_t i = 0; i < count; i++)
        total_weight += results[i]->weight;

    if (count == 1)
    {
        const SYMBOL *obj_sym = SymbolGet(graph->symbols, results[0]->object);
        AppendStr(out, out_size, obj_sym ? obj_sym->name : "?");
        AppendStr(out, out_size, ".");
    }
    else
    {
        for (uint32_t i = 0; i < count && i < 5; i++)
        {
            const SYMBOL *obj_sym = SymbolGet(graph->symbols, results[i]->object);
            float pct = total_weight > 0 ? (results[i]->weight / total_weight * 100.0f) : 0.0f;

            if (i > 0 && i == count - 1)
                AppendStr(out, out_size, " y ");
            else if (i > 0)
                AppendStr(out, out_size, ", ");

            AppendStr(out, out_size, obj_sym ? obj_sym->name : "?");
            AppendStr(out, out_size, " (");
            char pct_buf[16];
            snprintf(pct_buf, sizeof(pct_buf), "%.0f%%", pct);
            AppendStr(out, out_size, pct_buf);
            AppendStr(out, out_size, ")");
        }
        AppendStr(out, out_size, ".");
    }

    return 1;
}

void NLGGenerateInference(
    const GRAPH *graph,
    const INFERENCE_PATH *path,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object,
    char *out,
    uint32_t out_size)
{
    out[0] = '\0';

    const SYMBOL *subj_sym = SymbolGet(graph->symbols, subject);
    const SYMBOL *pred_sym = SymbolGet(graph->symbols, predicate);
    const SYMBOL *obj_sym = SymbolGet(graph->symbols, object);

    /* Inference connector */
    uint32_t idx = PickIndex(NUM_CONNECTORS_INF, subj_sym ? subj_sym->name : "");
    AppendStr(out, out_size, CONNECTORS_INFERENCE[idx]);

    AppendStr(out, out_size, subj_sym ? subj_sym->name : "?");
    AppendStr(out, out_size, " ");

    if (pred_sym && pred_sym->name[0])
    {
        char lc[128];
        lc[0] = (char)tolower((unsigned char)pred_sym->name[0]);
        strncpy(lc + 1, pred_sym->name + 1, sizeof(lc) - 2);
        lc[sizeof(lc) - 1] = '\0';
        AppendStr(out, out_size, lc);
        AppendStr(out, out_size, " ");
    }

    AppendStr(out, out_size, obj_sym ? obj_sym->name : "?");
    AppendStr(out, out_size, ".");

    /* Confidence and hop count */
    char conf_buf[64];
    snprintf(conf_buf, sizeof(conf_buf),
             " (confianza: %.0f%%, %u saltos)",
             path->accumulated_confidence * 100.0f,
             path->depth > 0 ? path->depth - 1 : 0);
    AppendStr(out, out_size, conf_buf);

    /* Justification trace */
    if (path->depth >= 2)
    {
        AppendStr(out, out_size, " Cadena: ");
        for (uint32_t i = 0; i < path->depth; i++)
        {
            const SYMBOL *node = SymbolGet(graph->symbols, path->step_nodes[i]);
            if (i > 0)
                AppendStr(out, out_size, " -> ");
            AppendStr(out, out_size, node ? node->name : "?");
        }
        AppendStr(out, out_size, ".");
    }
}

void NLGGenerateNoResults(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    char *out,
    uint32_t out_size)
{
    out[0] = '\0';

    uint32_t idx = PickIndex(NUM_NO_RES, "?");
    AppendStr(out, out_size, NO_RESULTS_PREFIX[idx]);

    /* Suggest related queries */
    RELATION *related[8];
    uint32_t n = GraphQuerySubject((GRAPH *)graph, subject, related, 8);

    if (n > 0)
    {
        idx = PickIndex(NUM_CONNECTORS_SUG, "?");
        AppendStr(out, out_size, CONNECTORS_SUGGEST[idx]);

        for (uint32_t i = 0; i < n && i < 3; i++)
        {
            const SYMBOL *pred = SymbolGet(graph->symbols, related[i]->predicate);
            if (i > 0)
                AppendStr(out, out_size, ", ");
            AppendStr(out, out_size, pred ? pred->name : "?");
        }
        AppendStr(out, out_size, ".");
    }
    else
    {
        AppendStr(out, out_size, "Puedes ensenarme hechos nuevos escribiendo una frase.");
    }
}

void NLGGenerateCompound(
    const GRAPH *graph,
    SYMBOL_ID subject,
    RELATION **taxonomic,
    uint32_t tax_count,
    RELATION **functional,
    uint32_t func_count,
    const INFERENCE_PATH *inferred,
    int has_inferred,
    char *out,
    uint32_t out_size)
{
    out[0] = '\0';

    const SYMBOL *subj = SymbolGet(graph->symbols, subject);
    const char *name = subj ? subj->name : "?";

    /* Opening with what we know */
    AppendStr(out, out_size, "De los datos que tengo sobre ");
    AppendStr(out, out_size, name);
    AppendStr(out, out_size, ": ");

    int need_separator = 0;

    /* Taxonomic facts */
    if (tax_count > 0)
    {
        for (uint32_t i = 0; i < tax_count && i < 3; i++)
        {
            const SYMBOL *pred = SymbolGet(graph->symbols, taxonomic[i]->predicate);
            const SYMBOL *obj = SymbolGet(graph->symbols, taxonomic[i]->object);
            if (i > 0) AppendStr(out, out_size, ", ");
            AppendStr(out, out_size, "es ");
            if (pred && strcmp(pred->name, "ES") != 0)
            {
                AppendStr(out, out_size, pred->name);
                AppendStr(out, out_size, " de ");
            }
            AppendStr(out, out_size, obj ? obj->name : "?");
        }
        need_separator = 1;
    }

    /* Functional facts */
    if (func_count > 0)
    {
        if (need_separator)
            AppendStr(out, out_size, "; ");

        for (uint32_t i = 0; i < func_count && i < 4; i++)
        {
            if (i > 0) AppendStr(out, out_size, ", ");
            const SYMBOL *pred = SymbolGet(graph->symbols, functional[i]->predicate);
            const SYMBOL *obj = SymbolGet(graph->symbols, functional[i]->object);
            AppendStr(out, out_size, pred ? pred->name : "?");
            AppendStr(out, out_size, " ");
            AppendStr(out, out_size, obj ? obj->name : "?");
        }
        need_separator = 1;
    }

    /* Inferred knowledge */
    if (has_inferred && inferred != NULL && inferred->depth >= 2)
    {
        if (need_separator)
            AppendStr(out, out_size, ". Ademas, por razonamiento deduje que ");

        const SYMBOL *obj = SymbolGet(graph->symbols, inferred->step_nodes[inferred->depth - 1]);
        const SYMBOL *pred = SymbolGet(graph->symbols, inferred->step_predicates[inferred->depth - 2]);
        AppendStr(out, out_size, pred ? pred->name : "?");
        AppendStr(out, out_size, " ");
        AppendStr(out, out_size, obj ? obj->name : "?");

        char conf[32];
        snprintf(conf, sizeof(conf), " (%.0f%%)", inferred->accumulated_confidence * 100.0f);
        AppendStr(out, out_size, conf);
    }

    AppendStr(out, out_size, ".");
}
