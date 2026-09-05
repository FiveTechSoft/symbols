#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "context.h"

#define ACTIVATION_DECAY_DEFAULT 0.70f
#define ACTIVATION_THRESHOLD     0.15f

CONTEXT *ContextCreate(void)
{
    CONTEXT *ctx = (CONTEXT *)calloc(1, sizeof(CONTEXT));
    if (ctx == NULL)
        return NULL;

    ctx->decay_rate = ACTIVATION_DECAY_DEFAULT;
    ctx->last_subject = SYMBOL_INVALID;
    ctx->last_object = SYMBOL_INVALID;
    return ctx;
}

void ContextDestroy(CONTEXT *ctx)
{
    if (ctx != NULL)
        free(ctx);
}

void ContextReset(CONTEXT *ctx)
{
    if (ctx == NULL)
        return;

    ctx->count = 0;
    ctx->last_subject = SYMBOL_INVALID;
    ctx->last_object = SYMBOL_INVALID;
    memset(ctx->entities, 0, sizeof(ctx->entities));
}

void ContextStepTurn(CONTEXT *ctx)
{
    if (ctx == NULL)
        return;

    uint32_t active_count = 0;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        ctx->entities[i].activation *= ctx->decay_rate;
        ctx->entities[i].turns_ago++;

        if (ctx->entities[i].activation >= ACTIVATION_THRESHOLD)
        {
            if (active_count != i)
            {
                ctx->entities[active_count] = ctx->entities[i];
            }
            active_count++;
        }
    }
    ctx->count = active_count;
}

void ContextPushEntity(
    CONTEXT *ctx,
    SYMBOL_ID symbol,
    const char *name,
    int is_subject)
{
    if (ctx == NULL || symbol == SYMBOL_INVALID || name == NULL)
        return;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entities[i].symbol == symbol)
        {
            ctx->entities[i].activation = 1.0f;
            ctx->entities[i].turns_ago = 0;
            ctx->entities[i].was_subject = is_subject;
            if (is_subject)
                ctx->last_subject = symbol;
            else
                ctx->last_object = symbol;
            return;
        }
    }

    uint32_t slot = ctx->count;
    if (slot >= CONTEXT_MAX_ENTITIES)
    {
        float min_act = 2.0f;
        uint32_t min_idx = 0;
        for (uint32_t i = 0; i < ctx->count; i++)
        {
            if (ctx->entities[i].activation < min_act)
            {
                min_act = ctx->entities[i].activation;
                min_idx = i;
            }
        }
        slot = min_idx;
    }
    else
    {
        ctx->count++;
    }

    CONTEXT_ENTITY *e = &ctx->entities[slot];
    e->symbol = symbol;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->activation = 1.0f;
    e->turns_ago = 0;
    e->was_subject = is_subject;

    if (is_subject)
        ctx->last_subject = symbol;
    else
        ctx->last_object = symbol;
}

/* ============================================================
   Resolucion de Pronombres
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

/* A lead token that names no symbol takes the top discourse entity
   (activation x subjecthood). Known tokens are kept as written:
   names are learned, pronouns resolve. No gender, no number,
   no entity types: only symbols, relations and recency. */
SYMBOL_ID ContextResolvePronoun(
    const GRAPH *graph,
    const CONTEXT *ctx,
    const char *pronoun)
{
    if (graph == NULL || ctx == NULL || pronoun == NULL || ctx->count == 0)
        return SYMBOL_INVALID;

    char p[32];
    StrToUpper(pronoun, p, sizeof(p));

    if (SymbolFind(graph->symbols, p) != SYMBOL_INVALID)
        return SYMBOL_INVALID;

    SYMBOL_ID best_symbol = SYMBOL_INVALID;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        const CONTEXT_ENTITY *e = &ctx->entities[i];

        float score = e->activation * (e->was_subject ? 1.3f : 1.0f);

        if (score > best_score)
        {
            best_score = score;
            best_symbol = e->symbol;
        }
    }

    return best_symbol;
}

SYMBOL_ID ContextResolveImplicitSubject(const CONTEXT *ctx)
{
    if (ctx == NULL || ctx->count == 0)
        return SYMBOL_INVALID;

    if (ctx->last_subject != SYMBOL_INVALID)
        return ctx->last_subject;

    SYMBOL_ID best_symbol = SYMBOL_INVALID;
    float max_act = -1.0f;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entities[i].activation > max_act)
        {
            max_act = ctx->entities[i].activation;
            best_symbol = ctx->entities[i].symbol;
        }
    }

    return best_symbol;
}

/* ============================================================
   Preprocesador de Frases
   ============================================================ */

int ContextPreprocessSentence(
    CONTEXT *ctx,
    const GRAPH *graph,
    const char *input_sentence,
    char *out_resolved,
    size_t out_size)
{
    if (ctx == NULL || graph == NULL || input_sentence == NULL ||
        out_resolved == NULL || out_size == 0)
        return 0;

    char temp[512];
    strncpy(temp, input_sentence, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *first_word = strtok(temp, " \t\r\n");
    if (first_word == NULL)
    {
        strncpy(out_resolved, input_sentence, out_size);
        return 1;
    }

    SYMBOL_ID resolved_id = ContextResolvePronoun(graph, ctx, first_word);

    if (resolved_id != SYMBOL_INVALID)
    {
        const char *replacement = NULL;
        for (uint32_t i = 0; i < ctx->count; i++)
        {
            if (ctx->entities[i].symbol == resolved_id)
            {
                replacement = ctx->entities[i].name;
                break;
            }
        }

        if (replacement != NULL)
        {
            const char *rest = input_sentence + strlen(first_word);
            while (*rest == ' ' || *rest == '\t')
                rest++;

            snprintf(out_resolved, out_size, "%s %s", replacement, rest);
            return 1;
        }
    }

    strncpy(out_resolved, input_sentence, out_size - 1);
    out_resolved[out_size - 1] = '\0';
    return 1;
}
