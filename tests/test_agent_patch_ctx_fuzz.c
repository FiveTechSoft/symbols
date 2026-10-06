#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* AgentPatch context-string and expected_line fuzz (POSIX and Windows/NTFS), no diff parser. Files: 6 to 8 lines, each "d0",
   "d1" or "d2" (so lines repeat on purpose), LF only, CRLF only or mixed, every line ending present, no NUL byte. A plan is ONE
   hunk: target = one line, replacement = the line "R<round>", context_before = 0 to 2 lines and context_after = 0 to 2 lines
   (clipped at the file ends, each with its newline, written with LF only even when the file is CRLF), expected_line 0, the true
   line, or a wrong one. A third of the rounds replace context_before by a line that is not in the file. Oracle, written from the
   documented rule and not from the source: the needle is the lines context_before + target + context_after; count the line
   positions where those lines occur in a row (count). count 1: verify is applicable, matched_line is the target's line at that
   place (1-based), line_drift is matched_line minus expected_line when expected_line is not 0, status OK or OFFSET_DRIFT; apply
   returns 1, only that line changed and it keeps its own line ending; PatchRollback restores the original bytes. count 0: NOT_FOUND.
   count 2 or more: AMBIGUOUS with occurrences_found == count, whatever expected_line says (expected_line does not choose a
   place); apply refuses and the file is byte-identical. The root holds only the target and .fstxn.lock. One runner per OS,
   cooperating writers, no crash points, files of a few lines (limits). */
#include "agent_patch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKWS(p) _mkdir(p)
#define RMWS "if exist " WS " rmdir /s /q " WS " >NUL 2>NUL"
#else
#include <sys/stat.h>
#include <dirent.h>
#define MKWS(p) mkdir((p), 0700)
#define RMWS "rm -rf " WS
#endif
#define WS "test_agent_patch_multi_fuzz_scratch"
#define MAXF 8192
#define NL 8
static uint32_t rs;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int g_k, g_cls, g_kind;
static void ck(int ok, const char *m)
{
    if (!ok) { fprintf(stderr, "FAIL %s (round %d, class %d, kind %d)\n", m, g_k, g_cls, g_kind); exit(1); }
}
static char file[MAXF], want[MAXF], after[MAXF], line[NL][8];
static size_t start[NL], llen[NL];
static int eol[NL];
static int nlines;
static size_t build(int cls)
{
    size_t n = 0;
    int i;
    nlines = 6 + (int)(rnd() % 3);
    for (i = 0; i < nlines; i++)
    {
        sprintf(line[i], "d%u", (unsigned)(rnd() % 3));
        eol[i] = cls == 0 ? 1 : cls == 1 ? 2 : (1 + (int)(rnd() & 1));
        start[i] = n;
        memcpy(file + n, line[i], 2);
        n += 2;
        if (eol[i] == 2) file[n++] = '\r';
        file[n++] = '\n';
        llen[i] = n - start[i];
    }
    return n;
}
static void put(const char *b, size_t n)
{
    FILE *f;
    ck(system(RMWS) == 0, "rm");
    ck(MKWS(WS) == 0, "mkdir");
    f = fopen(WS "/a.c", "wb");
    ck(f != NULL && fwrite(b, 1, n, f) == n, "put");
    ck(fclose(f) == 0, "put close");
}
static size_t slurp(char *b, size_t cap)
{
    FILE *f = fopen(WS "/a.c", "rb");
    size_t n;
    ck(f != NULL, "slurp open");
    n = fread(b, 1, cap, f);
    fclose(f);
    return n;
}
static int names(void) /* entries of the root other than the target and the lock file */
{
    int n = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA d;
    HANDLE f = FindFirstFileA(WS "\\*", &d);
    if (f == INVALID_HANDLE_VALUE) return -1;
    do { if (strcmp(d.cFileName, ".") && strcmp(d.cFileName, "..") && strcmp(d.cFileName, "a.c") && strcmp(d.cFileName, ".fstxn.lock")) n++; } while (FindNextFileA(f, &d));
    FindClose(f);
#else
    struct dirent *e;
    DIR *d = opendir(WS);
    if (!d) return -1;
    while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") && strcmp(e->d_name, "a.c") && strcmp(e->d_name, ".fstxn.lock")) n++;
    closedir(d);
#endif
    return n;
}
int main(void)
{
    static const uint32_t seeds[4] = {1786707969u, 1u, 7u, 12648430u};
    unsigned total = 0, n_one = 0, n_many = 0, n_none = 0, n_drift = 0, n_ctx = 0, n_exp_ambig = 0;
    int sd;
    uint32_t k;
    for (sd = 0; sd < 4; sd++)
    {
        rs = seeds[sd];
        for (k = 0; k < 100; k++)
        {
            int cls = (int)(k % 3), s, nb, na, wrong = (int)(rnd() % 3 == 0), i, m, count = 0, at = -1, ei;
            char cb[64] = "", ca[64] = "", tg[16], rp[32];
            uint32_t exp_line = 0;
            size_t fn, wn = 0, an;
            PATCH_PLAN p;
            PATCH_VERIFY_REPORT r;
            int rc, nl_b, nl_a;
            g_k = (int)k; g_cls = cls; g_kind = wrong;
            fn = build(cls);
            s = (int)(rnd() % (uint32_t)nlines);
            nb = (int)(rnd() % 3); na = (int)(rnd() % 3);
            if (nb > s) nb = s;
            if (na > nlines - 1 - s) na = nlines - 1 - s;
            nl_b = nb; nl_a = na;
            if (wrong) { strcpy(cb, "ZZ\n"); }
            for (i = s - nb; i < s; i++) { strcat(cb, line[i]); strcat(cb, "\n"); }
            for (i = s + 1; i <= s + na; i++) { strcat(ca, line[i]); strcat(ca, "\n"); }
            snprintf(tg, sizeof tg, "%s\n", line[s]);
            snprintf(rp, sizeof rp, "R%u\n", (unsigned)k);
            /* oracle: count line positions */
            if (!wrong)
            {
                for (m = 0; m + nl_b + 1 + nl_a <= nlines; m++)
                {
                    int ok = 1;
                    for (i = 0; i < nl_b + 1 + nl_a; i++) if (strcmp(line[m + i], line[s - nl_b + i])) ok = 0;
                    if (ok) { count++; if (count == 1) at = m + nl_b; }
                }
            }
            ei = (int)(rnd() % 3);
            if (ei == 1 && count == 1) exp_line = (uint32_t)at + 1;
            else if (ei == 1) exp_line = (uint32_t)s + 1;
            else if (ei == 2) exp_line = 1 + (uint32_t)(rnd() % 12);
            put(file, fn);
            PatchPlanInit(&p, WS "/a.c");
            PatchPlanSetWorkspace(&p, WS);
            ck(PatchPlanAddHunk(&p, exp_line, cb, tg, rp, ca), "add hunk");
            rc = PatchVerifyPlan(&p, &r);
            if (count == 1)
            {
                int32_t dr = exp_line ? (int32_t)(at + 1) - (int32_t)exp_line : 0;
                ck(rc == 1 && r.is_applicable, "verify applicable");
                ck(r.matched_line == (uint32_t)at + 1, "matched_line is the target's line at the unique place");
                ck(r.line_drift == dr, "line_drift is matched_line minus expected_line");
                ck(r.status == (dr ? PATCH_CHECK_OFFSET_DRIFT : PATCH_CHECK_OK), "status OK or OFFSET_DRIFT");
                for (i = 0; i < nlines; i++)
                {
                    if (i == at) { char l[16]; size_t ln = (size_t)sprintf(l, "R%u", (unsigned)k); memcpy(want + wn, l, ln); wn += ln; if (eol[i] == 2) want[wn++] = '\r'; want[wn++] = '\n'; }
                    else { memcpy(want + wn, file + start[i], llen[i]); wn += llen[i]; }
                }
                rc = PatchApplyAtomic(&p);
                ck(rc == 1, "apply");
                an = slurp(after, MAXF);
                ck(an == wn && !memcmp(after, want, wn), "file equals the model");
                rc = PatchRollback(&p);
                ck(rc == 1, "rollback");
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, file, fn), "rollback restores the original bytes");
                n_one++; if (dr) n_drift++; if (nb + na > 0) n_ctx++;
            }
            else
            {
                ck(rc == 0 && !r.is_applicable, "verify refuses");
                ck(r.status == (count == 0 ? PATCH_CHECK_NOT_FOUND : PATCH_CHECK_AMBIGUOUS), "status NOT_FOUND or AMBIGUOUS");
                if (count) ck(r.occurrences_found == (uint32_t)count, "occurrences_found equals the count");
                ck(strstr(r.diagnostic, "Hunk 1") != NULL, "diagnostic names the hunk");
                rc = PatchApplyAtomic(&p);
                ck(rc == 0 && strstr(p.io_diag, "Hunk 1") != NULL, "apply refuses and names the hunk");
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, file, fn), "refused plan leaves the file byte-identical");
                if (count) { n_many++; if (exp_line) n_exp_ambig++; } else n_none++;
            }
            ck(names() == 0, "the root holds only the target and the lock file");
            PatchPlanFree(&p);
            total++;
        }
        printf("ctx fuzz seed=%u rounds=100\n", (unsigned)seeds[sd]);
        fflush(stdout);
    }
    ck(system(RMWS) == 0, "rm final");
    ck(n_one && n_many && n_none && n_drift && n_ctx && n_exp_ambig, "every class was exercised");
    printf("ctx fuzz coverage: unique=%u ambiguous=%u not_found=%u drift=%u unique_with_context=%u ambiguous_with_expected_line=%u\n", n_one, n_many, n_none, n_drift, n_ctx, n_exp_ambig);
    printf("ctx fuzz done: seeds=4 rounds=400 ok=%u\n", total);
    return total == 400 ? 0 : 1;
}
