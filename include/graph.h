#ifndef GRAPH_H
#define GRAPH_H

#include <stdint.h>
#include "symbol.h"
#include "relation.h"
#include "embedding.h"

#define SIMILARITY_THRESHOLD_DEFAULT 0.70f

typedef enum
{
    CONFLICT_REJECT_NEW = 0,
    CONFLICT_OVERWRITE,
    CONFLICT_EVIDENCE_WINS,
    CONFLICT_ALLOW_BOTH
} CONFLICT_POLICY;

typedef struct
{
    int      has_conflict;
    uint64_t positive_evidence;
    uint64_t negative_evidence;
    float    positive_weight;
    float    negative_weight;
} CONTRADICTION_REPORT;

typedef struct
{
    SYMBOL_TABLE    *symbols;
    RELATION_TABLE  *relations;
    EMBEDDING_TABLE *embeddings;
} GRAPH;

GRAPH *GraphCreate(uint32_t symbol_capacity,
                   uint32_t relation_capacity);

void GraphDestroy(GRAPH *graph);

void GraphSetEmbeddingTable(GRAPH *graph, EMBEDDING_TABLE *embeddings);

SYMBOL_ID GraphAddSymbol(GRAPH *graph, const char *name);

int GraphAddRelation(GRAPH *graph,
                     SYMBOL_ID subject,
                     SYMBOL_ID predicate,
                     SYMBOL_ID object);

int GraphAddRelationPolar(GRAPH *graph,
                          SYMBOL_ID subject,
                          SYMBOL_ID predicate,
                          SYMBOL_ID object,
                          RELATION_POLARITY polarity,
                          CONFLICT_POLICY policy);

CONTRADICTION_REPORT GraphCheckContradiction(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object);

RELATION *GraphFindRelation(GRAPH *graph,
                            SYMBOL_ID subject,
                            SYMBOL_ID predicate,
                            SYMBOL_ID object);

uint32_t GraphQuerySubject(GRAPH *graph, SYMBOL_ID subject,
                           RELATION **results, uint32_t max_results);

uint32_t GraphQuerySubjectPredicate(GRAPH *graph, SYMBOL_ID subject,
                                    SYMBOL_ID predicate,
                                    RELATION **results, uint32_t max_results);

uint32_t GraphQueryObject(GRAPH *graph, SYMBOL_ID object,
                          RELATION **results, uint32_t max_results);

int GraphInferTransitive(GRAPH *graph,
                         SYMBOL_ID subject,
                         SYMBOL_ID predicate,
                         SYMBOL_ID object);

SYMBOL_ID GraphResolveSynonym(const GRAPH *graph,
                              SYMBOL_ID subject,
                              float min_similarity,
                              float *out_similarity);

uint32_t GraphQuerySubjectPredicateFuzzy(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t max_results,
    float min_similarity,
    SYMBOL_ID *out_resolved_subject);

#endif
