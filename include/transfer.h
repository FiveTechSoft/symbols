#ifndef TRANSFER_H
#define TRANSFER_H

#include <stdint.h>
#include <stddef.h>
#include "schema.h"
#include "metaschema.h"

/* ============================================================
   Transfer: derivation of novel sentences in a target domain,
   driven by schema (order/connective) + metaschema (structural
   properties) + presented vocabulary. The learned structure
   must survive the disappearance of the exemplars that taught
   it; the vocabulary gate keeps honest UNKNOWN otherwise.

   Authorization path for (S, conn, O):
     1. schema order admits the direction (two-mode check), OR
        a META property licenses the reversed direction
        (SYMMETRIC flips a dependent_first/dependent-role denial
         only when roles are declared: the swap is between tokens
         holding opposite roles, so the structural constraint is
         preserved under negation of directionality).
     2. both tokens presented into the KB vocabulary.
   Anything else -> 0 (UNKNOWN, never a hypothesis).
   ============================================================ */

/* Derive "S conn O." for a family. Returns 1 and fills out. */
int TransferDerive(const SCHEMA_KB *kb, const META_KB *mk,
                   const char *family, const char *subject,
                   const char *object, char *out, size_t out_size);

/* Meta-licensed derivation used when the plain schema order
   check fails but a structural property (e.g. SYMMETRIC with
   declared opposite roles) licenses the swap. Returns 1 + fills
   out, 0 otherwise. */
int TransferDeriveSwapped(const SCHEMA_KB *kb, const META_KB *mk,
                          const char *family, const char *subject,
                          const char *object, char *out, size_t out_size);

/* TRANSITIVE meta-licensed derivation: family F observed as
   links (S,M) AND (M,O) (re-presented pair evidence) derives
   the conclusion (S,O). Fail-closed: no property, no links, or
   already-direct pair -> 0. */
int TransferDeriveChain(const SCHEMA_KB *kb, const META_KB *mk,
                        const char *family, const char *subject,
                        const char *object, char *out, size_t out_size);

/* Same authorization logic as TransferDeriveChain but fills
    middle with the proving intermediate token M (the pair
    (S,M) + (M,O) that licensed the conclusion). Useful for
    explanations; returns 0 without touching middle on refusal. */
int TransferExplainChain(const SCHEMA_KB *kb, const META_KB *mk,
                         const char *family, const char *subject,
                         const char *object, char *out, size_t out_size,
                         char *middle, size_t middle_size);

/* ---- heterogeneous composition (Phase 3) ---- */

/* Compose two licensed families R1 o R2 => R3: given an observed
    pair (A,B) in family r1 and an observed pair (B,C) in family
    r2, with a meta-licensed rule rule(r1,r2 => r3), derives
    "A <r3> C". Fail-closed: no rule, no pair evidence for the
    premise links, direct evidence (r3,A,C) already present, or
    missing vocabulary -> 0 (UNKNOWN, never a hypothesis).
    NOTE: when the conclusion is already direct evidence the
    plain path owns it; this derives only NOVEL conclusions. */
int TransferCompose(const SCHEMA_KB *kb, const META_KB *mk,
                    const char *r1, const char *r2, const char *a,
                    const char *b, const char *c, char *out,
                    size_t out_size);

/* Justification of a composition CONCLUSION that is already
    observed fact (pair evidence in r3): the rule must be
    licensed and both premise links (A,B) in r1 and (B,C) in r2
    observed; B is given by the caller (the parsed bridging
    entity). Unlike TransferCompose this does NOT veto direct
    evidence (it explains known facts). Returns 1 + fills out,
    0 otherwise. */
int TransferExplainCompose(const SCHEMA_KB *kb, const META_KB *mk,
                           const char *r1, const char *r2,
                           const char *a, const char *b, const char *c,
                           char *out, size_t out_size);

#endif