/* ============================================================
   test_agent_diagnose.c: Unit test suite for Compiler & Linter
                          Error Abductive Engine.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_diagnose.h"
#include "code_graph.h"

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

/* Test 1: GCC missing struct member with did-you-mean */
static void test_gcc_missing_member(void)
{
    printf("\n=== Test 1: GCC Missing Member with 'did you mean' ===\n");
    const char *err = 
        "src/matrix.c:42:15: error: 'matrix_t' has no member named 'width'; did you mean 'cols'?\n"
        "   42 |     int w = m->width;\n"
        "      |                ^~~~~\n";

    DIAGNOSTIC_REPORT report;
    int ok = DiagnosticParseOutput(err, &report);
    TEST_ASSERT(ok == 1, "DiagnosticParseOutput parses GCC error");
    TEST_ASSERT(report.error_count == 1, "Found exactly 1 error");
    TEST_ASSERT(report.warning_count == 0, "Found 0 warnings");
    TEST_ASSERT(strcmp(report.root_file, "src/matrix.c") == 0, "Root file is src/matrix.c");
    TEST_ASSERT(report.root_line == 42, "Root line is 42");
    TEST_ASSERT(report.root_col == 15, "Root col is 15");
    TEST_ASSERT(report.root_type == DIAG_ERR_MISSING_MEMBER, "Error type is DIAG_ERR_MISSING_MEMBER");
    TEST_ASSERT(strcmp(report.root_symbol, "width") == 0, "Offending symbol is 'width'");
    TEST_ASSERT(strcmp(report.root_suggestion, "cols") == 0, "Did-you-mean suggestion is 'cols'");

    char remedy[256];
    DiagnosticAbduceRemedy(&report, NULL, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "replace invalid member 'width' with 'cols'") != NULL,
                "Remedy prescribes exact replacement of width with cols");
}

/* Test 2: Clang undeclared identifier with did-you-mean */
static void test_clang_undeclared_symbol(void)
{
    printf("\n=== Test 2: Clang Undeclared Identifier ===\n");
    const char *err = 
        "src/network.c:108:9: error: use of undeclared identifier 'buff_size'; did you mean 'buffer_size'?\n"
        "    if (buff_size > 1024) return 0;\n"
        "        ^~~~~~~~~\n"
        "        buffer_size\n";

    DIAGNOSTIC_REPORT report;
    int ok = DiagnosticParseOutput(err, &report);
    TEST_ASSERT(ok == 1, "DiagnosticParseOutput parses Clang error");
    TEST_ASSERT(report.error_count == 1, "1 error identified");
    TEST_ASSERT(strcmp(report.root_file, "src/network.c") == 0, "File matches src/network.c");
    TEST_ASSERT(report.root_line == 108, "Line is 108");
    TEST_ASSERT(report.root_type == DIAG_ERR_UNDECLARED_SYMBOL, "Type is DIAG_ERR_UNDECLARED_SYMBOL");
    TEST_ASSERT(strcmp(report.root_symbol, "buff_size") == 0, "Offending symbol is 'buff_size'");
    TEST_ASSERT(strcmp(report.root_suggestion, "buffer_size") == 0, "Suggestion is 'buffer_size'");

    char remedy[256];
    DiagnosticAbduceRemedy(&report, NULL, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "replace with candidate 'buffer_size'") != NULL, "Remedy uses buffer_size");
}

/* Test 3: GCC Arity Mismatch */
static void test_gcc_arity_mismatch(void)
{
    printf("\n=== Test 3: GCC Arity Mismatch ===\n");
    const char *err = 
        "src/graph_builder.c:75:5: error: too few arguments to function 'GraphAddRelation'; expected 4, have 3\n"
        "   75 |     GraphAddRelation(g, s1, r);\n"
        "      |     ^~~~~~~~~~~~~~~~\n";

    DIAGNOSTIC_REPORT report;
    DiagnosticParseOutput(err, &report);
    TEST_ASSERT(report.root_type == DIAG_ERR_ARITY_MISMATCH, "Detected DIAG_ERR_ARITY_MISMATCH");
    TEST_ASSERT(strcmp(report.root_symbol, "GraphAddRelation") == 0, "Target function is GraphAddRelation");
    TEST_ASSERT(report.items[0].expected_arity == 4, "Expected arity is 4");
    TEST_ASSERT(report.items[0].actual_arity == 3, "Actual arity is 3");

    char remedy[256];
    DiagnosticAbduceRemedy(&report, NULL, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "expected 4 arguments, provided 3") != NULL, "Remedy states exact arity difference");
}

/* Test 4: Missing Header (fatal error) */
static void test_missing_header(void)
{
    printf("\n=== Test 4: Missing Include Header ===\n");
    const char *err = 
        "src/agent_core.c:14:10: fatal error: json_parser.h: No such file or directory\n"
        "   14 | #include \"json_parser.h\"\n"
        "      |          ^~~~~~~~~~~~~~~\n"
        "compilation terminated.\n";

    DIAGNOSTIC_REPORT report;
    DiagnosticParseOutput(err, &report);
    TEST_ASSERT(report.root_type == DIAG_ERR_MISSING_HEADER, "Detected DIAG_ERR_MISSING_HEADER");
    TEST_ASSERT(strcmp(report.root_symbol, "json_parser.h") == 0, "Missing header is 'json_parser.h'");
    TEST_ASSERT(report.root_line == 14, "Line is 14");
}

/* Test 5: MSVC Error Format */
static void test_msvc_format(void)
{
    printf("\n=== Test 5: MSVC Compiler Format ===\n");
    const char *err = 
        "C:\\repo\\src\\service.c(56,12): error C2065: 'timeout_ms': undeclared identifier\n";

    DIAGNOSTIC_REPORT report;
    int ok = DiagnosticParseOutput(err, &report);
    TEST_ASSERT(ok == 1, "MSVC format parsed successfully");
    TEST_ASSERT(strcmp(report.root_file, "C:\\repo\\src\\service.c") == 0, "MSVC path matches");
    TEST_ASSERT(report.root_line == 56, "MSVC line is 56");
    TEST_ASSERT(report.root_col == 12, "MSVC col is 12");
    TEST_ASSERT(report.root_type == DIAG_ERR_UNDECLARED_SYMBOL, "MSVC error classified as undeclared");
    TEST_ASSERT(strcmp(report.root_symbol, "timeout_ms") == 0, "Extracted MSVC symbol timeout_ms");
}

/* Test 6: Abductive CodeGraph Linking */
static void test_codegraph_abductive_linking(void)
{
    printf("\n=== Test 6: CodeGraph Abductive Linking for Missing Symbol ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(128, 512);
    /* Ingest a header/source defining the function */
    CodeGraphIngestSource(cg, "include/query_engine.h", "int ExecuteQueryPlan(void) { return 0; }\n");

    const char *err = 
        "src/app.c:60:5: error: 'ExecuteQueryPlan' undeclared (first use in this function)\n";

    DIAGNOSTIC_REPORT report;
    DiagnosticParseOutput(err, &report);
    TEST_ASSERT(report.root_type == DIAG_ERR_UNDECLARED_SYMBOL, "Detected undeclared symbol");
    TEST_ASSERT(strcmp(report.root_symbol, "ExecuteQueryPlan") == 0, "Symbol is ExecuteQueryPlan");

    char remedy[512];
    DiagnosticAbduceRemedy(&report, cg, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "add '#include \"include/query_engine.h\"'") != NULL,
                "Abductive reasoner deduced missing include file from CodeGraph");

    CodeGraphDestroy(cg);
}

/* Test 7: STRIPS Predicate Projection & PR Reporting */
static void test_strips_projection_and_reporting(void)
{
    printf("\n=== Test 7: STRIPS Predicates and PR Reporting ===\n");
    const char *err = 
        "src/main.c:10:1: error: expected ';' before 'return'\n";

    DIAGNOSTIC_REPORT report;
    DiagnosticParseOutput(err, &report);
    uint32_t preds = DiagnosticToStripsPredicates(&report);
    TEST_ASSERT(preds & PRED_ERROR_DIAGNOSED, "Asserts PRED_ERROR_DIAGNOSED in STRIPS world state");

    char report_md[1024];
    int ok = DiagnosticFormatReport(&report, report_md, sizeof(report_md));
    TEST_ASSERT(ok == 1, "DiagnosticFormatReport succeeds");
    TEST_ASSERT(strstr(report_md, "Automated Diagnostic & Abductive Analysis") != NULL, "Markdown header present");
    TEST_ASSERT(strstr(report_md, "SYNTAX_ERROR") != NULL, "Classified as SYNTAX_ERROR");
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST SUITE: COMPILER & LINTER ERROR ABDUCTIVE ENGINE (agent_diagnose)\n");
    printf("======================================================================\n");

    test_gcc_missing_member();
    test_clang_undeclared_symbol();
    test_gcc_arity_mismatch();
    test_missing_header();
    test_msvc_format();
    test_codegraph_abductive_linking();
    test_strips_projection_and_reporting();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
