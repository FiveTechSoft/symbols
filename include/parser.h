#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "graph.h"
#include "context.h"

/* Maximum tokens in a parsed sentence */
#define PARSER_MAX_TOKENS 32

typedef struct
{
    char tokens[PARSER_MAX_TOKENS][64];
    uint32_t count;
} PARSED_SENTENCE;

typedef struct
{
    char subject[128];      /* concept to look up */
    char relation[64];     /* relation naming a stored triple slot */
    int  is_question;
    int  valid;
} QUESTION;

/* Detect if input is a question and extract intent. The graph provides
   the live vocabulary: descriptors resolve against the relations it
   actually holds, so learned words work immediately. */
QUESTION ParserDetectQuestion(const GRAPH *graph, const char *input);

/* Tokenize and normalize input (prose, code, formulas alike) */
int ParserTokenize(const char *input, PARSED_SENTENCE *out);

/* Parse a free-form sentence and ingest into graph.
   Input → syntax tree → symbols → relations. */
int ParserIngestSentence(GRAPH *graph, const char *sentence);

/* Same, with discourse: resolves lead anaphora through the context
   and pushes stored entities back into it, so later sentences see
   earlier ones. The file-ingest path uses this form. */
int ParserIngestSentenceCtx(GRAPH *graph, CONTEXT *ctx,
                            const char *sentence);

/* Answer a question from the graph */
int ParserAnswerQuestion(
    const GRAPH *graph,
    const QUESTION *q,
    char *out_answer,
    uint32_t max_len);

#endif
