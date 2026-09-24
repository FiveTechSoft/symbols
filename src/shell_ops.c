/* shell_ops.c: see shell_ops.h. No task ids, file names or answers live here. */
#include "shell_ops.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* case-insensitive substring */
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

static int starts_with(const char *s, const char *p)
{
    return !strncmp(s, p, strlen(p));
}

int ShellOpsIsScript(const char *rel, const char *data)
{
    size_t n = strlen(rel);
    if (n > 3 && !strcmp(rel + n - 3, ".sh"))
        return 1;
    return starts_with(data, "#!/bin/sh") || starts_with(data, "#!/bin/bash") ||
           starts_with(data, "#!/usr/bin/env sh") || starts_with(data, "#!/usr/bin/env bash");
}

/* does any line, trimmed, equal "set -e" or start with "set -e" / "set -eu"... */
static int has_set_e(const char *d)
{
    const char *l = d;
    while (*l) {
        while (*l == ' ' || *l == '\t')
            l++;
        if (starts_with(l, "set -") ) {
            const char *f = l + 5;
            while (isalpha((unsigned char)*f)) {
                if (*f == 'e')
                    return 1;
                f++;
            }
        }
        const char *nl = strchr(l, '\n');
        if (!nl)
            break;
        l = nl + 1;
    }
    return 0;
}

static char *dup_cat3(const char *a, size_t an, const char *b, const char *c)
{
    size_t bn = strlen(b), cn = strlen(c);
    char *o = (char *)malloc(an + bn + cn + 1);
    if (!o)
        return NULL;
    memcpy(o, a, an);
    memcpy(o + an, b, bn);
    memcpy(o + an + bn, c, cn + 1);
    return o;
}

/* length of a parameter expansion starting at s ('$'), 0 if none */
static size_t expansion_len(const char *s)
{
    if (s[0] != '$')
        return 0;
    if (s[1] == '{') {
        const char *e = strchr(s + 2, '}');
        if (!e || e == s + 2)
            return 0;
        for (const char *c = s + 2; c < e; c++)   /* plain ${name...} only */
            if (isspace((unsigned char)*c) || strchr("\"'\\$`{", *c))
                return 0;
        return (size_t)(e - s) + 1;
    }
    if (isdigit((unsigned char)s[1]) || (s[1] && strchr("@*#?", s[1])))
        return 2;
    size_t i = 1;
    while (isalnum((unsigned char)s[i]) || s[i] == '_')
        i++;
    return (i > 1 && !isdigit((unsigned char)s[1])) ? i : 0;
}

/* count bare (unquoted) expansions outside comments; if out != NULL write
   the text with each one double-quoted */
static int quote_bare(const char *d, char *out)
{
    int n = 0, dq = 0, sq = 0, comment = 0;
    size_t o = 0;
    for (size_t i = 0; d[i]; i++) {
        char c = d[i];
        if (c == '\n')
            comment = 0;
        if (!comment && !sq && !dq && c == '#' && (i == 0 || isspace((unsigned char)d[i - 1])))
            comment = 1;
        if (!comment) {
            if (c == '\\' && d[i + 1] && !sq) {
                if (out) { out[o++] = c; out[o++] = d[i + 1]; }
                i++;
                continue;
            }
            if (c == '\'' && !dq) sq = !sq;
            else if (c == '"' && !sq) dq = !dq;
            else if (c == '$' && !sq && !dq) {
                size_t L = expansion_len(d + i);
                /* leave $(( )), $( ) and assignments' right side alone? only bare params */
                if (L > 0) {
                    n++;
                    if (out) {
                        out[o++] = '"';
                        memcpy(out + o, d + i, L);
                        o += L;
                        out[o++] = '"';
                    }
                    i += L - 1;
                    continue;
                }
            }
        }
        if (out)
            out[o++] = c;
    }
    if (out)
        out[o] = '\0';
    return n;
}

/* first line of form "<sh|.|bash> F" or "./F" (trimmed); F copied out */
static const char *find_run_line(const char *d, char *file, size_t fsz, size_t *line_len, int *count)
{
    const char *hit = NULL, *l = d;
    *count = 0;
    while (*l) {
        const char *nl = strchr(l, '\n');
        size_t ll = nl ? (size_t)(nl - l) : strlen(l);
        const char *p = l;
        while (*p == ' ' || *p == '\t')
            p++;
        const char *arg = NULL;
        if (starts_with(p, "sh ") || starts_with(p, ". ") || starts_with(p, "bash "))
            arg = strchr(p, ' ') + 1;
        else if (starts_with(p, "./"))
            arg = p + 2;
        if (arg && arg < l + ll) {
            while (*arg == ' ')
                arg++;
            size_t fl = 0;
            while (arg + fl < l + ll && (isalnum((unsigned char)arg[fl]) || strchr("._-/", arg[fl])))
                fl++;
            if (fl > 0 && fl < fsz && arg + fl == l + ll) {
                (*count)++;
                if (!hit) {
                    memcpy(file, arg, fl);
                    file[fl] = '\0';
                    hit = l;
                    *line_len = ll;
                }
            }
        }
        if (!nl)
            break;
        l = nl + 1;
    }
    return hit;
}

/* "exit N" / "status N" / "code N" stated in the task, N in 1..125 */
static int task_exit_code(const char *task_in)
{
    char task[1024];
    size_t k0 = 0;
    for (; task_in[k0] && k0 + 1 < sizeof(task); k0++)
        task[k0] = (char)tolower((unsigned char)task_in[k0]);
    task[k0] = '\0';
    const char *keys[] = {"exit ", "status ", "code "};
    int found = -1;
    for (int k = 0; k < 3; k++)
        for (const char *p = task; (p = strstr(p, keys[k])) != NULL; p++) {
            const char *q = p + strlen(keys[k]);
            if (!isdigit((unsigned char)*q))
                continue;
            int v = atoi(q);
            if (v < 1 || v > 125)
                continue;
            if (found != -1 && found != v)
                return -1;
            found = v;
        }
    return found;
}

char *ShellOpsApply(const char *data, const char *task, char *rule, size_t rule_size,
                    char *detail, size_t detail_size)
{
    rule[0] = detail[0] = '\0';
    /* shebang */
    if (ci_has(task, "shebang") && !starts_with(data, "#!")) {
        snprintf(rule, rule_size, "shebang");
        snprintf(detail, detail_size, "added #!/bin/sh");
        return dup_cat3("", 0, "#!/bin/sh\n", data);
    }
    /* fail closed */
    if ((ci_has(task, "set -e") || (ci_has(task, "fail closed") && ci_has(task, "any command"))) &&
        !has_set_e(data)) {
        size_t head = 0;
        if (starts_with(data, "#!")) {
            const char *nl = strchr(data, '\n');
            head = nl ? (size_t)(nl - data) + 1 : strlen(data);
        }
        snprintf(rule, rule_size, "fail_closed");
        snprintf(detail, detail_size, "added set -e");
        char *o = dup_cat3(data, head, head && data[head - 1] != '\n' ? "\nset -e\n" : "set -e\n", data + head);
        return o;
    }
    /* quote bare expansions */
    if ((ci_has(task, "unquoted") || ci_has(task, "quote")) && quote_bare(data, NULL) > 0) {
        int n = quote_bare(data, NULL);
        char *o = (char *)malloc(strlen(data) + 2 * (size_t)n + 1);
        if (!o)
            return NULL;
        quote_bare(data, o);
        snprintf(rule, rule_size, "quote_vars");
        snprintf(detail, detail_size, "quoted %d expansion(s)", n);
        return o;
    }
    /* stated output: "print exactly TOKEN" and one echo line */
    {
        const char *k = NULL;
        const char *keys[] = {"print exactly ", "output exactly ", "prints exactly "};
        for (int i = 0; i < 3 && !k; i++) {
            const char *f = strstr(task, keys[i]);
            if (f) k = f + strlen(keys[i]);
        }
        if (k) {
            char tok[64];
            size_t tl = 0;
            while ((isalnum((unsigned char)k[tl]) || k[tl] == '_') && tl + 1 < sizeof(tok)) { tok[tl] = k[tl]; tl++; }
            tok[tl] = '\0';
            const char *hit = NULL, *l = data;
            size_t hl = 0;
            int echoes = 0;
            while (tl > 0 && *l) {
                const char *nl = strchr(l, '\n');
                size_t ll = nl ? (size_t)(nl - l) : strlen(l);
                const char *t = l;
                while (*t == ' ' || *t == '\t') t++;
                if (!strncmp(t, "echo ", 5) || !strncmp(t, "printf ", 7)) { echoes++; hit = t; hl = ll - (size_t)(t - l); }
                if (!nl) break;
                l = nl + 1;
            }
            if (tl > 0 && echoes == 1) {
                char line[96];
                snprintf(line, sizeof(line), "echo %s", tok);
                size_t at = (size_t)(hit - data), dl = strlen(data), il = strlen(line);
                if (hl == il && !strncmp(hit, line, il))
                    return NULL;   /* already prints it */
                char *o = (char *)malloc(dl - hl + il + 1);
                if (!o) return NULL;
                memcpy(o, data, at);
                memcpy(o + at, line, il);
                memcpy(o + at + il, data + at + hl, dl - at - hl + 1);
                snprintf(rule, rule_size, "stated_output");
                snprintf(detail, detail_size, "echo %s", tok);
                return o;
            }
        }
    }
    /* guard a run of a named file that may be missing */
    if (ci_has(task, "missing") || ci_has(task, "exist")) {
        char file[128];
        size_t ll = 0;
        int count = 0, code = task_exit_code(task);
        const char *l = find_run_line(data, file, sizeof(file), &ll, &count);
        if (l && count == 1 && code > 0 && strstr(task, file) && !strstr(data, "[ -f")) {
            char buf[512];
            snprintf(buf, sizeof(buf), "if [ -f %s ]; then\n    %.*s\nelse\n    exit %d\nfi", file, (int)ll, l, code);
            char *o = dup_cat3(data, (size_t)(l - data), buf, l + ll);
            snprintf(rule, rule_size, "file_guard");
            snprintf(detail, detail_size, "guarded %s, exit %d when missing", file, code);
            return o;
        }
    }
    return NULL;
}

int ShellOpsIntent(const char *data, const char *rule)
{
    if (!strcmp(rule, "shebang"))
        return starts_with(data, "#!");
    if (!strcmp(rule, "fail_closed"))
        return has_set_e(data);
    if (!strcmp(rule, "quote_vars"))
        return quote_bare(data, NULL) == 0;
    if (!strcmp(rule, "stated_output"))
        return strstr(data, "echo ") != NULL;
    if (!strcmp(rule, "file_guard"))
        return strstr(data, "[ -f ") != NULL && strstr(data, "exit ") != NULL;
    if (!strcmp(rule, "missing_then") || !strcmp(rule, "missing_do") || !strcmp(rule, "close_quote") ||
        !strcmp(rule, "close_block") || !strcmp(rule, "stray_closer"))
        return 1;   /* syntax repairs: the caller's sh -n is the evidence */
    return 0;
}

/* ------------------------------------------------ syntax candidates
   Single edits that could repair a script `sh -n` rejects. The caller keeps
   only an edit after which `sh -n` passes, and only when exactly one
   candidate of the lowest passing tier does. Task wording is not read. */

typedef struct { const char *p; size_t len; int indent; } SH_LINE;

static int sh_lines(const char *d, SH_LINE *ln, int max)
{
    int n = 0;
    for (const char *p = d; *p && n < max;) {
        const char *e = strchr(p, '\n');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        int ind = 0;
        while ((size_t)ind < l && (p[ind] == ' ' || p[ind] == '\t')) ind++;
        ln[n].p = p; ln[n].len = l; ln[n].indent = ind;
        n++;
        p = e ? e + 1 : p + l;
    }
    return n;
}

/* command-position words of one line (quotes and comments skipped) */
static int sh_words(const SH_LINE *L, char w[][16], int max)
{
    int n = 0, cmdpos = 1;
    size_t i = 0;
    while (i < L->len && n < max) {
        char c = L->p[i];
        if (c == '#' && (i == 0 || isspace((unsigned char)L->p[i - 1]))) break;
        if (c == ';' || c == '&' || c == '|' || c == '(' || c == ')' || c == '{' || c == '}') { cmdpos = 1; i++; continue; }
        if (isspace((unsigned char)c)) { i++; continue; }
        if (c == '"' || c == '\'') {
            char q = c;
            i++;
            while (i < L->len && L->p[i] != q) i += (q == '"' && L->p[i] == '\\') ? 2 : 1;
            i++;
            cmdpos = 0;
            continue;
        }
        size_t s = i;
        while (i < L->len && !isspace((unsigned char)L->p[i]) && !strchr(";&|(){}\"'", L->p[i])) i++;
        if (cmdpos && i - s < 16) {
            memcpy(w[n], L->p + s, i - s);
            w[n][i - s] = '\0';
            n++;
            const char *kw = w[n - 1];
            cmdpos = !strcmp(kw, "then") || !strcmp(kw, "do") || !strcmp(kw, "else") || !strcmp(kw, "elif") ||
                     !strcmp(kw, "if") || !strcmp(kw, "while") || !strcmp(kw, "until") || !strcmp(kw, "!");
        } else
            cmdpos = 0;
    }
    return n;
}

static char *sh_splice(const char *d, size_t at, size_t del, const char *ins)
{
    size_t n = strlen(d), il = strlen(ins);
    char *o = (char *)malloc(n - del + il + 1);
    if (!o) return NULL;
    memcpy(o, d, at);
    memcpy(o + at, ins, il);
    memcpy(o + at + il, d + at + del, n - at - del + 1);
    return o;
}

static int sh_add(SHELL_CAND *out, int max, int *n, char *text, int tier, const char *rule, int line)
{
    if (!text) return 0;
    if (*n >= max) { free(text); return 0; }
    out[*n].text = text;
    out[*n].tier = tier;
    snprintf(out[*n].rule, sizeof(out[*n].rule), "%s", rule);
    snprintf(out[*n].detail, sizeof(out[*n].detail), "line %d: %s", line, rule);
    (*n)++;
    return 1;
}

static int has_word(char w[][16], int nw, const char *k)
{
    for (int i = 0; i < nw; i++)
        if (!strcmp(w[i], k)) return 1;
    return 0;
}

int ShellSyntaxCandidates(const char *d, SHELL_CAND *out, int max)
{
    static SH_LINE ln[2048];
    int nl = sh_lines(d, ln, 2048), n = 0;
    if (nl >= 2048) return 0;
    /* block stack: opener line and closer word */
    int st_line[256], sp = 0;
    const char *st_close[256];
    for (int i = 0; i < nl; i++) {
        char w[32][16];
        int nw = sh_words(&ln[i], w, 32);
        for (int k = 0; k < nw; k++) {
            const char *c = !strcmp(w[k], "if") ? "fi" : (!strcmp(w[k], "for") || !strcmp(w[k], "while") || !strcmp(w[k], "until")) ? "done"
                          : !strcmp(w[k], "case") ? "esac" : NULL;
            if (c && sp < 256) { st_line[sp] = i; st_close[sp] = c; sp++; }
            else if ((!strcmp(w[k], "fi") || !strcmp(w[k], "done") || !strcmp(w[k], "esac")) && sp > 0 && !strcmp(st_close[sp - 1], w[k]))
                sp--;
        }
        /* tier 1: if/elif line without then (and the next line does not start with then); for/while without do */
        int need_then = (has_word(w, nw, "if") || has_word(w, nw, "elif")) && !has_word(w, nw, "then");
        int need_do = (has_word(w, nw, "for") || has_word(w, nw, "while") || has_word(w, nw, "until")) && !has_word(w, nw, "do");
        if (need_then || need_do) {
            int j = i + 1;
            while (j < nl && ln[j].len == (size_t)ln[j].indent) j++;
            char nx[4][16];
            int nn = j < nl ? sh_words(&ln[j], nx, 4) : 0;
            const char *kw = need_then ? "then" : "do";
            if (!(nn > 0 && !strcmp(nx[0], kw)) && memchr(ln[i].p, '#', ln[i].len) == NULL) {
                size_t at = (size_t)(ln[i].p - d) + ln[i].len;
                while (at > (size_t)(ln[i].p - d) && isspace((unsigned char)d[at - 1])) at--;
                char ins[16];
                snprintf(ins, sizeof(ins), "; %s", kw);
                sh_add(out, max, &n, sh_splice(d, at, 0, ins), 1, need_then ? "missing_then" : "missing_do", i + 1);
            }
        }
        /* tier 1: odd count of double quotes on a line with no single quote or backslash */
        {
            int dq = 0, other = 0;
            for (size_t q = 0; q < ln[i].len; q++) {
                dq += ln[i].p[q] == '"';
                other |= ln[i].p[q] == '\'' || ln[i].p[q] == '\\' || ln[i].p[q] == '#';
            }
            if ((dq & 1) && !other) {
                size_t at = (size_t)(ln[i].p - d) + ln[i].len;
                sh_add(out, max, &n, sh_splice(d, at, 0, "\""), 1, "close_quote", i + 1);
            }
        }
        /* tier 2: a closer alone on its line may be stray */
        if (nw == 1 && (!strcmp(w[0], "fi") || !strcmp(w[0], "done") || !strcmp(w[0], "esac"))) {
            size_t at = (size_t)(ln[i].p - d), del = ln[i].len + (ln[i].p[ln[i].len] == '\n');
            sh_add(out, max, &n, sh_splice(d, at, del, ""), 2, "stray_closer", i + 1);
        }
    }
    /* tier 1: exactly one block left open: close it where indentation returns
       to the opener's level (or at the end), only when the body is indented */
    if (sp == 1) {
        int o = st_line[0], at_line = nl;
        int body = o + 1;
        for (;; body++) {   /* skip blank lines and a "then"/"do" line of its own */
            if (body >= nl) break;
            if (ln[body].len == (size_t)ln[body].indent) continue;
            char bw[2][16];
            int bn = sh_words(&ln[body], bw, 2);
            if (bn == 1 && ln[body].indent == ln[o].indent && (!strcmp(bw[0], "then") || !strcmp(bw[0], "do"))) continue;
            break;
        }
        if (body < nl && ln[body].indent > ln[o].indent) {
            for (int j = body + 1; j < nl; j++) {
                if (ln[j].len == (size_t)ln[j].indent) continue;
                char w[4][16];
                int nw = sh_words(&ln[j], w, 4);
                if (ln[j].indent <= ln[o].indent && !(nw > 0 && (!strcmp(w[0], "else") || !strcmp(w[0], "elif") || !strcmp(w[0], "then") || !strcmp(w[0], "do")))) {
                    at_line = j;
                    break;
                }
            }
            size_t at = at_line < nl ? (size_t)(ln[at_line].p - d) : strlen(d);
            char ins[300];
            snprintf(ins, sizeof(ins), "%s%.*s%s\n", at > 0 && d[at - 1] != '\n' ? "\n" : "", ln[o].indent, ln[o].p, st_close[0]);
            sh_add(out, max, &n, sh_splice(d, at, 0, ins), 1, "close_block", at_line + 1);
        }
    }
    return n;
}

void ShellSyntaxCandidatesFree(SHELL_CAND *c, int n)
{
    for (int i = 0; i < n; i++) {
        free(c[i].text);
        c[i].text = NULL;
    }
}
