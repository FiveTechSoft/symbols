#ifndef STEM_H
#define STEM_H

#include <stdint.h>
#include "symbol.h"

/* ============================================================
   Stemmer espanol por reglas (solo minusculas/ASCII mayusculas,
   tokens ya normalizados a MAYUSCULAS sin diacriticos).

   Alcance deliberado: plurales y conjugacion verbal regular.
   NO toca genero (-A/-O) ni diptongos (TIENE/TENGO, FUE/ERA):
   esos casos pertenecen a sinonimos/embeddings, no al stemmer.
   ============================================================ */

/* Reduce una palabra a su raiz aplicando todas las reglas en
   cascada. Siempre NUL-termina; nunca devuelve cadena vacia. */
void StemWord(const char *word, char *out, uint32_t out_size);

/* Strips ONE suffix layer (longest applicable).
   Returns 1 if anything was stripped, 0 if the word is already a root. */
int StemStep(const char *word, char *out, uint32_t out_size);

/* Lookup with morphological fallback: exact first, then up to
   4 progressive reductions (COMEN -> COME -> COM). For QUERY paths
   (dialog/QA) only; never at ingest, where each form must keep
   its own symbol. */
SYMBOL_ID StemFindSymbol(const SYMBOL_TABLE *table, const char *word);

#endif
