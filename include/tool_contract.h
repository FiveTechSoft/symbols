#ifndef TOOL_CONTRACT_H
#define TOOL_CONTRACT_H

/* tool_contract: Agent Core planner (diagnostic, no execution).
   The planner never resolves: it only asks whether a tool whose
   contract can formally satisfy the goal exists. Decision depends
   on failure cause + applicable contract, never on UNKNOWN text. */

#include "bible_chat.h"

typedef enum
{
    TOOL_NONE = 0,
    TOOL_LOOKUP_PERSON,
    TOOL_LOOKUP_RELATION,
    TOOL_CALCULATOR,
    TOOL_SHELL,
    TOOL_FS_READ
} ToolId;

typedef enum
{
    DEC_UNKNOWN = 0,
    DEC_IS_ANSWER,
    DEC_AMBIGUOUS,
    DEC_NEEDS_TOOL
} ToolDecision;

typedef struct
{
    ToolId tool;
    char   subject[CHAT_TOKEN_MAX];
    char   relation[CHAT_TOKEN_MAX];
} ToolRequest;

/* ---- Paso 1: external agentic tables (data/agentic/tools.tsv) ----
   Row structs for the runtime tables. Loaded once by ToolInit():
   each table comes from the file when it provides >= 1 valid row,
   else from the frozen compiled fallback. Order = file order
   (priority), deterministic. Zero malloc: fixed caps. */
#define TOOLCFG_SHELL_MAX 32
#define TOOLCFG_CONTRACT_MAX 32
#define TOOLCFG_INFO_MAX 16
#define TOOLCFG_REL_MAX 64
#define TOOLCFG_PERSON_MAX 64

typedef struct
{
    char trigger[32];
    char backend[16];
    char dflt[64];
} ShellAllowRow;

typedef struct
{
    char   family[32];
    ToolId tool;
    int    needs_known;
} ToolContractRow;

typedef struct
{
    char name[32];
    char inputs[64];
    char outputs[64];
} ToolInfoRow;

typedef struct
{
    char subject[32];
    char rel[32];
    char object[64];
} FixtureRelRow;

typedef struct
{
    char name[32];
    char detail[64];
} FixturePersonRow;

void ToolInit(void);
void ToolInitFrom(const char *path);
void FixtureInitFrom(const char *path);
uint32_t ShellAllowCount(void);
const ShellAllowRow *ShellAllowAt(uint32_t i);
uint32_t ToolContractCount(void);
const ToolContractRow *ToolContractRowAt(uint32_t i);
uint32_t ToolInfoCount(void);
const ToolInfoRow *ToolInfoRowAt(uint32_t i);
uint32_t FixtureRelCount(void);
const FixtureRelRow *FixtureRelAt(uint32_t i);
uint32_t FixturePersonCount(void);
const FixturePersonRow *FixturePersonAt(uint32_t i);

/* Classify one resolved goal. status/cause/slot/family come from
   ChatResolveLine; line is needed only for PARSE_FAIL shape
   analysis (numbers, W-de/of-ARG, person mention). Pure and
   deterministic; executes nothing. */
ToolDecision ToolClassify(const CHAT *ch, const char *line,
                          GOAL_STATUS status, GoalCause cause,
                          const char *slot, const char *family,
                          ToolRequest *req);

/* ---- ToolExecute (minimal executor, static tables only) ----
   Tools never answer the user: they return structured data and the
   engine re-resolves the same QueryGoal. Tables below live in
   tool_executor.c as external-source stand-ins (never merged into
   the corpus KB, whose charter stays frozen). */
typedef struct
{
    ToolId tool;
    int    ok;
    char   items[8][CHAT_TOKEN_MAX];
    uint32_t nitems;
    char   number[64];
    char   text[4096];
    int    exit_code;
    int    timed_out;
} ToolResult;

void ToolExecute(const ToolRequest *req, ToolResult *res);

/* Shell allowlist lookup (runtime table: file rows win, compiled
   fallback otherwise): 1 when tok names a runnable command; backend
   + default argv out. Verbs before the trigger are never listed
   (ignored by position). */
int ShellLookup(const char *tok, char *backend, size_t bs, char *dflt,
                size_t ds);

/* Sandbox directory for shell side effects (system temp +
   fixed subdir, created on demand). */
void ShellSandboxPath(char *out, size_t size);

/* ---- Minimal execution interface (zero external deps) ----
   ShellResult carries exit code + bounded merged output
   (stdout+stderr). ShellExec enforces buffer cap and strict
   timeout (kills on expiry, never hangs the REPL). GccCompile
   validates basename + flags before delegating. */
#define SHELL_OUT_MAX 2048
#define SHELL_TIMEOUT_MS 30000

typedef struct
{
    int    exit_code;
    int    timed_out;
    size_t out_len;
    char   out_buf[SHELL_OUT_MAX];
} ShellResult;

ShellResult ShellExec(const char *cmd_line);
ShellResult GccCompile(const char *source_file, const char *extra_flags);

/* Sandbox-confined file read (flat sandbox root only, capped). */
int FsReadSandbox(const char *name, char *out, size_t size);

/* Re-resolve the goal from a ToolResult into answer text with source
   marking ("fuente externa" / "calculo"): the same goal, answered
   from tool data. Returns 1 when answered, 0 to keep the original
   UNKNOWN (tool miss fails closed). */
int ToolAnswerGoal(const ToolRequest *req, const ToolResult *res,
                   char *out, size_t size);

#endif
