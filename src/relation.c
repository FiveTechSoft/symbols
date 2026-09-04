#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "relation.h"


/* ============================================================
   Crear tabla de relaciones
   ============================================================ */

RELATION_TABLE *RelationTableCreate(uint32_t capacity)
{
    RELATION_TABLE *table;

    if (capacity == 0)
        capacity = 16;

    table = (RELATION_TABLE *)malloc(sizeof(RELATION_TABLE));
    if (table == NULL)
        return NULL;

    RelationTableInit(table, capacity);
    return table;
}

void RelationTableInit(RELATION_TABLE *table, uint32_t capacity)
{
    if (capacity == 0)
        capacity = 16;

    table->items = (RELATION *)calloc(capacity, sizeof(RELATION));
    table->count = 0;
    table->capacity = capacity;
}


/* ============================================================
   Destruir tabla
   ============================================================ */

void RelationTableDestroy(RELATION_TABLE *table)
{
    if (table == NULL)
        return;

    free(table->items);
    free(table);
}


/* ============================================================
   Ampliar tabla
   ============================================================ */

static int RelationTableGrow(RELATION_TABLE *table)
{
    RELATION *new_items;
    uint32_t new_capacity;

    if (table == NULL)
        return 0;

    new_capacity = table->capacity * 2;

    if (new_capacity < table->capacity)
        return 0; /* overflow */

    new_items = (RELATION *)realloc(
        table->items,
        new_capacity * sizeof(RELATION)
    );

    if (new_items == NULL)
        return 0;

    /*
     * Inicializar la zona nueva.
     */

    memset(
        new_items + table->capacity,
        0,
        (new_capacity - table->capacity) *
        sizeof(RELATION)
    );

    table->items = new_items;
    table->capacity = new_capacity;

    return 1;
}


/* ============================================================
   Buscar relacion exacta
   ============================================================ */

RELATION *RelationFind(
    RELATION_TABLE *table,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    uint32_t i;

    if (table == NULL)
        return NULL;

    for (i = 0; i < table->count; i++)
    {
        RELATION *r = &table->items[i];

        if (r->subject == subject &&
            r->predicate == predicate &&
            r->object == object)
        {
            return r;
        }
    }

    return NULL;
}


/* ============================================================
   Anadir relacion
   ============================================================ */

int RelationAdd(
    RELATION_TABLE *table,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    RELATION *relation;

    if (table == NULL)
        return 0;

    /*
     * Los IDs de simbolo 0 estan reservados
     * para SYMBOL_INVALID.
     */

    if (subject == SYMBOL_INVALID ||
        predicate == SYMBOL_INVALID ||
        object == SYMBOL_INVALID)
    {
        return 0;
    }

    /*
     * Comprobar si ya existe.
     */

    relation = RelationFind(
        table,
        subject,
        predicate,
        object
    );

    if (relation != NULL)
    {
        relation->count++;

        /*
         * Aumentamos ligeramente la fuerza.
         */

        RelationStrengthen(
            relation,
            0.01f
        );

        return 1;
    }

    /*
     * Necesitamos espacio.
     */

    if (table->count >= table->capacity)
    {
        if (!RelationTableGrow(table))
            return 0;
    }

    relation = &table->items[table->count];

    relation->subject = subject;
    relation->predicate = predicate;
    relation->object = object;

    relation->count = 1;
    relation->weight = 1.0f;

    table->count++;

    return 1;
}


/* ============================================================
   Reforzar relacion
   ============================================================ */

void RelationStrengthen(
    RELATION *relation,
    float amount)
{
    if (relation == NULL)
        return;

    relation->weight += amount;

    /*
     * Evitamos que la fuerza crezca indefinidamente.
     */

    if (relation->weight > 1.0e9f)
        relation->weight = 1.0e9f;
}


/* ============================================================
   Buscar por sujeto
   ============================================================ */

uint32_t RelationFindBySubject(
    const RELATION_TABLE *table,
    SYMBOL_ID subject,
    RELATION **results,
    uint32_t max_results)
{
    uint32_t i;
    uint32_t found = 0;

    if (table == NULL ||
        results == NULL ||
        max_results == 0)
    {
        return 0;
    }

    for (i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject)
        {
            results[found] = &table->items[i];

            found++;

            if (found >= max_results)
                break;
        }
    }

    return found;
}


/* ============================================================
   Buscar por sujeto + predicado
   ============================================================ */

uint32_t RelationFindBySubjectPredicate(
    const RELATION_TABLE *table,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t max_results)
{
    uint32_t i;
    uint32_t found = 0;

    if (table == NULL ||
        results == NULL ||
        max_results == 0)
    {
        return 0;
    }

    for (i = 0; i < table->count; i++)
    {
        if (table->items[i].subject == subject &&
            table->items[i].predicate == predicate)
        {
            results[found] = &table->items[i];

            found++;

            if (found >= max_results)
                break;
        }
    }

    return found;
}


/* ============================================================
   Buscar por objeto
   ============================================================ */

uint32_t RelationFindByObject(
    const RELATION_TABLE *table,
    SYMBOL_ID object,
    RELATION **results,
    uint32_t max_results)
{
    uint32_t i;
    uint32_t found = 0;

    if (table == NULL ||
        results == NULL ||
        max_results == 0)
    {
        return 0;
    }

    for (i = 0; i < table->count; i++)
    {
        if (table->items[i].object == object)
        {
            results[found] = &table->items[i];

            found++;

            if (found >= max_results)
                break;
        }
    }

    return found;
}


/* ============================================================
   Obtener relacion por indice
   ============================================================ */

const RELATION *RelationGet(
    const RELATION_TABLE *table,
    uint32_t index)
{
    if (table == NULL)
        return NULL;

    if (index >= table->count)
        return NULL;

    return &table->items[index];
}


/* ============================================================
   Numero de relaciones
   ============================================================ */

uint32_t RelationCount(
    const RELATION_TABLE *table)
{
    if (table == NULL)
        return 0;

    return table->count;
}
