#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* Multi-hunk and multi-line AgentPatch fuzz (POSIX and Windows/NTFS), no diff parser: a plan is one target and hunks of
   four strings. Files: LF only, CRLF only, mixed, a lone CR inside a line, no final newline; 5 to 7 lines "L<i> <letters>",
   every line text unique, no NUL byte. A plan holds 1 to 3 DISJOINT hunks in a random plan order; each covers one line or two
   consecutive lines and is replaced by 1 to 3 lines "R<round>.<hunk>.<n> <digits>" (never equal to any target, so each hunk is
   unique at its own step). Oracle, written from the documented rule and not from the source: the expected file is rebuilt line by
   line; a replaced region takes, for its k-th new newline, the ending of the region's k-th newline, extra newlines the last one;
   untouched lines keep their bytes. Per round: verify applicable and matched_line is the first-listed hunk's line in the original;
   apply returns 1, the file equals the model and applied_content; PatchRollback restores the original bytes; the root holds only
   the target and .fstxn.lock. A second plan kind adds a hunk whose target does not exist, or repeats a hunk: refused with the hunk
   number, file byte-identical. One runner per OS, cooperating writers, no crash points, files of a few lines (limits). */
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
static char file[MAXF], want[MAXF], after[MAXF], line[NL][160];
static size_t start[NL], llen[NL];
static int eol[NL]; /* 0 none, 1 LF, 2 CRLF */
static int nlines;
static size_t build(int cls)
{
    size_t n = 0;
    int i;
    nlines = 5 + (int)(rnd() % 3);
    for (i = 0; i < nlines; i++)
    {
        size_t w = (size_t)sprintf(line[i], "L%d ", i);
        int j, m = 3 + (int)(rnd() % 12);
        for (j = 0; j < m; j++) line[i][w++] = (char)('a' + rnd() % 26);
        if (cls == 3 && i == 2) { line[i][w++] = '\r'; line[i][w++] = 'q'; }
        line[i][w] = 0;
        eol[i] = cls == 0 ? 1 : cls == 1 ? 2 : cls == 2 ? (1 + (int)(rnd() & 1)) : 1;
        if (cls == 4 && i == nlines - 1) eol[i] = 0;
        start[i] = n;
        memcpy(file + n, line[i], w);
        n += w;
        if (eol[i] == 2) file[n++] = '\r';
        if (eol[i]) file[n++] = '\n';
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
typedef struct { int s, e, m; char rep[3][48]; char tgt[400], rtxt[400]; } HK; /* lines s..e, m new lines */
int main(void)
{
    static const uint32_t seeds[4] = {1786707969u, 1u, 7u, 12648430u};
    unsigned total = 0, st_hunks = 0, st_multi = 0, st_grow = 0, st_multihunk = 0;
    int sd;
    uint32_t k;
    for (sd = 0; sd < 4; sd++)
    {
        rs = seeds[sd];
        for (k = 0; k < 100; k++)
        {
            int cls = (int)(k % 5), kind = (int)((k / 5) % 4), nh, h, i, j, used[NL], hunk_of[NL];
            size_t fn, wn = 0, an;
            HK hk[3];
            int order[3];
            PATCH_PLAN p;
            PATCH_VERIFY_REPORT r;
            int rc;
            g_k = (int)k; g_cls = cls; g_kind = kind;
            fn = build(cls);
            memset(used, 0, sizeof used);
            for (i = 0; i < NL; i++) hunk_of[i] = -1;
            nh = 1 + (int)(rnd() % 3);
            for (h = 0; h < nh;)
            {
                int s = (int)(rnd() % (uint32_t)nlines), e = s + (int)(rnd() % 2);
                int clash = 0;
                if (e >= nlines) e = s;
                for (i = s; i <= e; i++) if (used[i]) clash = 1;
                if (clash) continue;
                for (i = s; i <= e; i++) { used[i] = 1; hunk_of[i] = h; }
                hk[h].s = s; hk[h].e = e;
                /* a region ending in a line with no final ending is replaced by lines with no final ending */
                hk[h].m = (e == nlines - 1 && eol[e] == 0 && s == e) ? 1 : 1 + (int)(rnd() % 3);
                for (j = 0; j < hk[h].m; j++) sprintf(hk[h].rep[j], "R%u.%d.%d %u", (unsigned)k, h, j, (unsigned)(rnd() % 1000));
                hk[h].tgt[0] = 0;
                for (i = s; i <= e; i++) { strcat(hk[h].tgt, line[i]); if (eol[i]) strcat(hk[h].tgt, "\n"); }
                hk[h].rtxt[0] = 0;
                for (j = 0; j < hk[h].m; j++)
                {
                    strcat(hk[h].rtxt, hk[h].rep[j]);
                    if (j < hk[h].m - 1 || eol[e]) strcat(hk[h].rtxt, "\n");
                }
                h++;
            }
            for (h = 0; h < nh; h++) order[h] = h;
            for (h = nh - 1; h > 0; h--) { int t = (int)(rnd() % (uint32_t)(h + 1)), x = order[h]; order[h] = order[t]; order[t] = x; }
            /* model */
            for (i = 0; i < nlines;)
            {
                int hh = hunk_of[i];
                if (hh < 0) { memcpy(want + wn, file + start[i], llen[i]); wn += llen[i]; i++; continue; }
                {
                    int s = hk[hh].s, e = hk[hh].e, ne = 0, kk, ends[2];
                    for (j = s; j <= e; j++) if (eol[j]) ends[ne++] = eol[j];
                    for (j = 0; j < hk[hh].m; j++)
                    {
                        size_t l = strlen(hk[hh].rep[j]);
                        memcpy(want + wn, hk[hh].rep[j], l); wn += l;
                        if (j < hk[hh].m - 1 || eol[e])
                        {
                            kk = j < ne ? j : ne - 1;
                            if (ends[kk] == 2) want[wn++] = '\r';
                            want[wn++] = '\n';
                        }
                    }
                    i = e + 1;
                }
            }
            put(file, fn);
            PatchPlanInit(&p, WS "/a.c");
            PatchPlanSetWorkspace(&p, WS);
            if (kind != 3)
            {
                for (h = 0; h < nh; h++)
                {
                    char t[400], rr[400];
                    snprintf(t, sizeof t, "%s", hk[order[h]].tgt);
                    snprintf(rr, sizeof rr, "%s", hk[order[h]].rtxt);
                    ck(PatchPlanAddHunk(&p, 0, "", t, rr, ""), "add hunk");
                }
                rc = PatchVerifyPlan(&p, &r);
                ck(rc == 1 && r.is_applicable, "verify applicable");
                ck(r.matched_line == (uint32_t)hk[order[0]].s + 1, "matched_line is the first listed hunk's line");
                rc = PatchApplyAtomic(&p);
                ck(rc == 1, "apply");
                an = slurp(after, MAXF);
                ck(an == wn && !memcmp(after, want, wn), "file equals the model");
                ck(p.applied_size == an && !memcmp(p.applied_content, after, an), "applied_content equals the file");
                rc = PatchRollback(&p);
                ck(rc == 1, "rollback");
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, file, fn), "rollback restores the original bytes");
            }
            else
            {
                /* refused plan: valid hunks, then one that cannot apply (absent target or a repeat of the first hunk) */
                char t[400], rr[400];
                for (h = 0; h < nh; h++)
                {
                    snprintf(t, sizeof t, "%s", hk[order[h]].tgt);
                    snprintf(rr, sizeof rr, "%s", hk[order[h]].rtxt);
                    ck(PatchPlanAddHunk(&p, 0, "", t, rr, ""), "add hunk");
                }
                if (k & 1) ck(PatchPlanAddHunk(&p, 0, "", "ZZZ not there\n", "X\n", ""), "add absent");
                else ck(PatchPlanAddHunk(&p, 0, "", hk[order[0]].tgt, "X\n", ""), "add repeat");
                rc = PatchVerifyPlan(&p, &r);
                {
                    char want_h[16];
                    snprintf(want_h, sizeof want_h, "Hunk %d", nh + 1);
                    ck(rc == 0 && !r.is_applicable && strstr(r.diagnostic, want_h) != NULL, "verify refuses and names the hunk");
                    rc = PatchApplyAtomic(&p);
                    ck(rc == 0 && strstr(p.io_diag, want_h) != NULL, "apply refuses and names the hunk");
                }
                an = slurp(after, MAXF);
                ck(an == fn && !memcmp(after, file, fn), "refused plan leaves the file byte-identical");
            }
            ck(names() == 0, "the root holds only the target and the lock file");
            /* coverage tallies, printed at the end */
            st_hunks += (unsigned)nh;
            for (h = 0; h < nh; h++) { if (hk[h].e > hk[h].s) st_multi++; if (hk[h].m > 1) st_grow++; }
            if (nh > 1) st_multihunk++;
            PatchPlanFree(&p);
            total++;
        }
        printf("multi fuzz seed=%u rounds=100\n", (unsigned)seeds[sd]);
        fflush(stdout);
    }
    ck(system(RMWS) == 0, "rm final");
    printf("multi fuzz coverage: hunks=%u two_line_regions=%u multi_line_replacements=%u multi_hunk_plans=%u\n", st_hunks, st_multi, st_grow, st_multihunk);
    printf("multi fuzz done: seeds=4 rounds=400 ok=%u\n", total);
    return total == 400 ? 0 : 1;
}
