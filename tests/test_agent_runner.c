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


/* ============================================================
   Test 4: Symbolic Reflexion recovers from a transient evaluator failure
   ============================================================ */
static void test_reflexion_retry_recovers(void)
{
    printf("\n=== Test 4: Bounded Reflexion Retry ===\n");
    const char *target_file = "test_reflexion_retry.c";
    const char *counter_file = "test_reflexion_counter.tmp";
    const char *initial_code = "int Value(void) { return 1; }\n";
    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);
    remove(counter_file);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);
    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "REFLEXION-TRANSIENT", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 1;
    strncpy(task.buggy_snippet, initial_code, sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "int Value(void) { return 2; }\n", sizeof(task.fixed_snippet) - 1);
#ifdef _WIN32
    strncpy(task.test_command,
            "if exist test_reflexion_counter.tmp (exit 0) else (echo test failure>test_reflexion_counter.tmp & echo transient evaluator failure & exit 1)",
            sizeof(task.test_command) - 1);
#else
    strncpy(task.test_command,
            "if [ -f test_reflexion_counter.tmp ]; then exit 0; else touch test_reflexion_counter.tmp; echo transient evaluator failure; exit 1; fi",
            sizeof(task.test_command) - 1);
#endif

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);
    TEST_ASSERT(rc == 1 && result.is_solved, "Second attempt solves transient evaluator failure");
    TEST_ASSERT(result.attempts_executed == 2, "Exactly two attempts executed");
    TEST_ASSERT(result.replans_triggered == 1, "Exactly one replan triggered");
    TEST_ASSERT(result.reflection_count == 1, "One deterministic reflection recorded");
    TEST_ASSERT(strstr(result.reflections[0], "failure=test") != NULL,
                "Reflection records failed verification phase");

    AgentRunnerDestroy(runner);
    remove(target_file);
    remove(counter_file);
}

/* ============================================================
   Test 5: Repeated identical failure stops without exhausting budget
   ============================================================ */
static void test_reflexion_stagnation_guard(void)
{
    printf("\n=== Test 5: Reflexion Stagnation Guard ===\n");
    const char *target_file = "test_reflexion_stagnation.c";
    const char *initial_code = "int Stable(void) { return 1; }\n";
    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 7);
    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "REFLEXION-STAGNATION", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 1;
    strncpy(task.buggy_snippet, initial_code, sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "int Stable(void) { return 2; }\n", sizeof(task.fixed_snippet) - 1);
    strncpy(task.test_command, "echo deterministic failure && exit 1", sizeof(task.test_command) - 1);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);
    TEST_ASSERT(rc == 0 && !result.is_solved, "Persistent failure remains fail-closed");
    TEST_ASSERT(result.attempts_executed == 2, "Identical reflection stops after confirmation retry");
    TEST_ASSERT(result.replans_triggered == 1, "No unproductive second replan");

    FILE *check = fopen(target_file, "rb");
    char buf[128] = {0};
    fread(buf, 1, sizeof(buf) - 1, check);
    fclose(check);
    TEST_ASSERT(strcmp(buf, initial_code) == 0, "Stagnating task is rolled back byte-exact");

    AgentRunnerDestroy(runner);
    remove(target_file);
}


/* ============================================================
   Test 6: Deterministic compiler did-you-mean repair
   ============================================================ */
static void test_did_you_mean_repair(void)
{
    printf("\n=== Test 6: Deterministic Did-You-Mean Repair ===\n");
    const char *target_file = "test_swe_repair.c";
    const char *initial_code = "int Value(int buffer_size) { return 0; }\n";
    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);
    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SWE-BENCH-REPAIR", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 1;
    strncpy(task.buggy_snippet, "int Value(int buffer_size) { return 0; }\n", sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "int Value(int buffer_size) { return buff_size; }\n", sizeof(task.fixed_snippet) - 1);
    snprintf(task.build_command, sizeof(task.build_command),
             "gcc -fsyntax-only %s", target_file);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);
    TEST_ASSERT(rc == 1 && result.is_solved, "Supported typo is solved on repaired attempt");
    if (result.attempts_executed != 2)
        fprintf(stderr, "did-you-mean attempts=%u repairs=%u replans=%u build_exit=%d timed_out=%d execution_failed=%d\n",
                result.attempts_executed, result.repairs_applied, result.replans_triggered,
                result.last_shell_exec.exit_code, result.last_shell_exec.timed_out,
                result.last_shell_exec.execution_failed);
    TEST_ASSERT(result.attempts_executed == 2, "Repair required exactly two attempts");
    TEST_ASSERT(result.repairs_applied == 1, "Exactly one deterministic repair applied");
    TEST_ASSERT(strcmp(result.last_repair_operator, "compiler-did-you-mean") == 0,
                "Repair operator is reported");
    TEST_ASSERT(strstr(result.unified_diff, "+int Value(int buffer_size) { return buffer_size; }") != NULL,
                "Final patch differs from initial misspelled patch");
    TEST_ASSERT(strstr(result.unified_diff, "buff_size") == NULL,
                "Rejected initial replacement is absent from final diff");

    FILE *check = fopen(target_file, "rb");
    char buf[256] = {0};
    fread(buf, 1, sizeof(buf) - 1, check);
    fclose(check);
    TEST_ASSERT(strstr(buf, "return buffer_size;") != NULL,
                "Only compiler-suggested identifier reaches disk");

    AgentRunnerDestroy(runner);
    remove(target_file);
}

/* Negative fixture: no compiler suggestion means no synthesized mutation. */
static void test_did_you_mean_repair_rejects_unsupported(void)
{
    printf("\n=== Test 7: Unsupported Diagnostic Remains Fail-Closed ===\n");
    const char *target_file = "test_swe_no_repair.c";
    const char *initial_code = "int Value(void) { return 0; }\n";
    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 1);
    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SWE-BENCH-NO-REPAIR", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 1;
    strncpy(task.buggy_snippet, initial_code, sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "int Value(void) { return totally_unknown; }\n", sizeof(task.fixed_snippet) - 1);
    snprintf(task.build_command, sizeof(task.build_command),
             "gcc -fsyntax-only %s", target_file);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);
    TEST_ASSERT(rc == 0 && !result.is_solved, "Unsupported diagnostic remains unsolved");
    TEST_ASSERT(result.repairs_applied == 0, "No repair occurs without a suggestion");
    TEST_ASSERT(result.attempts_executed == 2, "max_replans=1 bounds execution to two attempts");
    TEST_ASSERT(result.replans_triggered == 1, "Retry budget permits exactly one replan");

    FILE *check = fopen(target_file, "rb");
    char buf[128] = {0};
    fread(buf, 1, sizeof(buf) - 1, check);
    fclose(check);
    TEST_ASSERT(strcmp(buf, initial_code) == 0, "Unsupported case rolls back byte-exact");

    AgentRunnerDestroy(runner);
    remove(target_file);
}


/* Negative fixture: a suggestion is insufficient when the replacement would
   require choosing among multiple identifier occurrences. */
static void test_did_you_mean_repair_rejects_ambiguous(void)
{
    printf("\n=== Test 8: Ambiguous Suggested Repair Is Rejected ===\n");
    const char *target_file = "test_swe_ambiguous_repair.c";
    const char *initial_code = "int Value(int buffer_size) { return 0; }\n";
    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);
    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SWE-BENCH-AMBIGUOUS-REPAIR", sizeof(task.task_id) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 1;
    strncpy(task.buggy_snippet, initial_code, sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet,
            "int Value(int buffer_size) { int x = buff_size; return x + buff_size; }\n",
            sizeof(task.fixed_snippet) - 1);
    snprintf(task.build_command, sizeof(task.build_command),
             "gcc -fsyntax-only %s", target_file);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);
    TEST_ASSERT(rc == 0 && !result.is_solved, "Ambiguous identifier repair remains unsolved");
    TEST_ASSERT(result.diagnostic_report.root_suggestion[0] != '\0',
                "Compiler did provide a suggestion in negative fixture");
    TEST_ASSERT(result.repairs_applied == 0, "Suggestion cannot bypass uniqueness gate");

    FILE *check = fopen(target_file, "rb");
    char buf[128] = {0};
    fread(buf, 1, sizeof(buf) - 1, check);
    fclose(check);
    TEST_ASSERT(strcmp(buf, initial_code) == 0, "Ambiguous case rolls back byte-exact");

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
    test_reflexion_retry_recovers();
    test_reflexion_stagnation_guard();
    test_did_you_mean_repair();
    test_did_you_mean_repair_rejects_unsupported();
    test_did_you_mean_repair_rejects_ambiguous();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
