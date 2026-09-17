#ifndef LEARN_H
#define LEARN_H

#include <stdint.h>
#include <stddef.h>
#include "schema.h"
#include "metaschema.h"

/* ============================================================
   Learn: turns raw observed sentences into schema + meta
   evidence. The ONLY layer that parses input text; everything
   above (meta discovery, transfer) stays structural.

   Grammar accepted (lowercase, whitespace tokens):
     <subj> <connective> <obj>            3-token exemplar
     <subj> <c1> <c2> <obj>               4-token exemplar
   Lines are also FED BACK as meta observations, which is what
   lets both-direction evidence accumulate for discovery.
   ============================================================ */

#define LEARN_MAX_LINE 1024

typedef struct
{
    SCHEMA_KB *kb;
    META_KB   *mk;
    /* ids assigned per learned observation: obs-<n> */
    uint32_t   next_obs;
    /* last learn outcome, for CLI reporting */
    int        last_was_exemplar;  /* 1 = taught a schema */
    char       last_family[SCHEMA_TOKEN_MAX];
} LEARNER;

void LearnerInit(LEARNER *lr, SCHEMA_KB *kb, META_KB *mk);

/* ---- consultable lexical tables (HARDCODING=0 contract) ----
   The ONLY lexical knowledge in the pipeline: a connective ->
   family mapping consulted at ingest, same status as the
   template registry in schema.c (a derivation table, never
   asserted). Callers (CLI, chat, tests) must use these instead
   of keeping private duplicates. LearnerIsConnective returns
   1 when the token is a known connective; LearnerConnFamily
   maps a connective to its family (NULL when unknown). */
int LearnerIsConnective(const char *tok);
const char *LearnerConnFamily(const char *conn);

/* Learn one input line. Returns 1 if it produced evidence
   (exemplar and/or observation), 0 if rejected (no known
   connective). Role lexicon entries are declared separately
   by the world (SchemaDeclareRole), never by this call. */
int LearnerLearnLine(LEARNER *lr, const char *line);

/* Run meta discovery over accumulated observations. Returns
   number of NEW properties. */
uint32_t LearnerDiscoverMeta(LEARNER *lr);

/* Present a pair into the target world (transfer vocabulary +
   pair evidence) once the family exists. 0 if family unknown. */
int LearnerPresentPair(LEARNER *lr, const char *family,
                       const char *subject, const char *object);

#endif