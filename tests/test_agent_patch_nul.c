#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* A NUL byte in the target of PatchApplyAtomic (POSIX and Windows/NTFS). Before m221 a hunk before the first
   NUL succeeded and the file was cut there: measured m220 on Linux, 13 bytes
   61 62 0a 63 00 64 65 0a 74 61 69 6c 0a became 41 42 0a 63 with the call returning 1. The work
   buffers are C strings. Rule since m221: a target that holds a NUL byte is refused (read_target), so
   PatchVerifyPlan and PatchApplyAtomic both fail closed and the file stays as it was.
   Cells: A hunk before the NUL, refused and bytes unchanged; B the refusal names the NUL (io_diag);
   C hunk after the NUL, refused and unchanged; D that refusal names the NUL; E NUL as the first byte, hunk on
   later text, refused and unchanged; F NUL as the last byte, hunk before it, refused and unchanged;
   G PatchVerifyPlan on the file of A (before any apply) reports not applicable; H control: no NUL, CRLF file, the hunk applies
   and the bytes are exact (so the refusals are not blanket).
   AGENT_PATCH_NUL_MUTANT removes the check. Predicted before the first run, from the source: with the mutant
   exactly A, B, D, F, G fail (A and F: the file is cut or the NUL dropped; B and D: no NUL diagnostic; G:
   verify sees the text before the NUL as applicable). C and E stay refused (text after a NUL is not found
   by strstr; an empty buffer finds nothing) and H passes. The mutant build exits 0 only on exactly that set. */
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
#define WS "test_agent_patch_nul_scratch"
static unsigned fail_mask;
static int fails;
static void ck(int bit, int ok, const char *lbl)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", lbl);
    if (!ok) { fails++; fail_mask |= 1u << bit; }
}
static void put(const char *in, size_t n)
{
    FILE *f;
    if (system(RMWS) != 0) exit(2);
    MKWS(WS);
    f = fopen(WS "/a.bin", "wb");
    if (!f || fwrite(in, 1, n, f) != n) exit(2);
    fclose(f);
}
static size_t get(char *b, size_t cap)
{
    FILE *f = fopen(WS "/a.bin", "rb");
    size_t k = f ? fread(b, 1, cap, f) : 0;
    if (f) fclose(f);
    return k;
}
static int same(const char *in, size_t n)
{
    char b[256];
    size_t k = get(b, sizeof b);
    return k == n && memcmp(b, in, n) == 0;
}
static void plan(PATCH_PLAN *p, const char *old, const char *nw)
{
    PatchPlanInit(p, WS "/a.bin");
    PatchPlanSetWorkspace(p, WS);
    PatchPlanAddHunk(p, 0, "", old, nw, "");
}
int main(void)
{
    static const char A[] = {'a', 'b', '\n', 'c', 0, 'd', 'e', '\n', 't', 'a', 'i', 'l', '\n'};
    static const char E[] = {0, 'a', 'b', '\n', 'c', '\n'};
    static const char F[] = {'a', 'b', '\n', 'c', '\n', 0};
    PATCH_PLAN p;
    PATCH_VERIFY_REPORT rep;
    int rc;
    put(A, sizeof A);
    plan(&p, "ab\n", "AB\n");
    memset(&rep, 0, sizeof rep);
    rc = PatchVerifyPlan(&p, &rep); /* before any apply: the mutant must still see the file as it was */
    ck(6, rc == 0 && !rep.is_applicable, "G verify reports not applicable");
    rc = PatchApplyAtomic(&p);
    ck(0, rc != 1 && same(A, sizeof A), "A hunk before the NUL is refused, bytes unchanged");
    ck(1, strstr(p.io_diag, "NUL") != NULL, "B the refusal names the NUL");
    PatchPlanFree(&p);
    put(A, sizeof A);
    plan(&p, "tail\n", "TAIL\n");
    rc = PatchApplyAtomic(&p);
    ck(2, rc != 1 && same(A, sizeof A), "C hunk after the NUL is refused, bytes unchanged");
    ck(3, strstr(p.io_diag, "NUL") != NULL, "D that refusal names the NUL");
    PatchPlanFree(&p);
    put(E, sizeof E);
    plan(&p, "c\n", "C\n");
    rc = PatchApplyAtomic(&p);
    ck(4, rc != 1 && same(E, sizeof E), "E NUL as the first byte, refused, bytes unchanged");
    PatchPlanFree(&p);
    put(F, sizeof F);
    plan(&p, "ab\n", "AB\n");
    rc = PatchApplyAtomic(&p);
    ck(5, rc != 1 && same(F, sizeof F), "F NUL as the last byte, refused, bytes unchanged");
    PatchPlanFree(&p);
    {
        static const char H[] = "ab\r\nc\r\n", W[] = "AB\r\nc\r\n";
        char b[64];
        size_t k;
        put(H, sizeof H - 1);
        plan(&p, "ab\n", "AB\n");
        rc = PatchApplyAtomic(&p);
        k = get(b, sizeof b);
        ck(7, rc == 1 && k == sizeof W - 1 && memcmp(b, W, k) == 0, "H control: no NUL, CRLF file, hunk applies");
        PatchPlanFree(&p);
    }
    if (system(RMWS) != 0) return 2;
    /* bits: A 0, B 1, C 2, D 3, E 4, F 5, G 6, H 7 */
#ifdef AGENT_PATCH_NUL_MUTANT
    printf("mutant fail mask %02x (expected 6b: A B D F G)\n", fail_mask);
    if (fail_mask == 0x6bu) { printf("MUTANT killed by exactly A B D F G\n"); return 0; }
    printf("MUTANT: unexpected failing set\n");
    return 1;
#else
    printf("%s (%d cells wrong)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
#endif
}
