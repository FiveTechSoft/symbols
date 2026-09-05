#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>
#include "graph.h"

#define MAX_INFERENCE_DEPTH 8
#define MAX_EXPLANATION_PATH 16

typedef struct
{
    SYMBOL_ID step_nodes[MAX_EXPLANATION_PATH];
    SYMBOL_ID step_predicates[MAX_EXPLANATION_PATH];
    uint32_t  depth;
    float     accumulated_confidence;
} INFERENCE_PATH;

typedef struct
{
    SYMBOL_ID pred_first;
    SYMBOL_ID pred_second;
    SYMBOL_ID pred_result;
    float     rule_weight;
} COMPOSITION_RULE;

typedef struct
{
    SYMBOL_ID subject;
    SYMBOL_ID predicate;
    SYMBOL_ID object;
    float     confidence;
} INFERRED_TRIPLE;

typedef struct
{
    uint32_t max_depth;
    float    min_confidence;
    float    decay_factor;
} INFERENCE_CONFIG;

INFERENCE_CONFIG InferenceConfigDefault(void);

int InferenceProve(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID target_predicate,
    SYMBOL_ID target_object,
    const INFERENCE_CONFIG *config,
    INFERENCE_PATH *out_path
);

uint32_t InferenceMaterializeTransitive(
    GRAPH *graph,
    SYMBOL_ID predicate,
    const INFERENCE_CONFIG *config
);

uint32_t InferenceApplyCompositionRule(
    GRAPH *graph,
    const COMPOSITION_RULE *rule,
    const INFERENCE_CONFIG *config
);

/* Read-only dry-run: collects what ApplyCompositionRule would write
   under the same rule and config, without modifying the graph. Results
   are capped at max_out. Returns the number of candidates collected. */
uint32_t InferenceDryRun(
    GRAPH *graph,
    const COMPOSITION_RULE *rule,
    const INFERENCE_CONFIG *config,
    INFERRED_TRIPLE *out,
    uint32_t max_out);

void InferencePrintExplanation(
    const GRAPH *graph,
    const INFERENCE_PATH *path
);

#endif
