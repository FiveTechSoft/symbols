#include <stdlib.h>

#include "graph.h"


/* ============================================================
   Crear / destruir
   ============================================================ */

GRAPH *GraphCreate(
    uint32_t symbol_capacity,
    uint32_t relation_capacity)
{
    GRAPH *graph;

    graph = (GRAPH *)malloc(sizeof(GRAPH));
    if (graph == NULL)
        return NULL;

    graph->symbols = SymbolTableCreate(symbol_capacity);
    graph->relations = RelationTableCreate(relation_capacity);
    graph->embeddings = NULL;

    if (graph->symbols == NULL || graph->relations == NULL)
    {
        SymbolTableDestroy(graph->symbols);
        RelationTableDestroy(graph->relations);
        free(graph);
        return NULL;
    }

    return graph;
}


void GraphDestroy(GRAPH *graph)
{
    if (graph == NULL)
        return;

    SymbolTableDestroy(graph->symbols);
    RelationTableDestroy(graph->relations);
    free(graph);
}


void GraphSetEmbeddingTable(GRAPH *graph, EMBEDDING_TABLE *embeddings)
{
    if (graph == NULL)
        return;
    graph->embeddings = embeddings;
}


/* ============================================================
   Anadir simbolo
   ============================================================ */

SYMBOL_ID GraphAddSymbol(GRAPH *graph, const char *name)
{
    if (graph == NULL || name == NULL)
        return SYMBOL_INVALID;

    return SymbolAdd(graph->symbols, name);
}


/* ============================================================
   Anadir relacion
   ============================================================ */

int GraphAddRelation(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    if (graph == NULL)
        return 0;

    if (subject == SYMBOL_INVALID ||
        predicate == SYMBOL_INVALID ||
        object == SYMBOL_INVALID)
    {
        return 0;
    }

    return RelationAdd(graph->relations, subject, predicate, object);
}


/* ============================================================
   Buscar relacion exacta
   ============================================================ */

RELATION *GraphFindRelation(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    if (graph == NULL)
        return NULL;

    return RelationFind(graph->relations, subject, predicate, object);
}


/* ============================================================
   Query: subject --?--> ?
   ============================================================ */

uint32_t GraphQuerySubject(
    GRAPH *graph,
    SYMBOL_ID subject,
    RELATION **results,
    uint32_t max_results)
{
    if (graph == NULL)
        return 0;

    return RelationFindBySubject(
        graph->relations, subject, results, max_results);
}


/* ============================================================
   Query: subject --predicate--> ?
   ============================================================ */

uint32_t GraphQuerySubjectPredicate(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t max_results)
{
    if (graph == NULL)
        return 0;

    return RelationFindBySubjectPredicate(
        graph->relations, subject, predicate, results, max_results);
}


/* ============================================================
   Query: ? --?--> object
   ============================================================ */

uint32_t GraphQueryObject(
    GRAPH *graph,
    SYMBOL_ID object,
    RELATION **results,
    uint32_t max_results)
{
    if (graph == NULL)
        return 0;

    return RelationFindByObject(
        graph->relations, object, results, max_results);
}


/* ============================================================
   Inferencia transitiva
   ============================================================ */

int GraphInferTransitive(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    RELATION *chain[256];
    uint32_t n;
    uint32_t i;

    if (graph == NULL)
        return 0;

    if (subject == SYMBOL_INVALID ||
        predicate == SYMBOL_INVALID ||
        object == SYMBOL_INVALID)
    {
        return 0;
    }

    if (RelationFind(graph->relations, subject, predicate, object) != NULL)
        return 0;

    n = RelationFindBySubjectPredicate(
        graph->relations, subject, predicate, chain, 256);

    for (i = 0; i < n; i++)
    {
        SYMBOL_ID middle = chain[i]->object;

        if (RelationFind(graph->relations, middle, predicate, object) != NULL)
        {
            RelationAdd(graph->relations, subject, predicate, object);
            return 1;
        }
    }

    return 0;
}


/* ============================================================
   Resolucion de sinonimos por embeddings (H4)
   ============================================================ */

SYMBOL_ID GraphResolveSynonym(const GRAPH *graph,
                              SYMBOL_ID subject,
                              float min_similarity,
                              float *out_similarity)
{
    if (graph == NULL || graph->embeddings == NULL ||
        subject == SYMBOL_INVALID)
    {
        if (out_similarity != NULL)
            *out_similarity = 0.0f;
        return subject;
    }

    /* Si el simbolo ya tiene relaciones, usarlo directamente */
    RELATION *dummy[1];
    uint32_t direct = RelationFindBySubject(
        graph->relations, subject, dummy, 1);

    if (direct > 0)
    {
        if (out_similarity != NULL)
            *out_similarity = 1.0f;
        return subject;
    }

    /* Buscar el sinonimo mas cercano */
    EMBEDDING_MATCH matches[8];
    uint32_t n = EmbeddingFindSimilar(graph->embeddings, subject, matches, 8);

    for (uint32_t i = 0; i < n; i++)
    {
        if (matches[i].score < min_similarity)
            continue;

        uint32_t rels = RelationFindBySubject(
            graph->relations, matches[i].id, dummy, 1);

        if (rels > 0)
        {
            if (out_similarity != NULL)
                *out_similarity = matches[i].score;
            return matches[i].id;
        }
    }

    if (out_similarity != NULL)
        *out_similarity = 0.0f;
    return subject;
}


/* ============================================================
   Consulta hibrida tolerante a sinonimos
   ============================================================ */

uint32_t GraphQuerySubjectPredicateFuzzy(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    RELATION **results,
    uint32_t max_results,
    float min_similarity,
    SYMBOL_ID *out_resolved_subject)
{
    if (graph == NULL)
        return 0;

    /* Intento exacto primero */
    uint32_t n = RelationFindBySubjectPredicate(
        graph->relations, subject, predicate, results, max_results);

    if (n > 0)
    {
        if (out_resolved_subject != NULL)
            *out_resolved_subject = subject;
        return n;
    }

    /* Fallback hibrido: buscar sinonimo vectorial */
    float sim = 0.0f;
    SYMBOL_ID resolved = GraphResolveSynonym(
        graph, subject, min_similarity, &sim);

    if (resolved != subject && resolved != SYMBOL_INVALID)
    {
        n = RelationFindBySubjectPredicate(
            graph->relations, resolved, predicate, results, max_results);

        if (out_resolved_subject != NULL)
            *out_resolved_subject = resolved;
        return n;
    }

    if (out_resolved_subject != NULL)
        *out_resolved_subject = subject;
    return 0;
}
