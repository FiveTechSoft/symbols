#ifndef METASCHEMA_H
#define METASCHEMA_H

#include <stdint.h>
#include <stddef.h>
#include "schema.h"

/* ============================================================
   MetaSchema: structural properties OF schemas, discovered from
   evidence. Contains NO word knowledge: predicates are matched
   structurally (same family + swapped arguments), never by name.

   Discovery input = observations of realized sentences:
     R(A,B) observed AND R(B,A) observed  ->  R symmetric
   Single-direction observation proves nothing (fail-closed):
     R(A,B) alone                         ->  no property
     R(A,B) + S(B,A) (different family)   ->  no property
     R(A,B) + R(B,C) (unrelated pair)     ->  no property

   Heterogeneous composition (Phase 3): rule(R1,R2 => R3) with
   R1 != R2. Premise instances are observed pairs R1(A,B) and
   R2(B,C) with A != C; an instance CONFIRMS when R3(A,C) is
   observed too. Licensing gate (census-faithful, measured in
   tools/bible_compose_census.py): confirm rate 1.0 AND support
   >= 2. One counter-instance or thin support -> no rule (the
   KJV corpus licenses ZERO rules: best candidate 32/74 = 0.432).

   The absence of evidence produces UNKNOWN, not a hypothesis.
   ============================================================ */

typedef enum
{
    META_PROP_NONE = 0,
    META_PROP_SYMMETRIC = 1,
    META_PROP_TRANSITIVE = 2
} META_PROPERTY;

#define META_MAX 16
#define META_OBS_MAX 1024
#define META_RULE_MAX 32

typedef struct
{
    char family[SCHEMA_TOKEN_MAX];
    char subject[SCHEMA_TOKEN_MAX];
    char object[SCHEMA_TOKEN_MAX];
    char obs_id[SCHEMA_TOKEN_MAX];
} META_OBS;

/* composition rule R1 o R2 => R3 (R1 != R2), discovered from
   observations; support = confirming premise instances,
   confirm/support = 1.0 by the licensing gate */
typedef struct
{
    char     r1[SCHEMA_TOKEN_MAX];
    char     r2[SCHEMA_TOKEN_MAX];
    char     r3[SCHEMA_TOKEN_MAX];
    uint32_t support;
} META_RULE;

typedef struct
{
    char          family[SCHEMA_TOKEN_MAX];
    META_PROPERTY property;
    /* provenance: the observation pairs that taught it */
    char          prov[SCHEMA_PROV_MAX][SCHEMA_TOKEN_MAX];
    uint32_t      num_prov;
} META_SCHEMA;

typedef struct
{
    META_SCHEMA metas[META_MAX];
    uint32_t     num_metas;
    /* observation store: realized sentences fed back as evidence
       (family, subject, object, direction as given) */
    META_OBS     obs[META_OBS_MAX];
    uint32_t     num_obs;
    /* heterogeneous composition rules (R1,R2 => R3), licensed by
       the rate-1.0 / support>=2 gate over the same obs store */
    META_RULE    rules[META_RULE_MAX];
    uint32_t     num_rules;
} META_KB;

/* ---- lifecycle ---- */
void MetaKBInit(META_KB *mk);

/* ---- observation ----
   Feed a realized fact (family, subj, obj) with an observation id.
   Pure evidence storage; discovery is explicit (Discover below). */
int MetaObserve(META_KB *mk, const char *family, const char *subject,
                const char *object, const char *obs_id);

/* ---- discovery ----
   Scan observations: family F with BOTH (A,B) and (B,A) observed,
   where A != B, grants F the SYMMETRIC property. Family F with
   (A,B) AND (B,C) observed, A != C, grants TRANSITIVE. Anything
   else grants nothing. Idempotent: re-discovery only appends prov.
   Returns number of NEW properties discovered. */
uint32_t MetaDiscover(META_KB *mk);

/* ---- query ----
   1 if family has the property. 0 otherwise (UNKNOWN semantics:
   caller prints UNKNOWN, never invents). */
int MetaHasProperty(const META_KB *mk, const char *family,
                    META_PROPERTY prop);

/* ---- composition discovery (Phase 3) ----
   Scan the observation store for heterogeneous rules
   rule(R1,R2 => R3), R1 != R2, R1/R2/R3 distinct families that
   hold observations. Premise instance = obs pair (R1(A,B),
   R2(B,C)) with A != C (B free; degenerate self-links excluded).
   The rule is licensed ONLY when every premise instance confirms
   (R3(A,C) observed) and there are >= 2 instances. Idempotent.
   Returns number of NEW rules. */
uint32_t MetaRuleDiscover(META_KB *mk);

/* Rule lookup: fills *rule for the exact (r1, r2) premise pair.
   Returns 1 when licensed, 0 otherwise (fail-closed). */
int MetaFindRule(const META_KB *mk, const char *r1, const char *r2,
                 META_RULE *rule);

/* Number of licensed composition rules. */
uint32_t MetaRuleCount(const META_KB *mk);

/* ---- persistence: explicit meta facts only ----
   meta(<family>,<property>). metaprov(<family>,[ids]).
   compose(<r1>,<r2>,<r3>). rulesaveprov(<r1>,<r2>,<r3>,<support>).
   Observations are working state and are NOT saved. */
uint32_t MetaKBSave(const META_KB *mk, const char *filepath);
uint32_t MetaKBLoad(META_KB *mk, const char *filepath);

uint32_t MetaCount(const META_KB *mk);

#endif