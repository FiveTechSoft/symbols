/* ============================================================
   agent_planner.h: Goal-Directed STRIPS State-Space Planner
                    for Autonomous Agentic Coding Tasks.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Cognitive Capabilities:
     1. FORMAL OPERATOR SEMANTICS: Models OpenCode tools as STRIPS
        operators with explicit preconditions, add-effects, and del-effects.
     2. STATE-SPACE FORWARD SEARCH: Computes the optimal, shortest-path
        DAG of tool calls to satisfy user goals in microsecond time.
     3. DYNAMIC REPLANNING: When verification fails (exit_code != 0),
        automatically retracts invalid state predicates, incorporates error
        diagnostics, and replans repair steps without human intervention.
     4. SENIOR-ENGINEER PLAN NLG: Emits structured markdown schedules
        documenting dependencies, current progress, and rationale.
   ============================================================ */

#ifndef AGENT_PLANNER_H
#define AGENT_PLANNER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "agent_core.h"

#define MAX_PLAN_OPERATORS 16
#define MAX_PLAN_STEPS     16
#define MAX_OPERATOR_NAME  48
#define MAX_PLANNER_DIAG   512

/* World state predicates represented as atomic bitflags */
typedef enum
{
    PRED_NONE                  = 0,
    PRED_SYMBOL_KNOWN          = (1 << 0),  /* Target symbol identifier is known */
    PRED_FILE_LOCATED          = (1 << 1),  /* Symbol file path has been resolved */
    PRED_CODE_INSPECTED        = (1 << 2),  /* Implementation lines have been read */
    PRED_CALLERS_MAPPED        = (1 << 3),  /* Reverse callers & dependencies mapped */
    PRED_BLAST_RADIUS_COMPUTED = (1 << 4),  /* Impact analysis completed */
    PRED_PATCH_PREPARED        = (1 << 5),  /* Pre-flight verified patch hunk ready */
    PRED_PATCH_APPLIED         = (1 << 6),  /* Surgical patch written to disk */
    PRED_BUILD_VERIFIED        = (1 << 7),  /* Compiles cleanly with exit code 0 */
    PRED_TESTS_VERIFIED        = (1 << 8),  /* Regression tests pass exit code 0 */
    PRED_ERROR_DIAGNOSED       = (1 << 9),  /* Compilation failure root cause known */
    PRED_TASK_COMPLETED        = (1 << 10)  /* Final objective fully satisfied */
} PLAN_PREDICATE;

/* A formal STRIPS planning operator */
typedef struct
{
    OpenCodeToolId tool_id;
    char           name[MAX_OPERATOR_NAME];
    uint32_t       preconditions;   /* Required predicates (AND condition) */
    uint32_t       add_effects;     /* Asserted predicates upon success */
    uint32_t       del_effects;     /* Retracted predicates upon success */
    uint32_t       cost;            /* Heuristic cost (e.g. read=1, mutation=3) */
    char           description[128];
} STRIPS_OPERATOR;

/* An actionable plan step */
typedef struct
{
    STRIPS_OPERATOR op;
    char            custom_args[256];
    char            rationale[256];
    bool            is_executed;
    bool            execution_success;
} PLAN_STEP;

/* Executable state-space plan */
typedef struct
{
    char      goal_description[128];
    uint32_t  initial_state;
    uint32_t  current_state;
    uint32_t  goal_state;
    PLAN_STEP steps[MAX_PLAN_STEPS];
    uint32_t  step_count;
    uint32_t  current_step_idx;
    bool      is_achieved;
} STRIPS_PLAN;

/* The planner environment and operator registry */
typedef struct
{
    STRIPS_OPERATOR operators[MAX_PLAN_OPERATORS];
    uint32_t        operator_count;
} AGENT_PLANNER;

/* ============================================================
   Lifecycle API
   ============================================================ */

/* Initialize planner and register standard OpenCode operators */
int  AgentPlannerInit(AGENT_PLANNER *planner);

/* ============================================================
   Planning Engine API
   ============================================================ */

/* Formulate optimal STRIPS plan from initial state to goal state */
int  AgentPlannerFormulate(const AGENT_PLANNER *planner,
                           const char *goal_description,
                           uint32_t initial_state,
                           uint32_t goal_state,
                           STRIPS_PLAN *out_plan);

/* Advance the plan by recording execution of the current step */
int  AgentPlanAdvance(STRIPS_PLAN *plan, bool step_success);

/* Dynamic replanning upon unexpected error or compilation failure */
int  AgentPlannerReplanOnError(const AGENT_PLANNER *planner,
                               STRIPS_PLAN *plan,
                               const char *error_diagnostic);

/* ============================================================
   Formatting and NLG API
   ============================================================ */

/* Format the current plan and progress into senior-level markdown */
int  AgentPlanFormatMarkdown(const STRIPS_PLAN *plan,
                             char *out_buffer,
                             size_t buffer_size);

#endif /* AGENT_PLANNER_H */
