#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "nlg.h"

static const char *CONNECTORS_POSITIVE[] = {
    "Stored: ",
    "From the map: ",
    "Recorded: ",
    "Found: ",
    "On record: ",
    "Listed: "
};
#define NUM_CONNECTORS_POS 6

static const char *CONNECTORS_SUGGEST[] = {
    "Also stored about ",
    "See also ",
    "Related: "
};
#define NUM_CONNECTORS_SUG 3

static const char *NO_RESULTS_PREFIX[] = {
    "No direct record of that. ",
    "That fact is not stored. ",
    "No stored evidence for that. "
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
    SYMBOL_ID relation,
    RELATION **results,
    uint32_t count,
    char *out,
    uint32_t out_size)
{
    out[0] = '\0';

    if (count == 0)
        return 0;

    const SYMBOL *subj_sym = SymbolGet(graph->symbols, subject);
    const SYMBOL *rel_sym = SymbolGet(graph->symbols, relation);
    const char *subj_name = subj_sym ? subj_sym->name : "?";
    const char *rel_name = rel_sym ? rel_sym->name : "?";

    /* Connector */
    uint32_t idx = PickIndex(NUM_CONNECTORS_POS, subj_name);
    AppendStr(out, out_size, CONNECTORS_POSITIVE[idx]);

    /* Subject */
    AppendStr(out, out_size, subj_name);
    AppendStr(out, out_size, " ");

    /* Lowercase first letter of relation for grammar */
    if (rel_name[0])
    {
        char lc[128];
        lc[0] = (char)tolower((unsigned char)rel_name[0]);
        strncpy(lc + 1, rel_name + 1, sizeof(lc) - 2);
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

void NLGGenerateNoResults(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
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
            const SYMBOL *rel = SymbolGet(graph->symbols, related[i]->relation);
            if (i > 0)
                AppendStr(out, out_size, ", ");
            AppendStr(out, out_size, rel ? rel->name : "?");
        }
        AppendStr(out, out_size, ".");
    }
    else
    {
        AppendStr(out, out_size, "Teach new facts with /learn S P O.");
    }
}

void NLGGenerateCompound(
    const GRAPH *graph,
    SYMBOL_ID subject,
    RELATION **taxonomic,
    uint32_t tax_count,
    RELATION **functional,
    uint32_t func_count,
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
            const SYMBOL *rel = SymbolGet(graph->symbols, taxonomic[i]->relation);
            const SYMBOL *obj = SymbolGet(graph->symbols, taxonomic[i]->object);
            if (i > 0) AppendStr(out, out_size, ", ");
            AppendStr(out, out_size, "es ");
            if (rel && strcmp(rel->name, "ES") != 0)
            {
                AppendStr(out, out_size, rel->name);
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
            const SYMBOL *rel = SymbolGet(graph->symbols, functional[i]->relation);
            const SYMBOL *obj = SymbolGet(graph->symbols, functional[i]->object);
            AppendStr(out, out_size, rel ? rel->name : "?");
            AppendStr(out, out_size, " ");
            AppendStr(out, out_size, obj ? obj->name : "?");
        }
        need_separator = 1;
    }

    /* Inferred knowledge: not stored, not shown. Only identified
       relations are reported. */

    AppendStr(out, out_size, ".");
}
