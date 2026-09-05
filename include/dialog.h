#ifndef DIALOG_H
#define DIALOG_H

#include <stddef.h>
#include <stdint.h>
#include "graph.h"
#include "context.h"
#include "inference.h"

typedef enum
{
    SPEECH_ACT_UNKNOWN = 0,
    SPEECH_ACT_GREETING,
    SPEECH_ACT_FAREWELL,
    SPEECH_ACT_GRATITUDE,
    SPEECH_ACT_STATEMENT,
    SPEECH_ACT_QUERY_FACT,
    SPEECH_ACT_QUERY_WHY,
    SPEECH_ACT_QUERY_WHAT_IS,
    SPEECH_ACT_IDENTITY,
    SPEECH_ACT_CAPABILITY
} SPEECH_ACT;

typedef struct
{
    SPEECH_ACT act;
    char       subject[64];
    char       predicate[64];
    char       object[64];
    int        is_social_only;
} DIALOG_INTENT;

DIALOG_INTENT DialogClassify(const char *input);

int DialogGenerateResponse(
    GRAPH *graph,
    CONTEXT *ctx,
    const char *user_input,
    char *out_response,
    size_t max_len);

#endif
