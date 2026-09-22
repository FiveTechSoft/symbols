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

/* Fallback session key for clients that send no explicit identifier:
   FNV-1a over the stable request prefix (first system message content,
   first user message content), rendered "auto-<16 hex>". Returns 1 on
   success, 0 when neither span exists (caller keeps "default"). */
int ServerDeriveSessionKey(const char *body, char *out, size_t size);

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
    int  has_exit_code;
    int  exit_code;
    int  is_error;
} OPENAI_TOOL_RESPONSE;

/* Extract list of function names declared in "tools": [...] array */
int ServerExtractToolsDeclared(const char *body, char names[][64], uint32_t max_names);

/* Inspect tool response content for exit codes, error statuses, and diagnostics */
void ServerInspectToolResponse(OPENAI_TOOL_RESPONSE *resp);

/* Extract the most recent {"role":"tool", ...} message from the body */
int ServerExtractLastToolResponse(const char *body, OPENAI_TOOL_RESPONSE *out);

/* Format raw inspection tool output (JSON matches/files or plain text) into clean readable lines */
int ServerFormatInspectionOutput(const char *in, char *out, size_t size);

/* Extract the role of the very last message in the "messages" array */
int ServerExtractLastRole(const char *body, char *out, size_t size);

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

/* 1 when the prompt is a literal shell/CLI command (cmake, gcc, git, ls…).
   The engine must emit a tool_call; the harness runs it. */
int ServerIsShellTask(const char *text);

/* Map a shell prompt to bash or execute_command among declared tools.
   Fills out (name + arguments JSON). Returns 1 if a shell tool is available. */
int ServerMapShellToolCall(const char *query, const char names[][64],
                           uint32_t nnames, OPENAI_TOOL_CALL *out);

/* Classify whether a prompt represents a read-only code/workspace inspection task */
int ServerIsInspectionTask(const char *text);

/* Classify whether a prompt represents a direct code generation or synthesis task */
int ServerIsCodeSynthesisTask(const char *text);

/* Classify whether a prompt asks to create a new file in the workspace */
int ServerIsFileCreationTask(const char *text);

/* Filename for a create-file prompt. Never a directory, never CMakeLists.txt
   unless the user named it. Adds .txt when the user omitted an extension.
   Returns 1 and writes out on success. */
int ServerExtractCreatePath(const char *text, char *out, size_t n);

/* 1 when the user asks to display a diff (git diff / muestra el diff). */
int ServerIsDiffTask(const char *text);

/* 1 when the user asks to modify a named file (cambia X por Y en f.c). */
int ServerIsEditTask(const char *text);

/* Detect edit verbs (with or without enclitics) — no file check */
int ServerHasEditVerb(const char *text);

/* Exact bounded intent: swap the two lines of the active file. */
int ServerIsSwapLinesTask(const char *text);

/* Extract only tokens with recognized file extensions (for last_target tracking).
   Returns 1 if a file reference was found, 0 otherwise. */
int ServerExtractFileRef(const char *text, char *out, size_t n);

/* Parse "cambia OLD por NEW en FILE". has_replace is 1 when both strings exist. */
int ServerExtractEditSpec(const char *text, char *file, size_t fn,
                          char *old_s, size_t on, char *new_s, size_t nn,
                          int *has_replace);

/* bash/execute_command with "git diff". Returns 1 if a shell tool is declared. */
int ServerMapDiffToolCall(const char *query, const char names[][64],
                          uint32_t nnames, OPENAI_TOOL_CALL *out);

/* edit with oldString/newString, or read if the replacement is unknown. */
int ServerMapEditToolCall(const char *query, const char names[][64],
                          uint32_t nnames, OPENAI_TOOL_CALL *out);

/* Emit a canned C11 sample for a classified synthesis prompt.
   out is always NUL-terminated when out_sz > 0. */
void ServerSynthesizeCode(const char *query, char *out, size_t out_sz);

/* Classify whether a prompt represents a conversational greeting or identity question */
int ServerIsGreeting(const char *text);

/* Generate a friendly response for greetings and identity queries */
int ServerAnswerGreeting(const char *query, int persona_id, char *out, size_t out_sz);

#endif

