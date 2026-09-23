#ifndef SERVER_SUBAGENT_H
#define SERVER_SUBAGENT_H

#include <stddef.h>
#include "server_proto.h"

/* OpenCode 1.18.32 parent side (the primary session declares `task`).

   Delegation rule (approved): delegate only when the request splits into
   2+ independent parts that touch different files; otherwise the normal
   loop does the work directly. Parts are the workspace files the request
   names (one part per file, so parts are disjoint by construction).
   Nothing trusts the child: every part a child reports as done is rebuilt
   here; a failed part is resumed once through its task_id, and a part that
   fails again is done directly by the normal loop.

   No agent name appears in this code: agents come from the `task` tool
   description the client sends. */

#define SA_MAX_AGENTS 16
#define SA_MAX_PARTS  8
#define SA_MAX_TASKS  32
#define SA_VERIFY_MARK "symbols-reverify: "

typedef struct
{
    char name[64];
    char desc[512];
} SA_AGENT;

/* Agents listed in the `task` tool description ("- name: description"
   lines). Returns the count. */
int SaParseAgents(const char *body, SA_AGENT *out, int max);

/* Parse one task tool output. Accepts both OpenCode 1.18 shapes:
   `<task id="..." state="...">...</task>` and
   `task_id: ... <task_result>...</task_result>`. state is "completed" or
   "error"; returns 1 when an id was found. */
int SaParseTaskOutput(const char *out, char *task_id, size_t id_size,
                      char *state, size_t state_size,
                      char *text, size_t text_size);

/* Files the child says it changed, from the Symbols result block
   ("- files changed: a, b" or "none"). Returns -1 when the text carries
   no such block (child is not Symbols), else the count. */
int SaChildFiles(const char *text, char files[][260], int max);

/* Parts of a request: distinct workspace files named in `query` that
   appear in `listing` (one path per line, absolute or relative to
   `workdir`). Returns the count; out holds workdir-relative paths. */
int SaNamedParts(const char *query, const char *listing, const char *workdir,
                 char parts[][260], int max);

/* Opt-in outcome memory (SYMBOLS_SUBAGENT_MEMORY=<file.tsv>): one line
   per finished part: agent, file extension, outcome. */
void SaMemoryRecord(const char *agent, const char *part, const char *outcome);
/* Net verified successes minus failures for agent on this extension. */
int  SaMemoryScore(const char *agent, const char *part);

/* Choose the agent for a part: highest memory score, ties broken by the
   description's word overlap with SA_PARALLEL_WORDS (declared rule). */
#define SA_PARALLEL_WORDS "execute multiple units of work in parallel"
int SaChooseAgent(const SA_AGENT *agents, int n, const char *part);

/* Explicit @agent mention (declared protocol rule): OpenCode 1.18 appends
   this sentence, followed by the agent name, to a user message that starts
   with "@name". Honored only when the name is in the task tool's agent list;
   then even one named workspace file is delegated, to that agent. */
#define SA_MENTION_MARK "call the task tool with subagent: "
int SaMentionedAgent(const char *user_text, const SA_AGENT *agents, int n);

enum { SA_NONE = 0, SA_CALLS = 1, SA_TEXT = 2, SA_DIRECT = 3 };

typedef struct
{
    int kind;
    OPENAI_TOOL_CALLS calls;
    char text[8192];
} SA_DECISION;

/* One decision for this request. `declared` are the tool names the client
   declared this turn, `query` the user's request text. SA_NONE leaves the
   request to the normal loop; SA_DIRECT means delegation is exhausted and
   the normal loop must run on SaStripExchanges(body). */
int SaDecide(const char *body, char declared[][64], int ndeclared,
             const char *query, SA_DECISION *d);

/* Copy of body without the `task` tool, the task calls/results and our
   re-verification calls/results, so the normal loop sees a request it
   authored itself. Returns 1 on success. */
int SaStripExchanges(const char *body, char *out, size_t size);

#endif
