/* =========================================================================
   commonsense.h: Large-Scale Commonsense & World Knowledge Ingestion
   Pillar 3: Native C11 Streaming Ingestion & Intuitive World Reasoning
   - Streaming parser for ConceptNet 5.8 & WordNet ontologies
   - Strict 32 bytes per relation memory footprint (~320 MB for 10M triples)
   - Canonicalization of commonsense relations (HARDCODING=0)
   - Transitive physical, spatial, and functional inference
   - Sub-100ns random relation retrieval latency
   - Zero hallucination, fail-closed verification
   ========================================================================= */

#ifndef COMMONSENSE_H
#define COMMONSENSE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "graph.h"
#include "relation.h"
#include "i18n.h"


#ifdef __cplusplus
extern "C" {
#endif

#define CS_STR_MAX        64
#define CS_PATH_MAX_HOPS  8
#define CS_BUFFER_CHUNK   65536

/* Canonical Commonsense Relations */
typedef enum
{
    CS_REL_UNKNOWN = 0,
    CS_REL_IS_A,             /* Taxonomy: Dog IsA Canine */
    CS_REL_PART_OF,          /* Spatial / Meronym: Kitchen PartOf House */
    CS_REL_AT_LOCATION,      /* Spatial: Refrigerator AtLocation Kitchen */
    CS_REL_CAPABLE_OF,       /* Functional: Bird CapableOf Fly */
    CS_REL_USED_FOR,         /* Functional: Knife UsedFor Cut */
    CS_REL_MADE_OF,          /* Material: Glass MadeOf BrittleMaterial */
    CS_REL_HAS_PROPERTY,     /* Qualitative: Fire HasProperty Hot */
    CS_REL_CAUSES,           /* Causal: Rain Causes WetRoad */
    CS_REL_HAS_RESULT,       /* Consequence: Fall HasResult Shatter */
    CS_REL_HAS_PREREQUISITE, /* Workflow: Eat HasPrerequisite Cook */
    CS_REL_DESIRES,          /* Intentional: Human Desires Food */
    CS_REL_MOTIVATED_BY,     /* Teleological: Drink MotivatedBy Thirst */
    CS_REL_SYNONYM,          /* Lexical: Big Synonym Large */
    CS_REL_ANTONYM,          /* Lexical: Hot Antonym Cold */
    CS_REL_DEFINED_AS,       /* Ontological definition */
    CS_REL_SYMBOL_OF,        /* Cultural symbolism */
    CS_REL_COUNT
} CS_REL_TYPE;

/* Declarative Relation Canonicalization Map */
typedef struct
{
    const char *raw_rel;
    CS_REL_TYPE rel_type;
    const char *canonical_name;
} CS_REL_MAP_ENTRY;

/* Configuration for Commonsense Ingestion */
typedef struct
{
    char  filter_lang[8];     /* e.g. "en", "es", "fr", or "" for any */
    float min_weight;         /* Minimum confidence threshold (e.g. 1.0) */
    int   canonicalize_names; /* Replace raw /r/Rel with canonical CAPABLE_OF */
    int   track_provenance;   /* Record dataset source symbol */
} CS_CONFIG;

/* Ingestion Statistics & Telemetry */
typedef struct
{
    uint64_t lines_read;
    uint64_t triples_ingested;
    uint64_t symbols_created;
    uint64_t filtered_weight;
    uint64_t filtered_lang;
    double   elapsed_sec;
    double   triples_per_sec;
    size_t   ram_bytes;
} CS_STATS;

/* Parsed Commonsense Triple */
typedef struct
{
    char        subject[CS_STR_MAX];
    char        relation[CS_STR_MAX];
    char        object[CS_STR_MAX];
    CS_REL_TYPE rel_type;
    float       weight;
    char        lang[8];
} CS_TRIPLE;

/* Multi-hop Grounded Commonsense Deduction Path */
typedef struct
{
    char     hops_subject[CS_PATH_MAX_HOPS][CS_STR_MAX];
    char     hops_relation[CS_PATH_MAX_HOPS][CS_STR_MAX];
    char     hops_object[CS_PATH_MAX_HOPS][CS_STR_MAX];
    uint32_t hop_count;
    float    cumulative_confidence;
    int      verified;
} CS_INFERENCE_PATH;

/* =========================================================================
   Part 1: Configuration & Parsing
   ========================================================================= */

/* Default configuration: English, weight >= 1.0, canonicalize relations */
CS_CONFIG CommonsenseConfigDefault(void);

/* Canonicalize raw relation URI (/r/CapableOf -> CAPABLE_OF) */
CS_REL_TYPE CommonsenseCanonicalizeRelation(const char *raw_rel,
                                            char *out_canonical,
                                            size_t out_size);

/* Parse ConceptNet entity URI (/c/en/kitchen -> lang="en", concept="kitchen") */
int CommonsenseParseConceptNetURI(const char *uri,
                                  char *out_lang,
                                  size_t lang_size,
                                  char *out_concept,
                                  size_t concept_size);

/* Parse single line of ConceptNet 5.8 assertion (full 5-column or 3/4-col TSV) */
int CommonsenseParseLine(const char *line,
                         const CS_CONFIG *cfg,
                         CS_TRIPLE *triple);

/* =========================================================================
   Part 2: High-Throughput Streaming Ingestion (M3.1, M3.2)
   ========================================================================= */

/* Streaming ingestion from file pointer */
int CommonsenseIngestStream(GRAPH *graph,
                            FILE *fp,
                            const CS_CONFIG *cfg,
                            CS_STATS *stats);

/* Streaming ingestion from file path */
int CommonsenseIngestFile(GRAPH *graph,
                          const char *filepath,
                          const CS_CONFIG *cfg,
                          CS_STATS *stats);

/* High-speed chunked ingestion from memory buffer */
int CommonsenseIngestBuffer(GRAPH *graph,
                            const char *buffer,
                            size_t size,
                            const CS_CONFIG *cfg,
                            CS_STATS *stats);

/* Benchmark high-scale ingestion on N synthetic triples (M3.2 verification) */
int CommonsenseBenchmarkScale(uint32_t count, CS_STATS *stats);

/* =========================================================================
   Part 3: Intuitive Reasoning & Deductive QA (M3.3)
   ========================================================================= */

/* Ingest built-in curated foundational seed of physical, spatial, and functional world facts */
int CommonsenseIngestSeed(GRAPH *graph, CS_STATS *stats);

/* Query spatial containment / transitive location:
   "Where is the milk?" -> Milk AtLocation Refrigerator, Refrigerator AtLocation Kitchen
   Returns 1 if a verified location path was found, 0 if UNKNOWN */
int CommonsenseQueryLocation(const GRAPH *graph,
                             const char *entity,
                             CS_INFERENCE_PATH *path,
                             char *out,
                             size_t out_size);

/* Query functional capability or affordance:
   "What can birds do?" -> Bird CapableOf Fly
   "What is a knife used for?" -> Knife UsedFor Cut
   Returns 1 if verified affordance found, 0 if UNKNOWN */
int CommonsenseQueryAffordance(const GRAPH *graph,
                              const char *entity,
                              const char *relation_name,
                              char *out,
                              size_t out_size);

/* Query physical consequence / causal chain:
   "What happens when glass is dropped on concrete?"
   Traces: Glass MadeOf BrittleMaterial + BrittleMaterial + DropOn + Concrete -> Shatter
   Returns 1 if verified physical consequence derived, 0 if UNKNOWN */
int CommonsenseQueryPhysicalConsequence(const GRAPH *graph,
                                        const char *subject,
                                        const char *action,
                                        const char *target,
                                        CS_INFERENCE_PATH *path,
                                        char *out,
                                        size_t out_size);

/* Query physical consequence with multilingual realization support (EN/ES/FR) */
int CommonsenseQueryPhysicalConsequenceLang(const GRAPH *graph,
                                            LANG_ID lang,
                                            const char *subject,
                                            const char *action,
                                            const char *target,
                                            CS_INFERENCE_PATH *path,
                                            char *out,
                                            size_t out_size);

struct PERSONA_FILTER_;

/* Query physical consequence with persona projection and multilingual support */
int CommonsenseQueryPhysicalConsequencePersona(const GRAPH *graph,
                                               const struct PERSONA_FILTER_ *filter,
                                               LANG_ID lang,
                                               const char *subject,
                                               const char *action,
                                               const char *target,
                                               CS_INFERENCE_PATH *path,
                                               char *out,
                                               size_t out_size);

/* =========================================================================
   Part 4: High-Performance Binary Serialization & mmap Ingestion (M3.4)
   ========================================================================= */

#define CS_BIN_MAGIC    0x53594D43  /* "SYMC" in ASCII little-endian */
#define CS_BIN_VERSION  1

typedef struct
{
    uint32_t magic;            /* CS_BIN_MAGIC */
    uint32_t version;          /* CS_BIN_VERSION */
    uint32_t symbol_count;     /* Total unique symbols */
    uint32_t relation_count;   /* Total relational triples */
    uint64_t string_table_len; /* Total bytes in names arena */
    uint64_t file_size;        /* Total binary file size */
    uint32_t flags;            /* Bit flags: 0x1 = aligned */
    uint32_t checksum;         /* Validation checksum */
} CS_BIN_HEADER;

/* Save commonsense graph to binary snapshot file */
int CommonsenseSaveBinary(const GRAPH *graph, const char *filepath);

/* Load commonsense graph from binary snapshot file via high-speed bulk read (< 5 ms) */
GRAPH *CommonsenseLoadBinary(const char *filepath);

/* Parse commonsense graph from pre-loaded or memory-mapped binary buffer */
GRAPH *CommonsenseParseBinaryBuffer(const uint8_t *buffer, size_t size);

/* Memory-mapped binary commonsense context */
typedef struct
{
    void   *os_handle;     /* Windows HANDLE or POSIX fd */
    void   *map_handle;    /* Windows FileMapping or NULL on POSIX */
    void   *map_view;      /* Base pointer of memory-mapped view */
    size_t  file_size;     /* Total mapped size */
    GRAPH  *graph;         /* Reconstructed graph */
} CS_MMAP_CONTEXT;

/* Load commonsense graph using virtual memory mapping (zero file copy, sub-millisecond mapping) */
GRAPH *CommonsenseLoadMmap(const char *filepath, CS_MMAP_CONTEXT *ctx);

/* Unmap and release memory mapping context */
void CommonsenseMmapClose(CS_MMAP_CONTEXT *ctx);


#ifdef __cplusplus
}
#endif

#endif /* COMMONSENSE_H */
