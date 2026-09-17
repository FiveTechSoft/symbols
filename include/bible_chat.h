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

/* ---- Fase 2: BFS >= 3-hop over taxonomy (fail-closed) ---- */

#define CHAT_BFS_PATH_MAX 16 /* path nodes incl. both ends */
#define CHAT_BFS_ROW_MAX 128 /* reachable novel entities per source */

/* Breadth-first path search over taxonomy pairs ONLY (the
   transitive family; child edges = pairs whose SUBJECT is the
   node, i.e. "S isa O"). Static memory: frontier/parent/visited
   sized over SCHEMA_VOCAB_MAX, no dynamic allocation. The start
   node must be in the KB vocabulary (honest gate); the goal is
   reached only if it is in the vocabulary too. The pair (start,
   goal) must NOT be direct evidence (plain path owns 1-hop) and
   self-loops/cycles never enter the queue. Returns the number of
   path edges (>= 2) and fills path[0]=start .. path[k]=goal
   (CHAT_BFS_PATH_MAX bounds the reported trace), 0 when unknown
   (never a hypothesis). */
int ChatBfsPath(const CHAT *ch, const char *start, const char *goal,
                char path[][CHAT_TOKEN_MAX]);

/* Exhaustive reachability census for one source: every reachable
   entity with min edge distance >= 2, as (name, distance) rows
   (dedup, distances exact). Returns the row count (capped at
   CHAT_BFS_ROW_MAX), 0 when start is not in the vocabulary. */
uint32_t ChatBfsReach(const CHAT *ch, const char *start,
                      char names[][CHAT_TOKEN_MAX], uint32_t *depths,
                      uint32_t max_out);

#endif