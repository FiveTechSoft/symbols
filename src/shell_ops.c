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
    return 0;
}
