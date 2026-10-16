/* ============================================================
   agent_core: shared tool metadata and fixture-only session demonstrations.

   AgentSessionInit, AgentDecideNextAction and AgentProcessObservation are
   fixture-only APIs for deterministic protocol/state-transition tests. They
   remain linked and callable in the common library: this is not runtime
   enforcement. Do not execute their generated actions against arbitrary
   projects. They are not a general repair API or an execution-safety gate.

   Their fixed fallback is src/engine.c:10; patch choices include
   OldFunction/NewFunctionFixed and wrong.h/correct.h, independent of the
   inspected source. Exit code zero can produce COMPLETED/FINAL; generated
   verification wording is a simulated result, not proof of a real repair.

   OpenCodeToolId, TOOL_CONTRACT_SPEC and AgentGetToolContract remain shared
   tool metadata. Including this header does not invoke the fixture routines.
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

/* State of the fixture-only multi-step session demonstration. */
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

/* Session storage for the fixture-only demonstration. */
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

/* Fixture-only: initialize a demonstration session, not a production task. */
void AgentSessionInit(
    AGENT_SESSION *session,
    const char *goal,
    const char *initial_symbol,
    const char *verification_command);

/* Fixture-only: emit the next demonstration action. Do not execute it against
   arbitrary projects; this function does not validate its safety or intent. */
int AgentDecideNextAction(
    AGENT_SESSION *session,
    const GRAPH *graph,
    const COGNITIVE_LEARNER *learner,
    AgentAction *out_action);

/* Fixture-only: consume a simulated observation and select fixed transitions.
   Patch strings are not inferred from the observation or verified as correct. */
int AgentProcessObservation(
    AGENT_SESSION *session,
    GRAPH *graph,
    COGNITIVE_LEARNER *learner,
    const AgentObservation *obs);

/* Inspect shared tool metadata, independently of the fixture session API. */
const TOOL_CONTRACT_SPEC *AgentGetToolContract(OpenCodeToolId id);

#endif /* AGENT_CORE_H */
