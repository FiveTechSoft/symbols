#ifndef GENERATOR_H
#define GENERATOR_H

#include <stddef.h>
#include <stdint.h>
#include "graph.h"
#include "learning.h"

typedef enum
{
    GEN_STYLE_CONCISE = 0,
    GEN_STYLE_EXPLAIN,
    GEN_STYLE_PROBABILISTIC
} GENERATOR_STYLE;

typedef struct
{
    GENERATOR_STYLE style;
    int             capitalize_first;
    int             add_period;
} GENERATOR_CONFIG;

GENERATOR_CONFIG GeneratorConfigDefault(void);

int GeneratorFromRelation(
    const GRAPH *graph,
    const RELATION *relation,
    const GENERATOR_CONFIG *config,
    char *out_text,
    size_t max_len
);

int GeneratorAggregateRelations(
    const GRAPH *graph,
    const RELATION **relations,
    uint32_t count,
    const GENERATOR_CONFIG *config,
    char *out_text,
    size_t max_len
);

int GeneratorFromPredictions(
    const GRAPH *graph,
    const char *subject_name,
    const char *relation_name,
    const PREDICTION *predictions,
    uint32_t count,
    char *out_text,
    size_t max_len
);

int GeneratorAnswerQuery(
    const GRAPH *graph,
    const char *subject_name,
    const char *relation_name,
    char *out_text,
    size_t max_len
);

#endif
