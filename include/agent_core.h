/* ============================================================
   agent_core: Agentic AI Core & Tool Contract Dispatcher for OpenCode.
   Pure C11, zero tensors, zero backprop, fail-closed truth preservation.

   Core capabilities:
     1. TOOL CONTRACT SPECIFICATION: Formal preconditions, arguments, and effects
        for standard OpenCode tools (grep_search, view_file, replace_file_content, run_command).
     2. GOAL-TO-TOOL ACTION DISPATCHING: Deterministic state machine advancing
        through perception, inspection, patch application, and test verification.
     3. OBSERVATION INGESTION & STATE TRANSITIONS: Ingests environment feedback,
        updates Working Memory, and triggers abductive error diagnosis on failure.
     4. PROTOCOL INTEGRATION: Emits standard ACTION_TOOL_CALL, ACTION_FINAL,
        and ACTION_ABSTAIN serialized for OpenCode harness execution.

   Design principles:
     - HARDCODING=0: Tool contracts and state rules are declarative.
     - Fail-closed: Never executes unverified mutations without inspection.
   ============================================================ */

#ifndef AGENT_CORE_H
#define AGENT_CORE_H

#include <stdint.h>
#include <stddef.h>
#include "graph.h"
#include "graph_reasoning.h"
#include "cognitive_learning.h"
#include "metacognition.h"
#include "agent_action.h"

#define AGENT_MAX_GOAL_LEN   256
#define AGENT_MAX_PATH_LEN   128
#define AGENT_MAX_STEPS      16

/* Standard OpenCode tool IDs */
typedef enum
{
    OP_TOOL_NONE = 0,
    OP_TOOL_GREP_SEARCH,      /* Search for pattern/symbol across codebase */
    OP_TOOL_FIND_BY_NAME,     /* Search for files matching glob */
    OP_TOOL_VIEW_FILE,        /* Read specific file lines */
    OP_TOOL_REPLACE_CONTENT,  /* Surgical file patch replacement */
    OP_TOOL_RUN_COMMAND,      /* Terminal command execution (build/test) */
    OP_TOOL_COUNT
} OpenCodeToolId;

/* Formal tool contract specification */
typedef struct
{
    OpenCodeToolId id;
    const char    *name;            /* e.g. "grep_search", "view_file" */
    const char    *description;
    const char    *required_arg;    /* Primary parameter expected */
    uint32_t       is_mutating;     /* 1 if alters disk state, 0 if read-only */
} TOOL_CONTRACT_SPEC;

/* Current cognitive state of the agent in a multi-step task */
typedef enum
{
    AGENT_STATE_IDLE = 0,
    AGENT_STATE_LOCATING_SYMBOL,   /* Needs grep / find */
    AGENT_STATE_INSPECTING_CODE,   /* Needs view_file */
    AGENT_STATE_APPLYING_FIX,      /* Needs replace_file_content */
    AGENT_STATE_VERIFYING_BUILD,   /* Needs run_command */
    AGENT_STATE_DIAGNOSING_ERROR,  /* Abductive recovery from exit != 0 */
    AGENT_STATE_COMPLETED          /* Goal satisfied -> ACTION_FINAL */
} AGENT_TASK_STATE;

/* The agent's working session in the OpenCode harness */
typedef struct
{
    char             task_goal[AGENT_MAX_GOAL_LEN];
    char             target_symbol[64];
    char             target_file[AGENT_MAX_PATH_LEN];
    uint32_t         target_line;
    AGENT_TASK_STATE state;
    uint32_t         step_count;
    int              last_exit_code;
    char             last_error_diag[256];
    char             patch_target[256];
    char             patch_replacement[256];
    char             verification_cmd[128];
    WORKING_MEMORY   wm;
    PROVENANCE_TABLE pt;
} AGENT_SESSION;

/* Initialize an agentic session for a goal */
void AgentSessionInit(
    AGENT_SESSION *session,
    const char *goal,
    const char *initial_symbol,
    const char *verification_command);

/* Decide the next formal action based on the task state and working memory */
int AgentDecideNextAction(
    AGENT_SESSION *session,
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    AgentAction *out_action);

/* Process observation returned by the OpenCode harness */
int AgentProcessObservation(
    AGENT_SESSION *session,
    GRAPH *graph,
    COGNITIVE_LEARNER *learner,
    const AgentObservation *obs);

/* Inspect tool contracts */
const TOOL_CONTRACT_SPEC *AgentGetToolContract(OpenCodeToolId id);

#endif /* AGENT_CORE_H */
