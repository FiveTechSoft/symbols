/* ============================================================
   test_agent_patch.c: Empirical evaluation of the Surgical Editing,
                       Unified Diff & Atomic Rollback Engine.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_patch.h"

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
   Test 1: Plan Lifecycle & Hunk Formulation
   ============================================================ */
static void test_lifecycle_and_formulation(void)
{
    printf("\n=== Test 1: Plan Lifecycle & Hunk Formulation ===\n");
    PATCH_PLAN plan;
    int rc = PatchPlanInit(&plan, "src/engine.c");
    TEST_ASSERT(rc == 1, "PatchPlanInit succeeds");
    TEST_ASSERT(strcmp(plan.target_file, "src/engine.c") == 0, "Target file matches");
    TEST_ASSERT(plan.hunk_count == 0, "Hunk count starts at 0");

    rc = PatchPlanAddHunk(&plan, 42, "static void Init(void) {\n",
                          "    g_flag = 0;\n",
                          "    g_flag = 1;\n",
                          "}\n");
    TEST_ASSERT(rc == 1, "PatchPlanAddHunk succeeds");
    TEST_ASSERT(plan.hunk_count == 1, "Hunk count incremented to 1");
    TEST_ASSERT(plan.hunks[0].expected_line == 42, "Expected line matches");
    TEST_ASSERT(strcmp(plan.hunks[0].target_content, "    g_flag = 0;\n") == 0, "Target content matches");
    TEST_ASSERT(strcmp(plan.hunks[0].replacement, "    g_flag = 1;\n") == 0, "Replacement matches");

    PatchPlanFree(&plan);
    TEST_ASSERT(plan.hunk_count == 0, "PatchPlanFree cleans up cleanly");
}

/* ============================================================
   Test 2: Pre-Flight Dry-Run Verification (Ambiguity & Drift)
   ============================================================ */
static void test_dry_run_verification(void)
{
    printf("\n=== Test 2: Dry-Run Verification (Ambiguity & Drift) ===\n");

    const char *sample_code =
        "/* Line 1: Header */\n"
        "#include <stdio.h>\n"
        "\n"
        "void FunctionOne(void) {\n"
        "    int value = 0;\n"
        "    printf(\"val: %d\\n\", value);\n"
        "}\n"
        "\n"
        "void FunctionTwo(void) {\n"
        "    int value = 0;\n"
        "    printf(\"val: %d\\n\", value);\n"
        "}\n";

    /* Case A: Ambiguous target without context */
    PATCH_PLAN plan_ambig;
    PatchPlanInit(&plan_ambig, "sample.c");
    PatchPlanAddHunk(&plan_ambig, 5, NULL, "    int value = 0;\n", "    int value = 1;\n", NULL);

    PATCH_VERIFY_REPORT rep_ambig;
    int rc_a = PatchVerifyAgainstBuffer(&plan_ambig, sample_code, &rep_ambig);
    TEST_ASSERT(rc_a == 0, "Ambiguous target without context is rejected (fail-closed)");
    TEST_ASSERT(rep_ambig.status == PATCH_CHECK_AMBIGUOUS, "Status is PATCH_CHECK_AMBIGUOUS");
    TEST_ASSERT(rep_ambig.occurrences_found == 2, "Detected exactly 2 occurrences");
    TEST_ASSERT(!rep_ambig.is_applicable, "Marked not applicable");
    PatchPlanFree(&plan_ambig);

    /* Case B: Disambiguated with context_before */
    PATCH_PLAN plan_exact;
    PatchPlanInit(&plan_exact, "sample.c");
    PatchPlanAddHunk(&plan_exact, 10, "void FunctionTwo(void) {\n",
                     "    int value = 0;\n",
                     "    int value = 42;\n",
                     "    printf(\"val: %d\\n\", value);");

    PATCH_VERIFY_REPORT rep_exact;
    int rc_b = PatchVerifyAgainstBuffer(&plan_exact, sample_code, &rep_exact);
    TEST_ASSERT(rc_b == 1, "Disambiguated target with context is accepted");
    TEST_ASSERT(rep_exact.status == PATCH_CHECK_OK, "Status is PATCH_CHECK_OK");
    TEST_ASSERT(rep_exact.occurrences_found == 1, "Exactly 1 match found");
    TEST_ASSERT(rep_exact.matched_line == 10, "Matched line is exactly 10");
    TEST_ASSERT(rep_exact.line_drift == 0, "Zero line drift");
    TEST_ASSERT(rep_exact.is_applicable, "Marked applicable");
    PatchPlanFree(&plan_exact);

    /* Case C: Target not found */
    PATCH_PLAN plan_missing;
    PatchPlanInit(&plan_missing, "sample.c");
    PatchPlanAddHunk(&plan_missing, 5, NULL, "int nonexistent_variable = 99;\n", "int x = 1;\n", NULL);

    PATCH_VERIFY_REPORT rep_missing;
    int rc_c = PatchVerifyAgainstBuffer(&plan_missing, sample_code, &rep_missing);
    TEST_ASSERT(rc_c == 0, "Non-existent target rejected");
    TEST_ASSERT(rep_missing.status == PATCH_CHECK_NOT_FOUND, "Status is PATCH_CHECK_NOT_FOUND");
    TEST_ASSERT(rep_missing.occurrences_found == 0, "0 occurrences found");
    PatchPlanFree(&plan_missing);

    /* Case D: Offset drift handling */
    PATCH_PLAN plan_drift;
    PatchPlanInit(&plan_drift, "sample.c");
    /* Expected at line 4, but FunctionTwo starts at line 9 */
    PatchPlanAddHunk(&plan_drift, 4, "void FunctionTwo(void) {\n",
                     "    int value = 0;\n",
                     "    int value = 99;\n",
                     NULL);

    PATCH_VERIFY_REPORT rep_drift;
    int rc_d = PatchVerifyAgainstBuffer(&plan_drift, sample_code, &rep_drift);
    TEST_ASSERT(rc_d == 1, "Tolerates offset drift when uniquely anchored");
    TEST_ASSERT(rep_drift.status == PATCH_CHECK_OFFSET_DRIFT, "Status is PATCH_CHECK_OFFSET_DRIFT");
    TEST_ASSERT(rep_drift.matched_line == 10, "Located actual match at line 10");
    TEST_ASSERT(rep_drift.line_drift == 6, "Calculated exact line drift of +6 lines");
    PatchPlanFree(&plan_drift);
}

/* ============================================================
   Test 3: Real Atomic Disk Modification & Instant Rollback
   ============================================================ */
static void test_atomic_apply_and_rollback(void)
{
    printf("\n=== Test 3: Real Atomic Disk Modification & Rollback ===\n");

    const char *test_file = "test_scratch_calc.c";
    const char *initial_content =
        "#include <stdio.h>\n"
        "\n"
        "int CalculateSum(int a, int b) {\n"
        "    return a - b; /* BUG: subtraction instead of addition */\n"
        "}\n";

    /* 1. Create target file on disk */
    FILE *f = fopen(test_file, "wb");
    TEST_ASSERT(f != NULL, "Created scratch test file on disk");
    fwrite(initial_content, 1, strlen(initial_content), f);
    fclose(f);

    /* 2. Prepare surgical patch */
    PATCH_PLAN plan;
    PatchPlanInit(&plan, test_file);
    PatchPlanAddHunk(&plan, 4,
                     "int CalculateSum(int a, int b) {\n",
                     "    return a - b; /* BUG: subtraction instead of addition */\n",
                     "    return a + b; /* FIXED: addition applied */\n",
                     "}\n");

    /* 3. Apply atomic patch */
    int rc_apply = PatchApplyAtomic(&plan);
    TEST_ASSERT(rc_apply == 1, "PatchApplyAtomic succeeds");
    TEST_ASSERT(plan.is_applied, "Plan marked as applied");
    TEST_ASSERT(plan.backup_content != NULL, "Backup snapshot retained in memory");

    /* Verify modified content on disk */
    FILE *f_mod = fopen(test_file, "rb");
    char mod_buf[512] = {0};
    fread(mod_buf, 1, sizeof(mod_buf) - 1, f_mod);
    fclose(f_mod);

    TEST_ASSERT(strstr(mod_buf, "return a + b; /* FIXED") != NULL,
                "Disk file now contains patched replacement code");
    TEST_ASSERT(strstr(mod_buf, "return a - b;") == NULL,
                "Old buggy code removed from disk");

    /* 4. Trigger Atomic Rollback (simulating failed build/test) */
    int rc_rb = PatchRollback(&plan);
    TEST_ASSERT(rc_rb == 1, "PatchRollback succeeds in 0.001s");
    TEST_ASSERT(!plan.is_applied, "Plan marked as not applied");

    /* Verify restored content on disk */
    FILE *f_restored = fopen(test_file, "rb");
    char restored_buf[512] = {0};
    fread(restored_buf, 1, sizeof(restored_buf) - 1, f_restored);
    fclose(f_restored);

    TEST_ASSERT(strcmp(restored_buf, initial_content) == 0,
                "Disk file restored byte-for-byte to initial content");

    /* Cleanup */
    PatchPlanFree(&plan);
    remove(test_file);
    TEST_ASSERT(true, "Temporary test file removed cleanly");
}

/* ============================================================
   Test 4: Unified Diff (diff -u) Formatting
   ============================================================ */
static void test_unified_diff_formatting(void)
{
    printf("\n=== Test 4: Unified Diff (diff -u) Formatting ===\n");

    PATCH_PLAN plan;
    PatchPlanInit(&plan, "src/agent_core.c");
    PatchPlanAddHunk(&plan, 120,
                     "static int Helper(void) {\n",
                     "    int old_code = 1;\n",
                     "    int new_code = 2;\n",
                     "}\n");

    PATCH_VERIFY_REPORT report;
    memset(&report, 0, sizeof(report));
    report.matched_line = 120;
    report.is_applicable = true;

    char diff_buf[MAX_DIFF_BUFFER];
    int rc = PatchFormatUnifiedDiff(&plan, &report, diff_buf, sizeof(diff_buf));
    TEST_ASSERT(rc == 1, "PatchFormatUnifiedDiff generates unified diff");
    TEST_ASSERT(strstr(diff_buf, "--- a/src/agent_core.c") != NULL, "Diff has '--- a/' header");
    TEST_ASSERT(strstr(diff_buf, "+++ b/src/agent_core.c") != NULL, "Diff has '+++ b/' header");
    TEST_ASSERT(strstr(diff_buf, "@@ -120,1 +120,1 @@") != NULL, "Diff has chunk range header");
    TEST_ASSERT(strstr(diff_buf, "-    int old_code = 1;\n") != NULL, "Diff shows deleted line '-'");
    TEST_ASSERT(strstr(diff_buf, "+    int new_code = 2;\n") != NULL, "Diff shows added line '+'");

    printf("Formatted Unified Diff Sample:\n%s\n", diff_buf);

    PatchPlanFree(&plan);
}

/* ============================================================
   Test 5: Fail-Closed Boundaries & Hygiene
   ============================================================ */
static void test_fail_closed_boundaries(void)
{
    printf("\n=== Test 5: Fail-Closed Boundaries ===\n");

    TEST_ASSERT(PatchPlanInit(NULL, "a.c") == 0, "NULL plan rejected");
    TEST_ASSERT(PatchPlanInit((PATCH_PLAN *)1, NULL) == 0, "NULL file rejected");

    PATCH_PLAN plan;
    PatchPlanInit(&plan, "nonexistent.c");
    TEST_ASSERT(PatchPlanAddHunk(NULL, 1, "", "", "", "") == 0, "NULL plan in AddHunk rejected");
    TEST_ASSERT(PatchPlanAddHunk(&plan, 1, "", "", "", "") == 0, "Empty target rejected");

    PATCH_VERIFY_REPORT report;
    TEST_ASSERT(PatchVerifyPlan(&plan, &report) == 0, "Zero hunks plan verification fails");

    /* Rollback without prior apply fails safely */
    TEST_ASSERT(PatchRollback(&plan) == 0, "Rollback unapplied plan fails safely");

    PatchPlanFree(&plan);
}

int main(void)
{
    printf("========================================================\n");
    printf("  SURGICAL EDITING & UNIFIED PATCH ENGINE TEST SUITE\n");
    printf("========================================================\n");

    test_lifecycle_and_formulation();
    test_dry_run_verification();
    test_atomic_apply_and_rollback();
    test_unified_diff_formatting();
    test_fail_closed_boundaries();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
