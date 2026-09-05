#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>
#include "symbol.h"
#include "graph.h"

#define CONTEXT_MAX_ENTITIES 16

/* Working memory holds recent SYMBOLS, nothing else. No gender, no
   number, no entity types: those live in the map as symbols and
   relations, or nowhere. Anaphora resolves by discourse dynamics
   (activation x subjecthood); an unknown lead token takes the top. */
typedef struct
{
    SYMBOL_ID     symbol;
    char          name[64];
    float         activation;
    uint32_t      turns_ago;
    int           was_subject;
} CONTEXT_ENTITY;

typedef struct
{
    CONTEXT_ENTITY entities[CONTEXT_MAX_ENTITIES];
    uint32_t       count;
    float          decay_rate;
    SYMBOL_ID      last_subject;
    SYMBOL_ID      last_object;
} CONTEXT;


CONTEXT *ContextCreate(void);
void ContextDestroy(CONTEXT *ctx);
void ContextReset(CONTEXT *ctx);

void ContextStepTurn(CONTEXT *ctx);

void ContextPushEntity(
    CONTEXT *ctx,
    SYMBOL_ID symbol,
    const char *name,
    int is_subject
);

SYMBOL_ID ContextResolvePronoun(
    const GRAPH *graph,
    const CONTEXT *ctx,
    const char *pronoun
);

SYMBOL_ID ContextResolveImplicitSubject(const CONTEXT *ctx);

int ContextPreprocessSentence(
    CONTEXT *ctx,
    const GRAPH *graph,
    const char *input_sentence,
    char *out_resolved,
    size_t out_size
);

#endif
