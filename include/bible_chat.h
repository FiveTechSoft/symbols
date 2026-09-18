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

/* ---- Agent Core: execution memory (per-query, never the KB) ----
   Holds what each goal produced so later goals can use it
   (anaphora targets, tool data) and why (provenance log answering
   where each datum came from: KB, which tool, or abstain).
   Dialogue state like focus, not knowledge: the corpus KB is never
   written. Overwritten per multi-goal query; single-goal lines
   leave it untouched. */
#define EXEC_ENT_MAX 8
#define EXEC_PROV_MAX 8

typedef struct
{
    char     entities[EXEC_ENT_MAX][CHAT_TOKEN_MAX];
    uint32_t nent;
    char     number[64];
    int      has_number;
    char     prov[EXEC_PROV_MAX][160];
    uint32_t nprov;
} ExecCtx;

typedef struct CHAT_
{
    SCHEMA_KB kb;
    META_KB   mk;
    LEARNER   lr;
    REL_KW    kws[CHAT_KW_MAX]; /* deduced relation index */
    uint32_t  num_kws;
    char      focus[CHAT_TOKEN_MAX]; /* last entity talked about */
    int       focus_valid;
    ExecCtx   exec; /* per-query execution memory (never the KB) */
} CHAT;

void ChatInit(CHAT *ch, const char *corpus_path);
void ChatHandle(CHAT *ch, const char *line);

/* ---- Fase A/B goal outcomes: per-goal result of QUERY -> SET ---- */
typedef enum { GOAL_UNKNOWN = 0, GOAL_ANSWER, GOAL_AMBIGUOUS } GOAL_STATUS;

/* UNKNOWN causes (Agent Core): the planner routes on cause + tool
   contract, never on the UNKNOWN text. PARSE_FAIL covers vetoes
   (no goal constructed); NO_VOCAB = slot outside the ingested
   vocabulary; NO_DERIVATION = known slot, frame ok, no pairs. */
typedef enum
{
    CAUSE_NONE = 0,
    CAUSE_PARSE_FAIL,
    CAUSE_NO_VOCAB,
    CAUSE_NO_DERIVATION,
    CAUSE_ANAPHORA
} GoalCause;

/* Single-intent resolve for the clarification wrapper (Fase B):
   same frames and same strings as ChatHandle's legacy path, but the
   text lands in out[] and the outcome is returned: -1 = parse-fail
   (legacy "No entendi" line), else a GOAL_STATUS. slot receives the
   resolved slot-A ("" when anaphoric-empty). Pure reuse: no logic
   or NLG change. */
int ChatResolveLine(CHAT *ch, const char *line, char *out, size_t size,
                    char *slot, size_t slot_size,
                    char *family, size_t family_size,
                    GoalCause *cause);

/* Parent candidates of child in ingest order, deduped (Fase B). */
uint32_t ChatParentsList(const CHAT *ch, const char *child,
                         char out[][CHAT_TOKEN_MAX], uint32_t max_out);

/* Single-parent answer reusing the legacy PARENT template verbatim
   (Fase B resolution: the same goal with the user-resolved binding).
   No new NLG: identical bytes to the unambiguous answer. */
void ChatAnswerParentSingle(const CHAT *ch, const char *child,
                            const char *parent, char *out, size_t size);

/* Fold one raw token to canonical form (lowercase + accent fold),
   for wrapper-side name matching. */
void ChatNormTok(const char *in, char *out, size_t size);

/* Capitalize for display (same rule as answer templates). */
void ChatCapStr(const char *tok, char *out, size_t size);

/* Read-only parse inspection for observation logging (server): runs
   the frozen frames with zero side effects (no focus, no NLG) and
   reports intent name + slots + family. New code reusing ParseIntent;
   existing behavior untouched. */
typedef struct
{
    char intent[32];
    char slot_a[CHAT_TOKEN_MAX];
    char slot_b[CHAT_TOKEN_MAX];
    char family[64];
} ChatParse;

int ChatParseLine(CHAT *ch, const char *line, ChatParse *out);

/* ---- Fase 4 surface flags: punctuation is signal, not content.
   Split peels trailing ASCII punctuation (? ! . , ; :), drops
   leading inverted question marks, expands del and detaches 's,
   recording each as a flag. */
typedef struct
{
    int question;
    int comma;
    int period;
    int genitive;
} SURFACE_FLAGS;

/* ---- Fase A: multi-goal query plan ----
   A QueryGoal isolates one coordinated goal: its token span in the
   canonical stream plus the deduced relation bound in that span
   (kwx = index into ch->kws, -1 when the span has none). Elliptical
   spans inherit goal 1's relation (inherit = 1). Split points are
   deduced per input (droppable + trial-parse + viability +
   recursion); no coordinator is ever named. SymbolID canonical
   ordering is a wrapper-phase (B) concern; until then observable
   order is ingest order, deterministic for a frozen corpus. */
#define QP_MAX_GOALS 4

typedef struct
{
    uint32_t start;   /* token span [start,end) in canonical toks */
    uint32_t end;
    int      kwx;     /* deduced relation index (-1 = none in span) */
    int      kwpos;   /* absolute token pos (-1 when inherited/absent) */
    int      inherit; /* elliptical: relation inherited from goal 1 */
    int      connector; /* 0 = first goal, 1 = deduced separator */
} QueryGoal;

typedef struct
{
    QueryGoal goals[QP_MAX_GOALS];
    uint32_t  count;
} QueryPlan;

/* Segment the canonical token stream at deduced coordinators.
   Returns the goal count: 0 = refuse (empty input, or more spans
   than QP_MAX_GOALS); 1 = single goal (no clean coordination:
   legacy single-intent path). Pure representation: no G1/G2/G5
   vetoes here. */
uint32_t ChatBuildPlan(const CHAT *ch, const char *line, QueryPlan *plan,
                       char toks[][CHAT_TOKEN_MAX], uint32_t *ntok,
                       SURFACE_FLAGS *sfout);

/* per-family derivation policy (consultable, census-verified):
   returns 1 iff the family licenses 2-hop chain derivation */
int ChatFamilyChainAllowed(const char *family);

/* sibling scan: both directions of OBSERVED pairs only (swap) */
uint32_t ChatSiblings(const CHAT *ch, const char *who,
                      char out[][CHAT_TOKEN_MAX], uint32_t max_out);

/* ---- Phase 2: BFS >= 3-hop over taxonomy (fail-closed) ---- */

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