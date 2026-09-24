/* build_ops.c: see build_ops.h. No task ids, file names or answers live here. */
#include "build_ops.h"
#include "shell_ops.h"

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

static const char *base_name(const char *rel)
{
    const char *b = strrchr(rel, '/');
    return b ? b + 1 : rel;
}

static int ends_with(const char *s, const char *suf)
{
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && !strcmp(s + n - m, suf);
}

static int is_cmake(const char *rel) { return !strcmp(base_name(rel), "CMakeLists.txt"); }
static int is_make(const char *rel)
{
    const char *b = base_name(rel);
    return !strcmp(b, "Makefile") || !strcmp(b, "makefile") || !strcmp(b, "GNUmakefile");
}
static int is_c_file(const char *rel) { return ends_with(rel, ".c"); }

/* whole-word occurrence of w in task (word chars: alnum _ . -) */
static int task_names(const char *task, const char *w)
{
    size_t n = strlen(w);
    for (const char *p = task; (p = strstr(p, w)) != NULL; p++) {
        int l = p == task || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
        char c = p[n];
        int r = !(isalnum((unsigned char)c) || c == '_');
        if (l && r)
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

/* the one C source the task names, else the only C source; -1 otherwise */
static int pick_c(const char *const *rels, int n, const char *task)
{
    int named = -1, only = -1, count = 0, nn = 0;
    for (int i = 0; i < n; i++) {
        if (!is_c_file(rels[i]))
            continue;
        count++;
        only = i;
        if (task_names(task, rels[i])) {
            named = i;
            nn++;
        }
    }
    if (nn == 1)
        return named;
    if (nn == 0 && count == 1)
        return only;
    return -1;
}

/* ------------------------------------------------------------ CMake */

/* find "add_executable(" ; returns offset of '(' or -1; *count = how many */
static long find_call(const char *d, const char *name, int *count)
{
    long hit = -1;
    size_t n = strlen(name);
    *count = 0;
    for (const char *p = d; *p; p++)
        if (!strncmp(p, name, n) && (p == d || !(isalnum((unsigned char)p[-1]) || p[-1] == '_'))) {
            const char *q = p + n;
            while (*q == ' ' || *q == '\t')
                q++;
            if (*q == '(') {
                if (hit < 0)
                    hit = q - d;
                (*count)++;
            }
        }
    return hit;
}

/* split the argument list of the call at '(' offset into words */
static int call_args(const char *d, long paren, char args[][128], int max, size_t *close)
{
    const char *p = d + paren + 1;
    int n = 0;
    while (*p && *p != ')') {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p || *p == ')')
            break;
        size_t l = 0;
        while (p[l] && !isspace((unsigned char)p[l]) && p[l] != ')')
            l++;
        if (n < max && l < 128) {
            memcpy(args[n], p, l);
            args[n][l] = '\0';
        }
        n++;
        p += l;
    }
    if (*p != ')')
        return -1;
    *close = (size_t)(p - d);
    return n;
}

static int in_workspace(const char *const *rels, int n, const char *rel)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(rels[i], rel))
            return 1;
    return 0;
}

/* ev: cmake's own configure output (evidence mode, task == ""), or NULL */
static int cmake_rules(const char *const *rels, const char *const *datas, int n, const char *task,
                       int f, BUILD_EDIT *e, const char *ev)
{
    const char *d = datas[f];
    int count;
    long p = find_call(d, "add_executable", &count);
    char args[8][128];
    size_t close = 0;
    int na = p >= 0 && count == 1 ? call_args(d, p, args, 8, &close) : -1;

    /* project() missing */
    if (!ci_has(d, "project(") && !ci_has(d, "project (") &&
        (ev ? strstr(ev, "No project() command is present") != NULL : ci_has(task, "project("))) {
        const char *name = na >= 1 ? args[0] : "app";
        int c2;
        long m = find_call(d, "cmake_minimum_required", &c2);
        size_t at = 0;
        if (m >= 0) {
            const char *nl = strchr(d + m, '\n');
            at = nl ? (size_t)(nl - d) + 1 : strlen(d);
        }
        char ins[200];
        snprintf(ins, sizeof(ins), "%sproject(%s C)\n", at > 0 && d[at - 1] != '\n' ? "\n" : "", name);
        e->text = splice(d, at, 0, ins);
        snprintf(e->rule, sizeof(e->rule), "cmake_project");
        snprintf(e->detail, sizeof(e->detail), "added project(%s C)", name);
        return e->text != NULL;
    }
    if (na == 1) {   /* add_executable(NAME) with no sources */
        if (ev && !strstr(ev, "No SOURCES given to target"))
            return 0;
        int c = pick_c(rels, n, task);   /* evidence mode: the one C file */
        if (c < 0 || (!ev && !task_names(task, rels[c]) && !ci_has(task, "add_executable")))
            return 0;
        char ins[300];
        snprintf(ins, sizeof(ins), " %s", rels[c]);
        e->text = splice(d, close, 0, ins);
        snprintf(e->rule, sizeof(e->rule), "cmake_add_source");
        snprintf(e->detail, sizeof(e->detail), "add_executable(%s %s)", args[0], rels[c]);
        return e->text != NULL;
    }
    if (na >= 2 && na <= 8 && (ev ? strstr(ev, "Cannot find source file") != NULL : (ci_has(task, "missing") || ci_has(task, "recreate")))) {
        int missing = -1, nm = 0;
        for (int a = 1; a < na; a++)
            if (!in_workspace(rels, n, args[a])) {
                missing = a;
                nm++;
            }
        if (nm != 1 || !is_c_file(args[missing]) || strchr(args[missing], '$') || strchr(args[missing], '/') ||
            !task_names(ev ? ev : task, args[missing]))
            return 0;
        for (int i = 0; i < n; i++)   /* another source already defines main */
            if (is_c_file(rels[i]) && strstr(datas[i], "main("))
                return 0;
        e->file = -1;
        snprintf(e->rel, sizeof(e->rel), "%s", args[missing]);
        e->text = splice("int main(void)\n{\n    return 0;\n}\n", 0, 0, "");
        snprintf(e->rule, sizeof(e->rule), "cmake_missing_source");
        snprintf(e->detail, sizeof(e->detail), "created minimal %s", args[missing]);
        return e->text != NULL;
    }
    return 0;
}

/* ------------------------------------------------------------- Make */

/* word after "depend on"/"depends on"/"depend upon" in the task */
static int task_dep(const char *task, char *out, size_t sz)
{
    const char *keys[] = {"depends on ", "depend on ", "depend upon ", "depends upon "};
    for (int k = 0; k < 4; k++) {
        const char *p = strstr(task, keys[k]);
        if (!p)
            continue;
        p += strlen(keys[k]);
        size_t l = 0;
        while ((isalnum((unsigned char)p[l]) || p[l] == '_') && l + 1 < sz)
            l++;
        if (l == 0)
            continue;
        memcpy(out, p, l);
        out[l] = '\0';
        return 1;
    }
    return 0;
}

static int make_rule(const char *const *rels, int n, const char *task, const char *d, BUILD_EDIT *e)
{
    char t[64], src[80];
    if (!task_dep(task, t, sizeof(t)))
        return 0;
    snprintf(src, sizeof(src), "%s.c", t);
    if (!in_workspace(rels, n, src))
        return 0;
    /* "all:" line with no prerequisites */
    const char *l = d, *all = NULL;
    while (*l) {
        if (!strncmp(l, "all:", 4)) {
            const char *q = l + 4;
            while (*q == ' ' || *q == '\t')
                q++;
            if (*q != '\n' && *q != '\0')
                return 0;   /* already has prerequisites */
            all = l;
            break;
        }
        const char *nl = strchr(l, '\n');
        if (!nl)
            break;
        l = nl + 1;
    }
    if (!all)
        return 0;
    /* all's recipe: drop it only when every line is an echo placeholder */
    const char *rec = strchr(all, '\n');
    rec = rec ? rec + 1 : all + strlen(all);
    const char *r = rec;
    int placeholder = 1;
    while (*r == '\t') {
        const char *c = r + 1;
        if (*c == '@')
            c++;
        if (strncmp(c, "echo", 4) != 0)
            placeholder = 0;
        const char *nl = strchr(r, '\n');
        r = nl ? nl + 1 : r + strlen(r);
    }
    size_t cut_end = placeholder ? (size_t)(r - d) : (size_t)(rec - d);
    char has_rule[80];
    snprintf(has_rule, sizeof(has_rule), "%s:", t);
    int rule_exists = !strncmp(d, has_rule, strlen(has_rule));
    char pat[84];
    snprintf(pat, sizeof(pat), "\n%s:", t);
    rule_exists = rule_exists || strstr(d, pat) != NULL;
    char ins[400];
    if (rule_exists)
        snprintf(ins, sizeof(ins), "all: %s\n", t);
    else
        snprintf(ins, sizeof(ins), "all: %s\n\n%s: %s\n\t$(CC) -o %s %s\n", t, t, src, t, src);
    size_t at = (size_t)(all - d);
    if (placeholder)
        e->text = splice(d, at, cut_end - at, ins);
    else {
        /* keep the recipe: only rewrite the "all:" line */
        const char *nl = strchr(all, '\n');
        size_t ll = nl ? (size_t)(nl - all) + 1 : strlen(all);
        char line[80];
        snprintf(line, sizeof(line), "all: %s\n", t);
        e->text = splice(d, at, ll, line);
        if (e->text && !rule_exists) {
            char tail[300];
            snprintf(tail, sizeof(tail), "%s%s: %s\n\t$(CC) -o %s %s\n",
                     ends_with(e->text, "\n") ? "\n" : "\n\n", t, src, t, src);
            char *o = splice(e->text, strlen(e->text), 0, tail);
            free(e->text);
            e->text = o;
        }
    }
    snprintf(e->rule, sizeof(e->rule), "make_dep");
    snprintf(e->detail, sizeof(e->detail), "all: %s%s", t, rule_exists ? "" : " (+ rule from source)");
    snprintf(e->expect, sizeof(e->expect), "%s", src);
    return e->text != NULL;
}

/* ------------------------------------------------ shell build scripts */

static int has_compiler(const char *d)
{
    const char *cs[] = {"gcc", "clang", "cc", "$CC", "${CC}", "tcc", "cl"};
    for (int k = 0; k < 7; k++)
        if (task_names(d, cs[k]))
            return 1;
    return 0;
}

static int build_script_rule(const char *const *rels, int n, const char *task, const char *d, BUILD_EDIT *e)
{
    if (!ci_has(task, "compiler") && !task_names(task, "gcc") && !task_names(task, "clang") &&
        !task_names(task, "cc"))
        return 0;
    if (has_compiler(d))
        return 0;
    int c = pick_c(rels, n, task);
    if (c < 0 || strchr(rels[c], '"') || strchr(rels[c], ' '))
        return 0;
    const char *cc = task_names(task, "clang") ? "clang" : task_names(task, "gcc") ? "gcc" : "cc";
    /* flags the task states (safe characters only); default: syntax check */
    char flags[160] = "";
    size_t fl = 0;
    for (const char *p = task; *p; p++) {
        if (*p != '-' || (p > task && !isspace((unsigned char)p[-1])))
            continue;
        size_t l = 1;
        while (p[l] && (isalnum((unsigned char)p[l]) || strchr("=_+-", p[l])))
            l++;
        while (l > 1 && p[l - 1] == '-')
            l--;
        if (l < 3 || fl + l + 2 >= sizeof(flags))
            continue;
        memcpy(flags + fl, p, l);
        fl += l;
        flags[fl++] = ' ';
        flags[fl] = '\0';
    }
    if (!fl)
        snprintf(flags, sizeof(flags), "-fsyntax-only ");
    /* new body: shebang, set -e, original lines minus hardcoded "exit 0", compile */
    size_t cap = strlen(d) + 400;
    char *o = (char *)malloc(cap);
    if (!o)
        return 0;
    size_t used = 0;
    const char *l = d;
    int wrote_head = 0;
    if (!strncmp(d, "#!", 2)) {
        const char *nl = strchr(d, '\n');
        size_t hl = nl ? (size_t)(nl - d) + 1 : strlen(d);
        memcpy(o, d, hl);
        used = hl;
        if (!nl)
            o[used++] = '\n';
        l = d + hl;
    } else {
        used += (size_t)snprintf(o, cap, "#!/bin/sh\n");
    }
    used += (size_t)snprintf(o + used, cap - used, "set -e\n");
    wrote_head = 1;
    (void)wrote_head;
    while (*l) {
        const char *nl = strchr(l, '\n');
        size_t ll = nl ? (size_t)(nl - l) : strlen(l);
        const char *t = l;
        while (*t == ' ' || *t == '\t')
            t++;
        size_t tl = ll - (size_t)(t - l);
        int drop = (tl == 6 && !strncmp(t, "exit 0", 6)) || (tl == 6 && !strncmp(t, "set -e", 6));
        if (!drop) {
            memcpy(o + used, l, ll);
            used += ll;
            o[used++] = '\n';
        }
        if (!nl)
            break;
        l = nl + 1;
    }
    used += (size_t)snprintf(o + used, cap - used, "%s %s%s\n", cc, flags, rels[c]);
    e->text = o;
    snprintf(e->rule, sizeof(e->rule), "build_compile_step");
    snprintf(e->detail, sizeof(e->detail), "set -e; %s %.100s%.40s", cc, flags, rels[c]);
    snprintf(e->expect, sizeof(e->expect), "%s", rels[c]);
    return 1;
}

/* ------------------------------------------------ GitHub workflow */

static int ci_rule(const char *task, const char *d, BUILD_EDIT *e)
{
    if (!ci_has(task, "ctest") || strstr(d, "ctest") || !strstr(d, "jobs:"))
        return 0;
    /* exactly one "steps:" line */
    const char *s = NULL;
    int ns = 0;
    for (const char *p = d; (p = strstr(p, "steps:")) != NULL; p++) {
        const char *b = p;
        while (b > d && (b[-1] == ' '))
            b--;
        if (b == d || b[-1] == '\n') {
            s = p;
            ns++;
        }
    }
    if (ns != 1)
        return 0;
    const char *line = s;
    while (line > d && line[-1] == ' ')
        line--;
    size_t sind = (size_t)(s - line);
    /* walk following lines while indented deeper; remember item indent */
    const char *nl = strchr(s, '\n');
    if (!nl)
        return 0;
    const char *p = nl + 1, *end = p;
    int item = -1;
    while (*p) {
        const char *q = p;
        while (*q == ' ')
            q++;
        const char *e2 = strchr(p, '\n');
        size_t ll = e2 ? (size_t)(e2 - p) : strlen(p);
        if ((size_t)(q - p) == ll) {   /* blank */
            if (!e2) break;
            p = e2 + 1;
            continue;
        }
        if ((size_t)(q - p) <= sind)
            break;
        if (q[0] == '-' && item < 0)
            item = (int)(q - p);
        end = e2 ? e2 + 1 : p + ll;
        if (!e2)
            break;
        p = e2 + 1;
    }
    if (item < 0)
        return 0;
    char ins[128];
    int need_nl = end > d && end[-1] != '\n';
    snprintf(ins, sizeof(ins), "%s%*s- run: ctest --test-dir build\n", need_nl ? "\n" : "", item, "");
    e->text = splice(d, (size_t)(end - d), 0, ins);
    snprintf(e->rule, sizeof(e->rule), "ci_ctest");
    snprintf(e->detail, sizeof(e->detail), "added a ctest step");
    return e->text != NULL;
}

/* ------------------------------------------------------------ plan */

int BuildOpsPlan(const char *const *rels, const char *const *datas, int n, const char *task, BUILD_EDIT *out)
{
    int found = 0;
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < n; i++) {
        BUILD_EDIT e;
        memset(&e, 0, sizeof(e));
        e.file = i;
        snprintf(e.rel, sizeof(e.rel), "%s", rels[i]);
        int ok = 0;
        if (is_cmake(rels[i]))
            ok = cmake_rules(rels, datas, n, task, i, &e, NULL);
        else if (is_make(rels[i]))
            ok = make_rule(rels, n, task, datas[i], &e);
        else if (ShellOpsIsScript(rels[i], datas[i]))
            ok = build_script_rule(rels, n, task, datas[i], &e);
        else if (ends_with(rels[i], ".yml") || ends_with(rels[i], ".yaml"))
            ok = ci_rule(task, datas[i], &e);
        if (!ok) {
            free(e.text);
            continue;
        }
        if (found) {   /* ambiguous */
            free(e.text);
            free(out->text);
            memset(out, 0, sizeof(*out));
            return 0;
        }
        *out = e;
        found = 1;
    }
    return found;
}

int BuildOpsPlanEvidence(const char *const *rels, const char *const *datas, int n, const char *cmake_out, BUILD_EDIT *out)
{
    int found = 0;
    memset(out, 0, sizeof(*out));
    if (!cmake_out)
        return 0;
    for (int i = 0; i < n; i++) {
        if (!is_cmake(rels[i]) || strchr(rels[i], '/'))
            continue;   /* the top-level CMakeLists.txt that was configured */
        BUILD_EDIT e;
        memset(&e, 0, sizeof(e));
        e.file = i;
        snprintf(e.rel, sizeof(e.rel), "%s", rels[i]);
        if (!cmake_rules(rels, datas, n, "", i, &e, cmake_out)) {
            free(e.text);
            continue;
        }
        if (found) {
            free(e.text);
            free(out->text);
            memset(out, 0, sizeof(*out));
            return 0;
        }
        *out = e;
        found = 1;
    }
    return found;
}

int BuildOpsIntent(const char *text, const BUILD_EDIT *e)
{
    if (!text)
        return 0;
    if (!strcmp(e->rule, "ci_ctest"))
        return strstr(text, "- run: ctest") != NULL;
    if (!strcmp(e->rule, "cmake_project"))
        return ci_has(text, "project(");
    if (!strcmp(e->rule, "build_compile_step"))
        return has_compiler(text) && strstr(text, "set -e") != NULL;
    return 1;   /* cmake_add_source, cmake_missing_source, make_dep: verified by running the build */
}
