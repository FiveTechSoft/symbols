#include "neuro_prolog.h"
#include "relation.h"
#include "symbol.h"
#include "embedding.h"

#include <string.h>

#define NP_MAX_ES_FACTS 2048 /* cap de identidad (a ES b) coleccionado 1 vez */
#define NP_MAX_CANDS    64   /* cap de cands. por cierre de identidad */
#define NP_MAX_SCAN     1024 /* buffer de RelationFindBySubject */

typedef struct
{
    uint32_t u;
    uint32_t v;
} NP_ES_PAIR; /* hecho de identidad a ES b (simetrico en uso) */

typedef struct
{
    SYMBOL_ID constant;
    float     factor; /* 0.9 ^ saltos */
    uint32_t  hops;
} NP_CAND;

static float NPWeightCap(float weight)
{
    if (weight < 1.0f)
        return weight;
    return 1.0f;
}

void NPDefaultOptions(NP_OPTIONS *opt)
{
    if (opt == NULL)
        return;
    opt->min_conf = NP_MIN_CONF_DEF;
    opt->max_depth = NP_MAX_DEPTH_DEF;
    opt->max_solutions = NP_MAX_SOL_DEF;
    opt->allow_fuzzy = 1;
    opt->fuzzy_gate = NP_FUZZY_GATE;
}

SYMBOL_ID NPResolve(const NP_TERM *term, const NP_FRAME *frame)
{
    if (term == NULL || frame == NULL)
        return SYMBOL_INVALID;
    if (term->type == NP_TERM_CONSTANT)
        return term->id;
    if (term->id >= NP_MAX_VARS)
        return SYMBOL_INVALID;
    if (!frame->bound[term->id])
        return SYMBOL_INVALID;
    return frame->values[term->id];
}

int NPUnifyTerm(const NP_TERM *term, SYMBOL_ID value, NP_FRAME *frame)
{
    if (term == NULL || frame == NULL || value == SYMBOL_INVALID)
        return 0;
    if (term->type == NP_TERM_CONSTANT)
        return term->id == value;
    if (term->id >= NP_MAX_VARS)
        return 0;
    if (!frame->bound[term->id])
    {
        frame->values[term->id] = value;
        frame->bound[term->id] = 1;
        return 1;
    }
    return frame->values[term->id] == value;
}

/* Recupera los hechos de identidad positivos a ES b una sola vez.
   Devuelve (trunca) al llegar al cap. Orden de tabla = determinismo. */
static void NPIdentityFacts(const GRAPH *graph,
                            NP_ES_PAIR *pairs,
                            uint32_t *count,
                            uint32_t max_pairs)
{
    *count = 0;
    if (graph == NULL || graph->symbols == NULL || graph->relations == NULL)
        return;
    SYMBOL_ID es_pred = SymbolFind(graph->symbols, NP_IDENTITY_PRED);
    if (es_pred == SYMBOL_INVALID)
        return;
    uint32_t total = RelationCount(graph->relations);
    for (uint32_t i = 0; i < total && *count < max_pairs; i++)
    {
        const RELATION *r = RelationGet(graph->relations, i);
        if (r != NULL && r->relation == es_pred &&
            r->polarity == POLARITY_POSITIVE)
        {
            pairs[*count].u = r->subject;
            pairs[*count].v = r->object;
            (*count)++;
        }
    }
}

/* Cierre de identidad por BFS desde start: cada candidato es una
   constante alcanzable por cadena de ES con factor 0.9^saltos. start
   va incluido con factor 1.0. Visitados: sin re-expansion (ciclos).
   min_factor: bajo ese factor ya no hay solucion que pise min_conf. */
static uint32_t NPIdentityClosure(const NP_ES_PAIR *pairs,
                                  uint32_t npairs,
                                  SYMBOL_ID start,
                                  float min_factor,
                                  uint32_t max_depth,
                                  NP_CAND *out,
                                  uint32_t out_max)
{
    if (out == NULL || out_max == 0 || start == SYMBOL_INVALID)
        return 0;
    if (out_max > NP_MAX_CANDS)
        out_max = NP_MAX_CANDS;

    typedef struct
    {
        SYMBOL_ID id;
        float     factor;
        uint32_t  hops;
    } NP_NODE;

    NP_NODE  frontier[NP_MAX_CANDS];
    SYMBOL_ID visited[NP_MAX_CANDS];
    uint32_t  head = 0, tail = 0, nvis = 0, n = 0;

    frontier[tail].id = start;
    frontier[tail].factor = 1.0f;
    frontier[tail].hops = 0;
    tail++;
    visited[nvis] = start;
    nvis++;

    out[n].constant = start;
    out[n].factor = 1.0f;
    out[n].hops = 0;
    n++;

    while (head < tail)
    {
        SYMBOL_ID node = frontier[head].id;
        float factor = frontier[head].factor;
        uint32_t hops = frontier[head].hops;
        head++;

        if (hops >= max_depth)
            continue;
        float next = factor * NP_HOP_DECAY;
        if (next < min_factor)
            continue;

        for (uint32_t i = 0; i < npairs && nvis < NP_MAX_CANDS; i++)
        {
            SYMBOL_ID nb = SYMBOL_INVALID;
            if (pairs[i].u == node)
                nb = pairs[i].v;
            else if (pairs[i].v == node)
                nb = pairs[i].u;
            else
                continue;
            if (nb == SYMBOL_INVALID)
                continue;
            int seen = 0;
            for (uint32_t k = 0; k < nvis; k++)
            {
                if (visited[k] == nb)
                {
                    seen = 1;
                    break;
                }
            }
            if (seen)
                continue;
            visited[nvis] = nb;
            nvis++;

            out[n].constant = nb;
            out[n].factor = next;
            out[n].hops = hops + 1;
            n++;

            frontier[tail].id = nb;
            frontier[tail].factor = next;
            frontier[tail].hops = hops + 1;
            tail++;
        }
    }
    return n;
}

/* Duplicados: mismo triple conserva la max. confianza. Devuelve 1 si
   se anadio (o actualizo) una solucion. */
static int NPPushSolution(NP_SOLUTION *out,
                          uint32_t out_max,
                          uint32_t *count,
                          SYMBOL_ID s,
                          SYMBOL_ID p,
                          SYMBOL_ID o,
                          float conf,
                          uint32_t depth,
                          int fuzzy,
                          SYMBOL_ID es_id)
{
    if (out == NULL || count == NULL ||
        s == SYMBOL_INVALID || o == SYMBOL_INVALID)
        return 0;
    /* X ES X nunca es respuesta util (reflexividad trivial). */
    if (es_id != SYMBOL_INVALID && p == es_id && s == o)
        return 0;
    for (uint32_t i = 0; i < *count; i++)
    {
        if (out[i].subject == s && out[i].object == o && out[i].predicate == p)
        {
            if (conf > out[i].confidence)
                out[i].confidence = conf;
            return 0;
        }
    }
    if (*count >= out_max)
        return 0;
    out[*count].subject = s;
    out[*count].predicate = p;
    out[*count].object = o;
    out[*count].confidence = conf;
    out[*count].depth = (depth > 255) ? 255 : (uint8_t)depth;
    out[*count].fuzzy = (uint8_t)fuzzy;
    (*count)++;
    return 1;
}

uint32_t NPProve(const GRAPH *graph,
                 const NP_QUERY *query,
                 const NP_OPTIONS *opt_in,
                 NP_SOLUTION *out,
                 uint32_t out_max)
{
    NP_OPTIONS opt;
    NPDefaultOptions(&opt);
    if (opt_in != NULL)
        opt = *opt_in;
    if (opt.max_depth > 64)
        opt.max_depth = 64; /* cap de salidas del cierre; depth byte */
    if (opt.max_solutions > out_max)
        opt.max_solutions = out_max;

    if (graph == NULL || query == NULL || out == NULL || out_max == 0)
        return 0;
    const RELATION_TABLE *rt = graph->relations;
    const SYMBOL_TABLE   *st = graph->symbols;
    if (rt == NULL || st == NULL || rt->count == 0)
        return 0;

    NP_FRAME frame;
    memset(&frame, 0, sizeof(NP_FRAME));

    const SYMBOL_ID s_res = NPResolve(&query->subject, &frame);
    const SYMBOL_ID o_res = NPResolve(&query->object, &frame);
    const SYMBOL_ID p_res = query->predicate; /* 0 = predicado libre */
    const SYMBOL_ID es_id = SymbolFind(st, NP_IDENTITY_PRED);

    NP_ES_PAIR pairs[NP_MAX_ES_FACTS];
    uint32_t   np_pairs = 0;
    if (opt.max_depth > 0)
        NPIdentityFacts(graph, pairs, &np_pairs, NP_MAX_ES_FACTS);

    NP_CAND    s_cands[NP_MAX_CANDS];
    NP_CAND    o_cands[NP_MAX_CANDS];
    uint32_t   n_s = 0, n_o = 0;
    if (s_res != SYMBOL_INVALID)
        n_s = NPIdentityClosure(pairs, np_pairs, s_res,
                                opt.min_conf, opt.max_depth,
                                s_cands, NP_MAX_CANDS);
    if (o_res != SYMBOL_INVALID)
        n_o = NPIdentityClosure(pairs, np_pairs, o_res,
                                opt.min_conf, opt.max_depth,
                                o_cands, NP_MAX_CANDS);

    uint32_t count = 0;
    RELATION *facts[NP_MAX_SCAN];

    if (s_res != SYMBOL_INVALID)
    {
        /* Sujeto fijado: cada cand. del cierre de identidad x el
           indice por sujeto. */
        for (uint32_t i = 0; i < n_s; i++)
        {
            const NP_CAND *sc = &s_cands[i];
            uint32_t nf = RelationFindBySubject(rt, sc->constant,
                                                facts, NP_MAX_SCAN);
            for (uint32_t k = 0; k < nf; k++)
            {
                const RELATION *r = facts[k];
                if (r->polarity != POLARITY_POSITIVE)
                    continue;
                if (p_res != SYMBOL_INVALID && r->relation != p_res)
                    continue;
                float w = NPWeightCap(r->weight);

                if (o_res != SYMBOL_INVALID)
                {
                    for (uint32_t j = 0; j < n_o; j++)
                    {
                        if (r->object != o_cands[j].constant)
                            continue;
                        float conf = w * sc->factor * o_cands[j].factor;
                        if (conf < opt.min_conf)
                            continue;
                        NPPushSolution(out, opt.max_solutions, &count,
                                       s_res, r->relation, o_res,
                                       conf, sc->hops + o_cands[j].hops, 0,
                                       es_id);
                    }
                }
                else
                {
                    /* Objeto libre: cada hecho enlaza la variable */
                    NP_FRAME f2;
                    memset(&f2, 0, sizeof(NP_FRAME));
                    if (!NPUnifyTerm(&query->object, r->object, &f2))
                        continue;
                    float conf = w * sc->factor;
                    if (conf < opt.min_conf)
                        continue;
                    NPPushSolution(out, opt.max_solutions, &count,
                                   s_res, r->relation, r->object,
                                   conf, sc->hops, 0, es_id);
                }
            }
        }
    }
    else
    {
        /* Sujeto libre: escaneo total (orden de tabla = determinismo) */
        uint32_t total = RelationCount(rt);
        for (uint32_t i = 0; i < total; i++)
        {
            const RELATION *r = RelationGet(rt, i);
            if (r->polarity != POLARITY_POSITIVE)
                continue;
            if (p_res != SYMBOL_INVALID && r->relation != p_res)
                continue;
            NP_FRAME f2;
            memset(&f2, 0, sizeof(NP_FRAME));
            if (!NPUnifyTerm(&query->subject, r->subject, &f2))
                continue;
            float w = NPWeightCap(r->weight);

            if (o_res != SYMBOL_INVALID)
            {
                for (uint32_t j = 0; j < n_o; j++)
                {
                    if (r->object != o_cands[j].constant)
                        continue;
                    float conf = w * o_cands[j].factor;
                    if (conf < opt.min_conf)
                        continue;
                    NPPushSolution(out, opt.max_solutions, &count,
                                   r->subject, r->relation, o_res,
                                   conf, o_cands[j].hops, 0, es_id);
                }
            }
            else
            {
                if (!NPUnifyTerm(&query->object, r->object, &f2))
                    continue;
                float conf = w;
                if (conf < opt.min_conf)
                    continue;
                 NPPushSolution(out, opt.max_solutions, &count,
                               r->subject, r->relation, r->object,
                               conf, 0, 0, es_id);
            }
        }
    }

    /* Puente fuzzy: solo si no salio nada exacto/inferido, hay tabla
       de embeddings, el predicado es concreto y la similitud entre
       predicado pedido y guardado pisa la puerta. La distancia se mide
       con la metrica vigente (hoy coseno 32D; mas adelante Hamming
       256-bit: solo cambia EmbeddingCosineSimilarity). */
    if (count == 0 && opt.allow_fuzzy &&
        graph->embeddings != NULL && p_res != SYMBOL_INVALID)
    {
        const float *qvec = EmbeddingGetVector(graph->embeddings, p_res);
        if (qvec != NULL)
        {
            if (s_res != SYMBOL_INVALID)
            {
                uint32_t nf = RelationFindBySubject(rt, s_res,
                                                    facts, NP_MAX_SCAN);
                for (uint32_t k = 0; k < nf; k++)
                {
                    const RELATION *r = facts[k];
                    if (r->polarity != POLARITY_POSITIVE)
                        continue;
                    if (r->relation == p_res)
                        continue;
                    const float *ovec =
                        EmbeddingGetVector(graph->embeddings, r->relation);
                    if (ovec == NULL)
                        continue;
                    float gamma = EmbeddingCosineSimilarity(qvec, ovec);
                    if (gamma < opt.fuzzy_gate)
                        continue;
                    NP_FRAME f2;
                    memset(&f2, 0, sizeof(NP_FRAME));
                    SYMBOL_ID o = NPResolve(&query->object, &f2);
                    if (o != SYMBOL_INVALID && o != r->object)
                        continue;
                    if (o == SYMBOL_INVALID &&
                        !NPUnifyTerm(&query->object, r->object, &f2))
                        continue;
                    float conf = NPWeightCap(r->weight) * gamma;
                    if (conf < opt.min_conf)
                        continue;
                    NPPushSolution(out, opt.max_solutions, &count,
                                   s_res, r->relation,
                                   (o != SYMBOL_INVALID) ? o : r->object,
                                   conf, 0, 1, es_id);
                }
            }
            else
            {
                uint32_t total = RelationCount(rt);
                for (uint32_t i = 0; i < total; i++)
                {
                    const RELATION *r = RelationGet(rt, i);
                    if (r->polarity != POLARITY_POSITIVE)
                        continue;
                    if (r->relation == p_res)
                        continue;
                    const float *ovec =
                        EmbeddingGetVector(graph->embeddings, r->relation);
                    if (ovec == NULL)
                        continue;
                    float gamma = EmbeddingCosineSimilarity(qvec, ovec);
                    if (gamma < opt.fuzzy_gate)
                        continue;
                    NP_FRAME f2;
                    memset(&f2, 0, sizeof(NP_FRAME));
                    if (!NPUnifyTerm(&query->subject, r->subject, &f2))
                        continue;
                    SYMBOL_ID o = NPResolve(&query->object, &f2);
                    if (o != SYMBOL_INVALID && o != r->object)
                        continue;
                    if (o == SYMBOL_INVALID &&
                        !NPUnifyTerm(&query->object, r->object, &f2))
                        continue;
                    float conf = NPWeightCap(r->weight) * gamma;
                    if (conf < opt.min_conf)
                        continue;
                    NPPushSolution(out, opt.max_solutions, &count,
                                   r->subject, r->relation,
                                   (o != SYMBOL_INVALID) ? o : r->object,
                                   conf, 0, 1, es_id);
                }
            }
        }
    }

    return count;
}
