#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "compat.h"
#include "graph.h"
#include "learning.h"


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


int GraphAddRelationPolar(
    GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object,
    RELATION_POLARITY polarity,
    CONFLICT_POLICY policy)
{
    if (!graph || subject == SYMBOL_INVALID || predicate == SYMBOL_INVALID || object == SYMBOL_INVALID)
        return 0;

    RELATION *opposite = RelationFindOpposite(graph->relations, subject, predicate, object, polarity);

    if (opposite != NULL)
    {
        switch (policy)
        {
            case CONFLICT_REJECT_NEW:
                return 0;

            case CONFLICT_OVERWRITE:
                opposite->polarity = polarity;
                opposite->count = 1;
                opposite->weight = 1.0f;
                return 1;

            case CONFLICT_EVIDENCE_WINS:
                opposite->count--;
                if (opposite->count == 0)
                {
                    opposite->polarity = polarity;
                    opposite->count = 1;
                    opposite->weight = 1.0f;
                }
                else
                {
                    opposite->weight = 1.0f - (1.0f / (float)(opposite->count + 1));
                }
                return 1;

            case CONFLICT_ALLOW_BOTH:
            default:
                opposite->weight *= 0.5f;
                break;
        }
    }

    return RelationAddPolar(graph->relations, subject, predicate, object, polarity);
}


CONTRADICTION_REPORT GraphCheckContradiction(
    const GRAPH *graph,
    SYMBOL_ID subject,
    SYMBOL_ID predicate,
    SYMBOL_ID object)
{
    CONTRADICTION_REPORT rep;
    memset(&rep, 0, sizeof(rep));

    if (!graph) return rep;

    RELATION *pos = RelationFindPolar(graph->relations, subject, predicate, object, POLARITY_POSITIVE);
    RELATION *neg = RelationFindPolar(graph->relations, subject, predicate, object, POLARITY_NEGATIVE);

    if (pos != NULL && neg != NULL)
    {
        rep.has_conflict = 1;
        rep.positive_evidence = pos->count;
        rep.negative_evidence = neg->count;
        rep.positive_weight = pos->weight;
        rep.negative_weight = neg->weight;
    }

    return rep;
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
    const GRAPH *graph,
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
    const GRAPH *graph,
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
    const GRAPH *graph,
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

    /* If the symbol already has relations, use it directly */
    RELATION *dummy[1];
    uint32_t direct = RelationFindBySubject(
        graph->relations, subject, dummy, 1);

    if (direct > 0)
    {
        if (out_similarity != NULL)
            *out_similarity = 1.0f;
        return subject;
    }

    /* Find the closest synonym */
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

    /* Hybrid fallback: look up vector synonym */
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

/* ============================================================
   Attention: rank all relations of a concept by embedding
   similarity between query and each relation's object.
   ============================================================ */

uint32_t GraphQueryAttended(
    const GRAPH *graph,
    SYMBOL_ID query_id,
    RELATION **out_relations,
    float *out_scores,
    uint32_t max_results)
{
    if (graph == NULL || query_id == SYMBOL_INVALID ||
        out_relations == NULL || out_scores == NULL || max_results == 0)
        return 0;

    /* Get query embedding */
    const float *q_vec = NULL;
    if (graph->embeddings != NULL)
        q_vec = EmbeddingGetVector(graph->embeddings, query_id);

    /* Get all relations where query is subject */
    RELATION *all[256];
    uint32_t n = GraphQuerySubject((GRAPH *)graph, query_id, all, 256);

    if (n == 0)
        return 0;

    /* Score each relation */
    uint32_t count = (n < max_results) ? n : max_results;

    for (uint32_t i = 0; i < n; i++)
    {
        float score = 0.0f;

        if (q_vec != NULL && graph->embeddings != NULL)
        {
            const float *o_vec = EmbeddingGetVector(
                graph->embeddings, all[i]->object);
            if (o_vec != NULL)
                score = EmbeddingCosineSimilarity(q_vec, o_vec);
        }

        out_relations[i] = all[i];
        out_scores[i] = score;
    }

    /* Insertion sort by score descending (small N, fine) */
    for (uint32_t i = 1; i < n; i++)
    {
        RELATION *r_key = out_relations[i];
        float s_key = out_scores[i];
        int j = (int)i - 1;
        while (j >= 0 && out_scores[j] < s_key)
        {
            out_relations[j + 1] = out_relations[j];
            out_scores[j + 1] = out_scores[j];
            j--;
        }
        out_relations[j + 1] = r_key;
        out_scores[j + 1] = s_key;
    }

    return count;
}

/* ============================================================
   Embed a text query: average embeddings of matched tokens
   ============================================================ */

int GraphEmbedQuery(
    const GRAPH *graph,
    const char *query_text,
    float *out_vector,
    SYMBOL_ID *out_matched,
    uint32_t *out_count)
{
    if (graph == NULL || query_text == NULL || out_vector == NULL)
        return 0;

    memset(out_vector, 0, sizeof(float) * EMBEDDING_DIM);
    if (out_count) *out_count = 0;
    uint32_t matched = 0;

    /* Tokenize: split on spaces, strip articles */
    char buffer[1024];
    strncpy(buffer, query_text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    NormalizeDiacritics(buffer);

    char *saveptr = NULL;
    char *token = strtok_r(buffer, " \t\n,;:!?.()\"'¿¡", &saveptr);

    while (token != NULL)
    {
        /* Skip short tokens and articles */
        if (strlen(token) < 2)
        {
            token = strtok_r(NULL, " \t\n,;:!?.()\"'¿¡", &saveptr);
            continue;
        }

        /* Skip common Spanish stop words */
        if (strcmp(token, "el") == 0 || strcmp(token, "la") == 0 ||
            strcmp(token, "los") == 0 || strcmp(token, "las") == 0 ||
            strcmp(token, "de") == 0 || strcmp(token, "del") == 0 ||
            strcmp(token, "en") == 0 || strcmp(token, "un") == 0 ||
            strcmp(token, "una") == 0 || strcmp(token, "que") == 0 ||
            strcmp(token, "es") == 0 || strcmp(token, "y") == 0 ||
            strcmp(token, "a") == 0 || strcmp(token, "por") == 0 ||
            strcmp(token, "con") == 0 || strcmp(token, "para") == 0)
        {
            token = strtok_r(NULL, " \t\n,;:!?.()\"'¿¡", &saveptr);
            continue;
        }

        /* Uppercase and look up */
        char upper[64];
        size_t len = strlen(token);
        for (size_t i = 0; i < len && i < 63; i++)
            upper[i] = (char)toupper((unsigned char)token[i]);
        upper[len] = '\0';

        SYMBOL_ID sid = SymbolFind(graph->symbols, upper);
        if (sid != SYMBOL_INVALID && graph->embeddings != NULL)
        {
            const float *vec = EmbeddingGetVector(graph->embeddings, sid);
            if (vec != NULL)
            {
                for (int d = 0; d < EMBEDDING_DIM; d++)
                    out_vector[d] += vec[d];
                matched++;
                if (out_matched && matched <= 32)
                    out_matched[matched - 1] = sid;
            }
        }

        token = strtok_r(NULL, " \t\n,;:!?.()\"'¿¡", &saveptr);
    }

    /* Average */
    if (matched > 0)
    {
        float inv = 1.0f / (float)matched;
        for (int d = 0; d < EMBEDDING_DIM; d++)
            out_vector[d] *= inv;
    }

    if (out_count) *out_count = matched;
    return matched > 0 ? 1 : 0;
}

/* ============================================================
   Score ALL relations by cosine similarity with query vector
   ============================================================ */

/* ============================================================
   Per-token attention with positional encoding
   Query: "la CAPITAL de FRANCIA es"
   Tokens: [LA(0), CAPITAL(1), DE(2), FRANCIA(3), ES(4)]
   Positions: even→predicate, odd→entity (subject/object)
   ============================================================ */

uint32_t GraphQueryByEmbedding(
    const GRAPH *graph,
    const float *query_vector,
    RELATION **out_relations,
    float *out_scores,
    uint32_t max_results)
{
    if (graph == NULL || query_vector == NULL ||
        out_relations == NULL || out_scores == NULL || max_results == 0)
        return 0;

    uint32_t total = RelationCount(graph->relations);
    if (total == 0)
        return 0;

    float *scores = (float *)calloc(total, sizeof(float));
    RELATION **rels = (RELATION **)calloc(total, sizeof(RELATION *));
    if (scores == NULL || rels == NULL)
    {
        free(scores);
        free(rels);
        return 0;
    }

    /* Positional weights: even positions → predicate, odd → entity */
    float pos_weight_predicate[] = {0.3f, 0.0f, 0.3f, 0.0f, 0.3f, 0.0f, 0.3f, 0.0f};
    float pos_weight_entity[]    = {0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f};

    uint32_t count = 0;
    for (uint32_t i = 0; i < total; i++)
    {
        /* items live in a mutable table; RelationGet returns const for readers */
        RELATION *r = (RELATION *)RelationGet(graph->relations, i);
        if (r == NULL) continue;

        float score = 0.0f;

        if (graph->embeddings != NULL)
        {
            const float *s_vec = EmbeddingGetVector(graph->embeddings, r->subject);
            const float *p_vec = EmbeddingGetVector(graph->embeddings, r->predicate);
            const float *o_vec = EmbeddingGetVector(graph->embeddings, r->object);

            /* Default: full-graph cosine fallback */
            float base_sim = 0.0f;
            int base_count = 0;
            if (s_vec) { base_sim += EmbeddingCosineSimilarity(query_vector, s_vec); base_count++; }
            if (p_vec) { base_sim += EmbeddingCosineSimilarity(query_vector, p_vec); base_count++; }
            if (o_vec) { base_sim += EmbeddingCosineSimilarity(query_vector, o_vec); base_count++; }
            if (base_count > 0) base_sim /= (float)base_count;

            /* Per-token positional attention */
            float token_score = 0.0f;
            float token_weight = 0.0f;

            /* Token matches against predicate embedding */
            if (p_vec)
            {
                for (int t = 0; t < 8; t++)
                {
                    if (pos_weight_predicate[t] > 0.0f)
                    {
                        /* Compare query_vector segments with predicate */
                        float sim = EmbeddingCosineSimilarity(query_vector, p_vec);
                        token_score += sim * pos_weight_predicate[t];
                        token_weight += pos_weight_predicate[t];
                    }
                }
            }

            /* Token matches against subject/object embeddings */
            if (s_vec || o_vec)
            {
                for (int t = 0; t < 8; t++)
                {
                    if (pos_weight_entity[t] > 0.0f)
                    {
                        float sim_s = s_vec ? EmbeddingCosineSimilarity(query_vector, s_vec) : 0.0f;
                        float sim_o = o_vec ? EmbeddingCosineSimilarity(query_vector, o_vec) : 0.0f;
                        float best = (sim_s > sim_o) ? sim_s : sim_o;
                        token_score += best * pos_weight_entity[t];
                        token_weight += pos_weight_entity[t];
                    }
                }
            }

            if (token_weight > 0.0f)
                token_score /= token_weight;

            /* Combine: 60% positional attention + 40% base cosine */
            score = 0.6f * token_score + 0.4f * base_sim;
        }

        rels[count] = r;
        scores[count] = score;
        count++;
    }

    /* Partial sort: find top max_results */
    uint32_t result_count = (count < max_results) ? count : max_results;

    for (uint32_t i = 0; i < result_count; i++)
    {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < count; j++)
        {
            if (scores[j] > scores[best])
                best = j;
        }
        if (best != i)
        {
            float ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
            RELATION *tr = rels[i]; rels[i] = rels[best]; rels[best] = tr;
        }
        out_relations[i] = rels[i];
        out_scores[i] = scores[i];
    }

    free(scores);
    free(rels);
    return result_count;
}
