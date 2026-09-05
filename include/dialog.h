#ifndef DIALOG_H
#define DIALOG_H

#include <stddef.h>
#include <stdint.h>
#include "graph.h"
#include "context.h"

/* Intent is structural, never lexical: short content-free input is
   social, "?" marks a query, everything else is a statement to store.
   No word lists: only symbols and relations carry meaning. */
typedef enum
{
    SPEECH_ACT_UNKNOWN = 0,
    SPEECH_ACT_SOCIAL,
    SPEECH_ACT_STATEMENT,
    SPEECH_ACT_QUERY
} SPEECH_ACT;

typedef struct
{
    SPEECH_ACT act;
    char       subject[64];
    char       relation[64];
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
