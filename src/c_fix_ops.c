/* c_fix_ops.c: see c_fix_ops.h. No task ids or answers live here. */
#include "c_fix_ops.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ci_has(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < n && hay[i] && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n)
            return 1;
    }
    return 0;
}

static char *splice(const char *d, size_t at, size_t cut, const char *ins)
{
    size_t dl = strlen(d), il = strlen(ins);
    char *o = (char *)malloc(dl - cut + il + 1);
    if (!o)
        return NULL;
    memcpy(o, d, at);
    memcpy(o + at, ins, il);
    memcpy(o + at + il, d + at + cut, dl - at - cut + 1);
    return o;
}

static int word_at(const char *d, const char *p, const char *w)
{
    size_t n = strlen(w);
    return !strncmp(p, w, n) && (p == d || !(isalnum((unsigned char)p[-1]) || p[-1] == '_')) &&
           !(isalnum((unsigned char)p[n]) || p[n] == '_');
}

/* for ( init ; cond ; step ): flip the single < / <= in cond */
static char *loop_bound(const char *d, char *detail, size_t dsz)
{
    long at = -1;
    size_t cut = 0;
    const char *ins = NULL;
    int cands = 0;
    for (const char *p = d; *p; p++) {
        if (!word_at(d, p, "for"))
            continue;
        const char *q = p + 3;
        while (*q == ' ' || *q == '\t') q++;
        if (*q != '(') continue;
        const char *s1 = strchr(q, ';');
        const char *s2 = s1 ? strchr(s1 + 1, ';') : NULL;
        const char *nl = strchr(q, '\n');
        if (!s2 || (nl && s2 > nl)) continue;
        int ops = 0;
        long oat = -1;
        size_t ocut = 0;
        const char *oins = NULL;
        for (const char *c = s1 + 1; c < s2; c++) {
            if (c[0] == '<' && c[1] == '=' ) { ops++; oat = c - d; ocut = 2; oins = "<"; c++; }
            else if (c[0] == '<' && c[1] != '<' && (c == s1 + 1 || c[-1] != '<')) { ops++; oat = c - d; ocut = 1; oins = "<="; }
            else if (c[0] == '>' || (c[0] == '!' && c[1] == '=') || (c[0] == '=' && c[1] == '=')) ops += 2;
        }
        if (ops == 1) { cands++; at = oat; cut = ocut; ins = oins; }
    }
    if (cands != 1)
        return NULL;
    snprintf(detail, dsz, "loop condition %.*s -> %s", (int)cut, d + at, ins);
    return splice(d, (size_t)at, cut, ins);
}

/* char NAME[N] = "literal"; with strlen(literal) + 1 > N */
static char *array_fit(const char *d, char *detail, size_t dsz)
{
    long at = -1;
    size_t cut = 0;
    char ins[32];
    int cands = 0;
    for (const char *p = d; *p; p++) {
        if (!word_at(d, p, "char"))
            continue;
        const char *q = p + 4;
        while (*q == ' ') q++;
        while (isalnum((unsigned char)*q) || *q == '_') q++;
        if (*q != '[') continue;
        const char *num = q + 1;
        char *end;
        long n = strtol(num, &end, 10);
        if (end == num || *end != ']') continue;
        const char *r = end + 1;
        while (*r == ' ') r++;
        if (*r != '=') continue;
        r++;
        while (*r == ' ') r++;
        if (*r != '"') continue;
        long len = 0;
        const char *s = r + 1;
        for (; *s && *s != '"' && *s != '\n'; s++) {
            if (*s == '\\') { if (!s[1]) break; s++; if (*s == 'x' || isdigit((unsigned char)*s)) goto next; }
            len++;
        }
        if (*s != '"') continue;
        if (len + 1 > n) {
            cands++;
            at = num - d;
            cut = (size_t)(end - num);
            snprintf(ins, sizeof(ins), "%ld", len + 1);
        }
    next:;
    }
    if (cands != 1)
        return NULL;
    snprintf(detail, dsz, "array size %.*s -> %s", (int)cut, d + at, ins);
    return splice(d, (size_t)at, cut, ins);
}

/* if (C) goto L;  ...  L:\n return X;\n} */
static char *goto_return(const char *d, char *detail, size_t dsz)
{
    const char *g = NULL;
    int ng = 0;
    for (const char *p = d; (p = strstr(p, "goto")) != NULL; p++)
        if (word_at(d, p, "goto")) { g = p; ng++; }
    if (ng != 1)
        return NULL;
    const char *lab = g + 4;
    while (*lab == ' ') lab++;
    size_t ll = 0;
    while (isalnum((unsigned char)lab[ll]) || lab[ll] == '_') ll++;
    if (!ll || lab[ll] != ';' || ll > 60) return NULL;
    char label[64];
    memcpy(label, lab, ll);
    label[ll] = '\0';
    /* label line "L:" then "return X;" then "}" */
    char pat[80];
    snprintf(pat, sizeof(pat), "\n%s:", label);
    const char *L = strstr(d, pat);
    if (!L || strstr(L + 1, pat)) return NULL;
    const char *r = L + strlen(pat);
    while (*r == ' ' || *r == '\t' || *r == '\n') r++;
    if (strncmp(r, "return", 6) != 0) return NULL;
    const char *semi = strchr(r, ';');
    if (!semi) return NULL;
    const char *after = semi + 1;
    while (*after == ' ' || *after == '\t' || *after == '\n') after++;
    if (*after != '}') return NULL;
    /* line of the goto: find "if (...)" before it on the same line */
    const char *ls = g;
    while (ls > d && ls[-1] != '\n') ls--;
    const char *ind = ls;
    while (*ind == ' ' || *ind == '\t') ind++;
    if (strncmp(ind, "if", 2) != 0) return NULL;
    if (g > L) return NULL;   /* forward jump only */
    char ret[160];
    size_t rl = (size_t)(semi - r) + 1;
    if (rl >= sizeof(ret)) return NULL;
    memcpy(ret, r, rl);
    ret[rl] = '\0';
    /* build: head up to goto, "{ ret }" , rest of line after "L;", up to label, then from after the return */
    size_t indent = (size_t)(ind - ls);
    char block[400];
    snprintf(block, sizeof(block), "{\n%*s    %s\n%*s}", (int)indent, "", ret, (int)indent, "");
    char *a = splice(d, (size_t)(g - d), ll + (size_t)(lab - g) + 1, block);
    if (!a) return NULL;
    /* remove the label and its return (they are now after the edit point) */
    char *L2 = strstr(a, pat);
    const char *r2 = L2 ? strstr(L2, ret) : NULL;
    if (!L2 || !r2) { free(a); return NULL; }
    const char *e2 = r2 + strlen(ret);
    if (*e2 == '\n') e2++;
    char *o = splice(a, (size_t)(L2 - a) + 1, (size_t)(e2 - (L2 + 1)), "");
    free(a);
    snprintf(detail, dsz, "goto %s -> %s", label, ret);
    return o;
}

/* skip a comment or string/char literal starting at p; returns p if none */
static const char *skip_lit(const char *p)
{
    if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; return p; }
    if (p[0] == '/' && p[1] == '*') { const char *e = strstr(p + 2, "*/"); return e ? e + 2 : p + strlen(p); }
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        while (*p && *p != q && *p != '\n') { if (*p == '\\' && p[1]) p++; p++; }
        return *p == q ? p + 1 : p;
    }
    return p;
}

static int is_ident_start(int c) { return isalpha(c) || c == '_'; }
static int is_ident(int c) { return isalnum(c) || c == '_'; }

/* declare_local: the task names one identifier that is used but never
   declared; it gets "int X = 0;" at the top of the one function using it */
static char *declare_local(const char *d, const char *task, char *detail, size_t dsz)
{
    static const char *const nodecl[] = {"return", "sizeof", "case", "else", "do", "goto", NULL};
    char cand[64] = {0};
    int ncand = 0;
    for (const char *t = task; *t; t++) {
        if (!is_ident_start((unsigned char)*t) || (t > task && is_ident((unsigned char)t[-1])))
            continue;
        size_t l = 0;
        while (is_ident((unsigned char)t[l])) l++;
        char w[64];
        if (l < 3 || l >= sizeof(w)) { t += l - 1; continue; }
        memcpy(w, t, l); w[l] = '\0';
        t += l - 1;
        if (!strpbrk(w, "_0123456789") || !strcmp(w, cand))
            continue;
        /* used in the source, never declared, never called, never #defined */
        int uses = 0, decl = 0;
        for (const char *p = d; *p;) {
            const char *q = skip_lit(p);
            if (q != p) { p = q; continue; }
            if (!word_at(d, p, w)) { p++; continue; }
            uses++;
            const char *a = p + strlen(w);
            while (*a == ' ' || *a == '\t') a++;
            if (*a == '(') decl = 1;
            const char *b = p;
            while (b > d && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '*')) b--;
            if (b > d && is_ident((unsigned char)b[-1])) {
                const char *e = b;
                while (b > d && is_ident((unsigned char)b[-1])) b--;
                char prev[32];
                size_t pl = (size_t)(e - b);
                if (pl >= sizeof(prev)) decl = 1;
                else {
                    memcpy(prev, b, pl); prev[pl] = '\0';
                    int kw = 0;
                    for (int k = 0; nodecl[k]; k++) if (!strcmp(prev, nodecl[k])) kw = 1;
                    if (!kw) decl = 1;
                }
            }
            p += strlen(w);
        }
        if (uses > 0 && !decl) {
            if (ncand == 0) snprintf(cand, sizeof(cand), "%s", w);
            ncand++;
        }
    }
    if (ncand != 1)
        return NULL;
    /* every use inside the same top-level function body */
    int depth = 0;
    const char *open = NULL, *fn = NULL;
    for (const char *p = d; *p;) {
        const char *q = skip_lit(p);
        if (q != p) { p = q; continue; }
        if (*p == '{') { if (depth++ == 0) open = p; }
        else if (*p == '}') { if (depth > 0) depth--; }
        else if (word_at(d, p, cand)) {
            if (depth == 0 || (fn && fn != open)) return NULL;
            fn = open;
        }
        p++;
    }
    if (!fn)
        return NULL;
    const char *nl = strchr(fn, '\n');
    if (!nl)
        return NULL;
    const char *ind = nl + 1;
    size_t il = 0;
    while (ind[il] == ' ' || ind[il] == '\t') il++;
    char ins[128];
    if (il == 0 || il > 16 || ind[il] == '}')
        snprintf(ins, sizeof(ins), "    int %s = 0;\n", cand);
    else
        snprintf(ins, sizeof(ins), "%.*sint %s = 0;\n", (int)il, ind, cand);
    snprintf(detail, dsz, "int %s = 0", cand);
    return splice(d, (size_t)(nl + 1 - d), 0, ins);
}

/* the single-quoted phrase of the task (two or more words); 0 if none/several */
static int quoted_phrase(const char *task, char *q, size_t qsz, const char **qs, const char **qe)
{
    const char *a = strchr(task, '\''), *b = a ? strchr(a + 1, '\'') : NULL;
    if (!b || strchr(b + 1, '\'') || (size_t)(b - a - 1) >= qsz || b - a - 1 < 3)
        return 0;
    memcpy(q, a + 1, (size_t)(b - a - 1));
    q[b - a - 1] = '\0';
    if (!strchr(q, ' '))
        return 0;
    *qs = a; *qe = b + 1;
    return 1;
}

#define CF_MAXW 96
/* comment_fix: the task quotes the new wording 'Q' and repeats the stale
   wording W (three or more words) that appears in exactly one comment and
   nowhere in code; W becomes Q in that comment */
static char *comment_fix(const char *d, const char *task, char *detail, size_t dsz)
{
    char q[256];
    const char *qs, *qe;
    if (!quoted_phrase(task, q, sizeof(q), &qs, &qe) || strstr(d, q))
        return NULL;
    const char *ws[CF_MAXW], *we[CF_MAXW];
    int n = 0;
    for (const char *t = task; *t && n < CF_MAXW;) {
        while (*t && isspace((unsigned char)*t)) t++;
        if (!*t) break;
        const char *s = t;
        while (*t && !isspace((unsigned char)*t)) t++;
        if (s >= qs && s < qe) continue;
        ws[n] = s; we[n] = t; n++;
    }
    for (int len = n; len >= 3; len--) {
        char best[256] = {0};
        const char *cs = NULL, *ce = NULL;
        int hits = 0;
        for (int i = 0; i + len <= n; i++) {
            int split = 0;
            for (int k = i; k + 1 < i + len; k++)
                if ((ws[k] < qs) != (ws[k + 1] < qs)) split = 1;   /* window spans the quote */
            if (split) continue;
            size_t l = (size_t)(we[i + len - 1] - ws[i]);
            while (l && strchr(".,;:!?", ws[i][l - 1])) l--;
            char w[256];
            if (l == 0 || l >= sizeof(w)) continue;
            memcpy(w, ws[i], l); w[l] = '\0';
            if (!strcmp(w, best)) continue;
            /* count occurrences in comments vs code */
            int inc = 0, outc = 0;
            const char *c0 = NULL, *c1 = NULL;
            for (const char *p = d; *p;) {
                const char *e = skip_lit(p);
                if (e != p) {
                    int com = p[0] == '/';
                    for (const char *f = p; f && f < e; ) {
                        const char *h = strstr(f, w);
                        if (!h || h + l > e) break;
                        if (com) { inc++; c0 = p; c1 = e; } else outc++;
                        f = h + 1;
                    }
                    p = e;
                    continue;
                }
                if (!strncmp(p, w, l)) outc++;
                p++;
            }
            if (inc == 1 && outc == 0) {
                hits++;
                snprintf(best, sizeof(best), "%s", w);
                cs = c0; ce = c1;
            } else if (inc > 0) {
                return NULL;   /* stale wording in several places: abstain */
            }
        }
        if (hits > 1) return NULL;
        if (hits == 1) {
            const char *h = strstr(cs, best);
            if (!h || h >= ce) return NULL;
            snprintf(detail, dsz, "'%.50s' -> '%.50s'", best, q);
            return splice(d, (size_t)(h - d), strlen(best), q);
        }
    }
    return NULL;
}

static int ends_with(const char *s, size_t l, const char *suf)
{
    size_t n = strlen(suf);
    return l > n && !strncmp(s + l - n, suf, n);
}

int CFixSplit(const char *src, const char *src_rel, const char *task, CFIX_SPLIT *out)
{
    memset(out, 0, sizeof(*out));
    if (!(ci_has(task, "split") || ci_has(task, "move") || ci_has(task, "out of")) || !ci_has(task, "prototype"))
        return 0;
    /* S.c (not the source itself) and S.h named in the task; F() named */
    char stem[64] = {0}, fn[64] = {0};
    int nstem = 0, nfn = 0, hdr = 0;
    for (const char *t = task; *t;) {
        while (*t && (isspace((unsigned char)*t) || strchr("'\"`(,;", *t))) t++;
        const char *s = t;
        while (*t && !isspace((unsigned char)*t)) t++;
        size_t l = (size_t)(t - s);
        while (l && strchr(".,;:'\"`", s[l - 1]) && !(l >= 2 && s[l - 2] == '.' )) l--;
        while (l && strchr(",;:'\"`", s[l - 1])) l--;
        if (l == 0 || l >= 64) continue;
        char w[64];
        memcpy(w, s, l); w[l] = '\0';
        if (ends_with(w, l, ".c") && strcmp(w, src_rel) && !strchr(w, '/')) {
            w[l - 2] = '\0';
            if (nstem == 0 || strcmp(stem, w)) { snprintf(stem, sizeof(stem), "%s", w); nstem++; }
        } else if (ends_with(w, l, "()")) {
            w[l - 2] = '\0';
            int ok = w[0] && is_ident_start((unsigned char)w[0]);
            for (char *c = w; *c; c++) if (!is_ident((unsigned char)*c)) ok = 0;
            if (ok && (nfn == 0 || strcmp(fn, w))) { snprintf(fn, sizeof(fn), "%s", w); nfn++; }
        }
    }
    if (nstem != 1 || nfn != 1 || !stem[0])
        return 0;
    for (size_t i = 0; stem[i]; i++) if (!is_ident((unsigned char)stem[i])) return 0;
    snprintf(out->h_rel, sizeof(out->h_rel), "%s.h", stem);
    snprintf(out->c_rel, sizeof(out->c_rel), "%s.c", stem);
    for (const char *t = strstr(task, out->h_rel); t; t = strstr(t + 1, out->h_rel)) hdr = 1;
    if (!hdr)
        return 0;
    /* the one top-level definition "... F(...) {" */
    const char *def = NULL, *brace = NULL;
    int depth = 0, defs = 0;
    for (const char *p = src; *p;) {
        const char *q = skip_lit(p);
        if (q != p) { p = q; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') { if (depth > 0) depth--; }
        else if (depth == 0 && word_at(src, p, fn)) {
            const char *a = p + strlen(fn);
            while (*a == ' ' || *a == '\t') a++;
            if (*a == '(') {
                int pd = 0;
                const char *r = a;
                for (; *r; r++) { if (*r == '(') pd++; else if (*r == ')' && --pd == 0) break; }
                if (*r) {
                    r++;
                    while (isspace((unsigned char)*r)) r++;
                    if (*r == '{') { defs++; brace = r; def = p; }
                }
            }
        }
        p++;
    }
    if (defs != 1)
        return 0;
    const char *ls = def;
    while (ls > src && ls[-1] != '\n') ls--;
    depth = 0;
    const char *end = brace;
    for (const char *p = brace; *p;) {
        const char *q = skip_lit(p);
        if (q != p) { p = q; continue; }
        if (*p == '{') depth++;
        else if (*p == '}' && --depth == 0) { end = p + 1; break; }
        p++;
    }
    if (depth != 0)
        return 0;
    if (*end == '\n') end++;
    const char *hs = ls;
    int was_static = !strncmp(hs, "static ", 7);
    if (was_static) hs += 7;
    size_t hl = (size_t)(brace - hs);
    while (hl && isspace((unsigned char)hs[hl - 1])) hl--;
    size_t bl = (size_t)(end - hs);
    char guard[80];
    size_t g = 0;
    for (; stem[g] && g < 60; g++) guard[g] = (char)toupper((unsigned char)stem[g]);
    snprintf(guard + g, sizeof(guard) - g, "_H");
    size_t hcap = hl + 2 * strlen(guard) + 64, ccap = bl + strlen(out->h_rel) + 32;
    out->h_text = (char *)malloc(hcap);
    out->c_text = (char *)malloc(ccap);
    if (!out->h_text || !out->c_text) { CFixSplitFree(out); return 0; }
    snprintf(out->h_text, hcap, "#ifndef %s\n#define %s\n\n%.*s;\n\n#endif\n", guard, guard, (int)hl, hs);
    snprintf(out->c_text, ccap, "#include \"%s\"\n\n%.*s", out->h_rel, (int)bl, hs);
    if (out->c_text[strlen(out->c_text) - 1] != '\n') strcat(out->c_text, "\n");
    /* source: drop the definition, include the header after the last #include */
    const char *cut_end = end;
    if (*cut_end == '\n' && (ls == src || ls[-1] == '\n')) cut_end++;   /* one blank line after it */
    char *tmp = splice(src, (size_t)(ls - src), (size_t)(cut_end - ls), "");
    if (!tmp) { CFixSplitFree(out); return 0; }
    size_t at = 0;
    for (const char *p = tmp; (p = strstr(p, "#include")); p++)
        if (p == tmp || p[-1] == '\n') { const char *e = strchr(p, '\n'); at = e ? (size_t)(e + 1 - tmp) : strlen(tmp); }
    char inc[96];
    snprintf(inc, sizeof(inc), at ? "#include \"%s\"\n" : "#include \"%s\"\n\n", out->h_rel);
    if (!at && tmp[0] == '\n') snprintf(inc, sizeof(inc), "#include \"%s\"\n", out->h_rel);
    out->new_src = splice(tmp, at, 0, inc);
    free(tmp);
    if (!out->new_src) { CFixSplitFree(out); return 0; }
    snprintf(out->detail, sizeof(out->detail), "%.40s() -> %.40s + %.40s%s", fn, out->c_rel, out->h_rel, was_static ? " (static dropped)" : "");
    return 1;
}

void CFixSplitFree(CFIX_SPLIT *s)
{
    free(s->new_src); free(s->c_text); free(s->h_text);
    s->new_src = s->c_text = s->h_text = NULL;
}

/* ------------------------------------------------ evidence candidates
   Every single edit of the three kinds below, for a caller that tests each
   one against the program's own exit code (no task wording involved). */

static int cand_add(CFIX_CAND *c, int n, int max, const char *d, size_t at, size_t cut, const char *ins,
                    int tier, const char *rule, const char *detail)
{
    if (n >= max)
        return n;
    char *t = splice(d, at, cut, ins);
    if (!t)
        return n;
    c[n].text = t;
    c[n].tier = tier;
    snprintf(c[n].rule, sizeof(c[n].rule), "%s", rule);
    snprintf(c[n].detail, sizeof(c[n].detail), "%s", detail);
    return n + 1;
}

/* the line number of offset p, for details */
static int line_of(const char *d, const char *p)
{
    int l = 1;
    for (const char *q = d; q < p; q++) if (*q == '\n') l++;
    return l;
}

int CFixCandidates(const char *d, CFIX_CAND *c, int max)
{
    int n = 0, depth = 0, paren = 0, in_cond = 0;
    for (const char *p = d; *p && n < max;) {
        const char *q = skip_lit(p);
        if (q != p) { p = q; continue; }
        if (*p == '#') {   /* preprocessor line: #include <x.h> is not a comparison */
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p == '{') depth++;
        else if (*p == '}') { if (depth > 0) depth--; }
        if (depth > 0 && (word_at(d, p, "if") || word_at(d, p, "while") || word_at(d, p, "for"))) {
            const char *a = p;
            while (is_ident((unsigned char)*a)) a++;
            while (*a == ' ' || *a == '\t') a++;
            if (*a == '(') { in_cond = 1; paren = 0; p = a; continue; }
        }
        if (in_cond) {
            if (*p == '(') paren++;
            else if (*p == ')' && --paren == 0) in_cond = 0;
        }
        int in_ret = 0;
        if (depth > 0 && !in_cond) {   /* inside "return ...;" */
            const char *b = p;
            while (b > d && b[-1] != ';' && b[-1] != '{' && b[-1] != '}') b--;
            while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r') b++;
            in_ret = word_at(d, b, "return");
        }
        if ((in_cond || in_ret) && (*p == '<' || *p == '>') && p[1] != *p && p > d && p[-1] != *p &&
            p[-1] != '-' && !(p[1] == '=' && p[2] == '=')) {
            char ins[4], det[96];
            size_t cut = p[1] == '=' ? 2 : 1;
            snprintf(ins, sizeof(ins), "%c%s", *p, cut == 2 ? "" : "=");
            snprintf(det, sizeof(det), "line %d: '%.*s' -> '%s'", line_of(d, p), (int)cut, p, ins);
            n = cand_add(c, n, max, d, (size_t)(p - d), cut, ins, 1, "boundary", det);
            char ins2[4];   /* tier 3: the comparison points the wrong way */
            snprintf(ins2, sizeof(ins2), "%c%s", *p == '<' ? '>' : '<', cut == 2 ? "=" : "");
            snprintf(det, sizeof(det), "line %d: '%.*s' -> '%s'", line_of(d, p), (int)cut, p, ins2);
            n = cand_add(c, n, max, d, (size_t)(p - d), cut, ins2, 3, "direction", det);
            p += cut;
            continue;
        }
        if ((in_cond || in_ret) && (p[0] == '=' || p[0] == '!') && p[1] == '=' && p > d &&
            !strchr("=!<>", p[-1]) && p[2] != '=') {
            char det[96];
            const char *ins = p[0] == '=' ? "!=" : "==";
            snprintf(det, sizeof(det), "line %d: '%.2s' -> '%s'", line_of(d, p), p, ins);
            n = cand_add(c, n, max, d, (size_t)(p - d), 2, ins, 3, "equality", det);
            p += 2;
            continue;
        }
        /* char NAME[N] = "literal" that does not fit */
        if (depth >= 0 && word_at(d, p, "char")) {
            const char *a = p + 4;
            while (*a == ' ' || *a == '\t') a++;
            const char *nm = a;
            while (is_ident((unsigned char)*a)) a++;
            if (a > nm && *a == '[' && isdigit((unsigned char)a[1])) {
                const char *num = a + 1, *e = num;
                while (isdigit((unsigned char)*e)) e++;
                const char *r = e;
                if (*r == ']') {
                    r++;
                    while (*r == ' ' || *r == '\t') r++;
                    if (*r == '=') {
                        r++;
                        while (*r == ' ' || *r == '\t') r++;
                        if (*r == '"') {
                            const char *s0 = r + 1, *s1 = s0;
                            while (*s1 && *s1 != '"' && *s1 != '\\' && *s1 != '\n') s1++;
                            long size = strtol(num, NULL, 10), len = (long)(s1 - s0);
                            if (*s1 == '"' && len >= size) {
                                char ins[24], det[96];
                                snprintf(ins, sizeof(ins), "%ld", len + 1);
                                snprintf(det, sizeof(det), "line %d: %.*s[%ld] -> [%s]", line_of(d, p), (int)(a - nm), nm, size, ins);
                                n = cand_add(c, n, max, d, (size_t)(num - d), (size_t)(e - num), ins, 2, "array_fit", det);
                            }
                        }
                    }
                }
            }
        }
        /* TYPE NAME; inside a function, later updated before any plain assignment */
        if (depth > 0) {
            static const char *const ty[] = {"int", "long", "unsigned", "double", "float", "size_t", NULL};
            for (int k = 0; ty[k]; k++) {
                if (!word_at(d, p, ty[k])) continue;
                const char *a = p + strlen(ty[k]);
                while (*a == ' ' || *a == '\t') a++;
                const char *nm = a;
                while (is_ident((unsigned char)*a)) a++;
                size_t nl = (size_t)(a - nm);
                const char *semi = a;
                while (*semi == ' ' || *semi == '\t') semi++;
                if (nl == 0 || nl >= 48 || *semi != ';') break;
                char name[48];
                memcpy(name, nm, nl); name[nl] = '\0';
                const char *u = semi + 1, *first = NULL;
                for (; *u; u++) {
                    const char *z = skip_lit(u);
                    if (z != u) { u = z - 1; continue; }
                    if (word_at(d, u, name)) { first = u; break; }
                }
                if (!first) break;
                const char *after = first + nl;
                while (*after == ' ' || *after == '\t') after++;
                int updated = (after[0] == '+' && after[1] == '+') || (after[0] == '-' && after[1] == '-') ||
                              (strchr("+-*/", after[0]) && after[1] == '=') ||
                              (first >= d + 2 && ((first[-1] == '+' && first[-2] == '+') || (first[-1] == '-' && first[-2] == '-')));
                if (!updated) break;
                char ins[16], det[96];
                snprintf(ins, sizeof(ins), " = %s", (after[0] == '*' || after[0] == '/') ? "1" : "0");
                snprintf(det, sizeof(det), "line %d: %s%s", line_of(d, p), name, ins);
                n = cand_add(c, n, max, d, (size_t)(a - d), 0, ins, 2, "init_local", det);
                break;
            }
        }
        p++;
    }
    return n;
}

void CFixCandidatesFree(CFIX_CAND *c, int n)
{
    for (int i = 0; i < n; i++) { free(c[i].text); c[i].text = NULL; }
}

char *CFixApply(const char *src, const char *task, char *rule, size_t rule_size, char *detail, size_t detail_size)
{
    rule[0] = detail[0] = '\0';
    if (ci_has(task, "loop") && (ci_has(task, "off-by-one") || ci_has(task, "bound"))) {
        char *o = loop_bound(src, detail, detail_size);
        if (o) { snprintf(rule, rule_size, "loop_bound"); return o; }
    }
    if ((ci_has(task, "buffer") || ci_has(task, "array")) && (ci_has(task, "too small") || ci_has(task, "size"))) {
        char *o = array_fit(src, detail, detail_size);
        if (o) { snprintf(rule, rule_size, "array_fit"); return o; }
    }
    if (ci_has(task, "goto") && (ci_has(task, "no goto") || ci_has(task, "replace") || ci_has(task, "remove"))) {
        char *o = goto_return(src, detail, detail_size);
        if (o) { snprintf(rule, rule_size, "goto_return"); return o; }
    }
    if (ci_has(task, "declared") || (ci_has(task, "declare") && ci_has(task, "variable")) || ci_has(task, "introduce a local")) {
        char *o = declare_local(src, task, detail, detail_size);
        if (o) { snprintf(rule, rule_size, "declare_local"); return o; }
    }
    if (ci_has(task, "comment")) {
        char *o = comment_fix(src, task, detail, detail_size);
        if (o) { snprintf(rule, rule_size, "comment_fix"); return o; }
    }
    return NULL;
}
