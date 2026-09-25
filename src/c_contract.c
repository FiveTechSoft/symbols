/* c_contract.c: see c_contract.h. No task ids, file names or answers live here. */
#include "c_contract.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ci_find_n(const char *s, size_t n, const char *needle)
{
    size_t k = strlen(needle);
    for (size_t i = 0; i + k <= n; i++) {
        size_t j = 0;
        while (j < k && tolower((unsigned char)s[i + j]) == tolower((unsigned char)needle[j])) j++;
        if (j == k) return s + i;
    }
    return NULL;
}

/* whole-word, case-insensitive */
static const char *ci_word(const char *s, size_t n, const char *w)
{
    size_t k = strlen(w);
    for (const char *p = s; (p = ci_find_n(p, n - (size_t)(p - s), w)) != NULL; p++) {
        int lb = p == s || !isalnum((unsigned char)p[-1]);
        int rb = (size_t)(p - s) + k >= n || !isalpha((unsigned char)p[k]);
        if (lb && rb) return p;
        if ((size_t)(p - s) + 1 >= n) break;
    }
    return NULL;
}

static int ci_eq_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}

static int is_num(const char *s, size_t n)
{
    size_t i = 0, d = 0, dots = 0;
    if (i < n && s[i] == '-') i++;
    for (; i < n; i++) {
        if (isdigit((unsigned char)s[i])) d++;
        else if (s[i] == '.' && d && i + 1 < n && dots == 0) dots++;
        else return 0;
    }
    return d > 0;
}

static int set_out(C_CONTRACT *c, const char *v, size_t n)
{
    if (n == 0 || n >= sizeof(c->out) || memchr(v, '\n', n)) return 0;
    if (c->has_out && (strlen(c->out) != n || strncmp(c->out, v, n))) return -1;
    memcpy(c->out, v, n);
    c->out[n] = '\0';
    c->has_out = 1;
    return 1;
}

/* the value stated after a print/output key inside one clause */
static int clause_value(C_CONTRACT *c, const char *cl, size_t o, const char *key)
{
    const char *p = key;
    while (p < cl + o && isalpha((unsigned char)*p)) p++;   /* print / prints / printed / output */
    const char *end = cl + o;
    /* quoted value */
    for (const char *q = p; q < end; q++) {
        if (*q == '`' || *q == '"') {
            const char *e = memchr(q + 1, *q, (size_t)(end - q - 1));
            if (!e) break;
            return set_out(c, q + 1, (size_t)(e - q - 1));
        }
    }
    /* next token, after filler */
    static const char *fill[] = {"as", "out", "exactly", "is", "should", "be", "must", ":", "the number", "the value"};
    for (int again = 1; again; ) {
        again = 0;
        while (p < end && (*p == ' ' || *p == ':')) p++;
        for (size_t f = 0; f < sizeof(fill) / sizeof(fill[0]); f++) {
            size_t fl = strlen(fill[f]);
            if ((size_t)(end - p) > fl && ci_eq_n(p, fill[f], fl) && p[fl] == ' ') { p += fl; again = 1; break; }
        }
    }
    const char *t = p;
    while (p < end && !isspace((unsigned char)*p) && *p != ',') p++;
    size_t tl = (size_t)(p - t);
    const char *rest = p;
    while (rest < end && isspace((unsigned char)*rest)) rest++;
    if (tl && is_num(t, tl)) return set_out(c, t, tl);
    if (tl && rest == end) {
        static const char *stop[] = {"it", "them", "this", "that", "something", "nothing", "correctly", "anything", "out", "again", "properly", "right", "wrong"};
        int ok = 1;
        for (size_t i = 0; i < tl; i++)
            if (!isalnum((unsigned char)t[i]) && t[i] != '_' && t[i] != '=' && t[i] != '-') ok = 0;
        for (size_t s = 0; s < sizeof(stop) / sizeof(stop[0]) && ok; s++)
            if (strlen(stop[s]) == tl && ci_eq_n(t, stop[s], tl)) ok = 0;
        if (ok) return set_out(c, t, tl);
    }
    /* "..., which is N" / "should be N" */
    const char *keys2[] = {" is ", " be ", " = "};
    for (int k = 0; k < 3; k++) {
        const char *e = ci_find_n(key, (size_t)(end - key), keys2[k]);
        if (!e) continue;
        e += strlen(keys2[k]);
        const char *te = e;
        while (te < end && !isspace((unsigned char)*te) && *te != ',') te++;
        if (te > e && is_num(e, (size_t)(te - e))) return set_out(c, e, (size_t)(te - e));
    }
    return 0;
}

int CContractParse(const char *task, C_CONTRACT *c)
{
    memset(c, 0, sizeof(*c));
    const char *s = task;
    while (*s) {
        size_t n = 0;
        int inq = 0;
        while (s[n]) {   /* clause ends at . ; ! ? followed by space or end, outside quotes */
            if (s[n] == '`' || s[n] == '"') inq = !inq;
            if (!inq && strchr(".;!?\n", s[n]) && (s[n + 1] == '\0' || isspace((unsigned char)s[n + 1]))) break;
            n++;
        }
        char cl[512];
        size_t o = 0;
        int par = 0;
        for (size_t i = 0; i < n && o + 1 < sizeof(cl); i++) {   /* drop (...) */
            if (s[i] == '(') par++;
            else if (s[i] == ')' && par) par--;
            else if (!par) cl[o++] = s[i];
        }
        cl[o] = '\0';
        const char *but = ci_find_n(cl, o, ", but");
        if (!but) but = ci_find_n(cl, o, " but it");
        if (!but) but = ci_find_n(cl, o, " instead of");
        if (but) { o = (size_t)(but - cl); cl[o] = '\0'; }
        static const char *now[] = {"currently", "right now", "at the moment", "instead", "fails with", "is printing", "it prints", "prints out"};
        static const char *want[] = {"should", "must", "expected", "expect", "needs to", "need to", "has to", "have to", "so that", "want", "required", "supposed to"};
        int is_now = 0, is_want = 0;
        for (size_t k = 0; k < sizeof(now) / sizeof(now[0]); k++) is_now |= ci_find_n(cl, o, now[k]) != NULL;
        for (size_t k = 0; k < sizeof(want) / sizeof(want[0]); k++) is_want |= ci_word(cl, o, want[k]) != NULL;
        if (is_want && !is_now) {
            /* exit: only 0 is supported */
            static const char *ek[] = {"exit code ", "exit status ", "status ", "exit with ", "exit "};
            for (size_t k = 0; k < 5; k++) {
                const char *e = ci_find_n(cl, o, ek[k]);
                if (!e) continue;
                e += strlen(ek[k]);
                if (!strncmp(e, "status ", 7)) e += 7;
                if (isdigit((unsigned char)*e) && atoi(e) != 0) return 0;
                break;
            }
            if (ci_find_n(cl, o, "non-zero") || ci_find_n(cl, o, "nonzero")) return 0;
            static const char *keys[] = {"print", "output", "stdout"};
            if (!ci_find_n(cl, o, "file")) {   /* a file's content is not stdout */
                int got = 0;
                for (size_t k = 0; k < 3 && !got; k++)
                    for (const char *key = cl; (key = ci_find_n(key, o - (size_t)(key - cl), keys[k])) != NULL; key++) {
                        if (key > cl && isalpha((unsigned char)key[-1]))
                            continue;
                        int r = clause_value(c, cl, o, key);
                        if (r < 0) return 0;   /* two different wanted outputs */
                        got |= r > 0;
                    }
            }
        }
        s += n;
        if (*s) s++;
    }
    return c->has_out;
}

/* ------------------------------------------------------------ candidates */

static char *splice(const char *s, size_t at, size_t cut, const char *ins)
{
    size_t sl = strlen(s), il = strlen(ins);
    char *o = (char *)malloc(sl - cut + il + 1);
    if (!o) return NULL;
    memcpy(o, s, at);
    memcpy(o + at, ins, il);
    memcpy(o + at + il, s + at + cut, sl - at - cut + 1);
    return o;
}

static int is_idc(int ch) { return isalnum(ch) || ch == '_'; }

/* occurrences of v as a standalone token / string content in code */
static int count_lit(const char *s, const char *v)
{
    int n = 0;
    size_t k = strlen(v);
    if (k == 0) return 0;
    for (const char *p = s; (p = strstr(p, v)) != NULL; p++) {
        int lb = p == s || !is_idc((unsigned char)p[-1]);
        int rb = !is_idc((unsigned char)p[k]);
        if (lb && rb) n++;
    }
    return n;
}

typedef struct {
    const char *src;
    const C_CONTRACT *c;
    C_CAND *out;
    int n, max, base_lits;
} GEN;

static int line_of(const char *s, size_t at)
{
    int l = 1;
    for (size_t i = 0; i < at; i++) l += s[i] == '\n';
    return l;
}

static void push(GEN *g, char *text, int tier, const char *rule, size_t at, const char *from, const char *to)
{
    if (!text) return;
    if (!strcmp(text, g->src) || (g->c && g->c->has_out && count_lit(text, g->c->out) > g->base_lits)) {
        free(text);
        return;
    }
    for (int i = 0; i < g->n; i++)
        if (!strcmp(g->out[i].text, text)) {
            if (tier < g->out[i].tier) {
                g->out[i].tier = tier;
                snprintf(g->out[i].rule, sizeof(g->out[i].rule), "%s", rule);
            }
            free(text);
            return;
        }
    if (g->n >= g->max) { free(text); return; }
    C_CAND *k = &g->out[g->n++];
    k->text = text;
    k->tier = tier;
    snprintf(k->rule, sizeof(k->rule), "%s", rule);
    snprintf(k->detail, sizeof(k->detail), "line %d: %.40s -> %.40s", line_of(g->src, at), from, to);
}

/* mask: 0 code, 1 comment/preprocessor, 2 string or char literal content */
static char *make_mask(const char *s, size_t n)
{
    char *m = (char *)calloc(n + 1, 1);
    if (!m) return NULL;
    size_t i = 0;
    int bol = 1;
    while (i < n) {
        if (bol) {
            size_t j = i;
            while (j < n && (s[j] == ' ' || s[j] == '\t')) j++;
            if (j < n && s[j] == '#') {
                while (j < n && s[j] != '\n') { if (s[j] == '\\' && s[j + 1] == '\n') j++; j++; }
                memset(m + i, 1, j - i);
                i = j;
                continue;
            }
        }
        bol = s[i] == '\n';
        if (s[i] == '/' && s[i + 1] == '/') {
            size_t j = i;
            while (j < n && s[j] != '\n') j++;
            memset(m + i, 1, j - i);
            i = j;
        } else if (s[i] == '/' && s[i + 1] == '*') {
            const char *e = strstr(s + i + 2, "*/");
            size_t j = e ? (size_t)(e - s) + 2 : n;
            memset(m + i, 1, j - i);
            i = j;
        } else if (s[i] == '"' || s[i] == '\'') {
            char q = s[i];
            size_t j = i + 1;
            while (j < n && s[j] != q && s[j] != '\n') { if (s[j] == '\\') j++; j++; }
            if (j > i + 1) memset(m + i + 1, 2, j - i - 1);
            m[i] = 1;
            if (j < n) m[j] = 1;
            i = j + 1;
        } else
            i++;
    }
    return m;
}

typedef struct { size_t lo, hi; int is_main; int is_float; } FN;

static int find_functions(const char *s, const char *m, size_t n, FN *fn, int max)
{
    int k = 0, depth = 0;
    for (size_t i = 0; i < n; i++) {
        if (m[i]) continue;
        if (s[i] == '{') {
            if (depth == 0) {
                size_t j = i;
                while (j > 0 && (isspace((unsigned char)s[j - 1]) || m[j - 1] == 1)) j--;
                if (j > 0 && s[j - 1] == ')' && k < max) {
                    int pd = 0;
                    size_t p = j - 1;
                    for (;; p--) {   /* matching ( */
                        if (!m[p] && s[p] == ')') pd++;
                        if (!m[p] && s[p] == '(' && --pd == 0) break;
                        if (p == 0) break;
                    }
                    size_t e = p;
                    while (e > 0 && isspace((unsigned char)s[e - 1])) e--;
                    size_t b = e;
                    while (b > 0 && is_idc((unsigned char)s[b - 1])) b--;
                    size_t ls = b;
                    while (ls > 0 && s[ls - 1] != '\n' && s[ls - 1] != ';' && s[ls - 1] != '}') ls--;
                    fn[k].lo = i + 1;
                    fn[k].hi = n;
                    fn[k].is_main = e - b == 4 && !strncmp(s + b, "main", 4);
                    fn[k].is_float = ci_find_n(s + ls, b - ls, "double") != NULL || ci_find_n(s + ls, b - ls, "float") != NULL;
                    k++;
                    depth = 1;
                    continue;
                }
            }
            depth++;
        } else if (s[i] == '}') {
            if (depth > 0 && --depth == 0 && k > 0 && fn[k - 1].hi == n) fn[k - 1].hi = i;
        }
    }
    return k;
}

static void gen_fn(GEN *g, const char *s, const char *m, const FN *f, int code_ok)
{
    char from[48], to[48];
    for (size_t i = f->lo; i < f->hi; i++) {
        /* format widths in string literals */
        if (m[i] == 2 && s[i] == '%') {
            static const char *w[] = {"%d", "%ld", "%lld", "%i"};
            for (int a = 0; a < 4; a++) {
                size_t al = strlen(w[a]);
                if (strncmp(s + i, w[a], al)) continue;
                if (a == 1 && !strncmp(s + i, "%lld", 4)) continue;
                for (int b = 0; b < 3; b++) {
                    if (b == a || (a == 3 && b == 0)) continue;
                    push(g, splice(g->src, i, al, w[b]), 3, "format", i, w[a], w[b]);
                }
            }
            continue;
        }
        if (m[i] || !code_ok) continue;
        char ch = s[i], nx = s[i + 1], pv = i > 0 ? s[i - 1] : ' ';
        if ((ch == '<' || ch == '>') && nx != ch && pv != ch && !(ch == '>' && pv == '-')) {
            int eq = nx == '=';
            size_t cut = eq ? 2 : 1;
            snprintf(from, sizeof(from), "%c%s", ch, eq ? "=" : "");
            snprintf(to, sizeof(to), "%c%s", ch, eq ? "" : "=");
            push(g, splice(g->src, i, cut, to), 1, "boundary", i, from, to);
            snprintf(to, sizeof(to), "%c%s", ch == '<' ? '>' : '<', eq ? "=" : "");
            push(g, splice(g->src, i, cut, to), 3, "direction", i, from, to);
            if (eq) i++;
            continue;
        }
        if ((ch == '=' || ch == '!') && nx == '=' && pv != '=' && pv != '!' && pv != '<' && pv != '>' && s[i + 2] != '=') {
            snprintf(from, sizeof(from), "%c=", ch);
            snprintf(to, sizeof(to), "%c=", ch == '=' ? '!' : '=');
            push(g, splice(g->src, i, 2, to), 3, "equality", i, from, to);
            i++;
            continue;
        }
        if ((ch == '+' || ch == '-') && nx != ch && pv != ch && nx != '=' && nx != '>') {
            size_t p = i;
            while (p > f->lo && isspace((unsigned char)s[p - 1])) p--;
            if (p > f->lo && (is_idc((unsigned char)s[p - 1]) || s[p - 1] == ')' || s[p - 1] == ']') && !m[p - 1]) {
                snprintf(from, sizeof(from), "%c", ch);
                snprintf(to, sizeof(to), "%c", ch == '+' ? '-' : '+');
                push(g, splice(g->src, i, 1, to), 4, "plus_minus", i, from, to);
            }
            continue;
        }
        if (isdigit((unsigned char)ch) && !is_idc((unsigned char)pv) && pv != '.') {
            size_t e = i;
            while (e < f->hi && isdigit((unsigned char)s[e])) e++;
            if (!is_idc((unsigned char)s[e]) && s[e] != '.' && e - i <= 6 && !(e - i > 1 && ch == '0')) {
                long v = strtol(s + i, NULL, 10);
                snprintf(from, sizeof(from), "%ld", v);
                for (int d = -1; d <= 1; d += 2) {
                    if (v + d < 0) continue;
                    snprintf(to, sizeof(to), "%ld", v + d);
                    push(g, splice(g->src, i, e - i, to), 4, "int_literal", i, from, to);
                }
                /* init_mul: "x = 0;" and a later "x *=" in this function */
                if (v == 0 && s[e] == ';') {
                    size_t q = i;
                    while (q > f->lo && s[q - 1] == ' ') q--;
                    if (q > f->lo && s[q - 1] == '=' && !strchr("=!<>+-*/", s[q - 2])) {
                        q--;
                        while (q > f->lo && s[q - 1] == ' ') q--;
                        size_t nb = q;
                        while (nb > f->lo && is_idc((unsigned char)s[nb - 1])) nb--;
                        char name[64], pat[80];
                        if (q > nb && q - nb < sizeof(name)) {
                            snprintf(name, sizeof(name), "%.*s", (int)(q - nb), s + nb);
                            snprintf(pat, sizeof(pat), "%s *=", name);
                            const char *u = strstr(s + e, pat);
                            if (!u) { snprintf(pat, sizeof(pat), "%s*=", name); u = strstr(s + e, pat); }
                            if (u && (size_t)(u - s) < f->hi && !is_idc((unsigned char)u[-1]))
                                push(g, splice(g->src, i, 1, "1"), 2, "init_mul", i, "0", "1");
                        }
                    }
                }
            }
            i = e - 1;
            continue;
        }
        /* int_div: return A / B; in a double/float function */
        if (f->is_float && !strncmp(s + i, "return ", 7) && !is_idc((unsigned char)pv)) {
            const char *semi = strchr(s + i, ';');
            if (semi && (size_t)(semi - s) < f->hi) {
                const char *sl = memchr(s + i, '/', (size_t)(semi - s - i));
                const char *dot = memchr(s + i + 7, '.', (size_t)(semi - s - i - 7));
                const char *cast = memchr(s + i + 7, '(', (size_t)(semi - s - i - 7));
                if (sl && !dot && !cast)
                    push(g, splice(g->src, i + 7, 0, "(double)"), 2, "int_div", i, "return A / B", "return (double)A / B");
            }
        }
        /* index_shift: every [E] of this function */
        if (ch == '[') {
            const char *e = memchr(s + i, ']', f->hi - i);
            if (e && e - s - i - 1 > 0 && e - s - i - 1 < 40) {
                size_t el = (size_t)(e - s - i - 1);
                int has_id = 0, nested = 0, first = 1;
                for (size_t q = 0; q < el; q++) {
                    has_id |= isalpha((unsigned char)s[i + 1 + q]) || s[i + 1 + q] == '_';
                    nested |= s[i + 1 + q] == '[' || s[i + 1 + q] == '\n' || m[i + 1 + q] != 0;
                }
                char pat[48];
                snprintf(pat, sizeof(pat), "[%.*s]", (int)el, s + i + 1);
                for (size_t q = f->lo; q < i; q++)
                    if (!m[q] && !strncmp(s + q, pat, el + 2)) first = 0;
                if (has_id && !nested && first) {
                    for (int d = 0; d < 2; d++) {
                        char rep[64];
                        snprintf(rep, sizeof(rep), "[%.*s %c 1]", (int)el, s + i + 1, d ? '+' : '-');
                        size_t sl2 = strlen(g->src);
                        char *t = (char *)malloc(sl2 + 16 * 64 + 1);
                        if (!t) continue;
                        size_t o = 0, q = 0;
                        while (q < sl2) {
                            if (q >= f->lo && q < f->hi && !m[q] && !strncmp(g->src + q, pat, el + 2) && o < sl2 + 15 * 64) {
                                memcpy(t + o, rep, strlen(rep));
                                o += strlen(rep);
                                q += el + 2;
                            } else
                                t[o++] = g->src[q++];
                        }
                        t[o] = '\0';
                        push(g, t, d ? 4 : 2, "index_shift", i, pat, rep);   /* [E + 1] reads further: weakest */
                    }
                }
            }
        }
        /* stale_swap: t = X; X = Y; Y = X;  ->  Y = t; */
        if (ch == ';' || i == f->lo || (ch == '{')) {
            size_t st[4], en[4];
            size_t p = i == f->lo && ch != ';' && ch != '{' ? i : i + 1;
            int ok = 1;
            for (int k = 0; k < 3 && ok; k++) {
                while (p < f->hi && isspace((unsigned char)s[p])) p++;
                const char *sc = memchr(s + p, ';', f->hi - p);
                if (!sc || memchr(s + p, '{', (size_t)(sc - s) - p) || memchr(s + p, '}', (size_t)(sc - s) - p)) ok = 0;
                else { st[k] = p; en[k] = (size_t)(sc - s); p = en[k] + 1; }
            }
            if (ok) {
                char a[3][2][64];
                for (int k = 0; k < 3 && ok; k++) {
                    const char *eq = memchr(s + st[k], '=', en[k] - st[k]);
                    if (!eq || eq[1] == '=' || strchr("!<>+-*/", eq[-1])) { ok = 0; break; }
                    size_t l1 = (size_t)(eq - s) - st[k], r0 = (size_t)(eq - s) + 1;
                    while (l1 && s[st[k] + l1 - 1] == ' ') l1--;
                    size_t lb = l1;   /* last lvalue token run (drops a type) */
                    while (lb && s[st[k] + lb - 1] != ' ') lb--;
                    while (r0 < en[k] && s[r0] == ' ') r0++;
                    size_t rl = en[k] - r0;
                    while (rl && s[r0 + rl - 1] == ' ') rl--;
                    if (l1 - lb >= 64 || rl >= 64 || l1 == lb || rl == 0) { ok = 0; break; }
                    snprintf(a[k][0], 64, "%.*s", (int)(l1 - lb), s + st[k] + lb);
                    snprintf(a[k][1], 64, "%.*s", (int)rl, s + r0);
                }
                if (ok && !strcmp(a[0][1], a[1][0]) && !strcmp(a[1][1], a[2][0]) && !strcmp(a[2][1], a[1][0]) &&
                    strcmp(a[0][0], a[1][0])) {
                    const char *eq = memchr(s + st[2], '=', en[2] - st[2]);
                    size_t r0 = (size_t)(eq - s) + 1;
                    while (s[r0] == ' ') r0++;
                    char ins[80];
                    snprintf(ins, sizeof(ins), "%s", a[0][0]);
                    push(g, splice(g->src, r0, strlen(a[2][1]), ins), 2, "stale_swap", r0, a[2][1], ins);
                }
            }
        }
    }
}

int CContractCandidates(const char *src, const C_CONTRACT *c, int allow_main, C_CAND *out, int max)
{
    size_t n = strlen(src);
    GEN g = {src, c, out, 0, max, c && c->has_out ? count_lit(src, c->out) : 0};
    char *m = make_mask(src, n);
    if (!m) return 0;
    FN fn[64];
    int nf = find_functions(src, m, n, fn, 64);
    for (int k = 0; k < nf; k++)
        gen_fn(&g, src, m, &fn[k], !fn[k].is_main || allow_main);
    free(m);
    return g.n;
}

void CContractFree(C_CAND *c, int n)
{
    for (int i = 0; i < n; i++) {
        free(c[i].text);
        c[i].text = NULL;
    }
}
