#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* AgentPatch fuzz of plans with several hunks that carry context strings, and of files with no final newline (POSIX and Windows/NTFS).
   Files: 5 to 8 lines "d0".."d2" (lines repeat on purpose), all LF or all CRLF, and in a third of the rounds the last line has no ending.
   A plan holds 1 to 3 hunks. Each hunk is built from the text as the EARLIER hunks leave it: one target line (the last line without its
   newline when the file has none), replacement the line "R<round>.<hunk>" (same ending rule), context_before 0 to 2 lines and context_after
   0 to 2 lines written with LF only; in a quarter of the rounds the first context line is cut one byte from its start, so the context starts
   mid-line; in a fifth the first context line is replaced by a line not in the file. Oracle, written from the documented rule and not from the
   source: the text is held with LF endings; for each hunk in plan order count the places (overlaps included) where context_before + target +
   context_after occurs as a substring of the current text; exactly one place replaces that text by context_before + replacement + context_after,
   otherwise the plan is refused at that hunk (0 places NOT_FOUND, 2 or more AMBIGUOUS) and the generator stops adding hunks. Per round: verify
   and apply agree. Applicable plan: matched_line is the line of the first hunk's place in the ORIGINAL text, line_drift and status follow
   expected_line of hunk 1 (0, true, or wrong), occurrences_found is the first hunk's count, apply returns 1, the file equals the model (CRLF files
   with every LF turned to CRLF), PatchRollback restores the original bytes. Refused plan: verify rc 0 with the status and "Hunk N", apply rc 0 with
   "Hunk N" in io_diag, file byte-identical. The root holds only the target and .fstxn.lock. A hunk may act on text an earlier hunk wrote (the
   accepted sequential rule). Limits: mixed endings not covered here (m225 covers them without context), whole-line targets only, 3 hunks at most,
   files of a few lines, no NUL byte, one runner per OS, cooperating writers, no crash points. */
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
#define WS "test_agent_patch_ctxm_fuzz_scratch"
#define MAXF 8192
#define NL 8
static uint32_t rs;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int g_k, g_cls, g_kind;
static void ck(int ok, const char *m)
{
    if (!ok) { fprintf(stderr, "FAIL %s (round %d, class %d, kind %d)\n", m, g_k, g_cls, g_kind); exit(1); }
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
static char after[MAXF], cur[MAXF], orig[MAXF], model[MAXF];
static size_t count_sub(const char *hay, const char *nd, size_t *first)
{
    size_t n = 0, hl = strlen(hay), nl = strlen(nd), i;
    for (i = 0; i + nl <= hl; i++) if (!memcmp(hay + i, nd, nl)) { if (!n) *first = i; n++; }
    return n;
}
static void to_crlf(const char *in, char *out, size_t *on)
{
    size_t k = 0;
    for (; *in; in++) { if (*in == '\n') out[k++] = '\r'; out[k++] = *in; }
    out[k] = 0; *on = k;
}
int main(void)
{
    static const uint32_t seeds[4] = {1786707969u, 1u, 7u, 12648430u};
    unsigned total = 0, n_ok = 0, n_refused = 0, n_multi = 0, n_noeol = 0, n_cut = 0, n_dep = 0, n_late_refusal = 0, n_drift = 0;
    int sd;
    uint32_t k;
    for (sd = 0; sd < 4; sd++)
    {
        rs = seeds[sd];
        for (k = 0; k < 100; k++)
        {
            int crlf = (int)(k % 2), noeol = (int)((k / 2) % 3 == 0), cut = (int)(rnd() % 4 == 0), wrong = (int)(rnd() % 5 == 0);
            int nh = 1 + (int)(rnd() % 3), h, nplaced = 0, refused_at = -1, refused_cnt = 0, nlines, i;
            char L[NL][8];
            char cbs[3][64], tgs[3][16], rps[3][32], cas[3][64];
            uint32_t exp_line = 0;
            size_t first0 = 0, cnt0 = 0, fn, an, wn;
            char crl[MAXF];
            PATCH_PLAN p;
            PATCH_VERIFY_REPORT r;
            int rc, wrote_r = 0;
            g_k = (int)k; g_cls = crlf; g_kind = noeol;
            nlines = 5 + (int)(rnd() % 4);
            cur[0] = 0;
            for (i = 0; i < nlines; i++) { sprintf(L[i], "d%u", (unsigned)(rnd() % 3)); strcat(cur, L[i]); if (i < nlines - 1 || !noeol) strcat(cur, "\n"); }
            strcpy(orig, cur);
            if (crlf) to_crlf(cur, crl, &fn); else { strcpy(crl, cur); fn = strlen(cur); }
            for (h = 0; h < nh && refused_at < 0; h++)
            {
                /* split the current text into lines, pick a target line */
                char *ln[32]; int nl = 0, s, nb, na, hasnl_last; char tmp[MAXF]; size_t cntp, first = 0;
                char needle[256];
                strcpy(tmp, cur);
                { char *q = tmp; ln[nl++] = q; for (; *q; q++) if (*q == '\n') { *q = 0; if (q[1]) ln[nl++] = q + 1; } }
                hasnl_last = cur[strlen(cur) - 1] == '\n';
                s = (int)(rnd() % (uint32_t)nl);
                nb = (int)(rnd() % 3); na = (int)(rnd() % 3);
                if (nb > s) nb = s;
                if (na > nl - 1 - s) na = nl - 1 - s;
                cbs[h][0] = cas[h][0] = 0;
                if (h == 0 && wrong) strcpy(cbs[h], "ZZ\n");
                for (i = s - nb; i < s; i++) { strcat(cbs[h], ln[i]); strcat(cbs[h], "\n"); }
                for (i = s + 1; i <= s + na; i++) { strcat(cas[h], ln[i]); if (i < nl - 1 || hasnl_last) strcat(cas[h], "\n"); }
                if (cut && nb > 0 && h == 0 && !wrong) { memmove(cbs[h], cbs[h] + 1, strlen(cbs[h])); n_cut++; }
                snprintf(tgs[h], sizeof tgs[h], "%s%s", ln[s], (s < nl - 1 || hasnl_last) ? "\n" : "");
                snprintf(rps[h], sizeof rps[h], "R%u.%d%s", (unsigned)k, h, (s < nl - 1 || hasnl_last) ? "\n" : "");
                snprintf(needle, sizeof needle, "%s%s%s", cbs[h], tgs[h], cas[h]);
                cntp = count_sub(cur, needle, &first);
                if (h == 0) { cnt0 = cntp; first0 = first; }
                if (cntp != 1) { refused_at = h; refused_cnt = (int)cntp; nplaced = h + 1; break; }
                /* apply to the model text */
                {
                    char nw[MAXF]; size_t pre = first, tl = strlen(needle);
                    memcpy(nw, cur, pre);
                    sprintf(nw + pre, "%s%s%s", cbs[h], rps[h], cas[h]);
                    strcat(nw, cur + pre + tl);
                    if (strstr(cur, "R")) wrote_r = 1;
                    if (h > 0 && strstr(tgs[h], "R")) n_dep++;
                    strcpy(cur, nw);
                }
                nplaced = h + 1;
            }
            { int ei = (int)(rnd() % 3); if (ei == 1) exp_line = 1; else if (ei == 2) exp_line = 1 + (uint32_t)(rnd() % 12); }
            (void)wrote_r;
            put(crl, fn);
            PatchPlanInit(&p, WS "/a.c");
            PatchPlanSetWorkspace(&p, WS);
            for (h = 0; h < nplaced; h++) ck(PatchPlanAddHunk(&p, h == 0 ? exp_line : 0, cbs[h], tgs[h], rps[h], cas[h]), "add hunk");
            rc = PatchVerifyPlan(&p, &r);
            if (refused_at < 0)
            {
                size_t pre0 = first0 + strlen(cbs[0]);
                uint32_t line = 1;
                int32_t dr;
                for (i = 0; (size_t)i < pre0; i++) if (orig[i] == '\n') line++;
                dr = exp_line ? (int32_t)line - (int32_t)exp_line : 0;
                ck(rc == 1 && r.is_applicable, "verify applicable");
                ck(r.occurrences_found == (uint32_t)cnt0, "occurrences_found is the first hunk's count");
                ck(r.matched_line == line, "matched_line is the first hunk's line in the original");
                ck(r.line_drift == dr, "line_drift");
                ck(r.status == (dr ? PATCH_CHECK_OFFSET_DRIFT : PATCH_CHECK_OK), "status OK or OFFSET_DRIFT");
                if (crlf) to_crlf(cur, model, &wn); else { strcpy(model, cur); wn = strlen(cur); }
                rc = PatchApplyAtomic(&p);
                ck(rc == 1, "apply");
                an = slurp(after, MAXF);
                ck(an == wn && !memcmp(after, model, wn), "file equals the model");
                rc = PatchRollback(&p);
                ck(rc == 1, "rollback");
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, crl, fn), "rollback restores the original bytes");
                n_ok++; if (nh > 1) n_multi++; if (noeol) n_noeol++; if (dr) n_drift++;
            }
            else
            {
                char want_h[16];
                snprintf(want_h, sizeof want_h, "Hunk %d", refused_at + 1);
                ck(rc == 0 && !r.is_applicable, "verify refuses");
                ck(r.status == (refused_cnt == 0 ? PATCH_CHECK_NOT_FOUND : PATCH_CHECK_AMBIGUOUS), "status NOT_FOUND or AMBIGUOUS");
                ck(strstr(r.diagnostic, want_h) != NULL, "diagnostic names the hunk");
                rc = PatchApplyAtomic(&p);
                ck(rc == 0 && strstr(p.io_diag, want_h) != NULL, "apply refuses and names the hunk");
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, crl, fn), "refused plan leaves the file byte-identical");
                n_refused++; if (refused_at > 0) n_late_refusal++;
            }
            ck(names() == 0, "the root holds only the target and the lock file");
            PatchPlanFree(&p);
            total++;
        }
        printf("ctxm fuzz seed=%u rounds=100\n", (unsigned)seeds[sd]);
        fflush(stdout);
    }
    ck(system(RMWS) == 0, "rm final");
    ck(n_ok && n_refused && n_multi && n_noeol && n_cut && n_dep && n_late_refusal && n_drift, "every class was exercised");
    printf("ctxm fuzz coverage: applied=%u refused=%u multi_hunk_applied=%u no_final_newline_applied=%u cut_context=%u dependent_hunks=%u refused_at_later_hunk=%u drift=%u\n", n_ok, n_refused, n_multi, n_noeol, n_cut, n_dep, n_late_refusal, n_drift);
    printf("ctxm fuzz done: seeds=4 rounds=400 ok=%u\n", total);
    return total == 400 ? 0 : 1;
}
