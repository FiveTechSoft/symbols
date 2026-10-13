#ifndef CONDITION_MATCH_H
#define CONDITION_MATCH_H
#include <stddef.h>
#include <stdint.h>
/* Exact matching of caller/extractor assertions, not source entailment. */
#define CM_VERSION 1u
#define CM_ID_CAP 64u
#define CM_QUAL_MAX 8u
#define CM_CLAIM_MAX 64u
#define CM_SOURCE_MAX 8u
#define CM_SOURCE_BYTES_MAX 1048576u
#define CM_TOTAL_BYTES_MAX 4194304u
#define CM_POSITIVE 1u
#define CM_NEGATIVE 2u
#define CM_CALLER_ASSERTED 1u
#define CM_EXTRACTOR_ASSERTED 2u

typedef struct { char key[CM_ID_CAP], value[CM_ID_CAP]; } CM_QUALIFIER;
typedef struct {
    uint32_t version, polarity, nqual;
    char subject[CM_ID_CAP], relation[CM_ID_CAP];
    CM_QUALIFIER qualifiers[CM_QUAL_MAX];
} CM_REQUIREMENT;
typedef struct { const unsigned char *bytes; size_t size; } CM_SOURCE;
typedef struct {
    CM_REQUIREMENT key;
    char object[CM_ID_CAP]; /* empty only for negative claims */
    uint32_t origin, source;
    unsigned char source_sha256[32];
    uint64_t first, end;
} CM_CLAIM;
typedef enum { CM_INVALID=0, CM_MATCH, CM_REFUTED, CM_CONFLICT, CM_UNDETERMINED } CM_STATUS;
typedef struct {
    CM_STATUS status;
    uint32_t claim_index; /* UINT32_MAX unless MATCH/REFUTED */
    char object[CM_ID_CAP];
} CM_RESULT;
/* No allocation, no persistent state. All input claims validated first.
   Source hash/span integrity does not validate the supplied annotation.
   Inputs/output must be disjoint. The caller owns valid memory for arrays.
   A well-formed but wrong assertion can MATCH. Refused result is cleared.
   Representative evidence is chosen by content hash/span/object ordering,
   never by first matching input; identical claims may have any identical
   evidence index. Byte offsets are arbitrary caller assertions, not sentence
   boundary proofs. UTF-8 identifiers are exact, with no normalization. */
CM_STATUS ConditionMatch(const CM_REQUIREMENT *requirement,
                         const CM_CLAIM *claims, size_t nclaims,
                         const CM_SOURCE *sources, size_t nsources,
                         CM_RESULT *out);
#endif
