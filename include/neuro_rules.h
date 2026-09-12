#ifndef NEURO_RULES_H
#define NEURO_RULES_H

#include <stdint.h>
#include "neuro_prolog.h"

/* P4b Horn layer over the P4a core (neuro_prolog.h, untouched).
   Declared head/body rules, Prolog cut, negation-as-failure, and the
   denial veto (the penguin line: an explicitly denied triple defeats
   rule-derived solutions, while fact-level contradiction stays with
   the conflict policies).

   Documented simplifications (honest, not Prolog-complete):
   - Shallow cut: a rule with cut yields at most its first solution.
     No cross-rule choice-point surgery.
   - Rule heads carry a concrete predicate (queries may use ANY).
   - NAF never binds: success iff the subgoal has zero solutions.
   - max_depth caps RULE nesting; P4a-internal chains keep their own
     cap via a reduced sub-option.
   - Confidence: one NP_HOP_DECAY per rule step over the minimum of
     the body confidences; min_conf gates at the end.

   Semantics: solutions = P4a fact-level (exact, identity, fuzzy) plus
   rule derivations, deduped by triple keeping max confidence.
   Deterministic: declaration order, fixed table order, no random.
   Rule variables are renamed apart per expansion (fresh slots above
   the query's); queries should use low slots 0..7. Slot overflow
   past 15 fails that expansion gracefully.
   Single-threaded use (static rule table, like the parser caches). */

#define NPR_MAX_BODY  8
#define NPR_MAX_RULES 64

typedef struct
{
    NP_TERM   subject;
    SYMBOL_ID predicate; /* concrete relation (no ANY in bodies) */
    NP_TERM   object;
    int       negated;   /* NAF goal: succeeds iff unprovable */
} NP_GOAL;

typedef struct
{
    NP_QUERY  head;                 /* concrete predicate */
    NP_GOAL   body[NPR_MAX_BODY];
    uint32_t  nbody;
    int       cut;                  /* first-solution commit */
} NP_RULE;

void NPRulesClear(void);

/* Returns the rule index, or -1 when full (64) or malformed
   (nbody 0 or > NPR_MAX_BODY, non-concrete head/body predicates). */
int NPRuleDeclare(const NP_RULE *rule);

/* Enumerate solutions of query against facts and declared rules.
   Stops after max_solutions. Returns count (0 = honest unknown). */
uint32_t NPProveRules(const GRAPH *graph,
                      const NP_QUERY *query,
                      const NP_OPTIONS *opt,
                      NP_SOLUTION *out,
                      uint32_t out_max);

#endif
