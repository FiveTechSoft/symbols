#ifndef SERVER_PROTO_H
#define SERVER_PROTO_H

/* server_proto: OpenAI-compatible wire layer (pure, testable).
   No sockets here: byte-level JSON extraction / building plus the
   engine call with stdout capture. Sockets live in symbols_server.c
   (harness). */

#include <stddef.h>
#include "chat.h"
#include "agent_git.h"

/* Last {"role":"user","content":"..."} in a chat-completions body.
   Returns 1 on success (unescaped UTF-8 in out). */
int ServerExtractQuery(const char *body, char *out, size_t size);

/* First {"role":"system","content":"..."} (unescaped). Returns 1 if found. */
int ServerExtractFirstSystem(const char *body, char *out, size_t size);

/* Message roles in order, one letter each: s(ystem) u(ser) a(ssistant)
   t(ool), '?' for others. Returns the number of messages. */
int ServerRoleSequence(const char *body, char *out, size_t size);

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

/* Recover the assistant tool call paired with a tool response.  OpenCode
   1.18.32 omits the tool name from role=tool messages, so correlation uses
   the exact tool_call_id from request history. */
int ServerExtractPairedToolCall(const char *body, const char *tool_call_id,
                                OPENAI_TOOL_CALL *out);

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

/* Existing-workspace change/fix/implement/optimize intent. This route wins
   over standalone code synthesis and requires client workspace evidence. */
int ServerIsRepositoryTask(const char *text);
/* Command-line shape (no program whitelist); had_verb/cmd_start optional. */
int ServerShellShape(const char *text, int *had_verb, size_t *cmd_start);
#define SERVER_SHELL_NOT_FOUND_MARK "symbols-probe:not-a-command:"
#define SERVER_SHELL_IS_COMMAND_MARK "symbols-probe:is-command:"
#include "episodic_memory.h"
/* Procedural memory of probe outcomes (percibir -> razonar -> actuar ->
   verificar -> aprender).  Records: word is_command|not_command scope. */
#define SERVER_PROC_IS  "is_command"
#define SERVER_PROC_NOT "not_command"
#define SERVER_PROC_NEG_TTL 3600   /* seconds a missing-program memory may skip the probe */
typedef struct {
    int p1_known, p2_known;      /* +1 command, -1 not a command, 0 unknown */
    int decision;                /* SERVER_PROC_* below */
    char p1[64], p2[64];
} SERVER_PROC_TRACE;
#define SERVER_PROC_PROBED        1   /* nothing known: tested in the environment */
#define SERVER_PROC_DIRECT        2   /* memory made the probe unnecessary */
#define SERVER_PROC_PARTIAL       3   /* memory removed one of two readings */
#define SERVER_PROC_FROM_MEMORY   4   /* answered from memory, no tool call */
void ServerProcScope(const char *body, char *out, size_t size);
int  ServerProcRecall(const EPISODIC_STORE *st, const char *scope, const char *word);
/* Learn from probe marker lines in a tool output; returns records changed. */
int  ServerProcLearnFromOutput(EPISODIC_STORE *st, const char *scope, const char *output,
                               char *learned, size_t learned_size);
/* Correct: a remembered command that the shell reports as missing is forgotten. */
int  ServerProcCorrectFromOutput(EPISODIC_STORE *st, const char *scope, const char *command,
                                 const char *output);
/* 1 when tool-call arguments carry our probe markers (ingest gate). */
int  ServerProcArgumentsCarryProbe(const char *arguments);
/* Remove probe marker lines from output shown to the user. */
void ServerStripProbeLines(const char *in, char *out, size_t size);
/* 0 not shell, 1 shell (tool call), 2 every reading already known not to be a command */
int  ServerShellRouteMem(const char *query, const EPISODIC_STORE *st, const char *scope,
                         SERVER_PROC_TRACE *trace);

/* 1 when the prompt is a literal shell/CLI command (cmake, gcc, git, ls…).
   The engine must emit a tool_call; the harness runs it. */
int ServerIsShellTask(const char *text);

/* Map a shell prompt to bash or execute_command among declared tools.
   Fills out (name + arguments JSON). Returns 1 if a shell tool is available. */
int ServerMapShellToolCallMem(const char *query, const char names[][64],
                              uint32_t nnames, const EPISODIC_STORE *st, const char *scope,
                              SERVER_PROC_TRACE *trace, OPENAI_TOOL_CALL *out);
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

/* --- Native read-only Git integration (agent_git) ------------------- */

/* Client working directory from the first system message <env> block
   ("Working directory: <path>"). Bound to the request prefix: the same
   literal after the first user message is client content, not env.
   JSON backslash pairs collapse (Windows paths). Returns 1 on success. */
int ServerExtractWorkingDir(const char *body, char *out, size_t size);

/* Evidence-driven workspace planning helpers. The listing must come from the
   client workspace; these functions never inspect the server repository. */
int ServerSelectWorkspaceFile(const char *issue, const char *listing,
                              char *out, size_t size);
int ServerIsExplicitStockTotalFeature(const char *issue);
int ServerSelectFeatureFiles(const char *listing, const char *first, char *implementation, size_t implementation_size, char *main_file, size_t main_size);
int ServerPlanStockHeader(const char *source, char *out, size_t size);
int ServerPlanStockImplementation(const char *source, char *out, size_t size);
int ServerPlanStockMain(const char *issue, const char *source, char *out, size_t size);
int ServerIssueRequestsSanitizer(const char *issue);
int ServerIssueRequestsTests(const char *issue);
int ServerSelectWorkspaceTestFile(const char *listing, const char *target, char *out, size_t size);
int ServerPlanTestObservedCRepair(const char *source, const char *test_source, char *old_text, size_t old_size, char *new_text, size_t new_size);
int ServerInferWorkspaceCommands(const char *listing,
                                 char *build, size_t build_size,
                                 char *test, size_t test_size);
int ServerIsAmbiguousCodingTask(const char *text);

/* Derive a bounded single-source C verification command only from an observed
   .c path and explicit task requirements. Returns 0 rather than inventing a
   command for other languages or ambiguous paths. */
int ServerDeriveSingleCCommand(const char *issue, const char *path,
                               char *out, size_t size);

/* Plan one conservative edit from observed C source plus an actual compiler or
   sanitizer diagnostic. Supports identifier suggestions and allocation-size
   mismatches; exact old/new strings are returned for the client's edit tool. */
int ServerPlanObservedCRepair(const char *source, const char *diagnostic,
                              char *old_text, size_t old_size,
                              char *new_text, size_t new_size);

/* 1 when the prompt asks, in natural language, about read-only Git
   repository state (branch, HEAD, dirty vs ignored paths, status).
   Literal commands ("git status") stay on the shell route; mutation
   intents (commit, push, nueva rama...) never match. */
int ServerIsGitInquiryTask(const char *text);

/* 1 when the prompt asks whether the repository is ready/safe to work
   on. Answered through AgentGitPreflight: abstain and say why on
   dirty, stale, detached or conflicted states. */
int ServerIsGitPreflightTask(const char *text);

/* Natural, bounded answers from one fresh inspection snapshot. Every
   path fails closed: failures and unsafe states are stated plainly,
   never guessed. Short HEAD is the first 8 chars. */
void ServerComposeGitStatusAnswer(const GIT_REPOSITORY_STATE *st,
                                  char *out, size_t size);
void ServerComposeGitInspectFailure(GIT_INSPECT_STATUS status,
                                    const char *error,
                                    const char *working_dir,
                                    char *out, size_t size);
void ServerComposeGitPreflightAnswer(GIT_PREFLIGHT_STATUS status,
                                     const GIT_REPOSITORY_STATE *observed,
                                     const char *expected_head,
                                     char *out, size_t size);

#endif
