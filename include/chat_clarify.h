#ifndef CHAT_CLARIFY_H
#define CHAT_CLARIFY_H

/* chat_clarify: conversational wrapper over the pure engine (Fase B).
   The core (bible_chat) never changes: the wrapper only converts a
   GOAL_AMBIGUOUS into a clarification interaction and re-runs the
   same goal with the user-resolved binding. Invariants: never guess
   an identity; never lose the originating goal; failed clarification
   stays AMBIGUOUS/UNKNOWN, never an arbitrary pick. A pending
   clarification never blocks: any non-resolving input discards it
   and is processed as a fresh query. */

#include "bible_chat.h"

#define CLAR_MAX_CAND 16
#define CLAR_OUT_MAX 4096

typedef struct
{
    CHAT     ch;     /* the pure engine, untouched */
    int      pending; /* clarification open */
    char     child[CHAT_TOKEN_MAX]; /* originating goal slot */
    char     cand[CLAR_MAX_CAND][CHAT_TOKEN_MAX]; /* ingest order */
    uint32_t ncand;
    int      attempts; /* failed resolutions (bounded re-ask) */
} CLARIFY;

void ClarifyInit(CLARIFY *w, const char *corpus_path);
void ClarifyHandle(CLARIFY *w, const char *line);

#endif
