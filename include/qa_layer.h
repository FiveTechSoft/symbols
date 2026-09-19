#ifndef QA_LAYER_H
#define QA_LAYER_H

#include "chat.h"

/* ================================================================
   QA Layer: open-domain question answering over the symbolic engine.

   Design principles (project rule: HARDCODING=0):
   - Question type detection uses structural position, never vocabulary.
   - Fail-closed: UNKNOWN preferred over fabricated answer.
   - Read-only over chat.c: does not modify the intent system.
   ================================================================ */

/* ---- Structural question type (detected by position, not words) ---- */
typedef enum
{
    QA_ENTITY,      /* <wh> <copula> <entity>      "quien es David"     */
    QA_RELATION,    /* <entity> <relation> ?        "hijos de David" */
    QA_BOOLEAN,     /* <copula> <entity> <rel> <entity> "es David hijo de Saul" */
    QA_COUNT,       /* <count_marker> <entity>      "cuantos hijos tiene David" */
    QA_WHERE,       /* <wh> <location> <entity>     "donde esta Jerusalem" */
    QA_WHY,         /* <wh_cause> <entity> <rel>    "por que David rey" */
    QA_OPEN,        /* fallback: no structural match */
    QA_NONE         /* not a question */
} QA_TYPE;

/* ---- Question parse (structural, no vocabulary) ---- */
#define QA_SLOTS_MAX 4
#define QA_TOKEN_MAX 32

typedef struct
{
    QA_TYPE   type;
    char      slots[QA_SLOTS_MAX][QA_TOKEN_MAX];
    uint32_t  nslots;
    int       is_question;
} QA_PARSE;

/* ---- Answer result ---- */
#define QA_ANSWER_MAX 1024

typedef struct
{
    char      text[QA_ANSWER_MAX];
    float     confidence;   /* 0.0 = UNKNOWN, 1.0 = certain */
    int       has_source;
    char      source[256];  /* provenance: "bible.txt:12:4" */
} QA_ANSWER;

/* ---- Core API ---- */

/* Initialize QA layer (call once at startup). */
void QA_Init(void);

/* Parse a question structurally (no vocabulary lists). */
int QAParseQuestion(const char *question, QA_PARSE *out);

/* Answer a question using KB + embeddings + text store.
   Returns 1 if an answer was found, 0 if UNKNOWN. */
int QAAnswer(CHAT *ch, const char *question, QA_ANSWER *out);

/* ---- Structural helpers (exported for testing) ---- */

/* Detect if token looks like a question word by position. */
int QALooksLikeQuestionWord(const char *tok, uint32_t pos,
                             const char toks[][QA_TOKEN_MAX],
                             uint32_t n);

/* Detect question type by structure after the wh-word. */
QA_TYPE QADetectQuestionType(const char toks[][QA_TOKEN_MAX],
                              uint32_t n, uint32_t wh_pos,
                              char *entity_out, size_t entity_size);

#endif /* QA_LAYER_H */
