/* ============================================================
   test_code_graph_indexer.c: Test Suite for Recursive Repository
                              Ingestion & Filtering.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "compat.h"
#include "code_graph.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static void write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    fputs(content, f);
    fclose(f);
}

static void remove_file(const char *path)
{
    remove(path);
}

int main(void)
{
    printf("=========================================================\n");
    printf("  CODE GRAPH RECURSIVE INDEXER TEST SUITE (ISO C11)      \n");
    printf("=========================================================\n\n");

    /* ========================================================
       Test 1: Directory & File Ignore Rules
       ======================================================== */
    printf("=== Test 1: Ignore Rules & Extension Filtering ===\n");
    assert(CodeGraphShouldIgnoreName("."));
    assert(CodeGraphShouldIgnoreName(".."));
    assert(CodeGraphShouldIgnoreName(".git"));
    assert(CodeGraphShouldIgnoreName(".github"));
    assert(CodeGraphShouldIgnoreName("build"));
    assert(CodeGraphShouldIgnoreName("build-gcc"));
    assert(CodeGraphShouldIgnoreName("node_modules"));
    assert(CodeGraphShouldIgnoreName("venv"));
    assert(CodeGraphShouldIgnoreName(".venv"));
    assert(CodeGraphShouldIgnoreName("__pycache__"));
    assert(CodeGraphShouldIgnoreName(".vscode"));
    assert(CodeGraphShouldIgnoreName(".idea"));
    printf("  [PASS] Standard VCS and build folders ignored\n");

    assert(!CodeGraphShouldIgnoreName("src"));
    assert(!CodeGraphShouldIgnoreName("include"));
    assert(!CodeGraphShouldIgnoreName("tests"));
    assert(!CodeGraphShouldIgnoreName("main.c"));
    assert(!CodeGraphShouldIgnoreName("models.py"));
    assert(!CodeGraphShouldIgnoreName("service.ts"));
    printf("  [PASS] Valid code folders and files not ignored\n");

    assert(CodeGraphIsSupportedFile("file.c"));
    assert(CodeGraphIsSupportedFile("header.h"));
    assert(CodeGraphIsSupportedFile("script.py"));
    assert(CodeGraphIsSupportedFile("worker.pyw"));
    assert(CodeGraphIsSupportedFile("component.ts"));
    assert(CodeGraphIsSupportedFile("view.tsx"));
    assert(CodeGraphIsSupportedFile("bundle.js"));
    assert(CodeGraphIsSupportedFile("module.mjs"));
    printf("  [PASS] Supported polyglot extensions recognized\n");

    assert(!CodeGraphIsSupportedFile("binary.exe"));
    assert(!CodeGraphIsSupportedFile("object.o"));
    assert(!CodeGraphIsSupportedFile("data.bin"));
    assert(!CodeGraphIsSupportedFile("readme.md"));
    assert(!CodeGraphIsSupportedFile("config.json"));
    assert(!CodeGraphIsSupportedFile("image.png"));
    printf("  [PASS] Non-code and binary extensions rejected\n");

    /* ========================================================
       Test 2: Recursive Repository Traversal & Ingestion
       ======================================================== */
    printf("\n=== Test 2: Recursive Repository Ingestion ===\n");

    const char *test_root = "tmp_repo_test";
    MKDIR(test_root);

    char dir_src[MAX_CODE_PATH], dir_py[MAX_CODE_PATH], dir_ts[MAX_CODE_PATH];
    char dir_git[MAX_CODE_PATH], dir_node[MAX_CODE_PATH], dir_build[MAX_CODE_PATH];

    snprintf(dir_src, sizeof(dir_src), "%s/src", test_root);
    snprintf(dir_py, sizeof(dir_py), "%s/python", test_root);
    snprintf(dir_ts, sizeof(dir_ts), "%s/frontend", test_root);
    snprintf(dir_git, sizeof(dir_git), "%s/.git", test_root);
    snprintf(dir_node, sizeof(dir_node), "%s/node_modules", test_root);
    snprintf(dir_build, sizeof(dir_build), "%s/build", test_root);

    MKDIR(dir_src);
    MKDIR(dir_py);
    MKDIR(dir_ts);
    MKDIR(dir_git);
    MKDIR(dir_node);
    MKDIR(dir_build);

    /* Valid source files */
    char file_c[MAX_CODE_PATH], file_py[MAX_CODE_PATH], file_ts[MAX_CODE_PATH];
    snprintf(file_c, sizeof(file_c), "%s/server.c", dir_src);
    snprintf(file_py, sizeof(file_py), "%s/models.py", dir_py);
    snprintf(file_ts, sizeof(file_ts), "%s/client.ts", dir_ts);

    /* Files in ignored folders */
    char file_git[MAX_CODE_PATH], file_node[MAX_CODE_PATH], file_build[MAX_CODE_PATH];
    snprintf(file_git, sizeof(file_git), "%s/git_internal.c", dir_git);
    snprintf(file_node, sizeof(file_node), "%s/package.js", dir_node);
    snprintf(file_build, sizeof(file_build), "%s/generated.c", dir_build);

    write_file(file_c,
        "int HelperFunc(void) { return 42; }\n"
        "int StartServer(void) {\n"
        "    return HelperFunc();\n"
        "}\n");

    write_file(file_py,
        "class Database:\n"
        "    def connect(self):\n"
        "        pass\n"
        "\n"
        "class PostgresDatabase(Database):\n"
        "    def connect(self):\n"
        "        execute_query()\n");

    write_file(file_ts,
        "class ApiService {\n"
        "    login(user: string): boolean {\n"
        "        return verifyCredentials(user);\n"
        "    }\n"
        "}\n");

    write_file(file_git, "int GitTrap(void) { return 0; }\n");
    write_file(file_node, "function NodeTrap() { return 0; }\n");
    write_file(file_build, "int BuildTrap(void) { return 0; }\n");

    CODE_GRAPH *cg = CodeGraphCreate(4096, 8192);
    assert(cg != NULL);

    uint32_t ingested_count = CodeGraphIngestDirectory(cg, test_root);
    printf("  [PASS] IngestDirectory executed on '%s' (ingested %u files)\n",
           test_root, ingested_count);
    assert(ingested_count == 3);
    printf("  [PASS] Exactly 3 valid source files ingested (ignored folders skipped)\n");

    /* Verify symbols from valid files exist */
    assert(CodeGraphHasSymbol(cg, "StartServer"));
    assert(CodeGraphHasSymbol(cg, "HelperFunc"));
    assert(CodeGraphHasSymbol(cg, "Database"));
    assert(CodeGraphHasSymbol(cg, "PostgresDatabase"));
    assert(CodeGraphHasSymbol(cg, "ApiService"));
    assert(CodeGraphHasSymbol(cg, "login"));
    printf("  [PASS] C, Python, and TypeScript symbols correctly registered in graph\n");

    /* Verify symbols from ignored folders DO NOT exist */
    assert(!CodeGraphHasSymbol(cg, "GitTrap"));
    assert(!CodeGraphHasSymbol(cg, "NodeTrap"));
    assert(!CodeGraphHasSymbol(cg, "BuildTrap"));
    printf("  [PASS] Ignored folders strictly excluded from code graph (zero leakage)\n");

    /* ========================================================
       Test 3: Cross-Module Blast Radius on Ingested Repo
       ======================================================== */
    printf("\n=== Test 3: Blast Radius on Ingested Repo ===\n");
    CODE_BLAST_RADIUS radius;
    int ok = CodeGraphComputeBlastRadius(cg, "HelperFunc", 3, &radius);
    assert(ok);
    assert(radius.entry_count >= 1);
    assert(strcmp(radius.entries[0].symbol_name, "StartServer") == 0);
    printf("  [PASS] Blast radius correctly identifies 'StartServer' as caller of 'HelperFunc'\n");

    char md_report[2048];
    ok = CodeGraphFormatBlastRadius(cg, &radius, md_report, sizeof(md_report));
    assert(ok);
    assert(strstr(md_report, "HelperFunc") != NULL);
    assert(strstr(md_report, "StartServer") != NULL);
    printf("  [PASS] Markdown impact report generated for repository entity\n");

    /* Cleanup */
    CodeGraphDestroy(cg);

    remove_file(file_c);
    remove_file(file_py);
    remove_file(file_ts);
    remove_file(file_git);
    remove_file(file_node);
    remove_file(file_build);

#ifdef _WIN32
    RemoveDirectoryA(dir_src);
    RemoveDirectoryA(dir_py);
    RemoveDirectoryA(dir_ts);
    RemoveDirectoryA(dir_git);
    RemoveDirectoryA(dir_node);
    RemoveDirectoryA(dir_build);
    RemoveDirectoryA(test_root);
#else
    rmdir(dir_src);
    rmdir(dir_py);
    rmdir(dir_ts);
    rmdir(dir_git);
    rmdir(dir_node);
    rmdir(dir_build);
    rmdir(test_root);
#endif
    printf("  [PASS] Fixtures and temporary directories cleaned up cleanly\n");

    printf("\n=========================================================\n");
    printf("  RESULTS: All indexer tests passed (100%%)               \n");
    printf("=========================================================\n");

    return 0;
}
