#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "generator.h"

GENERATOR_CONFIG GeneratorConfigDefault(void)
{
    GENERATOR_CONFIG cfg;
    cfg.style = GEN_STYLE_CONCISE;
    cfg.capitalize_first = 1;
    cfg.add_period = 1;
    return cfg;
}

/* ============================================================
   Reglas lexicas y morfologicas auxiliares
   ============================================================ */

static int IsProperNoun(const char *name)
{
    static const char *proper_nouns[] = {
        "ANTONIO", "JUAN", "MARIA", "PEDRO", "HARBOUR", "FIVEWIN",
        "C", "PYTHON", "LINUX", "WINDOWS", NULL
    };

    for (int i = 0; proper_nouns[i] != NULL; i++)
    {
        if (strcmp(name, proper_nouns[i]) == 0)
            return 1;
    }
    return 0;
}

static int IsFeminineNoun(const char *name)
{
    size_t len = strlen(name);
    if (len == 0) return 0;

    if (strcmp(name, "CARNE") == 0 || strcmp(name, "AGUA") == 0 ||
        strcmp(name, "COLA") == 0 || strcmp(name, "PERSONA") == 0)
        return 1;

    return (name[len - 1] == 'A');
}

static void FormatNoun(const char *src, char *dst, size_t max_len, int is_proper)
{
    if (!src || !dst || max_len == 0) return;

    size_t len = strlen(src);
    if (len >= max_len) len = max_len - 1;

    for (size_t i = 0; i < len; i++)
    {
        if (i == 0 && is_proper)
            dst[i] = (char)toupper((unsigned char)src[i]);
        else
            dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[len] = '\0';
}

static void FormatNounPhrase(const char *src, char *dst, size_t max_len, int with_definite_article)
{
    if (!src || !dst || max_len == 0) return;

    int proper = IsProperNoun(src);
    char formatted[64];
    FormatNoun(src, formatted, sizeof(formatted), proper);

    if (proper || !with_definite_article)
    {
        snprintf(dst, max_len, "%s", formatted);
    }
    else
    {
        const char *art = IsFeminineNoun(src) ? "la" : "el";
        snprintf(dst, max_len, "%s %s", art, formatted);
    }
}

static const char *RealizeVerbPhrase(const char *predicate)
{
    if (strcmp(predicate, "COME") == 0)      return "come";
    if (strcmp(predicate, "ES") == 0)        return "es un";
    if (strcmp(predicate, "PROGRAMA") == 0)  return "programa en";
    if (strcmp(predicate, "COMPILA") == 0)   return "compila con";
    if (strcmp(predicate, "TIENE") == 0)     return "tiene";
    if (strcmp(predicate, "VIVE_EN") == 0)   return "vive en";
    if (strcmp(predicate, "DUERME_EN") == 0) return "duerme en";
    if (strcmp(predicate, "NECESITA") == 0)  return "necesita";

    return "se relaciona con";
}

/* ============================================================
   Generacion de Oraciones
   ============================================================ */

int GeneratorFromRelation(
    const GRAPH *graph,
    const RELATION *relation,
    const GENERATOR_CONFIG *config,
    char *out_text,
    size_t max_len)
{
    if (!graph || !relation || !out_text || max_len == 0)
        return 0;

    const SYMBOL *s = SymbolGet(graph->symbols, relation->subject);
    const SYMBOL *p = SymbolGet(graph->symbols, relation->predicate);
    const SYMBOL *o = SymbolGet(graph->symbols, relation->object);

    if (!s || !p || !o)
        return 0;

    char subject_np[64];
    char object_np[64];

    FormatNounPhrase(s->name, subject_np, sizeof(subject_np), 1);

    int obj_article = (strcmp(p->name, "COME") != 0 &&
                       strcmp(p->name, "PROGRAMA") != 0 &&
                       strcmp(p->name, "ES") != 0);
    FormatNounPhrase(o->name, object_np, sizeof(object_np), obj_article);

    const char *verb = RealizeVerbPhrase(p->name);

    snprintf(out_text, max_len, "%s %s %s%s",
             subject_np, verb, object_np,
             (config && config->add_period) ? "." : "");

    if (config && config->capitalize_first && strlen(out_text) > 0)
    {
        out_text[0] = (char)toupper((unsigned char)out_text[0]);
    }

    return 1;
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
        return GeneratorFromRelation(graph, relations[0], config, out_text, max_len);

    const SYMBOL *s = SymbolGet(graph->symbols, relations[0]->subject);
    const SYMBOL *p = SymbolGet(graph->symbols, relations[0]->predicate);
    if (!s || !p) return 0;

    char subject_np[64];
    FormatNounPhrase(s->name, subject_np, sizeof(subject_np), 1);
    const char *verb = RealizeVerbPhrase(p->name);

    int written = snprintf(out_text, max_len, "%s %s ", subject_np, verb);
    if (written < 0 || (size_t)written >= max_len) return 0;

    for (uint32_t i = 0; i < count; i++)
    {
        const SYMBOL *o = SymbolGet(graph->symbols, relations[i]->object);
        if (!o) continue;

        char obj_formatted[64];
        FormatNoun(o->name, obj_formatted, sizeof(obj_formatted), IsProperNoun(o->name));

        char separator[16] = "";
        if (i > 0)
        {
            if (i == count - 1)
                strcpy(separator, " y ");
            else
                strcpy(separator, ", ");
        }

        strncat(out_text, separator, max_len - strlen(out_text) - 1);
        strncat(out_text, obj_formatted, max_len - strlen(out_text) - 1);
    }

    if (config && config->add_period)
        strncat(out_text, ".", max_len - strlen(out_text) - 1);

    if (config && config->capitalize_first && strlen(out_text) > 0)
        out_text[0] = (char)toupper((unsigned char)out_text[0]);

    return 1;
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
    if (!graph || !subject_name || !predicate_name || !predictions || count == 0 || !out_text)
        return 0;

    char subject_np[64];
    FormatNounPhrase(subject_name, subject_np, sizeof(subject_np), 1);
    const char *verb = RealizeVerbPhrase(predicate_name);

    if (count == 1)
    {
        char obj_formatted[64];
        FormatNoun(predictions[0].name, obj_formatted, sizeof(obj_formatted), IsProperNoun(predictions[0].name));

        snprintf(out_text, max_len, "%s %s %s con una probabilidad del %.1f%%.",
                 subject_np, verb, obj_formatted, predictions[0].probability * 100.0f);
    }
    else
    {
        char obj1[64], obj2[64];
        FormatNoun(predictions[0].name, obj1, sizeof(obj1), IsProperNoun(predictions[0].name));
        FormatNoun(predictions[1].name, obj2, sizeof(obj2), IsProperNoun(predictions[1].name));

        snprintf(out_text, max_len,
                 "%s %s principalmente %s (%.1f%%), aunque tambien %s (%.1f%%).",
                 subject_np, verb,
                 obj1, predictions[0].probability * 100.0f,
                 obj2, predictions[1].probability * 100.0f);
    }

    if (strlen(out_text) > 0)
        out_text[0] = (char)toupper((unsigned char)out_text[0]);

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
    uint32_t n = LearningPredictText(graph, subject_name, predicate_name, preds, 16);

    if (n == 0)
    {
        snprintf(out_text, max_len, "No poseo informacion sobre que %s %s.",
                 RealizeVerbPhrase(predicate_name), subject_name);
        return 1;
    }

    return GeneratorFromPredictions(graph, subject_name, predicate_name, preds, n, out_text, max_len);
}
