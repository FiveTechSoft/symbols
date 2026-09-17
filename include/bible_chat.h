#ifndef BIBLE_CHAT_H
#define BIBLE_CHAT_H

/* bible_chat: symbolic conversational engine over the frozen C
   layers (schema+meta+transfer). No tensors, no backprop. */

#define CHAT_TOKEN_MAX 32
#define CHAT_KW_MAX 16

#include "schema.h"
#include "metaschema.h"
#include "learn.h"

/* Relation keyword, DEDUCED from the corpus at ingest (never
   hardcoded): for each distinct TSV relation REL the stem is
   REL minus the "_DE" suffix (lowercase), the English stem is
   the surface connective it was learned with, and the family is
   where the pairs landed (lr.last_family). */
typedef struct
{
    char     es_stem[CHAT_TOKEN_MAX];
    char     en_stem[CHAT_TOKEN_MAX];
    char     family[SCHEMA_TOKEN_MAX];
    char     conn[SCHEMA_TOKEN_MAX];
} REL_KW;

typedef struct CHAT_
{
    SCHEMA_KB kb;
    META_KB   mk;
    LEARNER   lr;
    REL_KW    kws[CHAT_KW_MAX]; /* deduced relation index */
    uint32_t  num_kws;
    char      focus[CHAT_TOKEN_MAX]; /* last entity talked about */
    int       focus_valid;
} CHAT;

void ChatInit(CHAT *ch, const char *corpus_path);
void ChatHandle(CHAT *ch, const char *line);

/* per-family derivation policy (consultable, census-verified):
   returns 1 iff the family licenses 2-hop chain derivation */
int ChatFamilyChainAllowed(const char *family);

/* sibling scan: both directions of OBSERVED pairs only (swap) */
uint32_t ChatSiblings(const CHAT *ch, const char *who,
                      char out[][CHAT_TOKEN_MAX], uint32_t max_out);

#endif