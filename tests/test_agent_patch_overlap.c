#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* Overlapping matches in PatchVerifyPlan / PatchApplyAtomic (POSIX and Windows/NTFS).
   Found by the context-strings fuzz (m227 session) and probed on Linux: the occurrence count advanced past the whole needle after each
   match, so a needle that overlaps itself was counted once. Measured before the fix: file "aaa\n", hunk "aa" -> "XX": verify OK
   (occurrences_found 1, line 1) and apply wrote "XXa\n"; file "a\na\na\nb\n", hunk "a\na\n" -> "Y\n": verify OK, apply wrote "Y\na\nb\n".
   The same loop existed before m224. Rule since the fix: the count advances one byte per match, so overlapping matches count and
   the hunk is refused as ambiguous (Hunk N found 2 times), file unchanged. Not measured on Windows before the fix.
   Cells:
   A file "aaa\n", hunk "aa" -> "XX": verify rc 0, AMBIGUOUS, occurrences_found 2; apply rc 0, file unchanged.
   B file "a\na\na\nb\n", hunk "a\na\n" -> "Y\n": refused the same way, occurrences_found 2.
   C file "x\nx\nx\ny\n", context_before "x\n", target "x\n": needle "x\nx\n" overlaps itself, refused, occurrences_found 2.
   D control, non-overlapping twice: file "ab\nab\n", hunk "ab" -> "R": refused, occurrences_found 2 (passes with and without the fix).
   E control, unique: file "aaa\nb\n", hunk "aaa\n" -> "R\n": applies, "R\nb\n" (passes with and without the fix).
   F later hunk: file "k\naaa\n", hunk 1 "k\n" -> "k2\n", hunk 2 "aa" -> "XX": refused naming Hunk 2, file unchanged.
   Predicted before running: all cells pass on the fixed build. Predicted for AGENT_PATCH_OVERLAP_MUTANT (the old advance): exactly A B C F fail. */
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
#define WS "test_agent_patch_overlap_scratch"
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
static void refused(int bit, const char *lbl, const char *file, const char *cb, const char *tg, const char *rp, const char *h1t, const char *h1r, unsigned occ, const char *hn)
{
    PATCH_PLAN p; PATCH_VERIFY_REPORT r; int ok, rc1, rc2;
    put(file); start(&p);
    if (h1t) PatchPlanAddHunk(&p, 0, "", h1t, h1r, "");
    PatchPlanAddHunk(&p, 0, cb, tg, rp, "");
    rc1 = PatchVerifyPlan(&p, &r);
    ok = rc1 == 0 && !r.is_applicable && r.status == PATCH_CHECK_AMBIGUOUS && has(r.diagnostic, hn);
    if (!h1t) ok = ok && r.occurrences_found == occ;
    rc2 = PatchApplyAtomic(&p);
    ok = ok && rc2 == 0 && has(p.io_diag, hn) && is(file);
    ck(bit, ok, lbl);
    PatchPlanFree(&p);
}
int main(void)
{
    PATCH_PLAN p; int rc;
    refused(0, "A single line, text overlaps itself: refused, count 2", "aaa\n", "", "aa", "XX", NULL, NULL, 2, "Hunk 1");
    refused(1, "B two-line target overlaps itself: refused, count 2", "a\na\na\nb\n", "", "a\na\n", "Y\n", NULL, NULL, 2, "Hunk 1");
    refused(2, "C context makes the needle overlap itself: refused, count 2", "x\nx\nx\ny\n", "x\n", "x\n", "Z\n", NULL, NULL, 2, "Hunk 1");
    refused(3, "D control: two separate matches refused, count 2", "ab\nab\n", "", "ab", "R", NULL, NULL, 2, "Hunk 1");
    put("aaa\nb\n"); start(&p);
    PatchPlanAddHunk(&p, 0, "", "aaa\n", "R\n", "");
    rc = PatchApplyAtomic(&p);
    ck(4, rc == 1 && is("R\nb\n"), "E control: a unique hunk applies");
    PatchPlanFree(&p);
    refused(5, "F later hunk overlaps itself: refused naming Hunk 2", "k\naaa\n", "", "aa", "XX", "k\n", "k2\n", 0, "Hunk 2");
    if (system(RMWS) != 0) return 2;
#ifdef AGENT_PATCH_OVERLAP_MUTANT
    printf("mutant fail mask %02x (expected 27: A B C F)\n", fail_mask);
    if (fail_mask == 0x27u) { printf("MUTANT killed by exactly A B C F\n"); return 0; }
    printf("MUTANT: unexpected failing set\n");
    return 1;
#else
    printf("%s (%d cells wrong)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
#endif
}
