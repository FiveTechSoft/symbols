/* ============================================================
   deep_nlg: Deep Symbolic Natural Language Generation (Graph-to-Text).
   Pure C99, zero tensors, zero backprop, fail-closed honest UNKNOWN.

   Three-stage symbolic generation:
     1. MACROPLANNING: Discourse planning & rhetorical relations (RST)
     2. MICROPLANNING: Sentence aggregation & referring expressions
     3. SURFACE REALIZATION: Morphology, connectors & concord

   Design principles:
     - HARDCODING=0: Lexical tables and connectors are organized
       declaratively by role and language.
     - Fail-closed: UNKNOWN is articulated with epistemic honesty.
     - Deterministic: 100% idempotent across runs.
   ============================================================ */

#ifndef DEEP_NLG_H
#define DEEP_NLG_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "i18n.h"

#define DEEP_NLG_TOKEN_MAX 64
#define DEEP_NLG_PROPS_MAX 16
#define DEEP_NLG_BUF_MAX   2048

/* Discourse role of a proposition in the rhetorical structure */
typedef enum
{
    NLG_ROLE_ASSERTION = 0,    /* Direct factual assertion: "X is Y" */
    NLG_ROLE_CHAIN_STEP,       /* Intermediate step in a derivation */
    NLG_ROLE_CONCLUSION,       /* Logical deduction or multi-hop conclusion */
    NLG_ROLE_ELABORATION,      /* Additional attribute or domain fact */
    NLG_ROLE_CONTRAST,         /* Contrastive or qualifying clause */
    NLG_ROLE_EVIDENCE,         /* Provenance / grounding marker */
    NLG_ROLE_ABSTENTION        /* Fail-closed honest abstention */
} NLG_DISCOURSE_ROLE;

/* Polarity and epistemic certainty */
typedef enum
{
    NLG_POL_AFFIRMATIVE = 0,
    NLG_POL_NEGATIVE,
    NLG_POL_UNKNOWN
} NLG_POLARITY;

/* A structured semantic proposition */
typedef struct
{
    char               subject[DEEP_NLG_TOKEN_MAX];
    char               relation[DEEP_NLG_TOKEN_MAX];
    char               object[DEEP_NLG_TOKEN_MAX];
    char               attribute[DEEP_NLG_TOKEN_MAX];
    NLG_DISCOURSE_ROLE role;
    NLG_POLARITY       polarity;
    float              confidence;      /* 1.0 = direct fact, <1.0 = derived */
} NLG_PROP;

/* Document plan: ordered sequence of structured propositions */
typedef struct
{
    NLG_PROP props[DEEP_NLG_PROPS_MAX];
    uint32_t nprops;
    char     topic[DEEP_NLG_TOKEN_MAX];
    char     focus[DEEP_NLG_TOKEN_MAX];
    int      has_focus;
} NLG_PLAN;

/* ---- Plan Construction (Macroplanning) ---- */

/* Initialize an empty plan for a given topic/subject */
void DeepNLG_PlanInit(NLG_PLAN *plan, const char *topic);

/* Add a raw proposition to the plan */
int DeepNLG_PlanAdd(NLG_PLAN *plan,
                    const char *subj,
                    const char *rel,
                    const char *obj,
                    NLG_DISCOURSE_ROLE role,
                    NLG_POLARITY pol,
                    float conf);

/* Build a multi-hop derivation chain plan (e.g. genealogy, taxonomy) */
int DeepNLG_PlanChain(NLG_PLAN *plan,
                      const char *start_node,
                      const char *step_rel,
                      const char hops[][DEEP_NLG_TOKEN_MAX],
                      uint32_t nhops,
                      const char *conclusion_rel,
                      const char *end_node);

/* Build a compound entity overview plan (multiple facts about entity) */
int DeepNLG_PlanEntityOverview(NLG_PLAN *plan,
                               const char *entity,
                               const char relations[][DEEP_NLG_TOKEN_MAX],
                               const char objects[][DEEP_NLG_TOKEN_MAX],
                               uint32_t nfacts);

/* Build an honest abstention plan explaining lack of evidence */
void DeepNLG_PlanAbstain(NLG_PLAN *plan,
                         const char *subject,
                         const char *relation,
                         const char *hint);

/* ---- Microplanning and Surface Realization ---- */

/* Realize an NLG_PLAN into natural language prose.
   Uses the active language from LangGet() (LANG_ES, LANG_EN, LANG_FR).
   Returns the number of bytes written to out. */
uint32_t DeepNLG_Realize(const NLG_PLAN *plan, char *out, size_t out_size);

/* Convenience one-shot generators: */

/* Generate natural prose for a multi-hop reasoning chain */
uint32_t DeepNLG_GenerateChainText(const char *start,
                                   const char *step_rel,
                                   const char hops[][DEEP_NLG_TOKEN_MAX],
                                   uint32_t nhops,
                                   const char *conclusion_rel,
                                   const char *end_node,
                                   char *out,
                                   size_t out_size);

/* Generate natural compound description for multiple facts */
uint32_t DeepNLG_GenerateMultiFactText(const char *entity,
                                      const char relations[][DEEP_NLG_TOKEN_MAX],
                                      const char objects[][DEEP_NLG_TOKEN_MAX],
                                      uint32_t nfacts,
                                      char *out,
                                      size_t out_size);

/* Generate honest, articulated fail-closed abstention */
uint32_t DeepNLG_GenerateAbstainText(const char *entity,
                                    const char *relation,
                                    const char *context_hint,
                                    char *out,
                                    size_t out_size);

#endif /* DEEP_NLG_H */
