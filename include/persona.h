/* =========================================================================
   persona.h: Pragmatic Conditioning, Epistemic Perspectives & Persona Filters
   Pillar 4: Deterministic Rhetorical Projection over Reflexive Meta-Graph
   - Mathematical persona projection operator Pi_style : G -> G_biased
   - Zero prompt injection: structural masks, not stochastic prompt prefixes
   - Modulates rhetorical framing, connectives, and vocabulary density
   - Strict Factual Invariance Invariant: Facts(Pi_P(Q)) == Facts(Q)
   - Zero hallucination guarantee, fail-closed epistemic boundaries
   ========================================================================= */

#ifndef PERSONA_H
#define PERSONA_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "relation.h"
#include "meta_graph.h"
#include "ccg_realizer.h"
#include "i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERSONA_NAME_MAX 32
#define PERSONA_STR_MAX  128

/* Standard Epistemic Personas / Communicative Roles */
typedef enum
{
    PERSONA_NEUTRAL = 0,   /* Objective, minimal, direct factual assertion */
    PERSONA_ARCHITECT,     /* Senior Systems/Software Architect: invariants, contracts */
    PERSONA_AUDITOR,       /* Formal Auditor: verification, provenance, skeptical rigor */
    PERSONA_TUTOR,         /* Didactic Tutor: intuitive, step-by-step causal explanations */
    PERSONA_CONCISE,       /* Executive / Minimalist: telegraphic density, zero fluff */
    PERSONA_SOCRATIC,      /* Socratic Explorer: prompts inquiry, examines premises */
    PERSONA_PIRATE_QUANTUM,/* 18th-century pirate who just grasped quantum mechanics */
    PERSONA_COUNT
} PERSONA_ID;

/* Rhetorical Framing Style Features */
typedef struct
{
    float epistemic_threshold;  /* Minimum relation weight required (e.g. 1.0 vs 2.5) */
    int   verbosity_level;      /* 0 = concise, 1 = standard, 2 = detailed / didactic */
    int   require_provenance;   /* Always cite source if available */
    int   use_rhetorical_intro; /* Include role-specific framing introductory clause */
    int   prefer_causal_chain;  /* Expand transitive causal links */
} PERSONA_PROFILE;

/* Declarative Persona Lexicon (Introductions, Connectives, Abstentions) */
typedef struct
{
    const char *role_name;
    const char *intro[LANG_COUNT];
    const char *chain_connective[LANG_COUNT];
    const char *conclusion_connective[LANG_COUNT];
    const char *abstain_template[LANG_COUNT];
    const char *evidence_prefix[LANG_COUNT];
} PERSONA_LEXICON;

/* Persona Filter / Mask Struct */
typedef struct PERSONA_FILTER_
{
    PERSONA_ID      id;
    PERSONA_PROFILE profile;
    const PERSONA_LEXICON *lex;
} PERSONA_FILTER;

/* =========================================================================
   Part 1: Persona Filter Configuration & Activation
   ========================================================================= */

/* Initialize a persona filter by ID */
void PersonaFilterInit(PERSONA_FILTER *filter, PERSONA_ID id);

/* Retrieve persona filter by name ("architect", "auditor", "tutor", "concise") */
PERSONA_ID PersonaFindByName(const char *name);

const char *PersonaGetName(PERSONA_ID id);

/* Get declarative lexicon for a persona */
const PERSONA_LEXICON *PersonaGetLexicon(PERSONA_ID id);

/* =========================================================================
   Part 2: Projection Operator Pi_style over Graph & Meta-Graph
   ========================================================================= */

/* Computes relation preference bias under active persona mask.
   Returns a scalar weight modifier [-50..+50] without altering underlying fact truth */
int PersonaComputeRelationBias(const PERSONA_FILTER *filter,
                               const RELATION *rel,
                               const METAGRAPH *mg);

/* Checks whether a relation satisfies the persona's epistemic threshold */
int PersonaAcceptsRelation(const PERSONA_FILTER *filter, const RELATION *rel);

/* Realize natural language answer for a fact or chain under persona projection.
   Guarantees 100% semantic equivalence with underlying graph facts. */
uint32_t PersonaRealizeFact(const PERSONA_FILTER *filter,
                            LANG_ID lang,
                            const char *subject,
                            const char *relation,
                            const char *object,
                            const char *source,
                            char *out,
                            size_t out_size);

/* Realize multi-hop deduction chain under persona projection */
uint32_t PersonaRealizeChain(const PERSONA_FILTER *filter,
                             LANG_ID lang,
                             const char *start_node,
                             const char hops[][CCG_STR_MAX],
                             uint32_t nhops,
                             const char *conclusion_rel,
                             const char *end_node,
                             char *out,
                             size_t out_size);

/* Realize honest epistemic abstention under persona perspective */
uint32_t PersonaRealizeAbstain(const PERSONA_FILTER *filter,
                               LANG_ID lang,
                               const char *entity,
                               const char *relation,
                               char *out,
                               size_t out_size);

/* Realize physical causal consequence under persona perspective */
uint32_t PersonaRealizePhysicalConsequence(const PERSONA_FILTER *filter,
                                           LANG_ID lang,
                                           const char *subject,
                                           const char *action,
                                           const char *target,
                                           const char *material,
                                           const char *consequence,
                                           char *out,
                                           size_t out_size);

/* =========================================================================
   Part 3: Mathematical Non-Interference Verification
   ========================================================================= */

/* Proves that persona projection Pi_P cannot introduce facts not in G,
   and cannot alter deduction validity across all registered personas */
int PersonaVerifyNonInterference(const GRAPH *graph,
                                 const char *test_subject,
                                 const char *test_relation);

#ifdef __cplusplus
}
#endif

#endif /* PERSONA_H */
