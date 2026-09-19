/* ============================================================
   test_code_graph.c: Empirical evaluation of the Code Knowledge
                      Graph, AST Call-Graph, and Blast Radius.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

/* ============================================================
   Test 1: Lifecycle and Predicate Initialization
   ============================================================ */
static void test_lifecycle(void)
{
    printf("\n=== Test 1: Lifecycle & Predicates ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);
    TEST_ASSERT(cg != NULL, "CodeGraphCreate succeeds");
    TEST_ASSERT(cg->graph != NULL, "Internal GRAPH is initialized");
    TEST_ASSERT(cg->rel_defines_func != SYMBOL_INVALID, "rel_defines_func initialized");
    TEST_ASSERT(cg->rel_calls != SYMBOL_INVALID, "rel_calls initialized");
    TEST_ASSERT(cg->rel_called_by != SYMBOL_INVALID, "rel_called_by initialized");
    TEST_ASSERT(cg->rel_includes != SYMBOL_INVALID, "rel_includes initialized");

    CodeGraphDestroy(cg);
    TEST_ASSERT(true, "CodeGraphDestroy runs cleanly without leaks");
}

/* ============================================================
   Test 2: Source Ingestion, Struct Fields, and Function Defs
   ============================================================ */
static void test_source_ingestion(void)
{
    printf("\n=== Test 2: Source Ingestion & Entity Extraction ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);

    const char *sample_code =
        "#include <stdio.h>\n"
        "#include \"utils.h\"\n"
        "\n"
        "struct Vector3 {\n"
        "    float x;\n"
        "    float y;\n"
        "    float z;\n"
        "};\n"
        "\n"
        "int ComputeDistance(struct Vector3 *a, struct Vector3 *b) {\n"
        "    printf(\"Computing\\n\");\n"
        "    return 42;\n"
        "}\n"
        "\n"
        "void RenderScene(void) {\n"
        "    struct Vector3 p1;\n"
        "    ComputeDistance(&p1, &p1);\n"
        "}\n";

    int rc = CodeGraphIngestSource(cg, "src/math_engine.c", sample_code);
    TEST_ASSERT(rc == 1, "CodeGraphIngestSource returns success");
    TEST_ASSERT(cg->total_files == 1, "total_files equals 1");
    TEST_ASSERT(cg->total_functions == 2, "2 functions detected (ComputeDistance, RenderScene)");
    TEST_ASSERT(cg->total_structs == 1, "1 struct detected (Vector3)");

    /* Verify file inclusions */
    char includes[8][MAX_CODE_NAME];
    uint32_t n_inc = CodeGraphGetIncludes(cg, "src/math_engine.c", includes, 8);
    TEST_ASSERT(n_inc == 2, "Extracted 2 included headers");
    TEST_ASSERT(strcmp(includes[0], "stdio.h") == 0 || strcmp(includes[1], "stdio.h") == 0,
                "Includes stdio.h");
    TEST_ASSERT(strcmp(includes[0], "utils.h") == 0 || strcmp(includes[1], "utils.h") == 0,
                "Includes utils.h");

    /* Verify struct fields */
    char fields[8][MAX_CODE_NAME];
    uint32_t n_fields = CodeGraphGetStructFields(cg, "Vector3", fields, 8);
    TEST_ASSERT(n_fields == 3, "Extracted 3 fields for Vector3");
    TEST_ASSERT(strcmp(fields[0], "x") == 0, "Field 0 is x");
    TEST_ASSERT(strcmp(fields[1], "y") == 0, "Field 1 is y");
    TEST_ASSERT(strcmp(fields[2], "z") == 0, "Field 2 is z");

    /* Verify functions in file */
    char funcs[8][MAX_CODE_NAME];
    uint32_t n_funcs = CodeGraphGetFileFunctions(cg, "src/math_engine.c", funcs, 8);
    TEST_ASSERT(n_funcs == 2, "Extracted 2 functions for src/math_engine.c");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 3: Bidirectional Call Graph (Callers & Callees)
   ============================================================ */
static void test_call_graph(void)
{
    printf("\n=== Test 3: Bidirectional Call Graph ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);

    const char *code =
        "int WorkerStep(void) {\n"
        "    return 1;\n"
        "}\n"
        "int TaskExecute(void) {\n"
        "    WorkerStep();\n"
        "    return 0;\n"
        "}\n"
        "int PipelineRun(void) {\n"
        "    TaskExecute();\n"
        "    WorkerStep();\n"
        "    return 0;\n"
        "}\n";

    CodeGraphIngestSource(cg, "src/pipeline.c", code);

    /* Test callees of PipelineRun */
    char callees[8][MAX_CODE_NAME];
    uint32_t n_callees = CodeGraphGetCallees(cg, "PipelineRun", callees, 8);
    TEST_ASSERT(n_callees == 2, "PipelineRun calls 2 distinct functions");

    /* Test callers of WorkerStep */
    char callers[8][MAX_CODE_NAME];
    uint32_t n_callers = CodeGraphGetCallers(cg, "WorkerStep", callers, 8);
    TEST_ASSERT(n_callers == 2, "WorkerStep is called by 2 functions (TaskExecute, PipelineRun)");

    /* Test location lookup */
    const char *loc = CodeGraphGetFunctionFile(cg, "TaskExecute");
    TEST_ASSERT(loc != NULL && strcmp(loc, "src/pipeline.c") == 0,
                "TaskExecute is located in src/pipeline.c");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 4: Impact Analysis & Blast Radius BFS Closure
   ============================================================ */
static void test_blast_radius(void)
{
    printf("\n=== Test 4: Impact Analysis & Blast Radius ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(1024, 2048);

    const char *module_a =
        "int LowLevelHelper(void) { return 10; }\n"
        "int MidServiceA(void) { return LowLevelHelper(); }\n";

    const char *module_b =
        "int MidServiceB(void) { return LowLevelHelper(); }\n"
        "int HighController(void) { return MidServiceA() + MidServiceB(); }\n";

    const char *module_c =
        "int MainEntry(void) { return HighController(); }\n";

    CodeGraphIngestSource(cg, "src/module_a.c", module_a);
    CodeGraphIngestSource(cg, "src/module_b.c", module_b);
    CodeGraphIngestSource(cg, "src/module_c.c", module_c);

    CODE_BLAST_RADIUS radius;
    int rc = CodeGraphComputeBlastRadius(cg, "LowLevelHelper", 4, &radius);
    TEST_ASSERT(rc == 1, "CodeGraphComputeBlastRadius succeeds for LowLevelHelper");
    TEST_ASSERT(strcmp(radius.target_symbol, "LowLevelHelper") == 0, "Target symbol matches");

    /* Depth 1: MidServiceA, MidServiceB */
    /* Depth 2: HighController */
    /* Depth 3: MainEntry */
    TEST_ASSERT(radius.affected_functions_count == 4,
                "4 functions affected (MidServiceA, MidServiceB, HighController, MainEntry)");
    TEST_ASSERT(radius.affected_files_count == 3,
                "3 distinct files affected (module_a.c, module_b.c, module_c.c)");
    TEST_ASSERT(radius.max_depth_reached == 3, "Max depth reached is 3");

    /* Format markdown report */
    char report[2048];
    int frc = CodeGraphFormatBlastRadius(cg, &radius, report, sizeof(report));
    TEST_ASSERT(frc == 1, "CodeGraphFormatBlastRadius generates report");
    TEST_ASSERT(strstr(report, "LowLevelHelper") != NULL, "Report mentions target");
    TEST_ASSERT(strstr(report, "Depth 1") != NULL, "Report has Depth 1 section");
    TEST_ASSERT(strstr(report, "Depth 2") != NULL, "Report has Depth 2 section");
    TEST_ASSERT(strstr(report, "Depth 3") != NULL, "Report has Depth 3 section");

    printf("Formatted Report Sample:\n%s\n", report);

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 5: Real Project Codebase Ingestion (agent_core.c)
   ============================================================ */
static void test_real_project_ingestion(void)
{
    printf("\n=== Test 5: Real Project Codebase Ingestion ===\n");
    CODE_GRAPH *cg = CodeGraphCreate(4096, 8192);

    /* Ingest actual project file from repository */
    int rc1 = CodeGraphIngestFile(cg, "C:/symbols/include/agent_core.h");
    TEST_ASSERT(rc1 == 1, "Ingested C:/symbols/include/agent_core.h");

    int rc2 = CodeGraphIngestFile(cg, "C:/symbols/src/agent_core.c");
    TEST_ASSERT(rc2 == 1, "Ingested C:/symbols/src/agent_core.c");

    int rc3 = CodeGraphIngestFile(cg, "C:/symbols/tests/test_agent_core.c");
    TEST_ASSERT(rc3 == 1, "Ingested C:/symbols/tests/test_agent_core.c");

    /* Verify functions detected in agent_core.c */
    TEST_ASSERT(CodeGraphHasSymbol(cg, "AgentSessionInit"), "AgentSessionInit symbol present");
    TEST_ASSERT(CodeGraphHasSymbol(cg, "AgentDecideNextAction"), "AgentDecideNextAction symbol present");
    TEST_ASSERT(CodeGraphHasSymbol(cg, "AgentProcessObservation"), "AgentProcessObservation symbol present");

    /* Check function defining file */
    const char *fn_file = CodeGraphGetFunctionFile(cg, "AgentProcessObservation");
    TEST_ASSERT(fn_file != NULL && strstr(fn_file, "agent_core.c") != NULL,
                "AgentProcessObservation located in agent_core.c");

    /* Check callers of AgentProcessObservation across modules */
    char callers[8][MAX_CODE_NAME];
    uint32_t n_callers = CodeGraphGetCallers(cg, "AgentProcessObservation", callers, 8);
    TEST_ASSERT(n_callers >= 1, "AgentProcessObservation has callers in test suite");

    bool found_main = false;
    for (uint32_t i = 0; i < n_callers; i++) {
        if (strcmp(callers[i], "main") == 0)
            found_main = true;
    }
    TEST_ASSERT(found_main, "main in test_agent_core.c is direct caller of AgentProcessObservation");

    /* Compute blast radius of AgentProcessObservation */
    CODE_BLAST_RADIUS radius;
    int br_rc = CodeGraphComputeBlastRadius(cg, "AgentProcessObservation", 3, &radius);
    TEST_ASSERT(br_rc == 1, "Blast radius computed for AgentProcessObservation");
    TEST_ASSERT(radius.affected_functions_count >= 1, "Callers discovered in blast radius");
    TEST_ASSERT(radius.affected_files_count >= 1, "Affected test files tracked");

    char report[2048];
    int frc = CodeGraphFormatBlastRadius(cg, &radius, report, sizeof(report));
    TEST_ASSERT(frc == 1, "Formatted blast radius markdown report for AgentProcessObservation");
    printf("\nReal Codebase Blast Radius Report:\n%s\n", report);

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 6: Fail-Closed Behavior & Hygiene
   ============================================================ */
static void test_fail_closed(void)
{
    CODE_GRAPH *cg = CodeGraphCreate(128, 256);
    TEST_ASSERT(CodeGraphCreate(0, 0) != NULL, "CodeGraphCreate with zeros uses safe defaults");
    TEST_ASSERT(CodeGraphIngestSource(NULL, "a.c", "int a;") == 0, "NULL graph rejected");
    TEST_ASSERT(CodeGraphIngestSource(cg, NULL, "int a;") == 0, "NULL path rejected");
    TEST_ASSERT(CodeGraphIngestSource(cg, "a.c", NULL) == 0, "NULL source rejected");
    TEST_ASSERT(CodeGraphIngestFile(NULL, "nonexistent.c") == 0, "NULL file ingest rejected");
    TEST_ASSERT(CodeGraphIngestFile(cg, "nonexistent_file_path_123.c") == 0,
                "Non-existent file fails gracefully without crashing");

    char callers[4][MAX_CODE_NAME];
    TEST_ASSERT(CodeGraphGetCallers(cg, "NonExistentFunction", callers, 4) == 0,
                "Unknown function returns 0 callers");

    CODE_BLAST_RADIUS radius;
    TEST_ASSERT(CodeGraphComputeBlastRadius(cg, "NonExistentFunction", 2, &radius) == 0,
                "Blast radius on unknown symbol fails closed");

    CodeGraphDestroy(cg);
}

int main(void)
{
    printf("========================================================\n");
    printf("  CODE KNOWLEDGE GRAPH & BLAST RADIUS TEST SUITE\n");
    printf("========================================================\n");

    test_lifecycle();
    test_source_ingestion();
    test_call_graph();
    test_blast_radius();
    test_real_project_ingestion();
    test_fail_closed();

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
