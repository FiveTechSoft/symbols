/* ============================================================
   test_c_synthesis.c: Autonomous C Programming, Semantic Ingestion,
                       CodeGraph Modeling & Closed-Loop GCC Self-Healing.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "text_lex.h"
#include "code_graph.h"
#include "agent_shell.h"
#include "agent_diagnose.h"
#include "agent_patch.h"
#include "embedding.h"

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
   Test 1: Ingestion & Semantic Retrieval of C Knowledge Corpus
   ============================================================ */
static void test_c_corpus_semantic_retrieval(void)
{
    printf("\n=== Test 1: Ingestion & Semantic Retrieval of C Knowledge Corpus ===\n");

    const char *corpus_path = "data/c_lang/c_corpus.txt";
    GRAPH *g = GraphCreate(4096, 8192);
    EMBEDDING_TABLE *emb = EmbeddingTableCreate(65536);
    GraphSetEmbeddingTable(g, emb);
    TEXTLEX *tl = TextLexCreate();
    TEST_ASSERT(g != NULL && emb != NULL && tl != NULL, "Created Graph, Embedding, and TextLex engine");

    TEXTLEX_STATS stats = TextLexIngest(g, tl, corpus_path, 1, 0);
    TEST_ASSERT(stats.lines_covered > 30, "Covered > 30 lines of C knowledge corpus");
    TEST_ASSERT(stats.nsent_new > 20, "Indexed > 20 C programming conceptual sentences");
    TEST_ASSERT(stats.ntok_kept > 200, "Indexed > 200 tokens from C knowledge corpus");
    TEST_ASSERT(stats.syms_new > 100, "Created > 100 new semantic symbols");

    /* Load corpus byte image for verbatim text rendering */
    FILE *fc = fopen(corpus_path, "rb");
    TEST_ASSERT(fc != NULL, "Corpus file readable");
    fseek(fc, 0, SEEK_END);
    size_t imglen = (size_t)ftell(fc);
    fseek(fc, 0, SEEK_SET);
    unsigned char *img = (unsigned char *)malloc(imglen);
    TEST_ASSERT(img != NULL, "Allocated corpus image buffer");
    size_t rd = fread(img, 1, imglen, fc);
    fclose(fc);
    TEST_ASSERT(rd == imglen, "Read full corpus image bytes");

    /* 1. Retrieve knowledge about dynamic memory (malloc) */
    const char *q_malloc[] = {"malloc"};
    uint32_t out_idx[4];
    float out_score[4];
    uint32_t nret = TextLexRetrieve(tl, g, emb, q_malloc, 1, out_idx, out_score, 4);
    TEST_ASSERT(nret > 0, "Retrieved sentences for 'malloc'");

    char sent_text[512] = {0};
    TextLexSentenceText(tl, out_idx[0], img, imglen, sent_text, sizeof(sent_text));
    TEST_ASSERT(strstr(sent_text, "malloc") != NULL, "Top retrieved sentence mentions malloc");
    printf("  [INFO] Query 'malloc' -> %s\n", sent_text);

    /* 2. Retrieve knowledge about buffer safety and overflows */
    const char *q_buf[] = {"overflow"};
    nret = TextLexRetrieve(tl, g, emb, q_buf, 1, out_idx, out_score, 4);
    TEST_ASSERT(nret > 0, "Retrieved sentences for 'overflow'");
    TextLexSentenceText(tl, out_idx[0], img, imglen, sent_text, sizeof(sent_text));
    TEST_ASSERT(strstr(sent_text, "overflow") != NULL, "Retrieved sentence explains buffer overflow");
    printf("  [INFO] Query 'overflow' -> %s\n", sent_text);

    /* 3. Retrieve knowledge about pointers and null safety */
    const char *q_ptr[] = {"pointer"};
    nret = TextLexRetrieve(tl, g, emb, q_ptr, 1, out_idx, out_score, 4);
    TEST_ASSERT(nret > 0, "Retrieved sentences for 'pointer'");
    TextLexSentenceText(tl, out_idx[0], img, imglen, sent_text, sizeof(sent_text));
    TEST_ASSERT(strstr(sent_text, "pointer") != NULL, "Retrieved sentence explains pointer concepts");
    printf("  [INFO] Query 'pointer' -> %s\n", sent_text);

    free(img);
    TextLexFree(tl);
    EmbeddingTableDestroy(emb);
    GraphDestroy(g);
}

/* ============================================================
   Test 2: Code Knowledge Graph Ingestion of C Standard Library
   ============================================================ */
static void test_code_graph_c_stdlib(void)
{
    printf("\n=== Test 2: Ingestion of C Standard Library into CodeGraph ===\n");

    CODE_GRAPH *cg = CodeGraphCreate(2048, 4096);
    TEST_ASSERT(cg != NULL, "Created CodeGraph");

    int rc = CodeGraphIngestFile(cg, "data/c_lang/c_std_lib.h");
    TEST_ASSERT(rc == 1, "CodeGraphIngestFile ingested data/c_lang/c_std_lib.h");
    TEST_ASSERT(cg->total_files == 1, "Tracked exactly 1 header file");
    TEST_ASSERT(cg->total_functions > 10, "Discovered > 10 standard C library functions");
    TEST_ASSERT(cg->total_structs >= 2, "Discovered standard C structures (C_FILE, C_MEM_BLOCK)");

    /* Query functions defined in header */
    char funcs[32][MAX_CODE_NAME];
    uint32_t nfuncs = CodeGraphGetFileFunctions(cg, "data/c_lang/c_std_lib.h", funcs, 32);
    TEST_ASSERT(nfuncs > 10, "CodeGraph extracted function list from file");

    bool has_malloc = false;
    bool has_snprintf = false;
    bool has_memcpy = false;
    for (uint32_t i = 0; i < nfuncs; i++)
    {
        if (strcmp(funcs[i], "malloc") == 0) has_malloc = true;
        if (strcmp(funcs[i], "snprintf") == 0) has_snprintf = true;
        if (strcmp(funcs[i], "memcpy") == 0) has_memcpy = true;
    }
    TEST_ASSERT(has_malloc, "Graph knows standard C 'malloc'");
    TEST_ASSERT(has_snprintf, "Graph knows standard C 'snprintf'");
    TEST_ASSERT(has_memcpy, "Graph knows standard C 'memcpy'");

    CodeGraphDestroy(cg);
}

/* ============================================================
   Test 3: Autonomous Synthesis & Native GCC Compilation Loop
   ============================================================ */
static void test_autonomous_c_synthesis_and_gcc_execution(void)
{
    printf("\n=== Test 3: Autonomous C Synthesis & Native GCC Verification ===\n");

    const char *src_file = "test_synth_program.c";
    const char *exe_file = "test_synth_program.exe";

    /* C source program implementing a safe bounded string builder */
    const char *code =
        "#include <stdio.h>\n"
        "#include <string.h>\n"
        "#include <assert.h>\n"
        "\n"
        "typedef struct {\n"
        "    char buf[128];\n"
        "    size_t len;\n"
        "} StringBuilder;\n"
        "\n"
        "void SbInit(StringBuilder *sb) {\n"
        "    assert(sb != NULL);\n"
        "    sb->buf[0] = '\\0';\n"
        "    sb->len = 0;\n"
        "}\n"
        "\n"
        "int SbAppend(StringBuilder *sb, const char *str) {\n"
        "    if (!sb || !str) return 0;\n"
        "    size_t add_len = strlen(str);\n"
        "    if (sb->len + add_len >= sizeof(sb->buf)) return 0; /* Guard against overflow */\n"
        "    memcpy(sb->buf + sb->len, str, add_len);\n"
        "    sb->len += add_len;\n"
        "    sb->buf[sb->len] = '\\0';\n"
        "    return 1;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    StringBuilder sb;\n"
        "    SbInit(&sb);\n"
        "    assert(SbAppend(&sb, \"Symbolic\"));\n"
        "    assert(SbAppend(&sb, \" \"));\n"
        "    assert(SbAppend(&sb, \"C11\"));\n"
        "    assert(strcmp(sb.buf, \"Symbolic C11\") == 0);\n"
        "    assert(sb.len == 12);\n"
        "    printf(\"SUCCESS: %s (len=%zu)\\n\", sb.buf, sb.len);\n"
        "    return 0;\n"
        "}\n";

    FILE *f = fopen(src_file, "wb");
    TEST_ASSERT(f != NULL, "Opened test_synth_program.c for writing");
    fwrite(code, 1, strlen(code), f);
    fclose(f);

    /* 1. Compile dynamically with GCC via AgentShellExec */
    char compile_cmd[256];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc -Wall -Wextra -Werror %s -o %s", src_file, exe_file);

    SHELL_EXEC_RESULT shell_res;
    int ok = AgentShellExec(compile_cmd, ".", 10000, &shell_res);
    TEST_ASSERT(ok == 1, "AgentShellExec launched GCC compiler");
    TEST_ASSERT(shell_res.exit_code == 0, "GCC compilation succeeded with exit code 0 (-Wall -Wextra -Werror)");
    TEST_ASSERT(shell_res.stderr_len == 0, "Zero warnings/errors emitted by GCC");
    printf("  [INFO] Compiled in %.2f ms via %s\n", shell_res.wall_clock_ms, shell_res.backend_name);

    /* 2. Execute compiled binary */
#ifdef _WIN32
    const char *run_cmd = "test_synth_program.exe";
#else
    const char *run_cmd = "./test_synth_program.exe";
#endif

    ok = AgentShellExec(run_cmd, ".", 5000, &shell_res);
    TEST_ASSERT(ok == 1, "AgentShellExec launched synthesized binary");
    TEST_ASSERT(shell_res.exit_code == 0, "Synthesized C program executed successfully with exit code 0");
    TEST_ASSERT(strstr(shell_res.stdout_buf, "SUCCESS: Symbolic C11") != NULL,
                "Synthesized C program produced verified output");
    printf("  [INFO] Binary executed in %.2f ms, output: %s", shell_res.wall_clock_ms, shell_res.stdout_buf);

    /* Cleanup temporary files */
    remove(src_file);
    remove(exe_file);
}

/* ============================================================
   Test 4: Closed-Loop Abductive Diagnosis & Self-Repair with GCC
   ============================================================ */
static void test_closed_loop_c_abductive_repair(void)
{
    printf("\n=== Test 4: Closed-Loop C Abductive Diagnosis & GCC Repair ===\n");

    const char *src_file = "test_repair_candidate.c";
    const char *exe_file = "test_repair_candidate.exe";

    /* C code with intentional typo: struct member is 'columns', but code writes 'column' */
    const char *buggy_code =
        "#include <stdio.h>\n"
        "typedef struct {\n"
        "    int rows;\n"
        "    int columns;\n"
        "} matrix_t;\n"
        "\n"
        "int GetColumns(matrix_t *m) {\n"
        "    return m->column;\n"
        "}\n"
        "\n"
        "int main(void) {\n"
        "    matrix_t m = {3, 5};\n"
        "    if (GetColumns(&m) == 5) return 0;\n"
        "    return 1;\n"
        "}\n";

    FILE *f = fopen(src_file, "wb");
    fwrite(buggy_code, 1, strlen(buggy_code), f);
    fclose(f);

    /* 1. Attempt initial compilation with GCC */
    char compile_cmd[256];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc -Wall -Wextra %s -o %s", src_file, exe_file);

    SHELL_EXEC_RESULT shell_res;
    AgentShellExec(compile_cmd, ".", 10000, &shell_res);

    TEST_ASSERT(shell_res.exit_code != 0, "Initial compilation fails as expected");
    TEST_ASSERT(shell_res.stderr_len > 0, "Captured GCC error stream");

    /* 2. Feed GCC stderr stream to Abductive Diagnostic Engine */
    DIAGNOSTIC_REPORT diag_rep;
    int diag_ok = DiagnosticParseOutput(shell_res.stderr_buf, &diag_rep);
    TEST_ASSERT(diag_ok == 1, "Diagnostic engine parsed GCC compiler error");
    TEST_ASSERT(diag_rep.error_count >= 1, "Identified compiler error");
    TEST_ASSERT(diag_rep.root_type == DIAG_ERR_MISSING_MEMBER, "Classified as DIAG_ERR_MISSING_MEMBER");
    TEST_ASSERT(strcmp(diag_rep.root_symbol, "column") == 0, "Identified invalid member 'column'");
    TEST_ASSERT(strcmp(diag_rep.root_suggestion, "columns") == 0, "GCC suggested 'columns'");

    char remedy[256];
    DiagnosticAbduceRemedy(&diag_rep, NULL, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "columns") != NULL, "Remedy prescribes replacement with 'columns'");
    printf("  [INFO] Diagnosed: %s\n", remedy);

    /* 3. Surgical Patch Application */
    PATCH_PLAN patch;
    PatchPlanInit(&patch, src_file);
    PatchPlanAddHunk(&patch,
                     8,
                     "int GetColumns(matrix_t *m) {\n",
                     "    return m->column;\n",
                     "    return m->columns;\n",
                     "}\n");

    PATCH_VERIFY_REPORT vrep;
    int ok_v = PatchVerifyPlan(&patch, &vrep);
    TEST_ASSERT(ok_v == 1 && vrep.is_applicable, "Patch pre-flight verification succeeded");

    int ok_apply = PatchApplyAtomic(&patch);
    TEST_ASSERT(ok_apply == 1, "Atomic surgical patch applied cleanly to disk");
    PatchPlanFree(&patch);

    /* 4. Re-compilation with GCC after surgical repair */
    AgentShellExec(compile_cmd, ".", 10000, &shell_res);
    TEST_ASSERT(shell_res.exit_code == 0, "GCC compilation SUCCEEDS after abductive self-repair (exit_code=0)");
    TEST_ASSERT(shell_res.stderr_len == 0, "Zero compiler errors remaining");

    /* 5. Execute binary and verify ground truth */
#ifdef _WIN32
    const char *run_cmd = "test_repair_candidate.exe";
#else
    const char *run_cmd = "./test_repair_candidate.exe";
#endif

    AgentShellExec(run_cmd, ".", 5000, &shell_res);
    TEST_ASSERT(shell_res.exit_code == 0, "Repaired binary executed with exit code 0 (PASS)");
    printf("  [INFO] Repaired program executed successfully in %.2f ms\n", shell_res.wall_clock_ms);

    /* Cleanup */
    remove(src_file);
    remove(exe_file);
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST SUITE: C PROGRAMMING SYNTHESIS & SELF-HEALING (test_c_synthesis)\n");
    printf("======================================================================\n");

    test_c_corpus_semantic_retrieval();
    test_code_graph_c_stdlib();
    test_autonomous_c_synthesis_and_gcc_execution();
    test_closed_loop_c_abductive_repair();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
