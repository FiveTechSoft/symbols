#ifndef SERVER_PROTO_H
#define SERVER_PROTO_H

/* server_proto: OpenAI-compatible wire layer (pure, testable).
   No sockets here: byte-level JSON extraction / building plus the
   engine call with stdout capture. Sockets live in symbols_server.c
   (harness). */

#include <stddef.h>
#include "bible_chat.h"

/* Last {"role":"user","content":"..."} in a chat-completions body.
   Returns 1 on success (unescaped UTF-8 in out). */
int ServerExtractQuery(const char *body, char *out, size_t size);

/* Full chat.completion JSON for content (already mapped). */
int ServerBuildResponse(const char *model, long created,
                        unsigned long seq, const char *content,
                        const char *query_for_tokens, char *out,
                        size_t size);

/* GET /v1/models body (single "symbols" model). */
int ServerBuildModels(const char *out_model, char *out, size_t size);

/* 1 when engine text is an honest abstention (parse-fail, UNKNOWN,
   span-echo, ambiguity): the model-facing "I don't know" set. */
int ServerIsUnknown(const char *text);

/* Status for the observation log (prefix rules over frozen NLG). */
const char *ServerStatusOf(const char *text);

/* Run one query through the conversational engine (REPL parity)
   and return the raw reply text. */
int ServerAnswerQuery(CHAT *ch, const char *query, char *out,
                      size_t size);

/* Map raw reply to model-facing content (unknown family -> the
   frozen "I don't know" sentence). */
int ServerMapContent(const char *raw, char *out, size_t size);

/* One JSONL observation line (all strings pre-escaped by caller
   except query/result, escaped here). Deterministic field order. */
int ServerBuildObservation(const char *ts, const char *model, int nmsg,
                           int has_system, const char *query,
                           const ChatParse *parsed, const char *focus,
                           const char *entities_json,
                           const char *prov_json, const char *result,
                           const char *status, long latency_ms,
                           char *out, size_t size);

/* Minimal JSON string escaper for log fields. */
int ServerJsonEscape(const char *in, char *out, size_t size);

#endif
