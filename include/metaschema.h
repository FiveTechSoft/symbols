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

typedef struct
{
    char family[SCHEMA_TOKEN_MAX];
    char subject[SCHEMA_TOKEN_MAX];
    char object[SCHEMA_TOKEN_MAX];
    char obs_id[SCHEMA_TOKEN_MAX];
} META_OBS;

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

/* ---- persistence: explicit meta facts only ----
   meta(<family>,<property>). metaprov(<family>,[ids]).
   Observations are working state and are NOT saved. */
uint32_t MetaKBSave(const META_KB *mk, const char *filepath);
uint32_t MetaKBLoad(META_KB *mk, const char *filepath);

uint32_t MetaCount(const META_KB *mk);

#endif