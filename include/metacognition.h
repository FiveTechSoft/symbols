/* ============================================================
   metacognition: Working Memory, Activation Spreading & Epistemic Self-Auditing.
   Pure C11, zero tensors, zero backprop, fail-closed verification.

   Cognitive capabilities:
     1. WORKING MEMORY & ACTIVATION SPREADING:
        - Maintains an active cognitive focus of attention (Working Memory).
        - Energy diffusion (spreading activation) along relational paths.
        - Decay and threshold pruning per conversational turn.
     2. EPISTEMIC PROVENANCE DAG:
        - Deep derivation tracking: links deduced beliefs to parent axioms & rules.
        - Robustness propagation: R(conclusion) = Conf(rule) * Prod(R(premises)).
     3. METACOGNITIVE SELF-AUDITING:
        - "Why do I believe this?": Recursive justification tree to grounded axioms.
        - Weakest link analysis: Identifies the most fragile premise or rule.
        - Hypothetical loss analysis: Forward cascade of beliefs that collapse if X is refuted.
        - Global epistemic health: Audits belief stability, axiom ratio, and fragility.

   Design principles:
     - HARDCODING=0: Relational propagation weights and thresholds are parameterized.
     - Fail-closed: Robustness reflects strictly verified derivation paths.
   ============================================================ */

#ifndef METACOGNITION_H
#define METACOGNITION_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"

#define MAX_WM_NODES       64
#define MAX_PROV_ENTRIES   512
#define MAX_PREMISES       4
#define MAX_IMPACTED_FACTS 64

/* ---- 1. WORKING MEMORY & ACTIVATION SPREADING ---- */

typedef struct
{
    SYMBOL_ID symbol;
    float     activation; /* [0.0, 1.0] */
    uint32_t  last_turn;
} WM_NODE;

typedef struct
{
    WM_NODE  nodes[MAX_WM_NODES];
    uint32_t count;
    float    decay_rate;     /* e.g. 0.85f per turn */
    float    threshold;      /* minimum energy to stay in memory (e.g. 0.10f) */
    uint32_t current_turn;
} WORKING_MEMORY;

/* Initialize working memory */
void WM_Init(WORKING_MEMORY *wm, float decay_rate, float threshold);

/* Stimulate a symbol with activation energy */
void WM_Stimulate(WORKING_MEMORY *wm, SYMBOL_ID symbol, float energy);

/* Spread activation along graph edges (1 or more hops) */
uint32_t WM_SpreadActivation(
    WORKING_MEMORY *wm,
    const GRAPH *graph,
    float spread_factor,
    uint32_t max_hops);

/* Step conversational turn (decays all activations, removes sub-threshold nodes) */
void WM_StepTurn(WORKING_MEMORY *wm);

/* Get top-K activated symbols in working memory */
uint32_t WM_GetTopActive(
    const WORKING_MEMORY *wm,
    SYMBOL_ID *out_symbols,
    float *out_activations,
    uint32_t max_k);

/* Get activation level of a specific symbol (0.0 if not in WM) */
float WM_GetActivation(const WORKING_MEMORY *wm, SYMBOL_ID symbol);


/* ---- 2. EPISTEMIC PROVENANCE & METACOGNITION ---- */

typedef enum
{
    PROV_AXIOM = 0,     /* Directly observed / grounded in corpus */
    PROV_DEDUCED,       /* Inferred via rule forward-chaining */
    PROV_ABDUCED,       /* Hypothesized via abductive reasoning */
    PROV_ASSIMILATED    /* Acquired via active epistemic inquiry */
} PROV_ORIGIN_TYPE;

typedef struct
{
    SYMBOL_ID subject;
    SYMBOL_ID relation;
    SYMBOL_ID object;
    PROV_ORIGIN_TYPE origin;
    float     robustness;     /* [0.0, 1.0] epistemic stability */
    uint32_t  premises[MAX_PREMISES]; /* Indices of parent facts in table */
    uint32_t  num_premises;
    int32_t   rule_idx;       /* Index of rule in RULE_BASE, or -1 */
    float     rule_conf;      /* Confidence of rule used */
} PROVENANCE_ENTRY;

typedef struct
{
    PROVENANCE_ENTRY entries[MAX_PROV_ENTRIES];
    uint32_t         count;
} PROVENANCE_TABLE;

/* Initialize provenance table */
void ProvenanceInit(PROVENANCE_TABLE *pt);

/* Register an axiom (direct observation) */
int32_t ProvenanceRecordAxiom(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    float initial_confidence);

/* Register a deduction derived from premises and a rule */
int32_t ProvenanceRecordDeduction(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    const int32_t *premise_indices,
    uint32_t num_premises,
    int32_t rule_idx,
    float rule_confidence);

/* Register an abductive hypothesis or assimilated inquiry */
int32_t ProvenanceRecordAssimilated(
    PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o,
    float user_confidence);

/* Find index of a fact in provenance table (-1 if not found) */
int32_t ProvenanceFind(
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID s, SYMBOL_ID r, SYMBOL_ID o);

/* Metacognitive query: "Why do I believe this?"
   Constructs a human-readable justification tree tracing back to axioms. */
uint32_t MetacognitiveExplainBelief(
    const GRAPH *graph,
    const PROVENANCE_TABLE *pt,
    const GRAPH_RULE_BASE *rb,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    char *out,
    size_t out_size);

/* Metacognitive query: "What is the weakest link / premise in this belief?"
   Finds the lowest-confidence rule or axiom upon which this fact depends. */
float MetacognitiveFindWeakestLink(
    const GRAPH *graph,
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    char *out_weakest_desc,
    size_t desc_size);

/* Metacognitive query: "Hypothetical loss analysis: What would collapse if fact X is refuted?"
   Returns number of derived facts that would lose support. */
uint32_t MetacognitiveAuditHypotheticalLoss(
    const PROVENANCE_TABLE *pt,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    SYMBOL_ID object,
    int32_t *out_impacted_indices,
    uint32_t max_impacted);

/* Metacognitive audit: "Assess overall epistemic health of the knowledge graph" */
typedef struct
{
    uint32_t total_beliefs;
    uint32_t observed_axioms;
    uint32_t deduced_beliefs;
    uint32_t assimilated_beliefs;
    float    avg_robustness;
    uint32_t high_vulnerability_count; /* Robustness < 0.70 */
} EPISTEMIC_HEALTH_REPORT;

EPISTEMIC_HEALTH_REPORT MetacognitiveAuditGraphHealth(const PROVENANCE_TABLE *pt);

#endif /* METACOGNITION_H */
