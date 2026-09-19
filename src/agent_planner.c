/* ============================================================
   agent_planner.c: Goal-Directed STRIPS State-Space Planner
                    for Autonomous Agentic Coding Tasks.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_planner.h"

#define MAX_SEARCH_QUEUE 512

/* Internal search node for forward state-space exploration */
typedef struct
{
    uint32_t state;
    uint32_t op_indices[MAX_PLAN_STEPS];
    uint32_t step_count;
    uint32_t total_cost;
} SEARCH_NODE;

/* ============================================================
   Lifecycle API
   ============================================================ */

int AgentPlannerInit(AGENT_PLANNER *planner)
{
    if (!planner)
        return 0;

    memset(planner, 0, sizeof(*planner));

    /* Register standard OpenCode STRIPS operators */

    /* 1. locate_symbol */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_GREP_SEARCH,
        .name          = "locate_symbol",
        .preconditions = PRED_SYMBOL_KNOWN,
        .add_effects   = PRED_FILE_LOCATED,
        .del_effects   = 0,
        .cost          = 1,
        .description   = "Search codebase using grep to resolve symbol file and line location"
    };

    /* 2. inspect_code */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_VIEW_FILE,
        .name          = "inspect_code",
        .preconditions = PRED_FILE_LOCATED,
        .add_effects   = PRED_CODE_INSPECTED,
        .del_effects   = 0,
        .cost          = 1,
        .description   = "Read source code context around target location"
    };

    /* 3. analyze_blast_radius */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_NONE,
        .name          = "analyze_blast_radius",
        .preconditions = PRED_CODE_INSPECTED,
        .add_effects   = PRED_CALLERS_MAPPED | PRED_BLAST_RADIUS_COMPUTED,
        .del_effects   = 0,
        .cost          = 1,
        .description   = "Analyze AST call graph and compute transitive blast radius"
    };

    /* 4. diagnose_error */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_VIEW_FILE,
        .name          = "diagnose_error",
        .preconditions = PRED_ERROR_DIAGNOSED,
        .add_effects   = PRED_PATCH_PREPARED,
        .del_effects   = PRED_ERROR_DIAGNOSED,
        .cost          = 1,
        .description   = "Inspect diagnostic error output and abduce corrected patch"
    };

    /* 5. prepare_surgical_patch */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_NONE,
        .name          = "prepare_surgical_patch",
        .preconditions = PRED_CODE_INSPECTED | PRED_BLAST_RADIUS_COMPUTED,
        .add_effects   = PRED_PATCH_PREPARED,
        .del_effects   = 0,
        .cost          = 1,
        .description   = "Formulate and dry-run pre-flight verified patch hunk"
    };

    /* 6. apply_patch */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_REPLACE_CONTENT,
        .name          = "apply_patch",
        .preconditions = PRED_PATCH_PREPARED,
        .add_effects   = PRED_PATCH_APPLIED,
        .del_effects   = PRED_BUILD_VERIFIED | PRED_TESTS_VERIFIED,
        .cost          = 2,
        .description   = "Atomically apply surgical patch to target file on disk"
    };

    /* 7. verify_build */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_RUN_COMMAND,
        .name          = "verify_build",
        .preconditions = PRED_PATCH_APPLIED,
        .add_effects   = PRED_BUILD_VERIFIED,
        .del_effects   = 0,
        .cost          = 3,
        .description   = "Execute compilation build command and verify exit code 0"
    };

    /* 8. run_regression_tests */
    planner->operators[planner->operator_count++] = (STRIPS_OPERATOR){
        .tool_id       = OP_TOOL_RUN_COMMAND,
        .name          = "run_regression_tests",
        .preconditions = PRED_BUILD_VERIFIED,
        .add_effects   = PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED,
        .del_effects   = 0,
        .cost          = 3,
        .description   = "Run test suites covering affected functions in blast radius"
    };

    return 1;
}

/* ============================================================
   Planning Engine (State-Space Forward Search)
   ============================================================ */

int AgentPlannerFormulate(const AGENT_PLANNER *planner,
                           const char *goal_description,
                           uint32_t initial_state,
                           uint32_t goal_state,
                           STRIPS_PLAN *out_plan)
{
    if (!planner || !out_plan)
        return 0;

    memset(out_plan, 0, sizeof(*out_plan));
    if (goal_description)
        strncpy(out_plan->goal_description, goal_description, sizeof(out_plan->goal_description) - 1);

    out_plan->initial_state = initial_state;
    out_plan->current_state = initial_state;
    out_plan->goal_state    = goal_state;

    /* Trivial case: goal already satisfied */
    if ((initial_state & goal_state) == goal_state)
    {
        out_plan->is_achieved = true;
        return 1;
    }

    /* BFS state-space search queue */
    SEARCH_NODE queue[MAX_SEARCH_QUEUE];
    uint32_t head = 0;
    uint32_t tail = 0;

    uint32_t visited_states[MAX_SEARCH_QUEUE];
    uint32_t visited_count = 0;

    queue[tail++] = (SEARCH_NODE){
        .state       = initial_state,
        .step_count  = 0,
        .total_cost  = 0
    };
    visited_states[visited_count++] = initial_state;

    bool found_plan = false;
    SEARCH_NODE best_node;
    memset(&best_node, 0, sizeof(best_node));

    while (head < tail)
    {
        SEARCH_NODE curr = queue[head++];

        /* Check if goal state is satisfied */
        if ((curr.state & goal_state) == goal_state)
        {
            found_plan = true;
            best_node = curr;
            break;
        }

        if (curr.step_count >= MAX_PLAN_STEPS)
            continue;

        /* Try expanding with each valid operator */
        for (uint32_t i = 0; i < planner->operator_count; i++)
        {
            const STRIPS_OPERATOR *op = &planner->operators[i];

            /* Preconditions check */
            if ((curr.state & op->preconditions) == op->preconditions)
            {
                uint32_t next_state = (curr.state | op->add_effects) & ~op->del_effects;

                /* Check visited */
                bool visited = false;
                for (uint32_t v = 0; v < visited_count; v++)
                {
                    if (visited_states[v] == next_state)
                    {
                        visited = true;
                        break;
                    }
                }

                if (!visited && visited_count < MAX_SEARCH_QUEUE && tail < MAX_SEARCH_QUEUE)
                {
                    visited_states[visited_count++] = next_state;

                    SEARCH_NODE next_node = curr;
                    next_node.state = next_state;
                    next_node.op_indices[next_node.step_count++] = i;
                    next_node.total_cost += op->cost;

                    queue[tail++] = next_node;
                }
            }
        }
    }

    if (!found_plan)
        return 0;

    /* Assemble the plan */
    out_plan->step_count = best_node.step_count;
    for (uint32_t s = 0; s < best_node.step_count; s++)
    {
        uint32_t op_idx = best_node.op_indices[s];
        out_plan->steps[s].op = planner->operators[op_idx];
        out_plan->steps[s].is_executed = false;
        out_plan->steps[s].execution_success = false;

        snprintf(out_plan->steps[s].rationale, sizeof(out_plan->steps[s].rationale),
                 "Execute operator '%s' to assert target state effects",
                 planner->operators[op_idx].name);
    }

    return 1;
}

int AgentPlanAdvance(STRIPS_PLAN *plan, bool step_success)
{
    if (!plan || plan->current_step_idx >= plan->step_count)
        return 0;

    PLAN_STEP *step = &plan->steps[plan->current_step_idx];
    step->is_executed = true;
    step->execution_success = step_success;

    if (step_success)
    {
        plan->current_state = (plan->current_state | step->op.add_effects) & ~step->op.del_effects;
        plan->current_step_idx++;

        if ((plan->current_state & plan->goal_state) == plan->goal_state)
            plan->is_achieved = true;

        return 1;
    }

    return 0;
}

int AgentPlannerReplanOnError(const AGENT_PLANNER *planner,
                               STRIPS_PLAN *plan,
                               const char *error_diagnostic)
{
    if (!planner || !plan)
        return 0;

    /* Update current state to reflect failure, flawed patch retraction, and diagnosis */
    plan->current_state &= ~(PRED_BUILD_VERIFIED | PRED_TESTS_VERIFIED | PRED_PATCH_APPLIED | PRED_PATCH_PREPARED);
    plan->current_state |= PRED_ERROR_DIAGNOSED;

    /* Replan from updated state to the original goal */
    STRIPS_PLAN recovery_plan;
    int rc = AgentPlannerFormulate(planner,
                                   error_diagnostic ? error_diagnostic : "Recovery Plan",
                                   plan->current_state,
                                   plan->goal_state,
                                   &recovery_plan);
    if (!rc)
        return 0;

    /* Replace remaining unexecuted steps with recovery plan steps */
    uint32_t base_idx = plan->current_step_idx;
    uint32_t total = base_idx + recovery_plan.step_count;
    if (total > MAX_PLAN_STEPS)
        total = MAX_PLAN_STEPS;

    for (uint32_t i = 0; i < recovery_plan.step_count && (base_idx + i) < MAX_PLAN_STEPS; i++)
    {
        plan->steps[base_idx + i] = recovery_plan.steps[i];
    }
    plan->step_count = total;

    return 1;
}

/* ============================================================
   Formatting & NLG API
   ============================================================ */

int AgentPlanFormatMarkdown(const STRIPS_PLAN *plan,
                             char *out_buffer,
                             size_t buffer_size)
{
    if (!plan || !out_buffer || buffer_size == 0)
        return 0;

    int offset = snprintf(out_buffer, buffer_size,
                          "### Strategic STRIPS Plan: %s\n"
                          "- **Progress**: %u/%u steps executed | **Status**: %s\n"
                          "- **Execution Schedule**:\n",
                          plan->goal_description[0] ? plan->goal_description : "Code Task",
                          plan->current_step_idx,
                          plan->step_count,
                          plan->is_achieved ? "COMPLETED" : "IN_PROGRESS");

    if (offset < 0 || (size_t)offset >= buffer_size)
        return 0;

    for (uint32_t i = 0; i < plan->step_count && (size_t)offset < buffer_size; i++)
    {
        const PLAN_STEP *s = &plan->steps[i];
        const char *mark = "[ ]";
        const char *cursor = "";

        if (s->is_executed)
            mark = s->execution_success ? "[X]" : "[!]";
        else if (i == plan->current_step_idx)
            cursor = " <-- ACTIVE";

        int written = snprintf(out_buffer + offset, buffer_size - (size_t)offset,
                               "  %u. %s `%s` (%s)%s\n",
                               i + 1, mark, s->op.name, s->op.description, cursor);

        if (written > 0 && (size_t)(offset + written) < buffer_size)
            offset += written;
    }

    return 1;
}
