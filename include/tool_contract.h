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
    TOOL_CALCULATOR
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
} ToolResult;

void ToolExecute(const ToolRequest *req, ToolResult *res);

/* Re-resolve the goal from a ToolResult into answer text with source
   marking ("fuente externa" / "calculo"): the same goal, answered
   from tool data. Returns 1 when answered, 0 to keep the original
   UNKNOWN (tool miss fails closed). */
int ToolAnswerGoal(const ToolRequest *req, const ToolResult *res,
                   char *out, size_t size);

#endif
