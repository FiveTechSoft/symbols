#ifndef CONDITION_PARSE_H
#define CONDITION_PARSE_H
#include "condition_match.h"
#define CP_QUESTION_MAX 512u
#define CP_LINE_MAX 1024u
#define CP_SOURCE_MAX 65536u
typedef enum { CP_REJECTED=0, CP_PARSED=1 } CP_STATUS;
/* Controlled ASCII grammar only. No CHAT integration or natural-language
   guarantee. Caller supplies valid memory, disjoint inputs/output.
   Output is cleared on rejection. Explicit length excludes a final NUL. */
CP_STATUS ConditionParseQuestion(const unsigned char *bytes,size_t size,
                                CM_REQUIREMENT *out);
/* Caller-owned output capacity. All-or-nothing publication. Claims reference
   source index 0 and original line bytes excluding LF/CRLF. Origins remain
   extractor_asserted: syntactic extraction is not corpus-truth validation. */
CP_STATUS ConditionExtractClaims(const unsigned char *bytes,size_t size,
                                CM_CLAIM *out,size_t capacity,size_t *count);
#endif
