/* output_contract: honor output-shape constraints a request declares.

   Some requests are not tasks but transforms: the client states the
   shape of the answer ("a single line", "at most 50 characters",
   "5 words") and supplies the material to transform. This module reads
   those constraints from free text, composes a reply from the material
   itself, and verifies the reply against every constraint it read.

   Honest scope: the reader knows a small unit vocabulary (characters,
   words, lines; English and Spanish) and a few lower-bound markers.
   It reads format only; it never matches specific prompts or answers. */
#ifndef OUTPUT_CONTRACT_H
#define OUTPUT_CONTRACT_H

#include <stddef.h>

typedef struct
{
    int max_chars; /* 0 = unbounded; counted in UTF-8 code points */
    int max_words; /* 0 = unbounded */
    int max_lines; /* 0 = unbounded */
} OUTPUT_CONTRACT;

/* Parse upper bounds from text. Returns 1 if at least one bound found. */
int OutputContractParse(const char *text, OUTPUT_CONTRACT *c);

/* Compose a reply from subject that satisfies c: first non-empty line,
   whitespace collapsed, surrounding quotes/trailing periods removed,
   first letter upper-cased (ASCII), then cut at word boundaries until
   every bound holds. Returns 1 if a non-empty reply was produced. */
int OutputContractCompose(const char *subject, const OUTPUT_CONTRACT *c,
                          char *out, size_t size);

/* 1 if text satisfies every bound in c. */
int OutputContractCheck(const char *text, const OUTPUT_CONTRACT *c);

#endif
