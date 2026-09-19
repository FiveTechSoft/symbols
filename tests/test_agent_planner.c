/* ============================================================
   test_agent_planner.c: Empirical evaluation of the Goal-Directed
                         STRIPS Task Planner & Dynamic Replanner.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_planner.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* ============================================================
   Test 1: Planner Initialization & Operator Registry
   ============================================================ */
static void test_planner_init(void)
{
    printf("\n=== Test 1: Planner Initialization & Operators ===\n");
    AGENT_PLANNER planner;
    int rc = AgentPlannerInit(&planner);
    TEST_ASSERT(rc == 1, "AgentPlannerInit succeeds");
    TEST_ASSERT(planner.operator_count == 8, "8 standard STRIPS operators registered");

    /* Verify essential operator properties */
    TEST_ASSERT(strcmp(planner.operators[0].name, "locate_symbol") == 0, "Op 0 is locate_symbol");
    TEST_ASSERT(strcmp(planner.operators[1].name, "inspect_code") == 0, "Op 1 is inspect_code");
    TEST_ASSERT(strcmp(planner.operators[3].name, "diagnose_error") == 0, "Op 3 is diagnose_error");
    TEST_ASSERT(strcmp(planner.operators[5].name, "apply_patch") == 0, "Op 5 is apply_patch");
    TEST_ASSERT(strcmp(planner.operators[6].name, "verify_build") == 0, "Op 6 is verify_build");
}

/* ============================================================
   Test 2: Optimal Forward Plan Formulation
   ============================================================ */
static void test_forward_planning(void)
{
    printf("\n=== Test 2: Optimal Forward Plan Formulation ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    /* Goal: Given known symbol, achieve verified regression tests and task completion */
    uint32_t init_state = PRED_SYMBOL_KNOWN;
    uint32_t goal_state = PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED;

    STRIPS_PLAN plan;
    int rc = AgentPlannerFormulate(&planner, "Refactor OldFunction and verify tests",
                                   init_state, goal_state, &plan);

    TEST_ASSERT(rc == 1, "AgentPlannerFormulate finds valid plan");
    TEST_ASSERT(plan.step_count == 7, "Optimal path found in exactly 7 steps");
    TEST_ASSERT(strcmp(plan.steps[0].op.name, "locate_symbol") == 0, "Step 1: locate_symbol");
    TEST_ASSERT(strcmp(plan.steps[1].op.name, "inspect_code") == 0, "Step 2: inspect_code");
    TEST_ASSERT(strcmp(plan.steps[2].op.name, "analyze_blast_radius") == 0, "Step 3: analyze_blast_radius");
    TEST_ASSERT(strcmp(plan.steps[3].op.name, "prepare_surgical_patch") == 0, "Step 4: prepare_surgical_patch");
    TEST_ASSERT(strcmp(plan.steps[4].op.name, "apply_patch") == 0, "Step 5: apply_patch");
    TEST_ASSERT(strcmp(plan.steps[5].op.name, "verify_build") == 0, "Step 6: verify_build");
    TEST_ASSERT(strcmp(plan.steps[6].op.name, "run_regression_tests") == 0, "Step 7: run_regression_tests");

    /* Format markdown schedule */
    char schedule[2048];
    int frc = AgentPlanFormatMarkdown(&plan, schedule, sizeof(schedule));
    TEST_ASSERT(frc == 1, "Formatted plan into markdown schedule");
    TEST_ASSERT(strstr(schedule, "Strategic STRIPS Plan") != NULL, "Schedule has title");
    TEST_ASSERT(strstr(schedule, "1. [ ] `locate_symbol`") != NULL, "Schedule lists steps");
    printf("Initial Plan Schedule:\n%s\n", schedule);
}

/* ============================================================
   Test 3: Plan Execution and Step Advancement
   ============================================================ */
static void test_plan_execution(void)
{
    printf("\n=== Test 3: Plan Execution & State Transitions ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;
    AgentPlannerFormulate(&planner, "Step-by-step Execution Task",
                          PRED_SYMBOL_KNOWN, PRED_BUILD_VERIFIED, &plan);

    TEST_ASSERT(plan.step_count == 6, "Plan to build verification is 6 steps");
    TEST_ASSERT(!plan.is_achieved, "Plan starts unachieved");

    /* Step 1: locate_symbol */
    int rc1 = AgentPlanAdvance(&plan, true);
    TEST_ASSERT(rc1 == 1, "Step 1 executed successfully");
    TEST_ASSERT((plan.current_state & PRED_FILE_LOCATED) != 0, "File located in world state");

    /* Step 2: inspect_code */
    int rc2 = AgentPlanAdvance(&plan, true);
    TEST_ASSERT(rc2 == 1, "Step 2 executed successfully");
    TEST_ASSERT((plan.current_state & PRED_CODE_INSPECTED) != 0, "Code inspected in world state");

    /* Execute remaining steps to completion */
    while (plan.current_step_idx < plan.step_count)
    {
        AgentPlanAdvance(&plan, true);
    }

    TEST_ASSERT(plan.is_achieved, "Plan goal satisfied upon executing all steps");
    TEST_ASSERT((plan.current_state & PRED_BUILD_VERIFIED) != 0, "Build verified in final state");
}

/* ============================================================
   Test 4: Dynamic Replanning on Verification Failure
   ============================================================ */
static void test_dynamic_replanning(void)
{
    printf("\n=== Test 4: Dynamic Replanning on Verification Failure ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;
    AgentPlannerFormulate(&planner, "Self-Healing Bug Fix Task",
                          PRED_SYMBOL_KNOWN,
                          PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED,
                          &plan);

    /* Advance first 5 steps (up to apply_patch) */
    for (int i = 0; i < 5; i++)
    {
        AgentPlanAdvance(&plan, true);
    }

    /* Step 6: verify_build fails with exit code != 0! */
    TEST_ASSERT(strcmp(plan.steps[plan.current_step_idx].op.name, "verify_build") == 0,
                "Currently at verify_build step");
    int rc_fail = AgentPlanAdvance(&plan, false);
    TEST_ASSERT(rc_fail == 0, "verify_build records failure cleanly");

    /* Trigger dynamic replanning with compilation error diagnostic */
    int rc_replan = AgentPlannerReplanOnError(&planner, &plan,
                                              "gcc: error: undefined reference to 'NewFunc'");
    TEST_ASSERT(rc_replan == 1, "Dynamic replanning succeeds");

    /* Verify recovery schedule contains diagnosis and re-verification */
    char recovery_schedule[2048];
    AgentPlanFormatMarkdown(&plan, recovery_schedule, sizeof(recovery_schedule));
    printf("Replanned Recovery Schedule:\n%s\n", recovery_schedule);

    TEST_ASSERT(strstr(recovery_schedule, "`diagnose_error`") != NULL,
                "Recovery plan inserts diagnose_error step");
    TEST_ASSERT(strstr(recovery_schedule, "`verify_build`") != NULL,
                "Recovery plan re-inserts verify_build step");

    /* Execute through recovery steps to completion */
    while (plan.current_step_idx < plan.step_count)
    {
        AgentPlanAdvance(&plan, true);
    }
    TEST_ASSERT(plan.is_achieved, "Goal achieved through automated replanning loop");
}

/* ============================================================
   Test 5: Fail-Closed Boundaries & Trivial Goals
   ============================================================ */
static void test_fail_closed_boundaries(void)
{
    printf("\n=== Test 5: Fail-Closed Boundaries ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;

    /* Trivial goal: already in goal state */
    int rc_triv = AgentPlannerFormulate(&planner, "Trivial Task",
                                        PRED_TASK_COMPLETED, PRED_TASK_COMPLETED, &plan);
    TEST_ASSERT(rc_triv == 1, "Trivial goal formulation succeeds");
    TEST_ASSERT(plan.is_achieved, "Trivial goal marked achieved immediately");
    TEST_ASSERT(plan.step_count == 0, "Trivial goal requires 0 steps");

    /* Impossible goal with zero initial state */
    uint32_t impossible_goal = (1 << 30);
    int rc_imp = AgentPlannerFormulate(&planner, "Impossible Task", 0, impossible_goal, &plan);
    TEST_ASSERT(rc_imp == 0, "Unsatisfiable goal fails closed without infinite loop");

    /* Robustness checks */
    TEST_ASSERT(AgentPlannerInit(NULL) == 0, "NULL planner rejected");
    TEST_ASSERT(AgentPlannerFormulate(NULL, "", 0, 0, &plan) == 0, "NULL planner in formulate rejected");
    TEST_ASSERT(AgentPlanAdvance(NULL, true) == 0, "NULL plan in advance rejected");
}

int main(void)
{
    printf("========================================================\n");
    printf("  GOAL-DIRECTED STRIPS TASK PLANNER TEST SUITE\n");
    printf("========================================================\n");

    test_planner_init();
    test_forward_planning();
    test_plan_execution();
    test_dynamic_replanning();
    test_fail_closed_boundaries();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
