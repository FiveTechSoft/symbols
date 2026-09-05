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

/* Quita UNA capa de sufijo (la mas larga aplicable).
   Devuelve 1 si quito algo, 0 si la palabra ya es raiz. */
int StemStep(const char *word, char *out, uint32_t out_size);

/* Busqueda con fallback morfologico: exacta primero, luego hasta
   4 reducciones progresivas (COMEN -> COME -> COM). Solo para
   rutas de CONSULTA (dialogo/QA); nunca en ingesta, donde cada
   forma debe conservar su propio simbolo. */
SYMBOL_ID StemFindSymbol(const SYMBOL_TABLE *table, const char *word);

#endif
