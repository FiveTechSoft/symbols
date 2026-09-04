#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>
#include "graph.h"

#define MAX_TRANSFER_RULES 64
#define MAX_STRUCTURAL_FEATURES 16

/* Structural feature of an entity (what it does, what it needs) */
typedef struct
{
    SYMBOL_ID predicate;   /* e.g., ESCRIBE_MEMORIA, USA_PUNTERO */
    SYMBOL_ID object;      /* e.g., DESTINO, PUNTERO */
} STRUCTURAL_FEATURE;

/* A transfer rule: if entity has features A+B, infer it also has C */
typedef struct
{
    char     name[64];            /* Rule name for explanation */
    SYMBOL_ID required_pred[MAX_STRUCTURAL_FEATURES];  /* Must have these predicates */
    SYMBOL_ID required_obj[MAX_STRUCTURAL_FEATURES];   /* With these objects (or ANY) */
    uint32_t required_count;      /* How many required features must match */
    SYMBOL_ID inferred_pred;      /* Then it probably has this predicate */
    SYMBOL_ID inferred_obj;       /* With this object (or SAME as matched) */
    float    confidence;          /* How confident in this inference */
} TRANSFER_RULE;

/* Result of analogical transfer */
typedef struct
{
    char     rule_name[64];
    SYMBOL_ID source_entity;
    SYMBOL_ID target_entity;
    SYMBOL_ID inferred_pred;
    SYMBOL_ID inferred_obj;
    float    confidence;
} TRANSFER_RESULT;

/* Initialize the transfer rule system */
void TransferInit(GRAPH *graph);

/* Apply all applicable transfer rules to an entity, return new relations added */
uint32_t TransferApply(GRAPH *graph, SYMBOL_ID entity, TRANSFER_RESULT *results, uint32_t max_results);

/* Find structural similarity between two entities (0.0 to 1.0) */
float TransferSimilarity(GRAPH *graph, SYMBOL_ID a, SYMBOL_ID b);

/* Apply analogical transfer: what A knows that B doesn't, inferred by pattern */
uint32_t TransferAnalogy(GRAPH *graph, SYMBOL_ID source, SYMBOL_ID target,
                         TRANSFER_RESULT *results, uint32_t max_results);

/* Print transfer results */
void TransferPrintResults(const GRAPH *graph, const TRANSFER_RESULT *results, uint32_t count);

#endif
