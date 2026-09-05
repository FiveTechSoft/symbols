#ifndef NLG_H
#define NLG_H

#include <stdint.h>
#include "graph.h"

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
    SYMBOL_ID relation,
    RELATION **results,
    uint32_t count,
    char *out,
    uint32_t out_size);

/* Generate a "no results" response with proactive suggestions */
void NLGGenerateNoResults(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
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
    char *out,
    uint32_t out_size);

#endif
