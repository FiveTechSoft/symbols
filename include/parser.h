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
    char object[128];      /* second entity for (S,R,O) verification */
    int  is_question;
    int  is_count;         /* CUÁNTO-form: answer with the numeral */
    int  is_negative;      /* NO-form: verify (S,R,O), answer Sí/No */
    int  hole;             /* M8 conjunctive: subject is the hole; the
                              answer intersects conj_rel/conj_obj pairs */
    uint32_t nconj;
    char conj_rel[3][64];
    char conj_obj[3][128];
    int  valid;
} QUESTION;

/* Detect if input is a question and extract intent. The graph provides
   the live vocabulary: descriptors resolve against the relations it
   actually holds, so learned words work immediately. */
QUESTION ParserDetectQuestion(const GRAPH *graph, const char *input);

/* True when the line reads as a question, punctuation or not: '?' or a
   question-only closed-class token (QUIEN/CUAL/ERES/SOIS/WHO...).
   Single source of truth shared by the ingester and the dialog
   classifier, so "quien eres" without '?' is never small talk. */
int ParserIsQuestion(const char *sentence);

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

/* Order relations by semantic-area coherence around center. */
void ParserRankByArea(const GRAPH *graph, SYMBOL_ID center,
                      RELATION **rels, uint32_t n);

/* Render (subj, pred, obj) through the most-used learned mold of
   pred. Returns 1 on success, 0 when no mold exists. */
int SurfaceRender(const char *pred, const char *subj, const char *obj,
                  char *out, size_t out_size);

#endif
