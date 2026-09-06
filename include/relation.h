#ifndef RELATION_H
#define RELATION_H

#include <stdint.h>
#include "symbol.h"

typedef enum
{
    POLARITY_POSITIVE = 0,
    POLARITY_NEGATIVE = 1
} RELATION_POLARITY;

typedef struct
{
    SYMBOL_ID         subject;
    SYMBOL_ID         relation;
    SYMBOL_ID         object;
    RELATION_POLARITY polarity;
    uint64_t          count;
    float             weight;
    SYMBOL_ID         source;   /* procedencia (0 = desconocida) */
} RELATION;

typedef struct RELATION_INDEX RELATION_INDEX;

typedef struct
{
    RELATION *items;
    uint32_t  count;
    uint32_t  capacity;
    RELATION_INDEX *idx;

    /* Subject chain index: subject -> item list (insertion order) */
    uint32_t *subj_heads;
    uint32_t *subj_next;    /* per-item next index in chain */
    uint32_t  subj_capacity;
    uint32_t  subj_mask;
} RELATION_TABLE;

RELATION_TABLE *RelationTableCreate(uint32_t capacity);
void RelationTableInit(RELATION_TABLE *table, uint32_t capacity);
void RelationTableDestroy(RELATION_TABLE *table);

int RelationAddPolar(RELATION_TABLE *table,
                     SYMBOL_ID subject, SYMBOL_ID relation, SYMBOL_ID object,
                     RELATION_POLARITY polarity);

int RelationAdd(RELATION_TABLE *table,
                SYMBOL_ID subject, SYMBOL_ID relation, SYMBOL_ID object);

RELATION *RelationFindPolar(RELATION_TABLE *table,
                            SYMBOL_ID subject, SYMBOL_ID relation, SYMBOL_ID object,
                            RELATION_POLARITY polarity);

RELATION *RelationFind(RELATION_TABLE *table,
                       SYMBOL_ID subject, SYMBOL_ID relation, SYMBOL_ID object);

RELATION *RelationFindOpposite(RELATION_TABLE *table,
                               SYMBOL_ID subject, SYMBOL_ID relation, SYMBOL_ID object,
                               RELATION_POLARITY polarity);

uint32_t RelationFindBySubject(const RELATION_TABLE *table, SYMBOL_ID subject,
                               RELATION **results, uint32_t max_results);

uint32_t RelationFindBySubjectRelation(const RELATION_TABLE *table,
                                         SYMBOL_ID subject, SYMBOL_ID relation,
                                         RELATION **results, uint32_t max_results);

/* Exact count of (subject, relation) triples. Counting answers need
   totals, not capped samples: a truncated count would lie. */
uint32_t RelationCountBySubjectRelation(const RELATION_TABLE *table,
                                        SYMBOL_ID subject,
                                        SYMBOL_ID relation);

/* Same, restricted to one polarity: counting what IS (positives)
   must not include what is DENIED (negatives). */
uint32_t RelationCountBySubjectRelationPolar(const RELATION_TABLE *table,
                                             SYMBOL_ID subject,
                                             SYMBOL_ID relation,
                                             RELATION_POLARITY polarity);

uint32_t RelationFindByObject(const RELATION_TABLE *table, SYMBOL_ID object,
                              RELATION **results, uint32_t max_results);

void RelationStrengthen(RELATION *relation, float amount);

/* Sets provenance (e.g. "GEN 1:1" or "file.tsv:42").
   source==SYMBOL_INVALID clears it. No effect on NULL relation. */
void RelationSetSource(RELATION *relation, SYMBOL_ID source);

const RELATION *RelationGet(const RELATION_TABLE *table, uint32_t index);
uint32_t RelationCount(const RELATION_TABLE *table);

#endif
