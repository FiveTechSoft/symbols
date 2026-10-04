#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* Mixed line endings through PatchApplyAtomic (POSIX and Windows/NTFS).
   Before this test the writer used had_crlf = "any CRLF in the file" and
   rewrote EVERY LF to CRLF: patching one line of "a\r\nb\nc\r\n" changed the
   untouched line b. Measured 2026-10-04 on Linux, cells A, B, C below.
   With AGENT_PATCH_EOL_MUTANT defined (src restores CR on every LF once the
   file has any CRLF, the old behaviour) this target exits 0 only if some cell
   notices; exit 1 means the mutant SURVIVED.
   Lone CR (m97): strip_cr dropped EVERY CR, so a CR not before an LF was deleted from
   untouched text while the write reported success. Predicted from the source, then
   measured on Linux before any fix: J, K, L, M fail (got "ab\nC\n", "A\nb", "a\r\nB\n",
   "xy\nZ\n"), A to I pass. Rule after the fix: only the CR of a CRLF pair is dropped.
   Predictions before the first run of the fix: J to M and the new cells N, P, Q pass,
   O (needle with a lone CR against a file without it) is refused with the file unchanged.
   AGENT_PATCH_CR_MUTANT (strip every CR again) must be killed by J to M.
   R, S (the "\r\r\n replacement" edge, predicted before running): a replacement whose text
   ends in CR CR LF keeps one content CR and takes the file's own newline ending (LF file
   gives "b\r\n", CRLF file gives "b\r\r\n"). Measured on Linux: both pass; the CR mutant
   fails both.
   Windows: this file was POSIX only until m90. Prediction made before the first
   Windows run: all nine cells pass, because the line-ending logic is shared code
   and the Fs* write path was shown to pass newlines through (test_fs_win_newline). */
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

#define WS "test_agent_patch_eol_scratch"
static int mism;

static void cell(const char *lbl, const char *in, size_t n, const char *old, const char *nw,
                 const char *old2, const char *nw2, const char *want, size_t wn)
{
    PATCH_PLAN p;
    char b[512];
    size_t k;
    FILE *f;
    if (system(RMWS) != 0) exit(2);
    MKWS(WS);
    f = fopen(WS "/a.c", "wb");
    if (!f || fwrite(in, 1, n, f) != n) exit(2);
    fclose(f);
    PatchPlanInit(&p, WS "/a.c");
    PatchPlanSetWorkspace(&p, WS);
    PatchPlanAddHunk(&p, 1, "", old, nw, "");
    if (old2) PatchPlanAddHunk(&p, 1, "", old2, nw2, "");
    int rc = PatchApplyAtomic(&p);
    f = fopen(WS "/a.c", "rb");
    k = f ? fread(b, 1, sizeof b, f) : 0;
    if (f) fclose(f);
    int ok = rc == 1 && k == wn && memcmp(b, want, wn) == 0;
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", lbl);
    if (!ok)
    {
        mism++;
        printf("    rc=%d got=", rc);
        for (size_t i = 0; i < k; i++)
            b[i] == '\r' ? printf("\\r") : b[i] == '\n' ? printf("\\n") : printf("%c", b[i]);
        printf("\n");
    }
    PatchPlanFree(&p);
}

/* The hunk must NOT apply: rc != 1 and the file bytes stay exactly as they were. */
static void refused(const char *lbl, const char *in, size_t n, const char *old, const char *nw)
{
    PATCH_PLAN p;
    char b[512];
    size_t k;
    FILE *f;
    if (system(RMWS) != 0) exit(2);
    MKWS(WS);
    f = fopen(WS "/a.c", "wb");
    if (!f || fwrite(in, 1, n, f) != n) exit(2);
    fclose(f);
    PatchPlanInit(&p, WS "/a.c");
    PatchPlanSetWorkspace(&p, WS);
    PatchPlanAddHunk(&p, 1, "", old, nw, "");
    int rc = PatchApplyAtomic(&p);
    f = fopen(WS "/a.c", "rb");
    k = f ? fread(b, 1, sizeof b, f) : 0;
    if (f) fclose(f);
    int ok = rc != 1 && k == n && memcmp(b, in, n) == 0;
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", lbl);
    if (!ok)
    {
        mism++;
        printf("    rc=%d bytes %s\n", rc, (k == n && memcmp(b, in, n) == 0) ? "unchanged" : "CHANGED");
    }
    PatchPlanFree(&p);
}

int main(void)
{
#define C(l, in, o, n, o2, n2, w) cell(l, in, sizeof(in) - 1, o, n, o2, n2, w, sizeof(w) - 1)
    C("A mixed, edit the LF line b", "a\r\nb\nc\r\n", "b\n", "B\n", 0, 0, "a\r\nB\nc\r\n");
    C("B mixed, edit CRLF line a, LF line b untouched", "a\r\nb\nc\r\n", "a\n", "A\n", 0, 0, "A\r\nb\nc\r\n");
    C("C mostly LF, edit line a, CRLF line c untouched", "a\nb\nc\r\n", "a\n", "A\n", 0, 0, "A\nb\nc\r\n");
    C("D pure CRLF unchanged behaviour", "a\r\nb\r\nc\r\n", "b\n", "B\n", 0, 0, "a\r\nB\r\nc\r\n");
    C("E pure LF unchanged behaviour", "a\nb\nc\n", "b\n", "B\n", 0, 0, "a\nB\nc\n");
    C("F two hunks, first and last, middle LF kept", "a\r\nb\nc\r\n", "a\n", "A\n", "c\n", "C\n", "A\r\nb\nC\r\n");
    C("G no trailing newline, mixed", "a\r\nb\nc", "c", "C", 0, 0, "a\r\nb\nC");
    C("H hunk adds a line inside a CRLF line", "a\r\nb\nc\r\n", "a\n", "a\nx\n", 0, 0, "a\r\nx\r\nb\nc\r\n");
    C("I multi-line hunk keeps each line's own ending", "a\r\nb\nc\r\n", "a\nb\n", "A\nB\n", 0, 0, "A\r\nB\nc\r\n");
    C("J lone CR inside an untouched line", "a\rb\nc\n", "c\n", "C\n", 0, 0, "a\rb\nC\n");
    C("K lone CR at the end of an untouched last line", "a\nb\r", "a\n", "A\n", 0, 0, "A\nb\r");
    C("L CR CR LF line untouched", "a\r\r\nb\n", "b\n", "B\n", 0, 0, "a\r\r\nB\n");
    C("M lone CR inside an untouched line before the edit", "x\ry\nz\n", "z\n", "Z\n", 0, 0, "x\ry\nZ\n");
    C("N needle with a lone CR matches a file that has it", "a\rb\nc\n", "a\rb\n", "X\n", 0, 0, "X\nc\n");
    refused("O same needle does not match a file without the CR", "ab\nc\n", sizeof("ab\nc\n") - 1, "a\rb\n", "X\n");
    C("P CRLF needle still matches a CRLF file", "a\r\nb\r\nc\r\n", "a\r\nb\r\n", "A\r\nB\r\n", 0, 0, "A\r\nB\r\nc\r\n");
    C("Q replacement with a lone CR is written as is", "a\nb\n", "b\n", "b\rX\n", 0, 0, "a\nb\rX\n");
    C("R replacement CR CR LF in an LF file: content CR kept, LF ending kept", "a\nb\n", "b\n", "b\r\r\n", 0, 0, "a\nb\r\n");
    C("S replacement CR CR LF in a CRLF file: content CR kept, CRLF ending kept", "a\r\nb\r\n", "b\n", "b\r\r\n", 0, 0, "a\r\nb\r\r\n");
    if (system(RMWS) != 0) return 2;
#if defined(AGENT_PATCH_EOL_MUTANT) || defined(AGENT_PATCH_CR_MUTANT)
    if (mism) { printf("MUTANT killed: %d cells\n", mism); return 0; }
    printf("MUTANT SURVIVED\n");
    return 1;
#else
    printf("%s (%d cells wrong)\n", mism ? "FAILED" : "ALL PASS", mism);
    return mism ? 1 : 0;
#endif
}
