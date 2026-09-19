#ifndef SERVER_PROTO_H
#define SERVER_PROTO_H

/* server_proto: OpenAI-compatible wire layer (pure, testable).
   No sockets here: byte-level JSON extraction / building plus the
   engine call with stdout capture. Sockets live in symbols_server.c
   (harness). */

#include <stddef.h>
#include "chat.h"

/* Last {"role":"user","content":"..."} in a chat-completions body.
   Returns 1 on success (unescaped UTF-8 in out). */
int ServerExtractQuery(const char *body, char *out, size_t size);

/* Extract optional session identifier ("user" or "session_id" field)
   from chat-completions body. Returns 1 if found, 0 if absent. */
int ServerExtractSession(const char *body, char *out, size_t size);

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

/* 1 when the request asks for SSE streaming ("stream": true at top
   level; strings don't count). */
int ServerWantsStream(const char *body);

/* Full SSE payload: content chunk + finish chunk + [DONE]. */
int ServerBuildStreamResponse(const char *model, long created,
                              unsigned long seq, const char *content,
                              char *out, size_t size);

/* ============================================================
   OpenAI Tool Calling (Function Calling) Wire Protocol
   ============================================================ */

#define SERVER_MAX_TOOL_CALLS 8
#define SERVER_ARG_JSON_MAX 4096

typedef struct
{
    char id[64];
    char name[64];
    char arguments[SERVER_ARG_JSON_MAX];
} OPENAI_TOOL_CALL;

typedef struct
{
    OPENAI_TOOL_CALL calls[SERVER_MAX_TOOL_CALLS];
    uint32_t count;
} OPENAI_TOOL_CALLS;

typedef struct
{
    char tool_call_id[64];
    char name[64];
    char content[8192];
    int  has_response;
} OPENAI_TOOL_RESPONSE;

/* Extract list of function names declared in "tools": [...] array */
int ServerExtractToolsDeclared(const char *body, char names[][64], uint32_t max_names);

/* Extract the most recent {"role":"tool", ...} message from the body */
int ServerExtractLastToolResponse(const char *body, OPENAI_TOOL_RESPONSE *out);

/* Build full chat.completion JSON containing one or more tool_calls */
int ServerBuildToolCallResponse(const char *model, long created,
                                unsigned long seq, const OPENAI_TOOL_CALLS *tc,
                                const char *content_thought, char *out,
                                size_t size);

/* Build SSE streaming chunks for tool_calls */
int ServerBuildToolCallStreamResponse(const char *model, long created,
                                      unsigned long seq, const OPENAI_TOOL_CALLS *tc,
                                      char *out, size_t size);

/* Classify whether a prompt represents a coding/software engineering task */
int ServerIsCodingTask(const char *text);

#endif
