/* ============================================================
   graph_reasoning: Inductive Rule Mining & Abductive Reasoning.
   Pure C99, zero tensors, zero backprop, fail-closed verification.

   Cognitive capabilities:
     1. INDUCTION: Mines Horn rules (transitive, composition, inverse)
        automatically from graph topology using PCA confidence.
     2. DEDUCTION: Forward-chains verified rules to predict implicit
        links and enrich the graph without human-engineered axioms.
     3. ABDUCTION: When a goal is UNKNOWN, discovers missing pivot
        hypotheses that would logically explain and entail it.

   Design principles:
     - HARDCODING=0: Rules are deduced from observed topology.
     - Fail-closed: Only rules meeting support and confidence gates
       are promoted to the rule base.
   ============================================================ */

#ifndef GRAPH_REASONING_H
#define GRAPH_REASONING_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"

#define MAX_MINED_RULES 128
#define MAX_ABDUCTIVE_HYPS 32
#define MAX_RULE_NAME 64

/* Type of structural rule discovered from graph topology */
typedef enum
{
    RULE_TYPE_TRANSITIVE = 0,    /* R o R => R (homogeneous transitivity) */
    RULE_TYPE_COMPOSITION,       /* R1 o R2 => R3 (heterogeneous composition) */
    RULE_TYPE_INVERSE,           /* R1(A,B) => R2(B,A) */
    RULE_TYPE_SYMMETRIC          /* R(A,B) => R(B,A) */
} GRAPH_RULE_TYPE;

/* An inductively mined Horn rule */
typedef struct
{
    GRAPH_RULE_TYPE type;
    SYMBOL_ID       r1;          /* First premise relation */
    SYMBOL_ID       r2;          /* Second premise relation (or SYMBOL_INVALID) */
    SYMBOL_ID       head;        /* Inferred head relation */
    uint32_t        support;     /* Number of confirming instances in graph */
    uint32_t        premises;    /* Number of candidate premise paths */
    float           confidence;  /* support / premises (PCA confidence) */
    char            name[MAX_RULE_NAME];
} GRAPH_RULE;

/* Rule Base storing verified inductive rules */
typedef struct
{
    GRAPH_RULE rules[MAX_MINED_RULES];
    uint32_t   num_rules;
    float      min_confidence;   /* Confidence threshold (e.g. >= 0.8f) */
    uint32_t   min_support;      /* Minimum confirming observations (e.g. >= 2) */
} GRAPH_RULE_BASE;

/* An abductive hypothesis explaining an observed or queried goal */
typedef struct
{
    SYMBOL_ID subject;           /* Missing hypothesis subject */
    SYMBOL_ID relation;          /* Missing hypothesis relation */
    SYMBOL_ID object;            /* Missing hypothesis object */
    SYMBOL_ID target_subj;       /* Goal observation to be explained */
    SYMBOL_ID target_rel;
    SYMBOL_ID target_obj;
    float     plausibility;      /* Rule confidence supporting hypothesis */
    char      explanation[256];
} ABDUCTIVE_HYPOTHESIS;

/* ---- Core Reasoning API ---- */

/* Initialize rule base with gates */
void GraphRuleBaseInit(GRAPH_RULE_BASE *rb, float min_confidence, uint32_t min_support);

/* Mine inductive rules from the graph topology */
uint32_t GraphMineRules(const GRAPH *graph, GRAPH_RULE_BASE *rb);

/* Forward deduction: apply verified mined rules to infer implicit links */
uint32_t GraphApplyRules(GRAPH *graph, const GRAPH_RULE_BASE *rb);

/* Abductive reasoning: generate missing link hypotheses for an unproven fact */
uint32_t GraphAbduce(const GRAPH *graph,
                     const GRAPH_RULE_BASE *rb,
                     SYMBOL_ID subject,
                     SYMBOL_ID relation,
                     SYMBOL_ID object,
                     ABDUCTIVE_HYPOTHESIS *hyps,
                     uint32_t max_hyps);

/* Format rule into human-readable text */
void GraphRuleFormat(const GRAPH *graph, const GRAPH_RULE *rule, char *out, size_t out_size);

#endif /* GRAPH_REASONING_H */
