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

#endif