#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdint.h>
#include "symbol.h"
#include "graph.h"

#define CONTEXT_MAX_ENTITIES 16

typedef enum
{
    GENDER_UNKNOWN = 0,
    GENDER_MASCULINE,
    GENDER_FEMININE,
    GENDER_NEUTER
} ENTITY_GENDER;

typedef enum
{
    NUMBER_SINGULAR = 0,
    NUMBER_PLURAL
} ENTITY_NUMBER;

typedef enum
{
    ENTITY_TYPE_GENERIC = 0,
    ENTITY_TYPE_PERSON,
    ENTITY_TYPE_OBJECT,
    ENTITY_TYPE_ACTION
} ENTITY_TYPE;

typedef struct
{
    SYMBOL_ID     symbol;
    char          name[64];
    ENTITY_GENDER gender;
    ENTITY_NUMBER number;
    ENTITY_TYPE   type;
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
    ENTITY_GENDER gender,
    ENTITY_NUMBER number,
    ENTITY_TYPE type,
    int is_subject
);

SYMBOL_ID ContextResolvePronoun(
    const CONTEXT *ctx,
    const char *pronoun
);

SYMBOL_ID ContextResolveImplicitSubject(const CONTEXT *ctx);

int ContextPreprocessSentence(
    CONTEXT *ctx,
    const char *input_sentence,
    char *out_resolved,
    size_t out_size
);

#endif
