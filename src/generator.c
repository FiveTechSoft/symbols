#include <stdio.h>
#include <string.h>

#include "generator.h"

/* Surface realization without a lexicon: a stored relation verbalizes
   as its own symbol names in order. No articles, no verb tables, no
   morphology lists. Only symbols and relations. */

GENERATOR_CONFIG GeneratorConfigDefault(void)
{
    GENERATOR_CONFIG cfg;
    cfg.style = GEN_STYLE_CONCISE;
    cfg.capitalize_first = 0;
    cfg.add_period = 0;
    return cfg;
}

static void AppendName(const GRAPH *graph, SYMBOL_ID id,
                       char *out, size_t max_len)
{
    const SYMBOL *s = SymbolGet(graph->symbols, id);
    if (s == NULL || s->name == NULL || s->name[0] == '\0')
        return;
    if (out[0] != '\0')
        strncat(out, " ", max_len - strlen(out) - 1);
    strncat(out, s->name, max_len - strlen(out) - 1);
}

int GeneratorFromRelation(
    const GRAPH *graph,
    const RELATION *relation,
    const GENERATOR_CONFIG *config,
    char *out_text,
    size_t max_len)
{
    (void)config;
    if (!graph || !relation || !out_text || max_len == 0)
        return 0;

    out_text[0] = '\0';
    AppendName(graph, relation->subject, out_text, max_len);
    AppendName(graph, relation->predicate, out_text, max_len);
    AppendName(graph, relation->object, out_text, max_len);
    return (out_text[0] != '\0');
}

int GeneratorAggregateRelations(
    const GRAPH *graph,
    const RELATION **relations,
    uint32_t count,
    const GENERATOR_CONFIG *config,
    char *out_text,
    size_t max_len)
{
    if (!graph || !relations || count == 0 || !out_text || max_len == 0)
        return 0;

    if (count == 1)
        return GeneratorFromRelation(graph, relations[0], config,
                                     out_text, max_len);

    out_text[0] = '\0';
    const SYMBOL *s = SymbolGet(graph->symbols, relations[0]->subject);
    const SYMBOL *p = SymbolGet(graph->symbols, relations[0]->predicate);
    if (!s || !p) return 0;

    strncat(out_text, s->name, max_len - strlen(out_text) - 1);
    strncat(out_text, " ", max_len - strlen(out_text) - 1);
    strncat(out_text, p->name, max_len - strlen(out_text) - 1);

    for (uint32_t i = 0; i < count; i++)
    {
        if (relations[i] == NULL) continue;
        const SYMBOL *o = SymbolGet(graph->symbols, relations[i]->object);
        if (!o || !o->name) continue;
        strncat(out_text, i > 0 ? ", " : " ",
                max_len - strlen(out_text) - 1);
        strncat(out_text, o->name, max_len - strlen(out_text) - 1);
    }
    return (out_text[0] != '\0');
}

int GeneratorFromPredictions(
    const GRAPH *graph,
    const char *subject_name,
    const char *predicate_name,
    const PREDICTION *predictions,
    uint32_t count,
    char *out_text,
    size_t max_len)
{
    if (!graph || !subject_name || !predicate_name || !predictions ||
        count == 0 || !out_text || max_len == 0)
        return 0;

    snprintf(out_text, max_len, "%s %s", subject_name, predicate_name);
    for (uint32_t i = 0; i < count; i++)
    {
        char cell[128];
        snprintf(cell, sizeof(cell), "%s%s (%.1f)",
                 i > 0 ? ", " : " ",
                 predictions[i].name,
                 predictions[i].probability * 100.0f);
        strncat(out_text, cell, max_len - strlen(out_text) - 1);
    }
    return 1;
}

int GeneratorAnswerQuery(
    const GRAPH *graph,
    const char *subject_name,
    const char *predicate_name,
    char *out_text,
    size_t max_len)
{
    if (!graph || !subject_name || !predicate_name || !out_text)
        return 0;

    PREDICTION preds[16];
    uint32_t n = LearningPredictText(graph, subject_name, predicate_name,
                                     preds, 16);

    if (n == 0)
    {
        snprintf(out_text, max_len, "%s %s ?", subject_name, predicate_name);
        return 1;
    }

    return GeneratorFromPredictions(graph, subject_name, predicate_name,
                                    preds, n, out_text, max_len);
}
