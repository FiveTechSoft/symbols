#ifndef CONDITION_QUERY_H
#define CONDITION_QUERY_H
#include "condition_parse.h"
#define CQ_VERSION 1u
typedef enum { CQ_INVALID=0, CQ_ASSERTION_MATCH, CQ_ASSERTION_REFUTED,
               CQ_CONFLICT, CQ_UNKNOWN } CQ_STATUS;
typedef struct {
    uint32_t version;
    CQ_STATUS status;
    char object[CM_ID_CAP];
    uint32_t origin;
    unsigned char source_sha256[32];
    uint64_t first, end;
} CQ_RESULT;
/* Opt-in controlled ASCII assertions, not entailment or source truth.
   One immutable source, mandatory expected byte-image hash. No CHAT fallback,
   allocation or persistent state. Inputs/output must be valid disjoint memory;
   explicit lengths exclude final NUL. Entire source validates before matching.
   MATCH/REFUTED alone carry extractor_asserted evidence. Other statuses clear
   object/evidence. REFUTED has empty object. Output version is always 1 when
   output is non-NULL, including INVALID. NULL output returns INVALID. */
CQ_STATUS ConditionQuery(const unsigned char *question, size_t question_size,
                         const unsigned char *source, size_t source_size,
                         const unsigned char expected_source_sha256[32],
                         CQ_RESULT *out);
#endif
