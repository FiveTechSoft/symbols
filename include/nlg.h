#ifndef NLG_H
#define NLG_H

#include <stdint.h>
#include "graph.h"
#include "inference.h"

#define NLG_MAX_RESPONSE 2048

typedef struct
{
    const char *connector;
    float       min_confidence;
} NLG_CONNECTOR;

/* Generate a natural language response from direct query results */
uint32_t NLGGenerateDirect(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t count,
    char *out,
    uint32_t out_size);

/* Generate a natural language response from inference path */
void NLGGenerateInference(
    const GRAPH *graph,
    const INFERENCE_PATH *path,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object,
    char *out,
    uint32_t out_size);

/* Generate a "no results" response with proactive suggestions */
void NLGGenerateNoResults(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    char *out,
    uint32_t out_size);

/* Generate a compound response combining multiple knowledge domains */
void NLGGenerateCompound(
    const GRAPH *graph,
    SYMBOL_ID subject,
    RELATION **taxonomic,
    uint32_t tax_count,
    RELATION **functional,
    uint32_t func_count,
    const INFERENCE_PATH *inferred,
    int has_inferred,
    char *out,
    uint32_t out_size);

#endif
