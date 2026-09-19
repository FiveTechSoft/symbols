/* ============================================================
   stochastic_nlg: Non-Deterministic Truth-Preserving Natural Language Generation.
   Pure C11, zero tensors, zero backprop, fail-closed truth preservation.

   Grounding:
     1. RHETORICAL STOCHASTIC SAMPLING: Samples diverse grammatical structures,
        connectors, and rhetorical stances using temperature tau in [0.0, 1.0].
     2. PEIRCEAN TRIAD DIALOGUE ACTS:
        - Inductive assertions modulated by empirical rule confidence.
        - Abductive causal explanations for multi-hypothesis phenomena.
        - Active curiosity-driven Socratic inquiries for missing knowledge gaps.
        - Defeasible contrastive / concessive framing for exception guards.
     3. REPETITION PENALTY: Short-term discourse memory prevents monotone reuse.
     4. MATHEMATICAL TRUTH INVARIANCE: Entity symbols and logical truth values
        are strictly immutable; non-determinism operates solely on surface syntax.

   Design principles:
     - HARDCODING=0: Connectors and rhetorical frames are defined declaratively.
     - Multilingual: Dynamic runtime support for EN, ES, FR.
     - Deterministic reproducibility: tau = 0.0f or fixed seed guarantees byte-exact output.
   ============================================================ */

#ifndef STOCHASTIC_NLG_H
#define STOCHASTIC_NLG_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "i18n.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"

#define STOCHASTIC_MAX_TEMPLATES  8
#define STOCHASTIC_HISTORY_SIZE   16
#define STOCHASTIC_OUT_BUF_MAX    2048

/* Dialogue act types */
typedef enum
{
    COG_ACT_FACT_ASSERTION = 0,    /* Direct factual relation */
    COG_ACT_INDUCTIVE_REASONING,   /* Inductive inference with confidence */
    COG_ACT_ABDUCTIVE_EXPLANATION, /* Abductive causal hypothesis */
    COG_ACT_ACTIVE_INQUIRY,        /* Curiosity-driven Socratic question */
    COG_ACT_DEFEASIBLE_CONTRAST,   /* Non-monotonic exception handling */
    COG_ACT_COUNT
} COG_DIALOGUE_ACT;

/* Short-term discourse history to enforce repetition penalties */
typedef struct
{
    uint8_t  recent_templates[COG_ACT_COUNT][STOCHASTIC_HISTORY_SIZE];
    uint32_t counts[COG_ACT_COUNT];
} STOCHASTIC_DISCOURSE_HISTORY;

/* Configuration for stochastic generation */
typedef struct
{
    float    temperature;        /* 0.0 = deterministic (argmax), >0.0 = stochastic */
    float    repetition_penalty; /* Multiplier < 1.0 (e.g. 0.25f) for recently used forms */
    uint32_t rng_state;          /* PRNG seed/state for reproducible sampling */
    LANG_ID  lang;               /* Active language */
} STOCHASTIC_NLG_CONFIG;

/* Initialization functions */
STOCHASTIC_NLG_CONFIG StochasticNLG_DefaultConfig(void);
void StochasticNLG_InitHistory(STOCHASTIC_DISCOURSE_HISTORY *hist);
void StochasticNLG_ResetHistory(STOCHASTIC_DISCOURSE_HISTORY *hist);

/* Core dialogue act generators */

/* 1. Fact Assertion: verbalizes (S, R, O) with stochastic surface variations */
uint32_t StochasticNLG_FactAssertion(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* 2. Inductive Reasoning: verbalizes an inferred link modulated by rule confidence */
uint32_t StochasticNLG_InductiveAssertion(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    const GRAPH_RULE *rule,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* 3. Abductive Explanation: verbalizes causal hypotheses for why a goal holds */
uint32_t StochasticNLG_AbductiveExplanation(
    const GRAPH *graph,
    const ABDUCTIVE_HYPOTHESIS *hyps,
    uint32_t num_hyps,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* 4. Active Epistemic Inquiry: verbalizes curiosity-driven Socratic questions */
uint32_t StochasticNLG_ActiveInquiry(
    const GRAPH *graph,
    const EPISTEMIC_INQUIRY *inquiry,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* 5. Defeasible Contrast: verbalizes exception guards and non-monotonic revisions */
uint32_t StochasticNLG_DefeasibleContrast(
    const GRAPH *graph,
    SYMBOL_ID general_class,
    SYMBOL_ID relation,
    SYMBOL_ID property,
    SYMBOL_ID exception_entity,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

/* Autonomous multi-turn conversational turn dispatcher */
uint32_t StochasticNLG_TurnResponse(
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    int is_why_question,
    STOCHASTIC_NLG_CONFIG *cfg,
    STOCHASTIC_DISCOURSE_HISTORY *hist,
    char *out,
    size_t out_size);

#endif /* STOCHASTIC_NLG_H */
