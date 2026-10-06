#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* Multi-hunk plans in PatchApplyAtomic (POSIX and Windows/NTFS).
   Found by reading src/agent_patch.c and probing on Linux (m224 session): PatchVerifyPlan checked every hunk
   against the ORIGINAL file, but the apply loop located each hunk with strstr in the file as already changed by
   the earlier hunks. A plan that verified cleanly could therefore edit a site the verifier never checked.
   Measured before the fix: file "aaa bbb ccc ddd" (one word per line), hunk A "aaa" -> "ddd", hunk B "ddd" -> "ZZZ":
   verify reported applicable at line 1 and the apply wrote "ZZZ bbb ccc ddd" (B hit the copy A had written).
   Rule since the fix: verify and apply run ONE routine; hunks apply in plan order, each located in the buffer as the earlier
   hunks left it, and each must occur exactly once there, else the plan is refused with the hunk number. Kept on purpose:
   src/agent_runner.c builds a missing-header plan whose second hunk acts on text the first hunk re-emits.
   Measured limits, asserted below: the result depends on the ORDER of the hunks (cell I), and a plan whose later hunk
   becomes ambiguous only after an earlier hunk is refused even when it would be harmless (cell D, fail closed).
   Cells (LF files unless stated; one word per line "aaa bbb ccc ddd"):
   A hunk "aaa\nbbb\n" then hunk "bbb\n": the second target is gone after the first; refused (Hunk 2 not found) by verify and apply, file unchanged.
   B the same hunk twice: refused the same way.
   C wrong site: the case above is refused (Hunk 2 found 2 times), file unchanged.
   D hunk ddd->ccc then ccc->QQQ: refused (Hunk 2 found 2 times), file unchanged. Fail closed.
   E dependent hunk: aaa->NEW then NEW->YYY is accepted and gives "YYY bbb ccc ddd" (what the runner does).
   F disjoint hunks of different lengths in either plan order give the same bytes, rollback restores.
   G touching hunks ("aaa\n", "bbb\n") both apply.
   H mixed endings: file "aaa\r\nbbb\nccc\r\nddd\n", hunk "aaa\nbbb\n" -> three lines, hunk "ddd\n" -> "D\n": the k-th new newline takes
   the k-th ending of the replaced text, extra ones the last, untouched lines keep theirs: "A1\r\nA2\nA3\nccc\r\nD\n" in both plan orders.
   I order dependence: hunk ddd->ZZZ then aaa->ddd is accepted and gives "ddd bbb ccc ZZZ", while the reverse order is refused (cell C).
   Predicted before running the fixed build (from the source): all cells pass; against the old src A, B (silent refusal
   without "Hunk 2"), C, D, E, I differ. Predicted for the mutant AGENT_PATCH_UNIQ_MUTANT (later hunks take the first match):
   exactly C and D fail, the rest pass. */
#include "agent_patch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKWS(p) _mkdir(p)
#define RMWS "if exist " WS " rmdir /s /q " WS " >NUL 2>NUL"
#else
#include <sys/stat.h>
#define MKWS(p) mkdir((p), 0700)
#define RMWS "rm -rf " WS
#endif
#define WS "test_agent_patch_multi_scratch"
static unsigned fail_mask;
static int fails;
static void ck(int bit, int ok, const char *lbl)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", lbl);
    if (!ok) { fails++; fail_mask |= 1u << bit; }
}
static void put(const char *in)
{
    FILE *f;
    if (system(RMWS) != 0) exit(2);
    MKWS(WS);
    f = fopen(WS "/a.txt", "wb");
    if (!f || fwrite(in, 1, strlen(in), f) != strlen(in)) exit(2);
    fclose(f);
}
static size_t get(char *b, size_t cap)
{
    FILE *f = fopen(WS "/a.txt", "rb");
    size_t k = f ? fread(b, 1, cap - 1, f) : 0;
    if (f) fclose(f);
    b[k] = 0;
    return k;
}
static int is(const char *want)
{
    char b[512];
    get(b, sizeof b);
    return strcmp(b, want) == 0;
}
static void start(PATCH_PLAN *p)
{
    PatchPlanInit(p, WS "/a.txt");
    PatchPlanSetWorkspace(p, WS);
}
static int has(const char *hay, const char *needle) { return strstr(hay, needle) != NULL; }
int main(void)
{
    static const char O[] = "aaa\nbbb\nccc\nddd\n";
    PATCH_PLAN p;
    PATCH_VERIFY_REPORT r;
    int rc, vr;

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "aaa\nbbb\n", "AAA\nBBB\n", "");
    PatchPlanAddHunk(&p, 0, "", "bbb\n", "XXX\n", "");
    vr = PatchVerifyPlan(&p, &r);
    rc = PatchApplyAtomic(&p);
    ck(0, vr == 0 && !r.is_applicable && r.status == PATCH_CHECK_NOT_FOUND && has(r.diagnostic, "Hunk 2")
          && rc == 0 && has(p.io_diag, "Hunk 2") && is(O),
       "A second target gone after the first hunk: refused by verify and apply, file unchanged");
    PatchPlanFree(&p);

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "bbb\n", "BBB\n", "");
    PatchPlanAddHunk(&p, 0, "", "bbb\n", "BBB\n", "");
    vr = PatchVerifyPlan(&p, &r);
    rc = PatchApplyAtomic(&p);
    ck(1, vr == 0 && r.status == PATCH_CHECK_NOT_FOUND && has(r.diagnostic, "Hunk 2") && rc == 0 && has(p.io_diag, "Hunk 2") && is(O),
       "B the same hunk twice refused, file unchanged");
    PatchPlanFree(&p);

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "aaa\n", "ddd\n", "");
    PatchPlanAddHunk(&p, 0, "", "ddd\n", "ZZZ\n", "");
    vr = PatchVerifyPlan(&p, &r);
    rc = PatchApplyAtomic(&p);
    ck(2, vr == 0 && r.status == PATCH_CHECK_AMBIGUOUS && has(r.diagnostic, "Hunk 2") && has(r.diagnostic, "2 times")
          && rc == 0 && has(p.io_diag, "Hunk 2") && is(O),
       "C wrong site refused: the second target occurs twice after the first hunk, file unchanged");
    PatchPlanFree(&p);

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "ddd\n", "ccc\n", "");
    PatchPlanAddHunk(&p, 0, "", "ccc\n", "QQQ\n", "");
    vr = PatchVerifyPlan(&p, &r);
    rc = PatchApplyAtomic(&p);
    ck(3, vr == 0 && r.status == PATCH_CHECK_AMBIGUOUS && rc == 0 && is(O),
       "D later hunk ambiguous after an earlier one: refused, file unchanged (fail closed)");
    PatchPlanFree(&p);

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "aaa\n", "NEW\n", "");
    PatchPlanAddHunk(&p, 0, "", "NEW\n", "YYY\n", "");
    vr = PatchVerifyPlan(&p, &r);
    rc = PatchApplyAtomic(&p);
    ck(4, vr == 1 && r.is_applicable && rc == 1 && is("YYY\nbbb\nccc\nddd\n"),
       "E dependent hunk (target written by the first hunk) is accepted");
    PatchPlanFree(&p);

    {
        int ok = 1, ord;
        for (ord = 0; ord < 2; ord++)
        {
            put(O); start(&p);
            if (ord == 0)
            {
                PatchPlanAddHunk(&p, 0, "", "aaa\n", "AAAAAAAA\n", "");
                PatchPlanAddHunk(&p, 0, "", "ccc\n", "C\n", "");
            }
            else
            {
                PatchPlanAddHunk(&p, 0, "", "ccc\n", "C\n", "");
                PatchPlanAddHunk(&p, 0, "", "aaa\n", "AAAAAAAA\n", "");
            }
            rc = PatchApplyAtomic(&p);
            ok = ok && rc == 1 && is("AAAAAAAA\nbbb\nC\nddd\n");
            rc = PatchRollback(&p);
            ok = ok && rc == 1 && is(O);
            PatchPlanFree(&p);
        }
        ck(5, ok, "F plan order does not matter, different lengths, rollback restores");
    }

    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "aaa\n", "AAA\n", "");
    PatchPlanAddHunk(&p, 0, "", "bbb\n", "BBB\n", "");
    rc = PatchApplyAtomic(&p);
    ck(6, rc == 1 && is("AAA\nBBB\nccc\nddd\n"), "G touching hunks are not an overlap");
    PatchPlanFree(&p);

    {
        static const char M[] = "aaa\r\nbbb\nccc\r\nddd\n";
        int ok = 1, ord;
        for (ord = 0; ord < 2; ord++)
        {
            put(M); start(&p);
            if (ord == 0)
            {
                PatchPlanAddHunk(&p, 0, "", "aaa\nbbb\n", "A1\nA2\nA3\n", "");
                PatchPlanAddHunk(&p, 0, "", "ddd\n", "D\n", "");
            }
            else
            {
                PatchPlanAddHunk(&p, 0, "", "ddd\n", "D\n", "");
                PatchPlanAddHunk(&p, 0, "", "aaa\nbbb\n", "A1\nA2\nA3\n", "");
            }
            rc = PatchApplyAtomic(&p);
            ok = ok && rc == 1 && is("A1\r\nA2\nA3\nccc\r\nD\n");
            PatchPlanFree(&p);
        }
        ck(7, ok, "H mixed endings: k-th newline rule holds across two hunks in both orders");
    }
    put(O); start(&p);
    PatchPlanAddHunk(&p, 0, "", "ddd\n", "ZZZ\n", "");
    PatchPlanAddHunk(&p, 0, "", "aaa\n", "ddd\n", "");
    rc = PatchApplyAtomic(&p);
    ck(8, rc == 1 && is("ddd\nbbb\nccc\nZZZ\n"), "I order dependence: the reverse of C is accepted and edits the original ddd");
    PatchPlanFree(&p);
    if (system(RMWS) != 0) return 2;
    /* bits: A 0, B 1, C 2, D 3, E 4, F 5, G 6, H 7, I 8 */
#ifdef AGENT_PATCH_UNIQ_MUTANT
    printf("mutant fail mask %02x (expected 0c: C D)\n", fail_mask);
    if (fail_mask == 0x0cu) { printf("MUTANT killed by exactly C D\n"); return 0; }
    printf("MUTANT: unexpected failing set\n");
    return 1;
#else
    printf("%s (%d cells wrong)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
#endif
}
