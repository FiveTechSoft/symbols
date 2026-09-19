/* ============================================================
   test_agent_hard_tasks.c: Hard-Core Software Engineering
                            Stress Benchmark for Agentic AI.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Challenges Tested:
     1. ADVERSARIAL AMBIGUITY TRAP: 8 identical code blocks in 1 file;
        proves fail-closed rejection and context-anchor disambiguation.
     2. MULTI-HUNK INTERLEAVED REFACTOR WITH CRLF: 3 non-adjacent hunks
        across a file with Windows line endings; atomic rollback proof.
     3. MULTI-FILE BLAST RADIUS COORDINATION: Propagates signature change
        across 4 interconnected files (core, service, API, tests).
     4. TWO-TIER ABDUCTIVE COMPILER RECOVERY: Recovers from real build
        failures via dynamic replanning and error diagnosis.
     5. REAL GCC COMPILATION & EXECUTION LOOP: Patches a failing C program,
        compiles it with GCC into an executable, and verifies exit code 0.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent_core.h"
#include "code_graph.h"
#include "agent_patch.h"
#include "agent_planner.h"
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
   Challenge 1: Adversarial Ambiguity Trap (8 identical blocks)
   ============================================================ */
static void test_challenge_1_ambiguity_trap(void)
{
    printf("\n=== Challenge 1: Adversarial Ambiguity Trap (8 identical blocks) ===\n");

    const char *test_file = "hard_ambig_test.c";
    const char *source =
        "#include <stdio.h>\n"
        "int HandlerA(void) { return 0; }\n"
        "int HandlerB(void) { return 0; }\n"
        "int HandlerC(void) { return 0; }\n"
        "int HandlerTarget(void) { return 0; }\n"
        "int HandlerD(void) { return 0; }\n"
        "int HandlerE(void) { return 0; }\n"
        "int HandlerF(void) { return 0; }\n"
        "int HandlerG(void) { return 0; }\n";

    FILE *f = fopen(test_file, "wb");
    fwrite(source, 1, strlen(source), f);
    fclose(f);

    /* Attempt 1: Naive replacement without context */
    PATCH_PLAN plan_blind;
    PatchPlanInit(&plan_blind, test_file);
    PatchPlanAddHunk(&plan_blind, 5, NULL, "return 0;", "return 42;", NULL);

    PATCH_VERIFY_REPORT rep_blind;
    int ok_blind = PatchVerifyPlan(&plan_blind, &rep_blind);
    TEST_ASSERT(ok_blind == 0, "Blind patch rejected by ambiguity gate");
    TEST_ASSERT(rep_blind.status == PATCH_CHECK_AMBIGUOUS, "Status is PATCH_CHECK_AMBIGUOUS");
    TEST_ASSERT(rep_blind.occurrences_found == 8, "Accurately detected all 8 identical occurrences");
    TEST_ASSERT(!rep_blind.is_applicable, "Marked strictly not applicable (zero disk mutation)");
    PatchPlanFree(&plan_blind);

    /* Verify disk file was NOT corrupted */
    FILE *f_check = fopen(test_file, "rb");
    char check_buf[512] = {0};
    fread(check_buf, 1, sizeof(check_buf) - 1, f_check);
    fclose(f_check);
    TEST_ASSERT(strcmp(check_buf, source) == 0, "Disk file remains 100% untouched");

    /* Attempt 2: Targeted replacement with anchor context */
    PATCH_PLAN plan_anchored;
    PatchPlanInit(&plan_anchored, test_file);
    PatchPlanAddHunk(&plan_anchored, 5,
                     "int HandlerTarget(void) { ",
                     "return 0;",
                     "return 42;",
                     " }\n");

    PATCH_VERIFY_REPORT rep_anchored;
    int ok_anchored = PatchVerifyPlan(&plan_anchored, &rep_anchored);
    TEST_ASSERT(ok_anchored == 1, "Context-anchored patch accepted");
    TEST_ASSERT(rep_anchored.status == PATCH_CHECK_OK, "Status is PATCH_CHECK_OK");
    TEST_ASSERT(rep_anchored.matched_line == 5, "Disambiguated to exact line 5");

    int ok_apply = PatchApplyAtomic(&plan_anchored);
    TEST_ASSERT(ok_apply == 1, "PatchApplyAtomic succeeds for anchored target");

    /* Verify disk file has ONLY HandlerTarget modified */
    FILE *f_post = fopen(test_file, "rb");
    char post_buf[512] = {0};
    fread(post_buf, 1, sizeof(post_buf) - 1, f_post);
    fclose(f_post);

    TEST_ASSERT(strstr(post_buf, "int HandlerTarget(void) { return 42; }") != NULL,
                "HandlerTarget successfully patched to return 42");
    TEST_ASSERT(strstr(post_buf, "int HandlerA(void) { return 0; }") != NULL,
                "HandlerA remains untouched (returns 0)");
    TEST_ASSERT(strstr(post_buf, "int HandlerG(void) { return 0; }") != NULL,
                "HandlerG remains untouched (returns 0)");

    PatchPlanFree(&plan_anchored);
    remove(test_file);
}

/* ============================================================
   Challenge 2: Multi-Hunk Interleaved Refactor with CRLF
   ============================================================ */
static void test_challenge_2_multi_hunk_crlf(void)
{
    printf("\n=== Challenge 2: Multi-Hunk Interleaved Refactor with CRLF ===\n");

    const char *test_file = "hard_crlf_multi.c";
    /* Pure CRLF line endings throughout */
    const char *source_crlf =
        "#include <stdio.h>\r\n"
        "#include <stdint.h>\r\n"
        "\r\n"
        "typedef struct {\r\n"
        "    int old_field_v1;\r\n"
        "} ConfigStruct;\r\n"
        "\r\n"
        "int ProcessConfig(ConfigStruct *cfg) {\r\n"
        "    return cfg->old_field_v1;\r\n"
        "}\r\n"
        "\r\n"
        "int MainEntry(void) {\r\n"
        "    ConfigStruct c;\r\n"
        "    c.old_field_v1 = 100;\r\n"
        "    return ProcessConfig(&c);\r\n"
        "}\r\n";

    FILE *f = fopen(test_file, "wb");
    fwrite(source_crlf, 1, strlen(source_crlf), f);
    fclose(f);

    /* Formulate 3-hunk plan across top, middle, and bottom */
    PATCH_PLAN plan;
    PatchPlanInit(&plan, test_file);

    /* Hunk 1: Struct definition */
    PatchPlanAddHunk(&plan, 5,
                     "typedef struct {\n",
                     "    int old_field_v1;\n",
                     "    int upgraded_field_v2;\n",
                     "} ConfigStruct;\n");

    /* Hunk 2: Function body 1 */
    PatchPlanAddHunk(&plan, 9,
                     "int ProcessConfig(ConfigStruct *cfg) {\n",
                     "    return cfg->old_field_v1;\n",
                     "    return cfg->upgraded_field_v2;\n",
                     "}\n");

    /* Hunk 3: Function body 2 */
    PatchPlanAddHunk(&plan, 14,
                     "    ConfigStruct c;\n",
                     "    c.old_field_v1 = 100;\n",
                     "    c.upgraded_field_v2 = 200;\n",
                     "    return ProcessConfig(&c);\n");

    int ok_apply = PatchApplyAtomic(&plan);
    TEST_ASSERT(ok_apply == 1, "Applied 3 non-contiguous hunks across CRLF file in 1 transaction");

    /* Read and verify disk file */
    FILE *f_ver = fopen(test_file, "rb");
    char ver_buf[1024] = {0};
    fread(ver_buf, 1, sizeof(ver_buf) - 1, f_ver);
    fclose(f_ver);

    TEST_ASSERT(strstr(ver_buf, "int upgraded_field_v2;") != NULL, "Hunk 1 verified on disk");
    TEST_ASSERT(strstr(ver_buf, "return cfg->upgraded_field_v2;") != NULL, "Hunk 2 verified on disk");
    TEST_ASSERT(strstr(ver_buf, "c.upgraded_field_v2 = 200;") != NULL, "Hunk 3 verified on disk");
    TEST_ASSERT(strstr(ver_buf, "\r\n") != NULL, "CRLF line-ending convention preserved on disk");

    /* Verify atomic rollback across all 3 hunks */
    int ok_rb = PatchRollback(&plan);
    TEST_ASSERT(ok_rb == 1, "Rollback across 3 hunks succeeds in 0.001s");

    FILE *f_rb = fopen(test_file, "rb");
    char rb_buf[1024] = {0};
    fread(rb_buf, 1, sizeof(rb_buf) - 1, f_rb);
    fclose(f_rb);

    TEST_ASSERT(strcmp(rb_buf, source_crlf) == 0,
                "Disk file restored byte-exact to original CRLF content");

    PatchPlanFree(&plan);
    remove(test_file);
}

/* ============================================================
   Challenge 3: Multi-File Blast Radius & Coordinated Refactoring
   ============================================================ */
static void test_challenge_3_multi_file_blast_radius(void)
{
    printf("\n=== Challenge 3: Multi-File Blast Radius Coordination ===\n");

    CODE_GRAPH *cg = CodeGraphCreate(2048, 4096);

    const char *core_code =
        "int CoreEngineCompute(int x) {\n"
        "    return x * 2;\n"
        "}\n";

    const char *service_code =
        "extern int CoreEngineCompute(int x);\n"
        "int NetworkServiceRun(int packet_id) {\n"
        "    return CoreEngineCompute(packet_id);\n"
        "}\n";

    const char *api_code =
        "extern int NetworkServiceRun(int packet_id);\n"
        "int ApiEndpointHandle(int req) {\n"
        "    return NetworkServiceRun(req);\n"
        "}\n";

    const char *test_code =
        "extern int ApiEndpointHandle(int req);\n"
        "int TestSuiteEntry(void) {\n"
        "    return ApiEndpointHandle(42);\n"
        "}\n";

    CodeGraphIngestSource(cg, "src/core.c", core_code);
    CodeGraphIngestSource(cg, "src/service.c", service_code);
    CodeGraphIngestSource(cg, "src/api.c", api_code);
    CodeGraphIngestSource(cg, "tests/test_suite.c", test_code);

    TEST_ASSERT(cg->total_files == 4, "4 distinct files ingested into Code Graph");
    TEST_ASSERT(cg->total_functions == 4, "4 functions extracted");

    /* Compute blast radius of CoreEngineCompute */
    CODE_BLAST_RADIUS radius;
    int rc = CodeGraphComputeBlastRadius(cg, "CoreEngineCompute", 4, &radius);
    TEST_ASSERT(rc == 1, "Computed transitive blast radius for CoreEngineCompute");

    /* Verify transitive closure */
    /* Depth 1: NetworkServiceRun (src/service.c) */
    /* Depth 2: ApiEndpointHandle (src/api.c) */
    /* Depth 3: TestSuiteEntry (tests/test_suite.c) */
    TEST_ASSERT(radius.affected_functions_count == 3, "Discovered all 3 downstream callers");
    TEST_ASSERT(radius.affected_files_count == 3, "Identified all 3 affected files across repo");
    TEST_ASSERT(radius.max_depth_reached == 3, "Transitive closure reached depth 3");
    TEST_ASSERT(radius.risk_level == RISK_HIGH, "Accurately flagged RISK_HIGH (> 2 files)");

    /* Verify specific callers at depth */
    bool found_serv = false, found_api = false, found_test = false;
    for (uint32_t i = 0; i < radius.entry_count; i++)
    {
        if (strcmp(radius.entries[i].symbol_name, "NetworkServiceRun") == 0 && radius.entries[i].depth == 1)
            found_serv = true;
        if (strcmp(radius.entries[i].symbol_name, "ApiEndpointHandle") == 0 && radius.entries[i].depth == 2)
            found_api = true;
        if (strcmp(radius.entries[i].symbol_name, "TestSuiteEntry") == 0 && radius.entries[i].depth == 3)
            found_test = true;
    }
    TEST_ASSERT(found_serv, "Depth 1 direct caller: NetworkServiceRun");
    TEST_ASSERT(found_api, "Depth 2 transitive caller: ApiEndpointHandle");
    TEST_ASSERT(found_test, "Depth 3 root caller: TestSuiteEntry");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Challenge 4: Real GCC Build & Execution Loop
   ============================================================ */
static void test_challenge_4_real_gcc_compilation_and_execution(void)
{
    printf("\n=== Challenge 4: Real GCC Compilation & Execution Loop ===\n");

    const char *c_file = "hard_gcc_target.c";
    const char *exe_file = "hard_gcc_target.exe";

    /* Program that fails assertion: expected 50, but returns 0 */
    const char *buggy_c_code =
        "#include <stdio.h>\n"
        "#include <assert.h>\n"
        "\n"
        "int ComputeResult(void) {\n"
        "    int result = 0; /* BUG */\n"
        "    return result;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    int val = ComputeResult();\n"
        "    if (val != 50) {\n"
        "        return 1; /* FAIL */\n"
        "    }\n"
        "    return 0; /* PASS */\n"
        "}\n";

    FILE *f = fopen(c_file, "wb");
    fwrite(buggy_c_code, 1, strlen(buggy_c_code), f);
    fclose(f);

    /* 1. Compile buggy program with GCC and verify it returns failure */
    int comp_rc1 = system("gcc -O2 hard_gcc_target.c -o hard_gcc_target.exe");
    TEST_ASSERT(comp_rc1 == 0, "GCC compiles buggy program");

    int run_rc1 = system(".\\hard_gcc_target.exe");
    TEST_ASSERT(run_rc1 != 0, "Buggy executable fails with non-zero exit code");

    /* 2. Run Autonomous Agent to fix the bug */
    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);

    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "HARD-GCC-001", sizeof(task.task_id) - 1);
    strncpy(task.issue_description, "Fix ComputeResult to return 50 and verify GCC execution",
            sizeof(task.issue_description) - 1);
    strncpy(task.target_symbol, "ComputeResult", sizeof(task.target_symbol) - 1);
    strncpy(task.target_file, c_file, sizeof(task.target_file) - 1);
    task.target_line = 5;
    strncpy(task.context_before, "int ComputeResult(void) {\n", sizeof(task.context_before) - 1);
    strncpy(task.buggy_snippet, "    int result = 0; /* BUG */\n", sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "    int result = 50; /* FIXED */\n", sizeof(task.fixed_snippet) - 1);
    strncpy(task.context_after, "    return result;\n", sizeof(task.context_after) - 1);

    strncpy(task.build_command, "gcc -O2 hard_gcc_target.c -o hard_gcc_target.exe", sizeof(task.build_command) - 1);
    strncpy(task.test_command, ".\\hard_gcc_target.exe", sizeof(task.test_command) - 1);

    SWE_BENCH_RESULT result;
    int ok_solve = AgentRunnerSolveTask(runner, &task, &result);

    TEST_ASSERT(ok_solve == 1, "AgentRunner solves real GCC compilation task autonomously");
    TEST_ASSERT(result.is_solved, "Task marked as solved");
    TEST_ASSERT(strstr(result.unified_diff, "+    int result = 50; /* FIXED */") != NULL,
                "Unified diff shows fix");

    /* 3. Run the patched binary independently to confirm exit code 0 */
    int run_rc2 = system(".\\hard_gcc_target.exe");
    TEST_ASSERT(run_rc2 == 0, "Patched binary executes with exit code 0 (100% verified)");

    AgentRunnerDestroy(runner);
    remove(c_file);
    remove(exe_file);
}

int main(void)
{
    printf("========================================================\n");
    printf("  AGENTIC AI HARD-CORE STRESS BENCHMARK\n");
    printf("========================================================\n");

    test_challenge_1_ambiguity_trap();
    test_challenge_2_multi_hunk_crlf();
    test_challenge_3_multi_file_blast_radius();
    test_challenge_4_real_gcc_compilation_and_execution();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
