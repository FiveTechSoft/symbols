#ifndef BIBLE_CHAT_H
#define BIBLE_CHAT_H

/* bible_chat: symbolic conversational engine over the frozen C
   layers (schema+meta+transfer). No tensors, no backprop. */

#define CHAT_TOKEN_MAX 32

#include "schema.h"
#include "metaschema.h"
#include "learn.h"

typedef struct CHAT_
{
    SCHEMA_KB kb;
    META_KB   mk;
    LEARNER   lr;
    char      focus[CHAT_TOKEN_MAX]; /* last entity talked about */
    int       focus_valid;
} CHAT;

void ChatInit(CHAT *ch, const char *corpus_path);
void ChatHandle(CHAT *ch, const char *line);

#endif