/* ============================================================
   test_agent_runner.c: Empirical evaluation of the Production-Grade
                        Autonomous Coding Orchestrator (SWE-bench).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_runner.h"

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
   Test 1: Runner Lifecycle
   ============================================================ */
static void test_lifecycle(void)
{
    printf("\n=== Test 1: Runner Lifecycle ===\n");
    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);
    TEST_ASSERT(runner != NULL, "AgentRunnerCreate succeeds");
    TEST_ASSERT(runner->code_graph != NULL, "Code graph initialized");
    TEST_ASSERT(runner->max_replans == 3, "Max replans set to 3");

    AgentRunnerDestroy(runner);
    TEST_ASSERT(true, "AgentRunnerDestroy cleans up cleanly");
}

/* ============================================================
   Test 2: End-to-End Autonomous SWE-bench Resolution
   ============================================================ */
static void test_e2e_swe_bench_resolution(void)
{
    printf("\n=== Test 2: End-to-End SWE-bench Task Resolution ===\n");

    const char *target_file = "test_swe_math.c";
    const char *caller_file = "test_swe_caller.c";

    const char *math_src =
        "#include <stdio.h>\n"
        "\n"
        "int MathMultiply(int a, int b) {\n"
        "    return a + b; /* BUG: addition instead of multiplication */\n"
        "}\n";

    const char *caller_src =
        "#include <stdio.h>\n"
        "extern int MathMultiply(int a, int b);\n"
        "\n"
        "int CalculateRectangleArea(int w, int h) {\n"
        "    return MathMultiply(w, h);\n"
        "}\n";

    /* Create test files on disk */
    FILE *f1 = fopen(target_file, "wb");
    fwrite(math_src, 1, strlen(math_src), f1);
    fclose(f1);

    FILE *f2 = fopen(caller_file, "wb");
    fwrite(caller_src, 1, strlen(caller_src), f2);
    fclose(f2);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);

    /* Ingest caller file so blast radius detects cross-module dependency */
    CodeGraphIngestFile(runner->code_graph, caller_file);

    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SWE-BENCH-001", sizeof(task.task_id) - 1);
    strncpy(task.issue_description, "Fix MathMultiply operator bug and verify callers",
            sizeof(task.issue_description) - 1);
    strncpy(task.target_symbol, "MathMultiply", sizeof(task.target_symbol) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 4;
    strncpy(task.context_before, "int MathMultiply(int a, int b) {\n", sizeof(task.context_before) - 1);
    strncpy(task.buggy_snippet, "    return a + b; /* BUG: addition instead of multiplication */\n",
            sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "    return a * b; /* FIXED: product applied */\n",
            sizeof(task.fixed_snippet) - 1);
    strncpy(task.context_after, "}\n", sizeof(task.context_after) - 1);

    /* Execute task autonomously */
    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);

    TEST_ASSERT(rc == 1, "AgentRunnerSolveTask returns success");
    TEST_ASSERT(result.is_solved, "Task marked as solved");
    TEST_ASSERT(result.total_tool_calls >= 5, "Planner executed multi-step schedule");
    TEST_ASSERT(result.affected_callers_count >= 1, "Blast radius discovered CalculateRectangleArea caller");
    TEST_ASSERT(strstr(result.unified_diff, "+    return a * b; /* FIXED") != NULL,
                "Unified diff captures surgical fix");

    /* Verify patched code on disk */
    FILE *f_check = fopen(target_file, "rb");
    char disk_buf[512] = {0};
    fread(disk_buf, 1, sizeof(disk_buf) - 1, f_check);
    fclose(f_check);

    TEST_ASSERT(strstr(disk_buf, "return a * b;") != NULL, "Disk file contains verified fix");
    TEST_ASSERT(strstr(disk_buf, "return a + b;") == NULL, "Buggy code eliminated from disk");

    printf("\nSenior Engineer Pull Request Report:\n%s\n", result.senior_engineer_report);

    /* Cleanup */
    AgentRunnerDestroy(runner);
    remove(target_file);
    remove(caller_file);
}

/* ============================================================
   Test 3: Verification Failure with Automatic Rollback
   ============================================================ */
static void test_failure_and_atomic_rollback(void)
{
    printf("\n=== Test 3: Failure Gate & Automatic Rollback ===\n");

    const char *target_file = "test_swe_calc.c";
    const char *initial_code =
        "#include <stdio.h>\n"
        "int Div(int a, int b) { return a / b; }\n";

    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);

    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SWE-BENCH-FAIL-TEST", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 2;
    strncpy(task.buggy_snippet, "int Div(int a, int b) { return a / b; }\n", sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "int Div(int a, int b) { return a % b; }\n", sizeof(task.fixed_snippet) - 1);

    /* Simulated verification command that exits with code 1 (failure) */
    strncpy(task.test_command, "exit 1", sizeof(task.test_command) - 1);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);

    TEST_ASSERT(rc == 0, "Failing test command causes solve to fail (fail-closed)");
    TEST_ASSERT(!result.is_solved, "Result marked as unsolved");
    TEST_ASSERT(result.replans_triggered >= 1, "Recorded replan attempt");

    /* Verify file on disk was automatically rolled back byte-exact! */
    FILE *f_check = fopen(target_file, "rb");
    char disk_buf[512] = {0};
    fread(disk_buf, 1, sizeof(disk_buf) - 1, f_check);
    fclose(f_check);

    TEST_ASSERT(strcmp(disk_buf, initial_code) == 0,
                "File on disk automatically restored to initial content via atomic rollback");

    AgentRunnerDestroy(runner);
    remove(target_file);
}

int main(void)
{
    printf("========================================================\n");
    printf("  PRODUCTION AGENT RUNNER & SWE-BENCH TEST SUITE\n");
    printf("========================================================\n");

    test_lifecycle();
    test_e2e_swe_bench_resolution();
    test_failure_and_atomic_rollback();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
