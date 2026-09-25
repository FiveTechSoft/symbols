/* shell_contract.c: see shell_contract.h. No task ids, file names or answers live here. */
#include "shell_contract.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ci_has_n(const char *s, size_t n, const char *needle)
{
    size_t k = strlen(needle);
    for (size_t i = 0; i + k <= n; i++) {
        size_t j = 0;
        while (j < k && tolower((unsigned char)s[i + j]) == tolower((unsigned char)needle[j])) j++;
        if (j == k) return 1;
    }
    return 0;
}

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

/* split a backticked command into words (single/double quotes group) */
static int split_words(const char *s, size_t n, char w[][96], int max)
{
    int k = 0;
    size_t i = 0;
    while (i < n && k < max) {
        while (i < n && s[i] == ' ') i++;
        if (i >= n) break;
        size_t o = 0;
        while (i < n && s[i] != ' ') {
            if (s[i] == '\'' || s[i] == '"') {
                char q = s[i++];
                while (i < n && s[i] != q) { if (o + 1 < 96) w[k][o++] = s[i]; i++; }
                if (i < n) i++;
            } else {
                if (o + 1 < 96) w[k][o++] = s[i];
                i++;
            }
        }
        w[k][o] = '\0';
        k++;
    }
    return k;
}

static int safe_word(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if (*s == '`' || *s == '$' || *s == '\\' || *s == '"' || *s == '\'' || *s == '\n') return 0;
    return 1;
}

int ShellContractParse(const char *task, SH_CONTRACT *c)
{
    memset(c, 0, sizeof(*c));
    c->exit_want = -1;
    int inv = 0;
    /* the one quoted invocation */
    for (const char *p = strchr(task, '`'); p; ) {
        const char *e = strchr(p + 1, '`');
        if (!e) break;
        char w[6][96];
        int k = split_words(p + 1, (size_t)(e - p - 1), w, 6);
        if (k >= 2 && (!strcmp(w[0], "sh") || !strcmp(w[0], "bash"))) {
            size_t l = strlen(w[1]);
            if (l < 4 || strcmp(w[1] + l - 3, ".sh") || strchr(w[1], '/') || !safe_word(w[1])) return 0;
            if (inv && strcmp(w[1], c->script)) return 0;   /* two scripts */
            if (!inv) {
                snprintf(c->script, sizeof(c->script), "%s", w[1]);
                c->nargs = k - 2 > 4 ? 4 : k - 2;
                for (int i = 0; i < c->nargs; i++) {
                    if (!safe_word(w[i + 2]) && strchr(w[i + 2], '$')) return 0;
                    snprintf(c->args[i], sizeof(c->args[i]), "%s", w[i + 2]);
                }
            } else if (k - 2 != c->nargs)
                return 0;   /* two different invocations: not one contract */
            inv = 1;
        }
        p = strchr(e + 1, '`');
    }
    if (!inv) return 0;
    /* clauses */
    const char *s = task;
    while (*s) {
        size_t n = strcspn(s, ".;");
        /* "x.txt" / "run.sh": a dot followed by a letter is not a clause end */
        while (s[n] == '.' && isalnum((unsigned char)s[n + 1])) n += 1 + strcspn(s + n + 1, ".;");
        char cl[480];
        size_t o = 0; int par = 0;
        for (size_t i = 0; i < n && o + 1 < sizeof(cl); i++) {   /* drop (...) */
            if (s[i] == '(') par++;
            else if (s[i] == ')' && par) par--;
            else if (!par) cl[o++] = s[i];
        }
        cl[o] = '\0';
        /* ", but ..." tail describes the current state */
        const char *but = ci_find_n(cl, o, ", but");
        if (but) { o = (size_t)(but - cl); cl[o] = '\0'; }
        int now = ci_has_n(cl, o, "currently") || ci_has_n(cl, o, "right now") || ci_has_n(cl, o, "instead") ||
                  ci_has_n(cl, o, "fails with") || ci_has_n(cl, o, "but it") || ci_has_n(cl, o, "at the moment");
        if (!now) {
            /* stdout */
            const char *pw = ci_find_n(cl, o, "print");
            if (!pw) pw = ci_find_n(cl, o, "stdout");
            if (!pw) pw = ci_find_n(cl, o, "output");
            if (pw && !ci_has_n(cl, o, "file")) {
                const char *q = strchr(pw, '`');
                if (q && strchr(q + 1, '`')) {
                    const char *qe = strchr(q + 1, '`');
                    char v[128];
                    snprintf(v, sizeof(v), "%.*s", (int)(qe - q - 1), q + 1);
                    if (strncmp(v, "sh ", 3) && strncmp(v, "bash ", 5)) {
                        if (c->has_out && strcmp(c->out, v)) return 0;
                        c->has_out = 1;
                        snprintf(c->out, sizeof(c->out), "%s", v);
                    }
                } else if (ci_find_n(pw, o - (size_t)(pw - cl), "the argument") && c->nargs > 0) {
                    c->has_out = 1;
                    snprintf(c->out, sizeof(c->out), "%s", c->args[c->nargs - 1]);
                }
            }
            /* exit */
            if (ci_has_n(cl, o, "non-zero") || ci_has_n(cl, o, "nonzero")) {
                if (c->exit_want >= 0) return 0;
                c->exit_want = -2;
            } else {
                const char *keys[] = {"exit code ", "status ", "exit with ", "exit "};
                for (int k = 0; k < 4; k++) {
                    const char *e = ci_find_n(cl, o, keys[k]);
                    if (!e) continue;
                    e += strlen(keys[k]);
                    if (!strncmp(e, "status ", 7)) e += 7;
                    else if (!strncmp(e, "code ", 5)) e += 5;
                    if (isdigit((unsigned char)*e)) {
                        int v = atoi(e);
                        if (c->exit_want != -1 && c->exit_want != v) return 0;
                        c->exit_want = v;
                        break;
                    }
                }
            }
            /* file content */
            const char *fw = ci_find_n(cl, o, "file ");
            if (fw && ci_has_n(cl, o, "contain")) {
                char f[96] = "";
                sscanf(fw + 5, "%95[A-Za-z0-9_.-]", f);
                const char *q = strchr(fw, '`');
                const char *qe = q ? strchr(q + 1, '`') : NULL;
                if (f[0] && strchr(f, '.') && qe && safe_word(f)) {
                    c->has_file = 1;
                    snprintf(c->file, sizeof(c->file), "%s", f);
                    snprintf(c->word, sizeof(c->word), "%.*s", (int)(qe - q - 1), q + 1);
                    if (!safe_word(c->word)) return 0;
                }
            }
        }
        s += n;
        if (*s) s++;
    }
    if (c->has_out && !safe_word(c->out) && strchr(c->out, '$')) return 0;
    return c->has_out || c->exit_want != -1 || c->has_file;
}

/* ---------------------------------------------------------------- edits */

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

static int push(SH_CAND *out, int n, int max, char *text, int tier, const char *rule, const char *detail)
{
    if (!text) return n;
    for (int i = 0; i < n; i++)
        if (!strcmp(out[i].text, text)) { free(text); return n; }
    if (n >= max) { free(text); return n; }
    out[n].text = text;
    out[n].tier = tier;
    snprintf(out[n].rule, sizeof(out[n].rule), "%s", rule);
    snprintf(out[n].detail, sizeof(out[n].detail), "%s", detail);
    return n + 1;
}

static int is_idc(int ch) { return isalnum(ch) || ch == '_'; }

int ShellContractCandidates(const char *s, const SH_CONTRACT *c, SH_CAND *out, int max)
{
    int n = 0, lineno = 0;
    char d[120];
    for (const char *l = s; *l; ) {
        const char *nl = strchr(l, '\n');
        size_t ll = nl ? (size_t)(nl - l) : strlen(l);
        size_t lo = (size_t)(l - s);
        lineno++;
        const char *t = l;
        while (*t == ' ' || *t == '\t') t++;
        size_t ind = (size_t)(t - l), tl = ll - ind;
        int comment = *t == '#';
        if (!comment) {
            /* eq_test: == inside [ ] */
            for (size_t i = 0; i + 1 < ll; i++)
                if (l[i] == '=' && l[i + 1] == '=' && memchr(l, '[', i) && (i + 2 >= ll || l[i + 2] != '=')) {
                    snprintf(d, sizeof(d), "== -> = at line %d", lineno);
                    n = push(out, n, max, splice(s, lo + i, 2, "="), 1, "eq_test", d);
                    break;
                }
            /* bracket_space */
            for (size_t i = 0; i + 1 < ll; i++) {
                if (l[i] == '[' && (i == 0 || l[i - 1] == ' ') && l[i + 1] != ' ' && l[i + 1] != '[') {
                    char *a = splice(s, lo + i + 1, 0, " ");
                    /* matching ] with no space before it on the same line */
                    if (a) {
                        const char *al = a + lo, *ae = strchr(al, '\n');
                        size_t al_n = ae ? (size_t)(ae - al) : strlen(al);
                        for (size_t j = i + 2; j < al_n; j++)
                            if (al[j] == ']' && al[j - 1] != ' ' && (j + 1 == al_n || al[j + 1] == ';' || al[j + 1] == ' ')) {
                                char *b = splice(a, lo + j, 0, " ");
                                free(a);
                                a = b;
                                break;
                            }
                        snprintf(d, sizeof(d), "spaces inside [ ] at line %d", lineno);
                        n = push(out, n, max, a, 1, "bracket_space", d);
                    }
                    break;
                }
            }
            /* assign_space: NAME = value (whole line) */
            {
                size_t k = 0;
                while (k < tl && is_idc((unsigned char)t[k])) k++;
                if (k > 0 && !isdigit((unsigned char)t[0]) && k + 3 <= tl && t[k] == ' ' && t[k + 1] == '=' && t[k + 2] == ' ' &&
                    t[k + 3] != '=' && strncmp(t, "if", 2) && strncmp(t, "test", 4)) {
                    char *a = splice(s, lo + ind + k, 3, "=");
                    snprintf(d, sizeof(d), "assignment without spaces at line %d", lineno);
                    n = push(out, n, max, a, 1, "assign_space", d);
                }
            }
            /* quote_expansion in echo/printf arguments: bare $x / ${x} */
            if (!strncmp(t, "echo ", 5) || !strncmp(t, "printf ", 7)) {
                char buf[1024];
                size_t o = 0;
                int changed = 0, dq = 0, sq = 0;
                for (size_t i = 0; i < tl && o + 4 < sizeof(buf); i++) {
                    char ch = t[i];
                    if (ch == '"' && !sq) dq = !dq;
                    else if (ch == '\'' && !dq) sq = !sq;
                    if (ch == '$' && !dq && !sq && i + 1 < tl && (is_idc((unsigned char)t[i + 1]) || t[i + 1] == '{')) {
                        size_t j = i + 1;
                        if (t[j] == '{') { while (j < tl && t[j] != '}') j++; j++; }
                        else while (j < tl && is_idc((unsigned char)t[j])) j++;
                        if (j > tl || o + (j - i) + 3 >= sizeof(buf)) break;
                        buf[o++] = '"';
                        memcpy(buf + o, t + i, j - i); o += j - i;
                        buf[o++] = '"';
                        i = j - 1;
                        changed = 1;
                        continue;
                    }
                    buf[o++] = ch;
                }
                buf[o] = '\0';
                if (changed) {
                    snprintf(d, sizeof(d), "quoted expansions at line %d", lineno);
                    n = push(out, n, max, splice(s, lo + ind, tl, buf), 1, "quote_expansion", d);
                }
            }
            /* exit_literal: every "exit K" command word on the line */
            if (c->exit_want >= 0)
                for (size_t i = 0; i + 6 <= tl; i++) {
                    if (strncmp(t + i, "exit ", 5) || !isdigit((unsigned char)t[i + 5]) ||
                        (i > 0 && !strchr(" \t;)&|", t[i - 1])))
                        continue;
                    size_t k = i + 5;
                    while (k < tl && isdigit((unsigned char)t[k])) k++;
                    if (atoi(t + i + 5) == c->exit_want) continue;
                    char num[16];
                    snprintf(num, sizeof(num), "%d", c->exit_want);
                    snprintf(d, sizeof(d), "exit %d at line %d", c->exit_want, lineno);
                    n = push(out, n, max, splice(s, lo + ind + i + 5, k - i - 5, num), 2, "exit_literal", d);
                }
            /* echo_literal: an echo with no expansion prints the wanted line */
            if (c->has_out && !strncmp(t, "echo ", 5) && !memchr(t, '$', tl) && !memchr(t, '`', tl) &&
                !memchr(t, '>', tl) && !memchr(t, '|', tl) && !memchr(t, ';', tl) && !memchr(t, '&', tl)) {
                char buf[200];
                snprintf(buf, sizeof(buf), "echo \"%s\"", c->out);
                snprintf(d, sizeof(d), "echo %.80s at line %d", c->out, lineno);
                n = push(out, n, max, splice(s, lo + ind, tl, buf), 2, "echo_literal", d);
            }
            /* default_param: $1 -> ${1:-X} when the invocation passes nothing */
            if (c->has_out && c->nargs == 0) {
                for (size_t i = 0; i + 1 < tl; i++)
                    if (t[i] == '$' && t[i + 1] == '1' && (i + 2 >= tl || !isdigit((unsigned char)t[i + 2]))) {
                        char buf[160];
                        snprintf(buf, sizeof(buf), "${1:-%s}", c->out);
                        snprintf(d, sizeof(d), "$1 defaults to %.60s at line %d", c->out, lineno);
                        n = push(out, n, max, splice(s, lo + ind + i, 2, buf), 2, "default_param", d);
                        break;
                    }
            }
        }
        if (!nl) break;
        l = nl + 1;
    }
    /* fail_closed */
    if (c->exit_want == -2 && !strstr(s, "set -e")) {
        size_t at = 0;
        if (!strncmp(s, "#!", 2)) { const char *e = strchr(s, '\n'); at = e ? (size_t)(e - s) + 1 : strlen(s); }
        n = push(out, n, max, splice(s, at, 0, at && s[at - 1] != '\n' ? "\nset -e\n" : "set -e\n"), 1, "fail_closed", "added set -e");
    }
    /* file_write */
    if (c->has_file && !strstr(s, c->file)) {
        char buf[240];
        size_t sl = strlen(s);
        snprintf(buf, sizeof(buf), "%secho %s > %s\n", sl && s[sl - 1] != '\n' ? "\n" : "", c->word, c->file);
        snprintf(d, sizeof(d), "echo %.40s > %.60s appended", c->word, c->file);
        n = push(out, n, max, splice(s, sl, 0, buf), 2, "file_write", d);
    }
    return n;
}

void ShellContractFree(SH_CAND *c, int n)
{
    for (int i = 0; i < n; i++) { free(c[i].text); c[i].text = NULL; }
}
