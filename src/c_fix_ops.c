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
    return NULL;
}
