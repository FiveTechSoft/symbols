#ifndef LEARNING_H
#define LEARNING_H

#include <stdint.h>
#include "graph.h"

typedef struct
{
    SYMBOL_ID object;
    char      name[64];
    uint64_t  count;
    float     probability;
} PREDICTION;

typedef struct
{
    uint32_t min_count_threshold;
    float    smoothing_epsilon;
} LEARNING_CONFIG;

LEARNING_CONFIG LearningConfigDefault(void);

int LearningSentence(GRAPH *graph, const char *sentence);

uint32_t LearningCorpus(GRAPH *graph, const char **sentences, uint32_t count);

uint32_t LearningPredict(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID relation,
    PREDICTION *out_predictions,
    uint32_t max_predictions
);

uint32_t LearningPredictText(
    const GRAPH *graph,
    const char *subject_name,
    const char *relation_name,
    PREDICTION *out_predictions,
    uint32_t max_predictions
);

void NormalizeDiacritics(char *str);

#endif
