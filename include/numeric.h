#ifndef NUMERIC_H
#define NUMERIC_H

#include <stddef.h>
#include <stdint.h>
#include "symbol.h"

/* M3a numeric sidecar: typed measures hanging off symbols, never new
   core relation types. A symbol naming a measure ("505.990_KM²",
   "1978") carries (value, unit) here; triples keep pointing at plain
   symbols. Owner-managed table, mirrored on the embeddings pattern:
   whoever creates it frees it (ModelCreate/ModelDestroy do both). */

#define NUMERIC_UNIT_MAX 16

typedef struct
{
    SYMBOL_ID id;
    double    value;
    char      unit[NUMERIC_UNIT_MAX];
} NUMERIC_ENTRY;

typedef struct
{
    NUMERIC_ENTRY *items;
    uint32_t       count;
    uint32_t       capacity;
} NUMERIC_TABLE;

NUMERIC_TABLE *NumericCreate(uint32_t capacity);
void           NumericDestroy(NUMERIC_TABLE *t);

/* Upsert by symbol id. Returns 1 on success, 0 on NULL/overflow. */
int NumericSet(NUMERIC_TABLE *t, SYMBOL_ID id,
               double value, const char *unit);

/* Returns 1 and fills value/unit when the symbol carries a measure. */
int NumericGet(const NUMERIC_TABLE *t, SYMBOL_ID id,
               double *value, char *unit, size_t unit_size);

/* Parse a measure in Spanish formats: "505.990 KM²" -> 505990,
   "8,47" -> 8.47, "47.5" -> 47.5, "-3" -> -3, "1978" -> 1978.
   Separator rule (documented heuristic): with both '.' and ',',
   dots are thousands and comma is decimal; with comma only it is
   decimal; with dots only, a single 3-digit tail means thousands
   ("505.990"), otherwise decimal ("47.5"). '/' rejects (dates and
   ratios are out of scope). Unit is the trailing non-numeric run
   (letters, UTF-8 bytes, '%'); empty when absent. Returns 1 on a
   well-formed measure with at least one digit, 0 otherwise. */
int NumericParseMeasure(const char *text, double *value,
                        char *unit, size_t unit_size);

#endif
