#ifndef DICT_H
#define DICT_H

#include <stdint.h>
#include <stddef.h>

/* Cross-lingual translation table.
   HARDCODING = 0: translations loaded from external file (english-spanish.txt).
   Format: ALIAS = CANONICAL (alias is typically query language, canonical
   is corpus language). Linear scan over ~100 entries — fast enough. */

#define DICT_TOKEN_MAX 64
#define DICT_MAX 512

typedef struct
{
    char alias[DICT_TOKEN_MAX];
    char canonical[DICT_TOKEN_MAX];
} DICT_ENTRY;

typedef struct
{
    DICT_ENTRY entries[DICT_MAX];
    uint32_t   count;
} DICT;

/* Initialize / destroy */
void DictInit(DICT *dict);

/* Load from file. Format: one entry per line "alias = canonical".
   Lines starting with '#' and blank lines are skipped.
   Returns number of entries loaded. */
uint32_t DictLoad(DICT *dict, const char *filepath);

/* Translate entity to canonical form.
   Case-insensitive linear scan. Returns canonical string if found,
   NULL if no translation exists. */
const char *DictTranslate(const DICT *dict, const char *entity);

/* Inverse: canonical (corpus EN) -> first alias (typically ES). */
const char *DictReverse(const DICT *dict, const char *canonical);

/* Check if a string is likely non-English (contains accented chars,
   common Spanish suffixes, etc.). Used to decide when to try translation. */
int DictLooksForeign(const char *entity);

#endif
