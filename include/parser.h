#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include "graph.h"

/* Maximum tokens in a parsed sentence */
#define PARSER_MAX_TOKENS 32

typedef struct
{
    char tokens[PARSER_MAX_TOKENS][64];
    uint32_t count;
} PARSED_SENTENCE;

typedef struct
{
    char subject[128];
    char predicate[64];
    char object[128];
    int  valid;
} PARSE_RESULT;

/* Tokenize and normalize a Spanish sentence */
int ParserTokenize(const char *input, PARSED_SENTENCE *out);

/* Extract S-P-O from a tokenized sentence */
PARSE_RESULT ParserExtractSPO(const PARSED_SENTENCE *tokens);

/* Parse a free-form sentence and ingest into graph */
int ParserIngestSentence(GRAPH *graph, const char *sentence);

/* ============================================================
   Question Answering: detect questions, extract intent, answer
   ============================================================ */

typedef struct
{
    char subject[128];      /* concept to look up */
    char predicate[64];     /* what to ask about (CAPITAL, TIENE, etc.) */
    int  is_question;
    int  valid;
} QUESTION;

/* Detect if input is a question and extract intent */
QUESTION ParserDetectQuestion(const char *input);

/* Answer a question from the graph */
int ParserAnswerQuestion(
    const GRAPH *graph,
    const QUESTION *q,
    char *out_answer,
    uint32_t max_len);

#endif
