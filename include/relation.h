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
    SYMBOL_ID         predicate;
    SYMBOL_ID         object;
    RELATION_POLARITY polarity;
    uint64_t          count;
    float             weight;
} RELATION;

typedef struct RELATION_INDEX RELATION_INDEX;

typedef struct
{
    RELATION *items;
    uint32_t  count;
    uint32_t  capacity;
    RELATION_INDEX *idx;
} RELATION_TABLE;

RELATION_TABLE *RelationTableCreate(uint32_t capacity);
void RelationTableInit(RELATION_TABLE *table, uint32_t capacity);
void RelationTableDestroy(RELATION_TABLE *table);

int RelationAddPolar(RELATION_TABLE *table,
                     SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object,
                     RELATION_POLARITY polarity);

int RelationAdd(RELATION_TABLE *table,
                SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object);

RELATION *RelationFindPolar(RELATION_TABLE *table,
                            SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object,
                            RELATION_POLARITY polarity);

RELATION *RelationFind(RELATION_TABLE *table,
                       SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object);

RELATION *RelationFindOpposite(RELATION_TABLE *table,
                               SYMBOL_ID subject, SYMBOL_ID predicate, SYMBOL_ID object,
                               RELATION_POLARITY polarity);

uint32_t RelationFindBySubject(const RELATION_TABLE *table, SYMBOL_ID subject,
                               RELATION **results, uint32_t max_results);

uint32_t RelationFindBySubjectPredicate(const RELATION_TABLE *table,
                                        SYMBOL_ID subject, SYMBOL_ID predicate,
                                        RELATION **results, uint32_t max_results);

uint32_t RelationFindByObject(const RELATION_TABLE *table, SYMBOL_ID object,
                              RELATION **results, uint32_t max_results);

void RelationStrengthen(RELATION *relation, float amount);

const RELATION *RelationGet(const RELATION_TABLE *table, uint32_t index);
uint32_t RelationCount(const RELATION_TABLE *table);

#endif
