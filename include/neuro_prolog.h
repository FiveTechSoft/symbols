#ifndef NEURO_PROLOG_H
#define NEURO_PROLOG_H

#include <stdint.h>
#include "graph.h"

/* Neuro-symbolic Prolog core over the knowledge graph.
   Terms are either constant symbol IDs or unbound variables (slots
   0..NP_MAX_VARS-1). The engine unifies queries against exact facts
   and extends paths through identity composition (Leibniz), the only
   sound composition for undecorated triples: A ES B, B R C => A R C
   and A R B, B ES C => A R C. Each inferred hop decays confidence
   by NP_HOP_DECAY. Fuzzy predicate bridging scales confidence by the
   embedding similarity gamma (metric-agnostic: 32D float today,
   swappable to 256-bit Hamming). Deterministic: no random, fixed
   table iteration order. */

#define NP_MAX_VARS      16
#define NP_HOP_DECAY     0.90f
#define NP_MIN_CONF_DEF  0.25f
#define NP_MAX_DEPTH_DEF 8
#define NP_MAX_SOL_DEF   32
#define NP_FUZZY_GATE    0.70f
#define NP_IDENTITY_PRED "ES"

typedef enum
{
    NP_TERM_CONSTANT = 0,
    NP_TERM_VARIABLE = 1
} NP_TERM_TYPE;

typedef struct
{
    uint8_t  type;
    uint32_t id; /* SYMBOL_ID if constant, variable slot if variable */
} NP_TERM;

static inline NP_TERM NPConst(SYMBOL_ID id)
{
    NP_TERM t; t.type = NP_TERM_CONSTANT; t.id = id; return t;
}

static inline NP_TERM NPVar(uint32_t slot)
{
    NP_TERM t; t.type = NP_TERM_VARIABLE; t.id = slot; return t;
}

typedef struct
{
    NP_TERM    subject;
    SYMBOL_ID  predicate; /* SYMBOL_INVALID (0) = any relation */
    NP_TERM    object;
} NP_QUERY;

/* Variable binding frame: slot -> value. Plain value copy is a cheap
   checkpoint (80 bytes); rollback is a single struct assignment. */
typedef struct
{
    SYMBOL_ID values[NP_MAX_VARS];
    uint8_t   bound[NP_MAX_VARS];
} NP_FRAME;

typedef struct
{
    float    min_conf;
    uint32_t max_depth;
    uint32_t max_solutions;
    int      allow_fuzzy;
    float    fuzzy_gate;
} NP_OPTIONS;

typedef struct
{
    /* Resolved triple (subject, predicate used, object) */
    SYMBOL_ID subject;
    SYMBOL_ID predicate;
    SYMBOL_ID object;
    float     confidence;
    uint8_t   depth;    /* inferred hops (0 = exact fact) */
    uint8_t   fuzzy;    /* path used a semantically-bridged predicate */
} NP_SOLUTION;

void NPDefaultOptions(NP_OPTIONS *opt);

/* Unify one term against a concrete value under a frame.
   Variable + unbound  -> bind, success.
   Variable + bound    -> strict identity.
   Constant            -> strict identity. O(1). */
int NPUnifyTerm(const NP_TERM *term, SYMBOL_ID value, NP_FRAME *frame);

/* Frame helper: resolve a term to its value (SYMBOL_INVALID if an
   unbound variable or out-of-range slot). */
SYMBOL_ID NPResolve(const NP_TERM *term, const NP_FRAME *frame);

/* Enumerate solutions of query against the graph. Stops after
   max_solutions. Returns the number of solutions (0 = honest unknown). */
uint32_t NPProve(const GRAPH *graph,
                 const NP_QUERY *query,
                 const NP_OPTIONS *opt,
                 NP_SOLUTION *out,
                 uint32_t out_max);

#endif
