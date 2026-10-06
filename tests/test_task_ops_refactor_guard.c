/*
 * test_task_ops_refactor_guard.c - rename_symbol and literal_to_constant must not keep an edit they cannot check.
 * Found by the wording probe (m229, tools/wording_probe.py) on the public bank, measured on Linux with the agent binary:
 *   eb_bi_004 with its sentences in reverse order: "Operator rename_symbol: cmake_minimum_required -> CMakeLists ... (verified, kept)";
 *     the workspace holds only CMakeLists.txt, so the probe saw "compile -1->-1, run -1->-1" and nothing was checked;
 *   eb_mf_004 with only its second sentence: "Operator literal_to_constant: 0 -> UTIL_H in main.c (verified, kept)" added
 *     "#define UTIL_H 0" to main.c; the "0" came from "so the program still exits 0" and UTIL_H from "#ifndef and #define".
 * Rules since m230 (src/task_ops.c): both operators need a build probe (compile_before >= 0); rename_symbol also refuses a
 * token that is a CMake command, "CMakeLists", "Makefile" or the name or stem of a workspace file; literal_to_constant
 * skips a sentence that holds a preprocessor directive. Cells (synthetic workspaces copied from the two bank fixtures, texts
 * as the rewrites produced them; the bank itself is not used as a test oracle):
 *   A rename text on a CMake-only workspace: nothing kept, file unchanged.
 *   B the same text with a buildable main.c next to CMakeLists.txt: nothing kept (the token rule alone), file unchanged.
 *   C the literal text of eb_mf_004 without its first sentence: nothing kept, the three files unchanged.
 *   D control: "Constant OLD_SIZE should be NEW_SIZE everywhere." is kept and verified.
 *   E control: "Name the magic 17 as WIDTH_CELLS. Output must not change." is kept and verified.
 * Predicted for the build with the guards switched off (TaskOpsTestRefactorGuards(0), isolated test library only): exactly cells
 * A, B and C fail. Not covered: Windows runs of the agent binary, other task wordings, the real-text bank (separate measurement).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat.h"
#include "task_ops.h"

#define ROOT "test_task_ops_refactor_guard_scratch"
#ifdef _WIN32
#define RMROOT "if exist " ROOT " rmdir /s /q " ROOT " >NUL 2>NUL"
#else
#define RMROOT "rm -rf " ROOT
#endif
static unsigned fail_mask;
static int fails;
static void ck(int bit, int ok, const char *lbl)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", lbl);
    if (!ok) { fails++; fail_mask |= 1u << bit; }
}
static void put(const char *dir, const char *rel, const char *text)
{
    char p[1024];
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", dir, rel);
    f = fopen(p, "wb");
    if (!f) exit(2);
    fputs(text, f);
    fclose(f);
}
static char *get(const char *dir, const char *rel)
{
    static char buf[2][4096];
    static int k;
    char p[1024];
    FILE *f;
    size_t n = 0;
    char *b = buf[k++ & 1];
    snprintf(p, sizeof(p), "%s/%s", dir, rel);
    f = fopen(p, "rb");
    if (f) { n = fread(b, 1, 4095, f); fclose(f); }
    b[n] = 0;
    return b;
}
static void mk(char *out, size_t sz, const char *tag)
{
    snprintf(out, sz, ROOT "/%s", tag);
    _mkdir(out);
}
int main(void)
{
    static const char CML[] = "cmake_minimum_required(VERSION 3.10)\n";
    static const char MAINC[] = "#include \"util.h\"\nint main(void) { return util_value() == 1 ? 0 : 1; }\n";
    static const char UTILC[] = "#include \"util.h\"\nint util_value(void) { return 1; }\n";
    static const char UTILH[] = "#ifndef MAIN_H\n#define MAIN_H\nint util_value(void);\n#endif\n";
    static const char T_RENAME[] = "The checker runs cmake configure and fails while cmake warns that project() is absent. "
                                   "Add a project(<name> C) line after cmake_minimum_required. CMakeLists.txt is missing project().";
    static const char T_LIT[] = "Change it to UTIL_H in both #ifndef and #define so the program still exits 0 and the guard names match.";
    char d[256];
    TASK_OPS_REPORT r;
    int kept;
    if (system(RMROOT) != 0) return 2;
    _mkdir(ROOT);
#ifdef REFACTOR_GUARD_MUTANT
    TaskOpsTestRefactorGuards(0);
#endif
    mk(d, sizeof d, "a");
    put(d, "CMakeLists.txt", CML);
    kept = TaskOpsSolve(d, T_RENAME, &r);
    ck(0, !kept && !strcmp(get(d, "CMakeLists.txt"), CML), "A rename on a CMake-only workspace: nothing kept, file unchanged");
    mk(d, sizeof d, "b");
    put(d, "CMakeLists.txt", CML);
    put(d, "main.c", "int main(void) { return 0; }\n");
    kept = TaskOpsSolve(d, T_RENAME, &r);
    ck(1, !kept && !strcmp(get(d, "CMakeLists.txt"), CML), "B the same with a buildable main.c: nothing kept, file unchanged");
    mk(d, sizeof d, "c");
    put(d, "main.c", MAINC);
    put(d, "util.c", UTILC);
    put(d, "util.h", UTILH);
    kept = TaskOpsSolve(d, T_LIT, &r);
    ck(2, !kept && !strcmp(get(d, "main.c"), MAINC) && !strcmp(get(d, "util.c"), UTILC) && !strcmp(get(d, "util.h"), UTILH),
       "C literal text with a directive sentence: nothing kept, the three files unchanged");
    mk(d, sizeof d, "d");
    put(d, "lib.h", "#define OLD_SIZE 3\nint twice(int);\n");
    put(d, "lib.c", "#include \"lib.h\"\nint twice(int v) { return v * 2 + OLD_SIZE - OLD_SIZE; }\n");
    put(d, "main.c", "#include \"lib.h\"\nint main(void) { return twice(OLD_SIZE) == 6 ? 0 : 1; }\n");
    kept = TaskOpsSolve(d, "Constant OLD_SIZE should be NEW_SIZE everywhere.", &r);
    ck(3, kept && r.verified && strstr(get(d, "main.c"), "NEW_SIZE"), "D control: a real rename is kept and verified");
    mk(d, sizeof d, "e");
    put(d, "q.c", "#include <stdio.h>\nint main(void) { int w = 17; printf(\"%d\\n\", w * 17); return w == 17 ? 0 : 1; }\n");
    kept = TaskOpsSolve(d, "Name the magic 17 as WIDTH_CELLS. Output must not change.", &r);
    ck(4, kept && r.verified && strstr(get(d, "q.c"), "#define WIDTH_CELLS 17"), "E control: a real literal replacement is kept and verified");
    if (system(RMROOT) != 0) return 2;
#ifdef REFACTOR_GUARD_MUTANT
    printf("mutant fail mask %02x (expected 07: A B C)\n", fail_mask);
    if (fail_mask == 0x07u) { printf("MUTANT killed by exactly A B C\n"); return 0; }
    printf("MUTANT: unexpected failing set\n");
    return 1;
#else
    printf("%s (%d cells wrong)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
#endif
}
