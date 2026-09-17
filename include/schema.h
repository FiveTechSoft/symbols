#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
   Relational schema engine (C port of gen_meta.pl semantics).

   A schema is discovered from exemplar triples that carry an
   observed surface connective. The template registry is a
   DERIVATION RULE keyed by the observed connective (never
   asserted), exactly like gm_template/3:

     cong            -> sym
     proportional    -> ord(dependent_first)
     after           -> ord(chain)
     acting on       -> ord(operator_first)

   Order check has two modes, mirroring the frozen Prolog
   reading:
     MODE 1 (evidence): a presented pair observed in the target
       domain admits only the observed direction.
     MODE 2 (constraint): no pair evidence; the schema's order
       term is the constraint. sym admits any direction;
       dependent_first/operator_first check roles; chain is NOT
       admitted cold (no direction evidence).
   ============================================================ */

#define SCHEMA_MAX 16
#define SCHEMA_CONN_MAX 4
#define SCHEMA_TOKEN_MAX 32
#define SCHEMA_PROV_MAX 8

typedef enum
{
    SCHEMA_ORDER_SYM = 0,      /* any direction */
    SCHEMA_ORDER_DEP_FIRST,    /* dependent as subject */
    SCHEMA_ORDER_OPER_FIRST,   /* operator as subject */
    SCHEMA_ORDER_CHAIN         /* no direction evidence cold */
} SCHEMA_ORDER;

typedef struct
{
    char          family[SCHEMA_TOKEN_MAX];
    SCHEMA_ORDER  order;
    char          connectives[SCHEMA_CONN_MAX][SCHEMA_TOKEN_MAX];
    uint32_t      num_connectives;
    /* provenance: exemplar ids that taught this schema */
    char          prov[SCHEMA_PROV_MAX][SCHEMA_TOKEN_MAX];
    uint32_t      num_prov;
} RELATIONAL_SCHEMA;

typedef struct
{
    char family[SCHEMA_TOKEN_MAX];
    char subject[SCHEMA_TOKEN_MAX];
    char object[SCHEMA_TOKEN_MAX];
} PAIR_EVID;

typedef struct
{
    char name[SCHEMA_TOKEN_MAX];
    /* role of the token for order checks */
    int  is_dependent;   /* dependent/independent axis */
    int  is_independent;
    int  is_operator;    /* operator/patient axis */
    int  is_patient;
} SCHEMA_ROLE;

#define SCHEMA_VOCAB_MAX 256
#define SCHEMA_ROLE_MAX 64
#define SCHEMA_PAIR_MAX 64

typedef struct
{
    RELATIONAL_SCHEMA schemas[SCHEMA_MAX];
    uint32_t          num_schemas;
    /* working memory derived from presentation (domain B) */
    PAIR_EVID         pairs[SCHEMA_PAIR_MAX];
    uint32_t          num_pairs;
    /* world vocabulary presented (gm_vocab analog) */
    char              vocab[SCHEMA_VOCAB_MAX][SCHEMA_TOKEN_MAX];
    uint32_t          num_vocab;
    /* role lexicon: derivation-rule entries (gm_brole analog) */
    SCHEMA_ROLE       roles[SCHEMA_ROLE_MAX];
    uint32_t          num_roles;
} SCHEMA_KB;

/* ---- lifecycle ---- */
void SchemaKBInit(SCHEMA_KB *kb);

/* ---- discovery ----
   Register an exemplar: id, domain-A triple tokens, surface
   sentence (tokens). The connective is looked up in the template
   registry; unknown connective -> no discovery (returns 0).
   First observation asserts the schema + prov[id]; repeat
   observation appends the id to prov (idempotent). */
int SchemaObserveExemplar(SCHEMA_KB *kb, const char *exemplar_id,
                          const char *const *triple, uint32_t num_triple,
                          const char *const *sentence, uint32_t num_sentence);

/* ---- presentation (domain B) ----
   Present a pair in the target domain: adds vocabulary + per-pair
   order evidence (gm_bpair analog). */
int SchemaPresentPair(SCHEMA_KB *kb, const char *family,
                      const char *subject, const char *object);

/* ---- role lexicon (derivation rule, never asserted) ---- */
void SchemaDeclareRole(SCHEMA_KB *kb, const char *token,
                       int dependent, int independent,
                       int operator_, int patient);

/* ---- query ----
   Returns 1 and fills out[SCHEMA_TOKEN_MAX] if the schema family
   admits subject-connective-object (two-mode order check).
   Otherwise returns 0; caller prints UNKNOWN. */
int SchemaRealize(const SCHEMA_KB *kb, const char *family,
                  const char *subject, const char *object,
                  char *out, size_t out_size);

/* Cold reconstruction analog: working facts are derived from
   schema facts alone (in C this is just a validation pass). */
uint32_t SchemaCount(const SCHEMA_KB *kb);
const RELATIONAL_SCHEMA *SchemaGet(const SCHEMA_KB *kb, uint32_t i);

/* Lookup by family name (NULL if absent). */
const RELATIONAL_SCHEMA *SchemaFindFamily(const SCHEMA_KB *kb,
                                          const char *family);

/* Vocabulary gate: 1 if the token was presented into this KB. */
int SchemaVocabKnown(const SCHEMA_KB *kb, const char *token);

/* Sentence builder shared with the meta layer: "Subj conn Obj."
   with capitalized first letter. No order/vocab checks here. */
int SchemaBuildSentence(const SCHEMA_KB *kb, const char *family,
                        const char *subject, const char *object,
                        char *out, size_t out_size);

/* ---- persistence (explicit schema facts only, like schema_kb.pl) ----
   Format: text, one line per schema:
   schema(<family>,<order>,[conns]). prov(<family>,[ids]).
   Returns number of schemas written / read. */
uint32_t SchemaKBSave(const SCHEMA_KB *kb, const char *filepath);
uint32_t SchemaKBLoad(SCHEMA_KB *kb, const char *filepath);

#endif