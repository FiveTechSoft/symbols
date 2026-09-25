/*
 * task_ops.c - see include/task_ops.h.
 */
#include "task_ops.h"
#include "git_ops.h"
#include "agent_shell.h"
#include "shell_ops.h"
#include "build_ops.h"
#include "c_fix_ops.h"
#include "compile_repair.h"
#include "shell_contract.h"
#include "code_graph.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ---------------------------------------------------------------- workspace */

static int is_text(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (data[i] == '\0')
            return 0;
    return 1;
}

static char *read_all(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0 || n > TASK_OPS_MAX_FILE) { fclose(f); return NULL; }
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    *out_len = got;
    return buf;
}

static void add_file(TASK_OPS_WORKSPACE *ws, const char *full, const char *rel)
{
    if (ws->count >= TASK_OPS_MAX_FILES)
        return;
    size_t len = 0;
    char *data = read_all(full, &len);
    if (!data)
        return;
    if (!is_text(data, len)) { free(data); return; }
    TASK_OPS_FILE *f = &ws->files[ws->count++];
    snprintf(f->rel, sizeof(f->rel), "%s", rel);
    f->data = data;
    f->len = len;
}

static void walk(TASK_OPS_WORKSPACE *ws, const char *dir, const char *rel, int depth)
{
    if (depth > 4 || ws->count >= TASK_OPS_MAX_FILES)
        return;
#ifdef _WIN32
    char pattern[TASK_OPS_MAX_PATH * 2];
    WIN32_FIND_DATAA fd;
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        const char *name = fd.cFileName;
        if (name[0] == '.' || CodeGraphShouldIgnoreName(name))
            continue;
        char full[TASK_OPS_MAX_PATH * 2], r[TASK_OPS_MAX_PATH * 2];
        snprintf(full, sizeof(full), "%s\\%s", dir, name);
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            walk(ws, full, r, depth + 1);
        else
            add_file(ws, full, r);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (name[0] == '.' || CodeGraphShouldIgnoreName(name))
            continue;
        char full[TASK_OPS_MAX_PATH * 2], r[TASK_OPS_MAX_PATH * 2];
        struct stat st;
        snprintf(full, sizeof(full), "%s/%s", dir, name);
        snprintf(r, sizeof(r), "%s%s%s", rel, rel[0] ? "/" : "", name);
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            walk(ws, full, r, depth + 1);
        else if (S_ISREG(st.st_mode))
            add_file(ws, full, r);
    }
    closedir(d);
#endif
}

static int cmp_file(const void *x, const void *y)
{
    return strcmp(((const TASK_OPS_FILE *)x)->rel, ((const TASK_OPS_FILE *)y)->rel);
}

int TaskOpsLoadWorkspace(const char *root, TASK_OPS_WORKSPACE *ws)
{
    if (!root || !ws)
        return 0;
    memset(ws, 0, sizeof(*ws));
    snprintf(ws->root, sizeof(ws->root), "%s", root);
    walk(ws, root, "", 0);
    qsort(ws->files, (size_t)ws->count, sizeof(ws->files[0]), cmp_file);
    return ws->count;
}

void TaskOpsFreeWorkspace(TASK_OPS_WORKSPACE *ws)
{
    if (!ws)
        return;
    for (int i = 0; i < ws->count; i++)
        free(ws->files[i].data);
    ws->count = 0;
}

/* ------------------------------------------------------------------ tokens */

static int ident_start(int c) { return isalpha(c) || c == '_'; }
static int ident_char(int c)  { return isalnum(c) || c == '_'; }

static int count_token_in(const char *s, const char *ident)
{
    size_t n = strlen(ident);
    int count = 0;
    if (n == 0)
        return 0;
    for (const char *p = strstr(s, ident); p; p = strstr(p + 1, ident)) {
        int left_ok = (p == s) || !ident_char((unsigned char)p[-1]);
        int right_ok = !ident_char((unsigned char)p[n]);
        if (left_ok && right_ok)
            count++;
    }
    return count;
}

int TaskOpsCountToken(const TASK_OPS_WORKSPACE *ws, const char *ident)
{
    int total = 0;
    for (int i = 0; ws && i < ws->count; i++)
        total += count_token_in(ws->files[i].data, ident);
    return total;
}

/* A task token that names code rather than prose: an identifier with an
   underscore, a digit or an inner capital, or any identifier the task quotes.
   Plain words ("the", "Rename") never qualify, so prose cannot be renamed. */
static int code_shaped(const char *id, int quoted)
{
    if (quoted)
        return 1;
    for (size_t i = 0; id[i]; i++) {
        if (id[i] == '_' || isdigit((unsigned char)id[i]))
            return 1;
        if (i > 0 && isupper((unsigned char)id[i]) && islower((unsigned char)id[i - 1]))
            return 1;
    }
    return 0;
}

typedef struct
{
    char   text[128];
    size_t start, end;   /* byte span in the task text */
} TASK_TOKEN;

/* Code-shaped identifiers in order of appearance. Tokens glued to '-' or '='
   are compiler flags, not names. */
static int c_keyword(const char *w)
{
    static const char *kw[] = {"int", "char", "void", "long", "short", "float", "double", "return", "if", "else",
                               "for", "while", "do", "const", "static", "struct", "enum", "union", "unsigned",
                               "signed", "sizeof", "switch", "case", "break", "continue", "goto", "main", NULL};
    for (int i = 0; kw[i]; i++)
        if (!strcmp(kw[i], w))
            return 1;
    return 0;
}

/* ground (optional): a plain word also counts as code when the workspace
   defines it as a file-level function (length >= 3, not a C keyword or main).
   Declared rule: workspace-grounded identifier. */
static int lex_code_tokens_g(const char *task, TASK_TOKEN *out, int max, const TASK_OPS_WORKSPACE *ground);
static int file_has_define(const TASK_OPS_WORKSPACE *ws, const char *name);
static int lex_code_tokens(const char *task, TASK_TOKEN *out, int max)
{
    return lex_code_tokens_g(task, out, max, NULL);
}

static int lex_code_tokens_g(const char *task, TASK_TOKEN *out, int max, const TASK_OPS_WORKSPACE *ground)
{
    int n = 0;
    size_t i = 0, len = strlen(task);
    while (i < len && n < max) {
        unsigned char c = (unsigned char)task[i];
        if (!ident_start(c) || (i > 0 && ident_char((unsigned char)task[i - 1]))) {
            i++;
            continue;
        }
        size_t s = i;
        while (i < len && ident_char((unsigned char)task[i]))
            i++;
        size_t e = i;
        char before = s > 0 ? task[s - 1] : ' ';
        char after = task[e];
        if (before == '-' || before == '=' || after == '=' || e - s >= sizeof(out[0].text))
            continue;
        int quoted = (before == '\'' || before == '"' || before == '`') && after == before;
        char word[128];
        memcpy(word, task + s, e - s);
        word[e - s] = '\0';
        if (!code_shaped(word, quoted) &&
            !(ground && e - s >= 3 && !c_keyword(word) && file_has_define(ground, word)))
            continue;
        snprintf(out[n].text, sizeof(out[n].text), "%s", word);
        out[n].start = s;
        out[n].end = e;
        n++;
    }
    return n;
}

int TaskOpsFindRename(const TASK_OPS_WORKSPACE *ws, const char *task,
                      char *a, size_t a_size, char *b, size_t b_size)
{
    TASK_TOKEN tok[64];
    int n = lex_code_tokens_g(task ? task : "", tok, 64, ws);
    int found = 0;
    char fa[128] = {0}, fb[128] = {0};
    for (int i = 0; i + 1 < n; i++) {
        const TASK_TOKEN *x = &tok[i], *y = &tok[i + 1];
        if (!strcmp(x->text, y->text))
            continue;
        /* adjacency: only a short connector between the two names */
        if (y->start - x->end > 16)
            continue;
        if (TaskOpsCountToken(ws, x->text) == 0 || TaskOpsCountToken(ws, y->text) != 0)
            continue;
        if (found && !strcmp(fa, x->text) && !strcmp(fb, y->text))
            continue;
        found++;
        snprintf(fa, sizeof(fa), "%s", x->text);
        snprintf(fb, sizeof(fb), "%s", y->text);
    }
    if (found == 1) {
        if (a) snprintf(a, a_size, "%s", fa);
        if (b) snprintf(b, b_size, "%s", fb);
    }
    return found;
}

static char *replace_token(const char *s, const char *from, const char *to)
{
    int hits = count_token_in(s, from);
    size_t fl = strlen(from), tl = strlen(to), sl = strlen(s);
    char *out = (char *)malloc(sl + (size_t)hits * (tl > fl ? tl - fl : 0) + 1);
    if (!out)
        return NULL;
    char *w = out;
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, from);
        while (q && !((q == s || !ident_char((unsigned char)q[-1])) &&
                      !ident_char((unsigned char)q[fl])))
            q = strstr(q + 1, from);
        if (!q) {
            size_t rest = strlen(p);
            memcpy(w, p, rest);
            w += rest;
            break;
        }
        memcpy(w, p, (size_t)(q - p));
        w += q - p;
        memcpy(w, to, tl);
        w += tl;
        p = q + fl;
    }
    *w = '\0';
    return out;
}

/* ------------------------------------------------------------------- probe */

/* Compiler flags the task states, kept only if every byte is a safe flag
   character (the task text never reaches the shell unfiltered). */
static void task_flags(const char *task, char *out, size_t size)
{
    out[0] = '\0';
    size_t len = strlen(task), used = 0;
    for (size_t i = 0; i < len; i++) {
        if (task[i] != '-' || (i > 0 && !isspace((unsigned char)task[i - 1])))
            continue;
        size_t e = i + 1;
        while (e < len && (isalnum((unsigned char)task[e]) ||
                           strchr("=_+.-", task[e])))
            e++;
        while (e > i + 1 && strchr(".-", task[e - 1]))
            e--;
        size_t fl = e - i;
        int known = (fl > 2 && (task[i + 1] == 'W' || task[i + 1] == 'D' ||
                               task[i + 1] == 'O' || !strncmp(task + i, "-std=", 5)));
        if (known && used + fl + 2 < size) {
            memcpy(out + used, task + i, fl);
            used += fl;
            out[used++] = ' ';
            out[used] = '\0';
        }
        i = e;
    }
}

static void temp_binary(char *out, size_t size)
{
    static unsigned counter;
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = "/tmp";
    counter++;
#ifdef _WIN32
    snprintf(out, size, "%s\\task_ops_%lu_%u.exe", t, (unsigned long)GetCurrentProcessId(), counter);
#else
    snprintf(out, size, "%s/task_ops_%lu_%u", t, (unsigned long)time(NULL), counter);
#endif
}

/* Build all *.c with the task's flags into a temp binary outside the
   workspace, then run it. compile: -1 no C sources or no compiler. */
static size_t c_sources(const TASK_OPS_WORKSPACE *ws, char *srcs, size_t size)
{
    size_t used = 0;
    srcs[0] = '\0';
    for (int i = 0; i < ws->count; i++) {
        size_t n = strlen(ws->files[i].rel);
        if (n < 3 || strcmp(ws->files[i].rel + n - 2, ".c") != 0)
            continue;
        if (strchr(ws->files[i].rel, '"') || used + n + 4 >= size)
            continue;
        used += (size_t)snprintf(srcs + used, size - used, "\"%s\" ", ws->files[i].rel);
    }
    return used;
}

static char probe_stdout[2][1024];   /* [0] before, [1] after: behavior evidence */

static void probe_out(const TASK_OPS_WORKSPACE *ws, const char *flags, int *compile, int *run, char *out)
{
    if (out) out[0] = '\0';
    char cmd[4096], srcs[3072], bin[TASK_OPS_MAX_PATH];
    *compile = -1;
    *run = -1;
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return;
    temp_binary(bin, sizeof(bin));
    snprintf(cmd, sizeof(cmd), "gcc %s-o \"%s\" %s", flags, bin, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    if (r->execution_failed || r->exit_code == 127 || r->exit_code == 9009) {
        free(r);
        return;
    }
    *compile = r->exit_code == 0;
    if (*compile) {
        char run_cmd[TASK_OPS_MAX_PATH + 8];
        snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin);
        AgentShellResultInit(r);
        AgentShellExec(run_cmd, ws->root, 15000, r);   /* 5 s flaked under load (cca91ab note) */
        *run = r->timed_out ? 124 : r->exit_code;
        if (out)
            snprintf(out, sizeof(probe_stdout[0]), "%.*s", (int)(r->stdout_len < 1023 ? r->stdout_len : 1023),
                     r->stdout_buf);
    }
    remove(bin);
    free(r);
}


/* ------------------------------------------------- stated code fragment */

/* The task quotes a code fragment Y that is absent from the workspace; find
   the unique place X that Y is a small token edit of, and replace X with Y.
   Names are anchors: X must contain every name of Y that the workspace
   already knows, and may hold no name Y lacks, so only operators, literals
   and punctuation change. */

#define FRAG_MAX_TOK 24

typedef struct
{
    size_t start, end;
    char   text[64];
    int    name;          /* identifier/keyword */
} CTOK;

static int ctok_lex(const char *s, size_t len, CTOK *out, int max)
{
    static const char *ops[] = {"<<=", ">>=", "<=", ">=", "==", "!=", "&&", "||", "->", "++",
                                "--", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", NULL};
    int n = 0;
    size_t i = 0;
    while (i < len && n < max) {
        unsigned char c = (unsigned char)s[i];
        if (isspace(c)) { i++; continue; }
        size_t st = i;
        int name = 0;
        if (ident_char(c)) {
            name = !isdigit(c);
            while (i < len && ident_char((unsigned char)s[i])) i++;
        } else if (c == '"' || c == '\'') {
            i++;
            while (i < len && s[i] != (char)c && s[i] != '\n') i += (s[i] == '\\' && i + 1 < len) ? 2 : 1;
            if (i < len && s[i] == (char)c) i++;
        } else {
            size_t k = 0;
            for (; ops[k]; k++)
                if (strlen(ops[k]) <= len - i && !strncmp(s + i, ops[k], strlen(ops[k])))
                    break;
            i += ops[k] ? strlen(ops[k]) : 1;
        }
        if (i - st >= sizeof(out[0].text))
            return -1;
        out[n].start = st;
        out[n].end = i;
        memcpy(out[n].text, s + st, i - st);
        out[n].text[i - st] = '\0';
        out[n].name = name;
        n++;
    }
    return n;
}

static int tok_distance(const CTOK *x, int nx, const CTOK *y, int ny)
{
    int d[FRAG_MAX_TOK + 1][FRAG_MAX_TOK + 1];
    for (int i = 0; i <= nx; i++) d[i][0] = i;
    for (int j = 0; j <= ny; j++) d[0][j] = j;
    for (int i = 1; i <= nx; i++)
        for (int j = 1; j <= ny; j++) {
            int sub = d[i - 1][j - 1] + (strcmp(x[i - 1].text, y[j - 1].text) != 0);
            int del = d[i - 1][j] + 1, ins = d[i][j - 1] + 1;
            d[i][j] = sub < del ? (sub < ins ? sub : ins) : (del < ins ? del : ins);
        }
    return d[nx][ny];
}

static int has_name(const CTOK *t, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (t[i].name && !strcmp(t[i].text, name))
            return 1;
    return 0;
}

/* Normalized containment: Y's tokens appear consecutively somewhere. */
static int ws_contains_tokens(const TASK_OPS_WORKSPACE *ws, const CTOK *y, int ny)
{
    for (int f = 0; f < ws->count; f++) {
        const char *line = ws->files[f].data;
        while (line && *line) {
            const char *nl = strchr(line, '\n');
            size_t len = nl ? (size_t)(nl - line) : strlen(line);
            CTOK t[256];
            int n = ctok_lex(line, len, t, 256);
            for (int s = 0; n > 0 && s + ny <= n; s++) {
                int k = 0;
                while (k < ny && !strcmp(t[s + k].text, y[k].text)) k++;
                if (k == ny) return 1;
            }
            line = nl ? nl + 1 : NULL;
        }
    }
    return 0;
}

/* A clause that states the fragment must NOT hold. Declared lexical cue. */
static int negated_clause(const char *task, size_t quote_start)
{
    static const char *cues[] = {"forbid", "Forbid", "must not", "never", "no longer", "prohib", NULL};
    size_t s = quote_start;
    while (s > 0 && task[s - 1] != '.' && task[s - 1] != ';' && task[s - 1] != '\n')
        s--;
    for (int k = 0; cues[k]; k++) {
        const char *p = strstr(task + s, cues[k]);
        if (p && (size_t)(p - task) < quote_start)
            return 1;
    }
    return 0;
}

typedef struct
{
    int    file;
    size_t from, to;
    int    dist, span;
} FRAG_HIT;

/* Unquoted target after a change cue (declared lexical cue: "change to" /
   "change it to", any case of the first letter) is quoted with backticks up
   to the end of its sentence, so the fragment search below sees it. */
static void quote_change_targets(const char *task, char *out, size_t size)
{
    static const char *cues[] = {"hange to ", "hange it to ", NULL};
    size_t o = 0;
    const char *p = task;
    while (*p && o + 3 < size) {
        int hit = 0;
        for (int k = 0; cues[k] && !hit; k++) {
            size_t cl = strlen(cues[k]);
            if ((p[0] == 'c' || p[0] == 'C') && !strncmp(p + 1, cues[k], cl) &&
                (p == task || !ident_char((unsigned char)p[-1]))) {
                const char *s = p + 1 + cl, *e = s;
                while (*e && *e != '\n' && !(*e == '.' && (e[1] == ' ' || e[1] == '\n' || !e[1])) && *e != ';')
                    e++;
                size_t l = (size_t)(e - p);
                if (strpbrk(s, "'\"`") && (size_t)(strpbrk(s, "'\"`") - s) < (size_t)(e - s))
                    break;   /* already quoted */
                if (o + l + 3 >= size)
                    break;
                memcpy(out + o, p, (size_t)(s - p)); o += (size_t)(s - p);
                out[o++] = '`';
                memcpy(out + o, s, (size_t)(e - s)); o += (size_t)(e - s);
                out[o++] = '`';
                p = e;
                hit = 1;
            }
        }
        if (!hit)
            out[o++] = *p++;
    }
    out[o] = '\0';
}

/* Returns the number of distinct best locations (1 = usable); fills y_text,
   hit. */
static int find_fragment_edit(const TASK_OPS_WORKSPACE *ws, const char *task_in, char *y_text,
                              size_t y_size, FRAG_HIT *out)
{
    static char quoted[8192];
    quote_change_targets(task_in, quoted, sizeof(quoted));
    const char *task = quoted;
    size_t len = strlen(task);
    int usable = 0;
    FRAG_HIT best = {-1, 0, 0, 1 << 20, 1 << 20};
    int ties = 0;
    char best_y[128] = {0};
    for (size_t i = 0; i < len; i++) {
        char q = task[i];
        if (q != '\'' && q != '"' && q != '`')
            continue;
        if (i > 0 && ident_char((unsigned char)task[i - 1]))
            continue;                               /* apostrophe inside a word */
        const char *e = strchr(task + i + 1, q);
        if (!e || e - (task + i + 1) < 3 || e - (task + i + 1) > 80)
            continue;
        size_t fl = (size_t)(e - (task + i + 1));
        CTOK y[FRAG_MAX_TOK];
        int ny = ctok_lex(task + i + 1, fl, y, FRAG_MAX_TOK);
        int punct = 0;
        for (int k = 0; k < ny; k++) punct += !y[k].name;
        if (ny < 3 || ny >= FRAG_MAX_TOK - 2 || punct == 0 || negated_clause(task, i) ||
            ws_contains_tokens(ws, y, ny)) {
            i = (size_t)(e - task);
            continue;
        }
        usable++;
        for (int f = 0; f < ws->count; f++) {
            const char *data = ws->files[f].data, *line = data;
            while (line && *line) {
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl - line) : strlen(line);
                CTOK t[256];
                int n = ctok_lex(line, ll, t, 256);
                for (int s = 0; n > 0 && s < n; s++)
                    for (int w = (ny > 2 ? ny - 2 : 1); w <= ny + 2 && s + w <= n; w++) {
                        int ok = 1;
                        for (int k = 0; k < ny && ok; k++)
                            if (y[k].name && TaskOpsCountToken(ws, y[k].text) > 0 &&
                                !has_name(t + s, w, y[k].text))
                                ok = 0;
                        int anchors = 0;
                        for (int k = 0; k < w && ok; k++)
                            if (t[s + k].name) {
                                if (!has_name(y, ny, t[s + k].text))
                                    ok = 0;
                                anchors++;
                            }
                        if (!anchors)
                            ok = 0;   /* an edit must be anchored on a shared name */
                        if (!ok)
                            continue;
                        int d = tok_distance(t + s, w, y, ny);
                        if (d > 2 || d >= ny)
                            continue;
                        size_t from = (size_t)(line - data) + t[s].start;
                        size_t to = (size_t)(line - data) + t[s + w - 1].end;
                        FRAG_HIT h = {f, from, to, d, w};
                        int overlap = best.file == f && from < best.to && best.from < to;
                        if (d < best.dist || (d == best.dist && overlap && w < best.span)) {
                            if (!(d == best.dist && overlap))
                                ties = 0;
                            best = h;
                            memcpy(best_y, task + i + 1, fl);
                            best_y[fl] = '\0';
                        } else if (d == best.dist && !overlap) {
                            ties++;
                        }
                    }
                line = nl ? nl + 1 : NULL;
            }
        }
        i = (size_t)(e - task);
    }
    if (!usable || best.file < 0)
        return 0;
    snprintf(y_text, y_size, "%s", best_y);
    *out = best;
    return 1 + ties;
}

/* ------------------------------------------------------ compiler fix-its */

typedef struct
{
    int    file;          /* index into ws->files */
    size_t from, to;      /* byte range replaced (to exclusive) */
    char   text[256];
} FIXIT;

static long line_col_offset(const char *data, size_t len, long line, long col)
{
    long l = 1;
    size_t i = 0;
    while (i < len && l < line) {
        if (data[i] == '\n')
            l++;
        i++;
    }
    if (l != line || col < 1)
        return -1;
    size_t off = i + (size_t)(col - 1);
    return off <= len ? (long)off : -1;
}

/* Parse gcc -fdiagnostics-parseable-fixits lines:
   fix-it:"file":{l1:c1-l2:c2}:"text"   (text uses C escapes) */
static int parse_fixits(const TASK_OPS_WORKSPACE *ws, const char *out, FIXIT *fx, int max)
{
    int n = 0;
    for (const char *p = strstr(out, "fix-it:\""); p && n < max; p = strstr(p + 1, "fix-it:\"")) {
        const char *q = p + 8, *e = strchr(q, '"');
        if (!e || e - q >= TASK_OPS_MAX_PATH)
            continue;
        char rel[TASK_OPS_MAX_PATH];
        memcpy(rel, q, (size_t)(e - q));
        rel[e - q] = '\0';
        long l1, c1, l2, c2;
        if (sscanf(e + 1, ":{%ld:%ld-%ld:%ld}:", &l1, &c1, &l2, &c2) != 4)
            continue;
        const char *t = strstr(e + 1, "}:\"");
        if (!t)
            continue;
        t += 3;
        int fi = -1;
        for (int i = 0; i < ws->count; i++)
            if (!strcmp(ws->files[i].rel, rel))
                fi = i;
        if (fi < 0)
            continue;
        FIXIT *f = &fx[n];
        size_t w = 0;
        int ok = 0;
        while (*t && *t != '\n' && w + 1 < sizeof(f->text)) {
            if (*t == '"') { ok = 1; break; }
            if (*t == '\\' && t[1]) {
                t++;
                char c = *t == 'n' ? '\n' : *t == 't' ? '\t' : *t;
                if (*t >= '0' && *t <= '7') {
                    int v = 0, k = 0;
                    while (k < 3 && *t >= '0' && *t <= '7') { v = v * 8 + (*t - '0'); t++; k++; }
                    f->text[w++] = (char)v;
                    continue;
                }
                f->text[w++] = c;
                t++;
                continue;
            }
            f->text[w++] = *t++;
        }
        f->text[w] = '\0';
        long a = line_col_offset(ws->files[fi].data, ws->files[fi].len, l1, c1);
        long b = line_col_offset(ws->files[fi].data, ws->files[fi].len, l2, c2);
        if (!ok || a < 0 || b < a)
            continue;
        f->file = fi;
        f->from = (size_t)a;
        f->to = (size_t)b;
        n++;
    }
    return n;
}

/* Rewrites of every file named by the compiler's own fix-its; 0 when the
   build has none, or when two fix-its in one file overlap (fail closed). */
static int compiler_fixits(const TASK_OPS_WORKSPACE *ws, const char *flags, char **next,
                           int *count)
{
    char cmd[4096], srcs[3072];
    *count = 0;
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return 0;
    snprintf(cmd, sizeof(cmd), "gcc %s-fsyntax-only -fdiagnostics-parseable-fixits %s", flags, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    FIXIT *fx = (FIXIT *)malloc(sizeof(FIXIT) * 32);
    if (!r || !fx) { free(r); free(fx); return 0; }
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    int n = (r->exit_code != 0 && !r->execution_failed) ? parse_fixits(ws, r->stderr_buf, fx, 32) : 0;
    free(r);
    int files = 0;
    for (int fi = 0; fi < ws->count && n > 0; fi++) {
        /* this file's fix-its, applied back to front */
        size_t total = ws->files[fi].len + 1, mine = 0;
        for (int k = 0; k < n; k++)
            if (fx[k].file == fi) { total += strlen(fx[k].text); mine++; }
        if (!mine)
            continue;
        for (int k = 0; k < n; k++)
            for (int j = 0; j < n; j++)
                if (k != j && fx[k].file == fi && fx[j].file == fi &&
                    fx[j].from < fx[k].to && fx[k].from < fx[j].to) {
                    free(fx);
                    return 0;
                }
        char *out = (char *)malloc(total);
        if (!out) break;
        const char *src = ws->files[fi].data;
        size_t pos = 0, w = 0;
        for (;;) {
            int best = -1;
            for (int k = 0; k < n; k++)
                if (fx[k].file == fi && fx[k].from >= pos &&
                    (best < 0 || fx[k].from < fx[best].from))
                    best = k;
            if (best < 0) break;
            memcpy(out + w, src + pos, fx[best].from - pos);
            w += fx[best].from - pos;
            size_t tl = strlen(fx[best].text);
            memcpy(out + w, fx[best].text, tl);
            w += tl;
            pos = fx[best].to;
            fx[best].file = -1;   /* consumed */
            if (fx[best].to == fx[best].from) pos = fx[best].from;
        }
        memcpy(out + w, src + pos, ws->files[fi].len - pos);
        w += ws->files[fi].len - pos;
        out[w] = '\0';
        next[fi] = out;
        files++;
        *count += (int)mine;
    }
    free(fx);
    return files;
}

/* ------------------------------------------- declare an implicit function */

/* gcc reports a call to an undeclared function; ground the name in the
   workspace: a header that declares it (include that header), or its own
   definition (declare it from the definition's head). */

#define DECL_MAX 8

typedef struct
{
    int  file;            /* file that uses the name */
    char name[128];
    char sys[64];         /* "<hdr.h>" when gcc's note names the standard header */
} IMPLICIT_USE;

/* gcc: "note: include '<string.h>' or provide a declaration of 'strlen'".
   Observed compiler state, not task wording. Declared rule (note shape). */
static void note_header(const char *err, const char *name, char *out, size_t size)
{
    out[0] = '\0';
    const char *key = "provide a declaration of ";
    for (const char *p = strstr(err, key); p; p = strstr(p + 1, key)) {
        const char *q = p + strlen(key);
        while (*q && !ident_start((unsigned char)*q) && *q != '\n') q++;
        size_t nl = strlen(name);
        if (strncmp(q, name, nl) != 0 || ident_char((unsigned char)q[nl]))
            continue;
        const char *ls = p;
        while (ls > err && ls[-1] != '\n') ls--;
        const char *lt = strchr(ls, '<'), *gt = lt ? strchr(lt, '>') : NULL;
        if (!lt || !gt || gt > p || (size_t)(gt - lt + 1) >= size)
            continue;
        for (const char *c = lt + 1; c < gt; c++)
            if (!(isalnum((unsigned char)*c) || *c == '.' || *c == '_' || *c == '/'))
                return;
        memcpy(out, lt, (size_t)(gt - lt + 1));
        out[gt - lt + 1] = '\0';
        return;
    }
}

static int implicit_uses(const TASK_OPS_WORKSPACE *ws, const char *flags, IMPLICIT_USE *out, int max)
{
    char cmd[4096], srcs[3072];
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return 0;
    snprintf(cmd, sizeof(cmd), "gcc %s-Wimplicit-function-declaration -fsyntax-only %s", flags, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    int n = 0;
    if (!r->execution_failed) {
        const char *key = "implicit declaration of function ";
        for (const char *p = strstr(r->stderr_buf, key); p && n < max; p = strstr(p + 1, key)) {
            /* file is the text before the first ':' of this diagnostic line */
            const char *ls = p;
            while (ls > r->stderr_buf && ls[-1] != '\n') ls--;
            const char *colon = strchr(ls, ':');
            const char *q = p + strlen(key);
            while (*q && !ident_start((unsigned char)*q) && *q != '\n') q++;   /* ' or UTF-8 quote */
            size_t nl = 0;
            while (ident_char((unsigned char)q[nl])) nl++;
            if (!colon || colon > p || nl == 0 || nl >= sizeof(out[0].name))
                continue;
            int fi = -1;
            for (int i = 0; i < ws->count; i++)
                if (strlen(ws->files[i].rel) == (size_t)(colon - ls) &&
                    !strncmp(ws->files[i].rel, ls, (size_t)(colon - ls)))
                    fi = i;
            if (fi < 0)
                continue;
            int dup = 0;
            for (int k = 0; k < n; k++)
                if (out[k].file == fi && strlen(out[k].name) == nl && !strncmp(out[k].name, q, nl))
                    dup = 1;
            if (dup)
                continue;
            out[n].file = fi;
            memcpy(out[n].name, q, nl);
            out[n].name[nl] = '\0';
            note_header(r->stderr_buf, out[n].name, out[n].sys, sizeof(out[n].sys));
            n++;
        }
    }
    free(r);
    return n;
}

static int is_header(const char *rel)
{
    size_t n = strlen(rel);
    return n > 2 && !strcmp(rel + n - 2, ".h");
}

/* Line in data (start offset) holding "name(" at file level followed by
   ';' (prototype) or '{' (definition) on the same line. */
static long file_level_decl(const char *data, const char *name, int want_def, size_t *line_len)
{
    const char *line = data;
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        size_t len = nl ? (size_t)(nl - line) : strlen(line);
        if (!isspace((unsigned char)line[0]) && line[0] != '#') {
            char buf[512];
            size_t bl = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, line, bl);
            buf[bl] = '\0';
            char *p = buf;
            size_t nlen = strlen(name);
            while ((p = strstr(p, name)) != NULL) {
                int left = p == buf || !ident_char((unsigned char)p[-1]);
                const char *a = p + nlen;
                while (*a == ' ') a++;
                if (left && *a == '(' && p != buf) {
                    const char *close = strchr(a, ')');
                    const char *after = close ? close + 1 : NULL;
                    while (after && *after == ' ') after++;
                    if (after && ((want_def && *after == '{') || (!want_def && *after == ';'))) {
                        *line_len = (size_t)(close + 1 - buf);
                        return (long)(line - data);
                    }
                }
                p += nlen;
            }
        }
        line = nl ? nl + 1 : NULL;
    }
    return -1;
}

/* Offset just after the last #include line, or 0. */
static size_t after_includes(const char *data)
{
    size_t off = 0;
    const char *line = data;
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        const char *t = line;
        while (*t == ' ' || *t == '\t') t++;
        if (!strncmp(t, "#include", 8))
            off = nl ? (size_t)(nl + 1 - data) : strlen(data);
        line = nl ? nl + 1 : NULL;
    }
    return off;
}

static char *insert_at(const char *data, size_t len, size_t at, const char *text)
{
    size_t tl = strlen(text);
    char *out = (char *)malloc(len + tl + 1);
    if (!out)
        return NULL;
    memcpy(out, data, at);
    memcpy(out + at, text, tl);
    memcpy(out + at + tl, data + at, len - at + 1);
    return out;
}

typedef struct {
    char  rel[TASK_OPS_MAX_PATH];
    char *data;
} NEW_FILE;

static int missing_named_header(const TASK_OPS_WORKSPACE *ws, const char *task, char *out, size_t out_size);

/* Append a prototype to the header the task asks for, creating it (with an
   include guard derived from its name) on first use. */
static int new_header_add(NEW_FILE *nf, const char *proto)
{
    if (!nf->data) {
        char guard[TASK_OPS_MAX_PATH];
        size_t g = 0;
        for (const char *c = nf->rel; *c && g + 1 < sizeof(guard); c++)
            guard[g++] = isalnum((unsigned char)*c) ? (char)toupper((unsigned char)*c) : '_';
        guard[g] = '\0';
        size_t cap = 3 * strlen(guard) + 64;
        nf->data = (char *)malloc(cap);
        if (!nf->data)
            return 0;
        snprintf(nf->data, cap, "#ifndef %s\n#define %s\n\n#endif\n", guard, guard);
    }
    if (strstr(nf->data, proto))
        return 1;
    const char *endif = strstr(nf->data, "#endif");
    size_t at = (size_t)(endif - nf->data);
    char *grown = insert_at(nf->data, strlen(nf->data), at > 0 && nf->data[at - 1] == '\n' && nf->data[at - 2] == '\n' ? at - 1 : at, proto);
    if (!grown)
        return 0;
    free(nf->data);
    nf->data = grown;
    return 1;
}

/* Fills next[] for every implicit use it can ground; returns files touched.
   When no header fits and the task names exactly one header file that does
   not exist, the prototype goes into that new header (nf). */
static int file_has_define(const TASK_OPS_WORKSPACE *ws, const char *name)
{
    for (int i = 0; i < ws->count; i++) {
        size_t ll;
        if (file_level_decl(ws->files[i].data, name, 1, &ll) >= 0)
            return 1;
    }
    return 0;
}

static int declare_implicit(const TASK_OPS_WORKSPACE *ws, const char *flags, const char *task, char **next,
                            NEW_FILE *nf, char *detail, size_t detail_size, int *uses_before)
{
    char want_hdr[TASK_OPS_MAX_PATH] = {0};
    int can_create = nf && missing_named_header(ws, task, want_hdr, sizeof(want_hdr));
    if (can_create && !nf->rel[0])
        snprintf(nf->rel, sizeof(nf->rel), "%s", want_hdr);
    IMPLICIT_USE use[DECL_MAX];
    int n = implicit_uses(ws, flags, use, DECL_MAX), files = 0;
    *uses_before = n;
    detail[0] = '\0';
    for (int k = 0; k < n; k++) {
        const TASK_OPS_FILE *f = &ws->files[use[k].file];
        if (next[use[k].file])
            continue;   /* one grounded edit per file per round */
        char text[640] = {0};
        /* a header (other than the user) that declares the name */
        int header = -1, headers = 0;
        for (int i = 0; i < ws->count; i++) {
            size_t ll;
            if (i != use[k].file && is_header(ws->files[i].rel) &&
                file_level_decl(ws->files[i].data, use[k].name, 0, &ll) >= 0) {
                header = i;
                headers++;
            }
        }
        if (headers == 1 && !strchr(ws->files[header].rel, '/') && !strchr(f->rel, '/')) {
            snprintf(text, sizeof(text), "#include \"%s\"\n", ws->files[header].rel);
        } else if (headers == 0) {
            /* the definition's head, from exactly one source file */
            int defs = 0, def_file = -1;
            for (int i = 0; i < ws->count; i++) {
                size_t ll;
                long at = file_level_decl(ws->files[i].data, use[k].name, 1, &ll);
                if (at >= 0 && ll < sizeof(text) - 3) {
                    defs++;
                    def_file = i;
                    memcpy(text, ws->files[i].data + at, ll);
                    memcpy(text + ll, ";\n", 3);
                }
            }
            if (defs != 1)
                text[0] = '\0';
            /* the declaration belongs in the one local header the defining
               file includes; the user then includes that header */
            int hdr = -1;
            /* first choice: the one local header the user already includes */
            for (int h = 0; text[0] && h < ws->count; h++) {
                char inc[TASK_OPS_MAX_PATH + 16];
                snprintf(inc, sizeof(inc), "#include \"%s\"", ws->files[h].rel);
                if (is_header(ws->files[h].rel) && strstr(f->data, inc))
                    hdr = hdr == -1 ? h : -2;
            }
            if (hdr == -2)
                hdr = -3;   /* several: keep the local prototype */
            for (int i = 0; text[0] && hdr == -1 && i < ws->count; i++) {
                size_t ll;
                if (file_level_decl(ws->files[i].data, use[k].name, 1, &ll) < 0)
                    continue;
                for (int h = 0; h < ws->count; h++) {
                    char inc[TASK_OPS_MAX_PATH + 16];
                    snprintf(inc, sizeof(inc), "#include \"%s\"", ws->files[h].rel);
                    if (is_header(ws->files[h].rel) && strstr(ws->files[i].data, inc))
                        hdr = hdr == -1 ? h : -2;
                }
            }
            if (hdr == -1 && text[0] && can_create && def_file >= 0 && !strchr(f->rel, '/') &&
                !strchr(ws->files[def_file].rel, '/') && new_header_add(nf, text)) {
                char inc[TASK_OPS_MAX_PATH + 16];
                snprintf(inc, sizeof(inc), "#include \"%s\"\n", nf->rel);
                size_t dl0 = strlen(detail);
                snprintf(detail + dl0, detail_size - dl0, "%snew %s: %.*s", dl0 ? "; " : "", nf->rel,
                         (int)(strlen(text) - 1), text);
                const TASK_OPS_FILE *d = &ws->files[def_file];
                if (def_file != use[k].file && !next[def_file] && !strstr(d->data, inc)) {
                    next[def_file] = insert_at(d->data, d->len, after_includes(d->data), inc);
                    files++;
                }
                files++;
                snprintf(text, sizeof(text), "%s", inc);
            } else if (hdr >= 0 && hdr != use[k].file && !next[hdr] && !strchr(ws->files[hdr].rel, '/')) {
                const TASK_OPS_FILE *h = &ws->files[hdr];
                size_t at = h->len;
                const char *endif = NULL;
                for (const char *q = strstr(h->data, "#endif"); q; q = strstr(q + 1, "#endif"))
                    endif = q;
                if (endif && strstr(h->data, "#ifndef"))
                    at = (size_t)(endif - h->data);
                char proto[700];
                snprintf(proto, sizeof(proto), "%s%s", (at == h->len && h->len && h->data[h->len - 1] != '\n') ? "\n" : "", text);
                next[hdr] = insert_at(h->data, h->len, at, proto);
                char inc[TASK_OPS_MAX_PATH + 16];
                snprintf(inc, sizeof(inc), "#include \"%s\"", h->rel);
                size_t dl0 = strlen(detail);
                snprintf(detail + dl0, detail_size - dl0, "%s%s: %.*s", dl0 ? "; " : "", h->rel,
                         (int)(strlen(text) - 1), text);
                if (strstr(f->data, inc)) {
                    text[0] = '\0';   /* header already included: prototype only */
                    files++;
                    continue;
                }
                snprintf(text, sizeof(text), "%s\n", inc);
                files++;
            } else if (text[0] && def_file >= 0 && def_file != use[k].file) {
                /* defined in another source file and no header takes the
                   prototype (none included, several, or not editable): a
                   local prototype would hide the missing header. Abstain. */
                text[0] = '\0';
            }
        }
        /* no workspace declaration or definition: the standard header the
           compiler's own note names */
        if (!text[0] && headers == 0 && use[k].sys[0] && !file_has_define(ws, use[k].name)) {
            char inc[96];
            snprintf(inc, sizeof(inc), "#include %s", use[k].sys);
            if (!strstr(f->data, inc))
                snprintf(text, sizeof(text), "%s\n", inc);
        }
        if (!text[0])
            continue;
        next[use[k].file] = insert_at(f->data, f->len, after_includes(f->data), text);
        if (next[use[k].file]) {
            files++;
            size_t dl = strlen(detail);
            snprintf(detail + dl, detail_size - dl, "%s%s: %.*s", dl ? "; " : "", f->rel,
                     (int)(strlen(text) - 1), text);
        }
    }
    return files;
}


/* Every file name the task mentions (name.ext, ext 1-4 alnum) must exist
   after the edit: an edit that leaves a named artifact missing cannot have
   done what the task asks. Declared lexical rule (file-name shape). */
/* Next file name the task mentions (name.ext, ext 1-4 alnum), from *pos. */
static int next_named_file(const char *task, size_t *pos, char *name, size_t name_size)
{
    size_t len = strlen(task);
    for (size_t i = *pos; i < len; i++) {
        if (!ident_start((unsigned char)task[i]) || (i > 0 && (ident_char((unsigned char)task[i - 1]) ||
                                                            task[i - 1] == '.' || task[i - 1] == '/' ||
                                                            task[i - 1] == '<')))   /* <hdr.h>: system header */
            continue;
        size_t e = i;
        while (e < len && (ident_char((unsigned char)task[e]) || task[e] == '/' || task[e] == '-')) e++;
        if (e >= len || task[e] != '.')
            { i = e; continue; }
        size_t x = e + 1;
        while (x < len && isalnum((unsigned char)task[x])) x++;
        /* a one-letter stem ("a.h") counts only with a known file extension,
           so "e.g." and "i.e." are not file names */
        int short_ok = 0;
        if (e - i == 1) {
            static const char *const ext[] = {"c", "h", "cc", "cpp", "hpp", "py", "sh", "md", "txt", "yml", "json", "mk", NULL};
            for (int k = 0; ext[k]; k++)
                if (strlen(ext[k]) == x - e - 1 && !strncmp(task + e + 1, ext[k], x - e - 1)) short_ok = 1;
        }
        if ((e - i < 2 && !short_ok) || x - e - 1 < 1 || x - e - 1 > 4 || (x < len && (task[x] == '.' && x + 1 < len &&
                                                          isalnum((unsigned char)task[x + 1]))))
            { i = x; continue; }
        if (x - i >= name_size) { i = x; continue; }
        memcpy(name, task + i, x - i);
        name[x - i] = '\0';
        *pos = x;
        return 1;
    }
    *pos = len;
    return 0;
}

static int ws_find_named(const TASK_OPS_WORKSPACE *ws, const char *name)
{
    for (int f = 0; f < ws->count; f++) {
        const char *r = ws->files[f].rel;
        size_t rl = strlen(r), nl = strlen(name);
        if (!strcmp(r, name) || (rl > nl && r[rl - nl - 1] == '/' && !strcmp(r + rl - nl, name)))
            return f;
    }
    return -1;
}

/* Every file name the task mentions must exist after the edit: an edit
   that leaves a named artifact missing cannot have done what the task asks.
   A named file counts as touched when it is new or its content changed.
   Declared lexical rule (file-name shape). */
static int named_files_exist(const TASK_OPS_WORKSPACE *before, const TASK_OPS_WORKSPACE *after,
                             const char *task, int *named, int *named_touched)
{
    *named = *named_touched = 0;
    size_t pos = 0;
    char name[TASK_OPS_MAX_PATH];
    while (next_named_file(task, &pos, name, sizeof(name))) {
        int found = 0;
        /* a file the task names that did not exist before must now exist at
           exactly that path: a same-named file elsewhere is not the new file */
        int was_there = ws_find_named(before, name) >= 0;
        for (int f = 0; f < after->count; f++) {
            const char *r = after->files[f].rel;
            size_t rl = strlen(r), nl = strlen(name);
            if (!(!strcmp(r, name) || (was_there && rl > nl && r[rl - nl - 1] == '/' && !strcmp(r + rl - nl, name))))
                continue;
            found = 1;
            (*named)++;
            int same = 0;
            for (int g = 0; g < before->count; g++)
                if (!strcmp(before->files[g].rel, r))
                    same = before->files[g].len == after->files[f].len &&
                           !memcmp(before->files[g].data, after->files[f].data, after->files[f].len);
            if (!same)
                (*named_touched)++;
        }
        if (!found)
            return 0;
    }
    return 1;
}

/* The one header name (x.h, workspace root) the task mentions that does not
   exist yet; 0 when there is none or more than one. */
static int missing_named_header(const TASK_OPS_WORKSPACE *ws, const char *task, char *out, size_t out_size)
{
    size_t pos = 0;
    char name[TASK_OPS_MAX_PATH];
    int n = 0;
    while (next_named_file(task, &pos, name, sizeof(name)))
        if (is_header(name) && !strchr(name, '/') && ws_find_named(ws, name) < 0 &&
            (n == 0 || strcmp(out, name) != 0)) {
            snprintf(out, out_size, "%s", name);
            n++;
        }
    return n == 1;
}

/* -------------------------------------------- literal -> named constant */

/* In one sentence the task names a new constant N (code-shaped, absent from
   the workspace) and a literal L (a number, or a quoted string) that the C
   sources use. Define N once and replace every use of L; a string literal
   that only starts/ends with L keeps the rest by C string concatenation.
   Sentence boundary ". " is a declared lexical rule. */

static int is_c_source(const char *rel)
{
    size_t n = strlen(rel);
    return n > 2 && (!strcmp(rel + n - 2, ".c") || !strcmp(rel + n - 2, ".h"));
}

typedef struct
{
    char name[128];
    char lit[96];      /* number text, or string contents without quotes */
    int  is_string;
} LIT_PLAN;

/* occurrences of the literal as a C token (numbers) or inside string tokens */
static int literal_uses(const TASK_OPS_WORKSPACE *ws, const LIT_PLAN *p, int *file_out, int *files_out)
{
    int uses = 0, files = 0;
    *file_out = -1;
    for (int f = 0; f < ws->count; f++) {
        if (!is_c_source(ws->files[f].rel))
            continue;
        int here = 0;
        const char *line = ws->files[f].data;
        while (line && *line) {
            const char *nl = strchr(line, '\n');
            size_t ll = nl ? (size_t)(nl - line) : strlen(line);
            CTOK t[256];
            int n = (line[0] == '#') ? 0 : ctok_lex(line, ll, t, 256);
            for (int k = 0; k < n; k++) {
                if (!p->is_string && !strcmp(t[k].text, p->lit))
                    here++;
                if (p->is_string && t[k].text[0] == '"') {
                    size_t tl = strlen(t[k].text), lt = strlen(p->lit);
                    if (tl >= lt + 2 && (!strncmp(t[k].text + 1, p->lit, lt) ||
                                         !strncmp(t[k].text + tl - 1 - lt, p->lit, lt)))
                        here++;
                }
            }
            line = nl ? nl + 1 : NULL;
        }
        if (here) { files++; *file_out = f; uses += here; }
    }
    *files_out = files;
    return uses;
}

static int find_literal_plan(const TASK_OPS_WORKSPACE *ws, const char *task, LIT_PLAN *out)
{
    size_t len = strlen(task), s = 0;
    int plans = 0;
    while (s < len) {
        size_t e = s;
        while (e < len && !(task[e] == '.' && (e + 1 >= len || isspace((unsigned char)task[e + 1]))))
            e++;
        char sent[512];
        size_t sl = e - s < sizeof(sent) - 1 ? e - s : sizeof(sent) - 1;
        memcpy(sent, task + s, sl);
        sent[sl] = '\0';
        /* the new name: code-shaped, absent from the workspace, unique here */
        TASK_TOKEN tok[32];
        int nt = lex_code_tokens(sent, tok, 32), names = 0;
        char name[128] = {0};
        for (int k = 0; k < nt; k++)
            if (TaskOpsCountToken(ws, tok[k].text) == 0 && strcmp(name, tok[k].text)) {
                names++;
                snprintf(name, sizeof(name), "%s", tok[k].text);
            }
        if (names == 1) {
            /* literals of the sentence that the sources use */
            int lits = 0;
            LIT_PLAN cand = {{0}, {0}, 0};
            for (size_t i = 0; i < sl; i++) {
                LIT_PLAN p;
                memset(&p, 0, sizeof(p));
                snprintf(p.name, sizeof(p.name), "%s", name);
                size_t j = i;
                if (sent[i] == '"') {
                    const char *q = strchr(sent + i + 1, '"');
                    if (!q || q - (sent + i + 1) < 1 || q - (sent + i + 1) >= (long)sizeof(p.lit))
                        continue;
                    memcpy(p.lit, sent + i + 1, (size_t)(q - (sent + i + 1)));
                    p.is_string = 1;
                    j = (size_t)(q - sent);
                } else if (isdigit((unsigned char)sent[i]) && (i == 0 || !ident_char((unsigned char)sent[i - 1]))) {
                    while (j < sl && ident_char((unsigned char)sent[j])) j++;
                    if (j - i >= sizeof(p.lit)) continue;
                    memcpy(p.lit, sent + i, j - i);
                    j--;
                } else {
                    continue;
                }
                int f, files;
                if (literal_uses(ws, &p, &f, &files) > 0 && strcmp(cand.lit, p.lit)) {
                    lits++;
                    cand = p;
                }
                i = j;
            }
            if (lits == 1) {
                plans++;
                *out = cand;
            }
        }
        s = e + 1;
    }
    return plans;
}

/* Rewrite of the one C file using the literal; NULL when uses span files. */
static char *apply_literal_plan(const TASK_OPS_WORKSPACE *ws, const LIT_PLAN *p, int *file)
{
    int files;
    if (literal_uses(ws, p, file, &files) == 0 || files != 1)
        return NULL;
    const TASK_OPS_FILE *f = &ws->files[*file];
    size_t cap = f->len * 2 + 1024;
    char *out = (char *)malloc(cap);
    if (!out)
        return NULL;
    size_t w = 0, at = after_includes(f->data);
    memcpy(out, f->data, at);
    w = at;
    if (p->is_string)
        w += (size_t)snprintf(out + w, cap - w, "#define %s \"%s\"\n", p->name, p->lit);
    else
        w += (size_t)snprintf(out + w, cap - w, "#define %s %s\n", p->name, p->lit);
    const char *line = f->data + at;
    size_t lt = strlen(p->lit);
    while (line && *line) {
        const char *nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) + 1 : strlen(line);
        CTOK t[256];
        int n = (line[0] == '#') ? 0 : ctok_lex(line, ll, t, 256);
        size_t pos = 0;
        for (int k = 0; k < n && w + ll + 256 < cap; k++) {
            const char *tx = t[k].text;
            size_t tl = strlen(tx);
            int hit = 0;
            char repl[512];
            if (!p->is_string && !strcmp(tx, p->lit)) {
                snprintf(repl, sizeof(repl), "%s", p->name);
                hit = 1;
            } else if (p->is_string && tx[0] == '"' && tl >= lt + 2) {
                if (tl == lt + 2 && !strncmp(tx + 1, p->lit, lt)) {
                    snprintf(repl, sizeof(repl), "%s", p->name); hit = 1;
                } else if (!strncmp(tx + 1, p->lit, lt)) {
                    snprintf(repl, sizeof(repl), "%s \"%.*s", p->name, (int)(tl - 1 - lt), tx + 1 + lt); hit = 1;
                } else if (!strncmp(tx + tl - 1 - lt, p->lit, lt)) {
                    snprintf(repl, sizeof(repl), "%.*s\" %s", (int)(tl - 1 - lt), tx, p->name); hit = 1;
                }
            }
            if (!hit)
                continue;
            memcpy(out + w, line + pos, t[k].start - pos);
            w += t[k].start - pos;
            size_t rl = strlen(repl);
            memcpy(out + w, repl, rl);
            w += rl;
            pos = t[k].end;
        }
        memcpy(out + w, line + pos, ll - pos);
        w += ll - pos;
        line = nl ? nl + 1 : NULL;
    }
    out[w] = '\0';
    return out;
}

/* ------------------------------------------------------------------ act */

/* ------------------------------------------------------ operator memory */

/* Learn step. Opt-in (SYMBOLS_TASK_OPS_MEMORY=1) so bank and CI numbers stay
   reproducible. Each verified or rolled-back attempt is appended to
   <workspace>/.symbols/task_ops_memory.tsv as "key<TAB>op<TAB>kept|rolled_back",
   key = FNV-1a 64 of the task text and every workspace file (path + bytes).
   Recall: an operator that rolled back on the same key is skipped; when
   several operators could apply, the order follows remembered net success
   (kept - rolled_back) per operator, ties keep the default order. */

/* ---------------------------------------------------------- doc_sync
   Operator 6: the task names a code-shaped term A that a documentation file
   (.md/.txt/.rst or README*) uses but no C source (.c/.h) contains, and
   exactly one other term B of the same shape that a C source does contain.
   Replace A with B in the documentation files only. Shapes (declared
   lexical rule): flag "-x"/"--name", call "name(...)", identifier with '_'
   or a digit for A (any identifier of 3+ chars, not a C keyword, for B). */

enum { DS_NONE, DS_FLAG, DS_CALL, DS_IDENT };

static int is_doc_file(const char *rel)
{
    const char *base = strrchr(rel, '/');
    base = base ? base + 1 : rel;
    size_t n = strlen(base);
    if (!strncmp(base, "README", 6))
        return 1;
    return (n > 3 && !strcmp(base + n - 3, ".md")) || (n > 4 && !strcmp(base + n - 4, ".txt")) ||
           (n > 4 && !strcmp(base + n - 4, ".rst"));
}

static int ds_boundary(int c) { return !ident_char(c) && c != '-'; }

static int ds_count(const char *s, const char *term)
{
    size_t n = strlen(term);
    int count = 0;
    if (!n)
        return 0;
    for (const char *p = strstr(s, term); p; p = strstr(p + 1, term))
        if ((p == s || ds_boundary((unsigned char)p[-1])) &&
            (!ident_char((unsigned char)term[n - 1]) && term[n - 1] != '-' ? 1 : ds_boundary((unsigned char)p[n])))
            count++;
    return count;
}

static char *ds_replace(const char *s, const char *from, const char *to)
{
    size_t fl = strlen(from), tl = strlen(to), sl = strlen(s);
    int hits = ds_count(s, from);
    char *out = (char *)malloc(sl + (size_t)hits * (tl > fl ? tl - fl : 0) + 1), *w = out;
    if (!out)
        return NULL;
    const char *p = s;
    while (*p) {
        const char *q = strstr(p, from);
        while (q && !((q == s || ds_boundary((unsigned char)q[-1])) &&
                      (!ident_char((unsigned char)from[fl - 1]) && from[fl - 1] != '-' ? 1 : ds_boundary((unsigned char)q[fl]))))
            q = strstr(q + 1, from);
        if (!q) {
            size_t rest = strlen(p);
            memcpy(w, p, rest);
            w += rest;
            break;
        }
        memcpy(w, p, (size_t)(q - p));
        w += q - p;
        memcpy(w, to, tl);
        w += tl;
        p = q + fl;
    }
    *w = '\0';
    return out;
}

static int ds_shape(const char *t)
{
    size_t n = strlen(t);
    if (n >= 2 && t[0] == '-' && (isalnum((unsigned char)t[1]) || (t[1] == '-' && n >= 3 && isalnum((unsigned char)t[2])))) {
        for (size_t i = 1; i < n; i++)
            if (!ident_char((unsigned char)t[i]) && t[i] != '-')
                return DS_NONE;
        return DS_FLAG;
    }
    if (!ident_start((unsigned char)t[0]))
        return DS_NONE;
    size_t i = 0;
    while (ident_char((unsigned char)t[i]))
        i++;
    if (t[i] == '(' && n > i + 1 && t[n - 1] == ')' && !strchr(t + i + 1, '(') && strchr(t + i, ')') == t + n - 1)
        return DS_CALL;
    return t[i] == '\0' ? DS_IDENT : DS_NONE;
}

static int ds_is_keyword(const char *t)
{
    static const char *const kw[] = { "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if", "int", "long", "register", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
        "volatile", "while", NULL };
    for (int i = 0; kw[i]; i++)
        if (!strcmp(t, kw[i]))
            return 1;
    return 0;
}

#define DS_MAX_TERMS 96
/* Terms of the task text: whitespace-separated, with surrounding quotes,
   backticks and trailing punctuation removed; "name(int n)" is rejoined. */
static int ds_terms(const char *task, char terms[][128], int max)
{
    int n = 0;
    const char *p = task;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        const char *s = p;
        int depth = 0;
        while (*p && (depth > 0 || !isspace((unsigned char)*p))) {
            if (*p == '(') depth++;
            else if (*p == ')' && depth > 0) depth--;
            p++;
        }
        size_t l = (size_t)(p - s);
        while (l && strchr("'\"`", *s)) { s++; l--; }
        while (l && strchr(".,;:!?'\"`", s[l - 1])) l--;
        if (l == 0 || l >= 128)
            continue;
        memcpy(terms[n], s, l);
        terms[n][l] = '\0';
        int dup = 0;
        for (int k = 0; k < n; k++)
            if (!strcmp(terms[k], terms[n])) dup = 1;
        if (!dup)
            n++;
    }
    return n;
}

static int ds_in_file(const char *data, const char *term, int shape)
{
    return shape == DS_IDENT ? count_token_in(data, term) : ds_count(data, term);
}

static int ds_in(const TASK_OPS_WORKSPACE *ws, const char *term, int shape, int docs)
{
    int total = 0;
    for (int i = 0; i < ws->count; i++) {
        if (docs ? !is_doc_file(ws->files[i].rel) : !is_c_source(ws->files[i].rel))
            continue;
        total += shape == DS_IDENT ? count_token_in(ws->files[i].data, term) : ds_count(ws->files[i].data, term);
    }
    return total;
}

/* 1 = unique plan in a/b; >1 = ambiguous; 0 = none */
static int find_doc_sync(const TASK_OPS_WORKSPACE *ws, const char *task, char *a, size_t as, char *b, size_t bs)
{
    static char terms[DS_MAX_TERMS][128];
    int n = ds_terms(task, terms, DS_MAX_TERMS), na = 0, ia = -1;
    for (int i = 0; i < n; i++) {
        int sh = ds_shape(terms[i]);
        if (sh == DS_NONE)
            continue;
        if (sh == DS_IDENT && !strpbrk(terms[i], "_0123456789"))
            continue;
        if (ds_in(ws, terms[i], sh, 1) > 0 && ds_in(ws, terms[i], sh, 0) == 0) {
            na++;
            ia = i;
        }
    }
    if (na != 1)
        return na;
    int sa = ds_shape(terms[ia]), nb = 0, ib = -1;
    for (int i = 0; i < n; i++) {
        if (i == ia || ds_shape(terms[i]) != sa)
            continue;
        if (sa == DS_IDENT && (strlen(terms[i]) < 3 || ds_is_keyword(terms[i])))
            continue;
        if (ds_in(ws, terms[i], sa, 0) > 0) {
            nb++;
            ib = i;
        }
    }
    if (nb != 1)
        return nb;
    snprintf(a, as, "%s", terms[ia]);
    snprintf(b, bs, "%s", terms[ib]);
    return 1;
}

/* ------------------------------------------------- remove_dead_function
   Operator 7: a task sentence with a removal verb (declared lexical cue:
   remove/delete/drop/eliminate) names a function X that is defined once in a
   C source and referenced nowhere else (a file-scope prototype line is the
   only other use allowed). Remove the definition (from the start of its
   line through the matching '}') and any such prototype lines. */

static const char *df_skip_lit(const char *p)
{
    if (p[0] == '/' && p[1] == '/') { while (*p && *p != '\n') p++; return p; }
    if (p[0] == '/' && p[1] == '*') { const char *e = strstr(p + 2, "*/"); return e ? e + 2 : p + strlen(p); }
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        while (*p && *p != q) { if (*p == '\\' && p[1]) p++; p++; }
        return *p ? p + 1 : p;
    }
    return NULL;
}

/* Span of the definition of `name` in data: [from, to). 1 when exactly one. */
static int df_definition(const char *data, const char *name, size_t *from, size_t *to)
{
    size_t nl = strlen(name);
    int found = 0, depth = 0;
    const char *p = data;
    while (*p) {
        const char *s = df_skip_lit(p);
        if (s) { p = s; continue; }
        if (*p == '{') { depth++; p++; continue; }
        if (*p == '}') { depth--; p++; continue; }
        if (depth == 0 && !strncmp(p, name, nl) && (p == data || !ident_char((unsigned char)p[-1])) &&
            !ident_char((unsigned char)p[nl])) {
            const char *q = p + nl;
            while (isspace((unsigned char)*q)) q++;
            if (*q == '(') {
                int par = 0;
                while (*q) { if (*q == '(') par++; else if (*q == ')' && --par == 0) break; q++; }
                if (*q) q++;
                while (isspace((unsigned char)*q)) q++;
                if (*q == '{') {
                    const char *b = q;
                    int d = 0;
                    while (*b) {
                        const char *s2 = df_skip_lit(b);
                        if (s2) { b = s2; continue; }
                        if (*b == '{') d++;
                        else if (*b == '}' && --d == 0) break;
                        b++;
                    }
                    if (!*b) return 0;
                    const char *ls = p;
                    while (ls > data && ls[-1] != '\n') ls--;
                    const char *le = b + 1;
                    while (*le == ' ' || *le == '\t') le++;
                    if (*le == '\n') le++;
                    if (*le == '\n' && (ls == data || ls[-1] == '\n')) le++;   /* one blank separator */
                    *from = (size_t)(ls - data);
                    *to = (size_t)(le - data);
                    found++;
                    p = b + 1;
                    continue;
                }
            }
        }
        p++;
    }
    return found == 1;
}

/* 1 when the line holding offset `at` is a file-scope prototype of name. */
static int df_prototype_line(const char *data, size_t at, size_t *ls, size_t *le)
{
    size_t s = at, e = at;
    while (s > 0 && data[s - 1] != '\n') s--;
    while (data[e] && data[e] != '\n') e++;
    size_t k = e;
    while (k > s && isspace((unsigned char)data[k - 1])) k--;
    if (k == s || data[k - 1] != ';' || memchr(data + s, '{', e - s) || memchr(data + s, '=', e - s))
        return 0;
    *ls = s;
    *le = data[e] ? e + 1 : e;
    return 1;
}

static int df_cue_sentence(const char *task, const char *name)
{
    static const char *cues[] = {"remove", "Remove", "delete", "Delete", "drop", "Drop", "eliminate", "Eliminate", NULL};
    const char *p = task;
    while (*p) {
        const char *e = p;
        while (*e && !(*e == '.' && (e[1] == ' ' || e[1] == '\n' || !e[1])) && *e != '\n' && *e != ';') e++;
        size_t l = (size_t)(e - p);
        char sent[1024];
        if (l >= sizeof(sent)) l = sizeof(sent) - 1;
        memcpy(sent, p, l);
        sent[l] = '\0';
        if (count_token_in(sent, name) > 0)
            for (int k = 0; cues[k]; k++)
                if (strstr(sent, cues[k]) && !strstr(sent, "must not") && !strstr(sent, "don't") && !strstr(sent, "do not"))
                    return 1;
        p = *e ? e + 1 : e;
    }
    return 0;
}

/* Plan: returns count of candidate names (1 = usable) and fills name. */
static int find_dead_function(const TASK_OPS_WORKSPACE *ws, const char *task, char *name, size_t ns)
{
    int n = 0;
    const char *p = task;
    char seen[16][128];
    int nseen = 0;
    while (*p) {
        if (!ident_start((unsigned char)*p) || (p > task && ident_char((unsigned char)p[-1]))) { p++; continue; }
        const char *s = p;
        while (ident_char((unsigned char)*p)) p++;
        size_t l = (size_t)(p - s);
        if (l >= 128 || (l == 4 && !strncmp(s, "main", 4)))
            continue;
        char id[128];
        memcpy(id, s, l);
        id[l] = '\0';
        int dup = 0;
        for (int k = 0; k < nseen; k++) if (!strcmp(seen[k], id)) dup = 1;
        if (dup || nseen >= 16) continue;
        snprintf(seen[nseen++], 128, "%s", id);
        int defs = 0, uses = 0, protos = 0;
        for (int f = 0; f < ws->count; f++) {
            if (!is_c_source(ws->files[f].rel)) continue;
            const char *d = ws->files[f].data;
            size_t a, b;
            defs += df_definition(d, id, &a, &b);
            size_t il = strlen(id);
            for (const char *q = strstr(d, id); q; q = strstr(q + 1, id))
                if ((q == d || !ident_char((unsigned char)q[-1])) && !ident_char((unsigned char)q[il])) {
                    size_t x, y;
                    uses++;
                    if (df_prototype_line(d, (size_t)(q - d), &x, &y)) protos++;
                }
        }
        if (defs == 1 && uses == 1 + protos && df_cue_sentence(task, id)) {
            n++;
            snprintf(name, ns, "%s", id);
        }
    }
    return n;
}

static char *df_apply(const char *data, const char *name)
{
    size_t len = strlen(data), a, b;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, data, len + 1);
    if (df_definition(out, name, &a, &b))
        memmove(out + a, out + b, strlen(out + b) + 1);
    size_t il = strlen(name);
    for (char *q = strstr(out, name); q; ) {
        size_t x, y;
        if ((q == out || !ident_char((unsigned char)q[-1])) && !ident_char((unsigned char)q[il]) &&
            df_prototype_line(out, (size_t)(q - out), &x, &y)) {
            memmove(out + x, out + y, strlen(out + y) + 1);
            q = strstr(out + x, name);
        } else
            q = strstr(q + 1, name);
    }
    return out;
}

/* ------------------------------------------------------ unmatched_brace
   Operator 8: the build fails and exactly one '}' in the C sources closes
   nothing (brace depth, outside comments/strings/chars, would go below 0).
   Remove it (its whole line when the line holds only that brace). No task
   cue: the compiler failure plus the brace count is the precondition, and
   the build must go from failing to ok. */
static int find_unmatched_brace(const TASK_OPS_WORKSPACE *ws, int *file, size_t *at)
{
    int n = 0;
    for (int f = 0; f < ws->count; f++) {
        if (!is_c_source(ws->files[f].rel)) continue;
        const char *d = ws->files[f].data, *p = d;
        int depth = 0;
        while (*p) {
            const char *s = df_skip_lit(p);
            if (s) { p = s; continue; }
            if (*p == '{') depth++;
            else if (*p == '}') {
                if (depth == 0) { n++; *file = f; *at = (size_t)(p - d); }
                else depth--;
            }
            p++;
        }
        if (depth != 0) return 0;   /* an unclosed '{' too: not this operator */
    }
    return n;
}

static char *ub_apply(const char *data, size_t at)
{
    size_t len = strlen(data), s = at, e = at + 1;
    while (s > 0 && (data[s - 1] == ' ' || data[s - 1] == '\t')) s--;
    size_t k = e;
    while (data[k] == ' ' || data[k] == '\t') k++;
    if ((s == 0 || data[s - 1] == '\n') && (data[k] == '\n' || !data[k]))
        e = data[k] ? k + 1 : k;
    else
        s = at;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, data, s);
    memcpy(out + s, data + e, len - e + 1);
    return out;
}

/* ---------------------------------------------------------- author_test
   Operator 10: write a test. Preconditions: the task names exactly one .c
   file that does not exist (the test file), exactly one function that a
   workspace header declares, and an integer the call is expected to return
   (the first integer after the call in its sentence). The call is the one
   written in the task; if the task names the function without arguments,
   arguments come from the prototype (declared rule: int-like -> 1, char
   pointer -> "abc"; any other parameter type = abstain). The file includes
   that header and returns 0 when the call equals the value. Verify: the test
   compiles with the workspace sources that have no main, and exits 0. */

typedef struct {
    char test_rel[TASK_OPS_MAX_PATH];
    char header[TASK_OPS_MAX_PATH];
    char call[256];
    long expect;
} TEST_PLAN;

static int defines_main(const char *data)
{
    for (const char *p = strstr(data, "main"); p; p = strstr(p + 1, "main")) {
        if ((p == data || !ident_char((unsigned char)p[-1])) && !ident_char((unsigned char)p[4])) {
            const char *q = p + 4;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '(') return 1;
        }
    }
    return 0;
}

/* header index declaring name; params copied (between the parentheses) */
static int at_prototype(const TASK_OPS_WORKSPACE *ws, const char *name, char *params, size_t ps)
{
    int found = -1;
    for (int f = 0; f < ws->count; f++) {
        if (!is_header(ws->files[f].rel)) continue;
        const char *d = ws->files[f].data;
        size_t nl = strlen(name);
        for (const char *p = strstr(d, name); p; p = strstr(p + 1, name)) {
            if ((p != d && ident_char((unsigned char)p[-1])) || ident_char((unsigned char)p[nl])) continue;
            const char *q = p + nl;
            while (*q == ' ') q++;
            if (*q != '(') continue;
            const char *e = strchr(q, ')');
            if (!e || !strchr(e, ';') || memchr(q, '\n', (size_t)(e - q))) continue;
            const char *semi = e + 1;
            while (*semi == ' ') semi++;
            if (*semi != ';') continue;
            if (found >= 0 && found != f) return -2;
            found = f;
            snprintf(params, ps, "%.*s", (int)(e - q - 1), q + 1);
        }
    }
    return found;
}

static int at_args_from_params(const char *params, char *out, size_t os)
{
    size_t o = 0;
    out[0] = '\0';
    if (!strcmp(params, "void") || !params[0]) return 1;
    const char *p = params;
    while (*p) {
        const char *e = strchr(p, ',');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        char one[128];
        if (l >= sizeof(one)) return 0;
        memcpy(one, p, l);
        one[l] = '\0';
        const char *val;
        if (strstr(one, "char") && strchr(one, '*')) val = "\"abc\"";
        else if (strchr(one, '*') || strchr(one, '[') || strstr(one, "struct") || strstr(one, "...")) return 0;
        else val = "1";
        o += (size_t)snprintf(out + o, os - o, "%s%s", o ? ", " : "", val);
        if (o >= os) return 0;
        p = e ? e + 1 : p + l;
    }
    return 1;
}

static int find_test_plan(const TASK_OPS_WORKSPACE *ws, const char *task, TEST_PLAN *tp)
{
    memset(tp, 0, sizeof(*tp));
    size_t pos = 0;
    char name[TASK_OPS_MAX_PATH];
    int nt = 0;
    while (next_named_file(task, &pos, name, sizeof(name))) {
        size_t n = strlen(name);
        if (n > 2 && !strcmp(name + n - 2, ".c") && ws_find_named(ws, name) < 0 &&
            (nt == 0 || strcmp(tp->test_rel, name) != 0)) {
            snprintf(tp->test_rel, sizeof(tp->test_rel), "%s", name);
            nt++;
        }
    }
    if (nt != 1 || strchr(tp->test_rel, '/'))
        return 0;
    int nf = 0;
    const char *after = NULL;
    int literal_call = 0;
    char seen[16][64];
    int nseen = 0;
    for (const char *p = task; *p; p++) {
        if (!ident_start((unsigned char)*p) || (p > task && (ident_char((unsigned char)p[-1]) || p[-1] == '.'))) continue;
        const char *s = p;
        while (ident_char((unsigned char)*p)) p++;
        size_t l = (size_t)(p - s);
        if (l >= 64 || *p == '.') { p--; continue; }
        char id[64], params[256];
        memcpy(id, s, l);
        id[l] = '\0';
        int dup = 0;
        for (int k = 0; k < nseen; k++) if (!strcmp(seen[k], id)) dup = 1;
        int hf = at_prototype(ws, id, params, sizeof(params));
        if (!dup && nseen < 16) snprintf(seen[nseen++], 64, "%s", id);
        if (hf < 0) { p--; continue; }
        if (!dup) nf++;
        snprintf(tp->header, sizeof(tp->header), "%s", ws->files[hf].rel);
        const char *q = p;
        while (*q == ' ') q++;
        if (*q == '(') {
            /* a written call; one with literal-only arguments wins */
            const char *e = strchr(q, ')');
            if (!e || e - s >= (long)sizeof(tp->call)) return 0;
            int literal = 1;
            for (const char *a = q + 1; a < e; a++)
                if (ident_start((unsigned char)*a) && !ident_char((unsigned char)a[-1])) literal = 0;
            if (!after || literal_call == 0 && literal) {
                snprintf(tp->call, sizeof(tp->call), "%.*s", (int)(e + 1 - s), s);
                after = e + 1;
                literal_call = literal;
            }
        } else if (!after) {
            char args[160];
            if (!at_args_from_params(params, args, sizeof(args))) return 0;
            snprintf(tp->call, sizeof(tp->call), "%s(%s)", id, args);
            after = p;
        }
        p--;
    }
    if (nf != 1 || !after)
        return 0;
    for (const char *q = after; *q && *q != '\n'; q++) {
        if (*q == '.' && (q[1] == ' ' || !q[1])) break;   /* same sentence */
        if ((isdigit((unsigned char)*q) || (*q == '-' && isdigit((unsigned char)q[1]))) &&
            !ident_char((unsigned char)q[-1])) {
            char *end;
            tp->expect = strtol(q, &end, 10);
            if (ident_char((unsigned char)*end) || *end == '.' && isdigit((unsigned char)end[1])) return 0;
            return 1;
        }
    }
    return 0;
}

static int run_test_plan(const TASK_OPS_WORKSPACE *ws, const TEST_PLAN *tp, const char *flags)
{
    char cmd[4096], bin[TASK_OPS_MAX_PATH];
    size_t o = 0;
    temp_binary(bin, sizeof(bin));
    o += (size_t)snprintf(cmd, sizeof(cmd), "gcc %s-o \"%s\" \"%s\"", flags, bin, tp->test_rel);
    for (int i = 0; i < ws->count && o < sizeof(cmd); i++) {
        size_t n = strlen(ws->files[i].rel);
        if (n < 3 || strcmp(ws->files[i].rel + n - 2, ".c") || !strcmp(ws->files[i].rel, tp->test_rel) ||
            defines_main(ws->files[i].data) || strchr(ws->files[i].rel, '"'))
            continue;
        o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, " \"%s\"", ws->files[i].rel);
    }
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r) return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    int ok = !r->execution_failed && r->exit_code == 0;
    if (ok) {
        char run_cmd[TASK_OPS_MAX_PATH + 8];
        snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin);
        AgentShellResultInit(r);
        AgentShellExec(run_cmd, ws->root, 15000, r);   /* 5 s flaked under load (cca91ab note) */
        ok = !r->timed_out && r->exit_code == 0;
    }
    remove(bin);
    free(r);
    return ok;
}



/* ------------------------------------------ task-stated examples (phase 3) */

/* Examples the task writes as f(<int literals>) == <int> or
   f(<int literals>) expect <int>, where f is defined in the workspace.
   They become an extra, stronger check for behavior-changing operators, and
   a probe for workspaces with no main. Declared rule (example shape). */
#define EX_MAX 8
typedef struct
{
    char call[160];
    long expect;
} EXAMPLE;

static const char *skip_int(const char *p)
{
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return NULL;
    while (isdigit((unsigned char)*p)) p++;
    return p;
}

static int task_examples(const TASK_OPS_WORKSPACE *ws, const char *task, EXAMPLE *ex, int max)
{
    int n = 0;
    size_t len = strlen(task);
    for (size_t i = 0; i < len && n < max; i++) {
        if (!ident_start((unsigned char)task[i]) || (i > 0 && ident_char((unsigned char)task[i - 1])))
            continue;
        size_t e = i;
        while (ident_char((unsigned char)task[e])) e++;
        if (task[e] != '(' || e - i >= 64)
            { i = e; continue; }
        const char *p = task + e + 1;
        int ok = 1, args = 0;
        while (*p == ' ') p++;
        while (*p && *p != ')') {
            const char *q = skip_int(p);
            if (!q) { ok = 0; break; }
            args++;
            p = q;
            while (*p == ' ') p++;
            if (*p == ',') { p++; while (*p == ' ') p++; }
            else if (*p != ')') { ok = 0; break; }
        }
        if (!ok || *p != ')' || args == 0)
            { i = e; continue; }
        const char *close = p;
        p++;
        while (*p == ' ') p++;
        if (!strncmp(p, "==", 2)) p += 2;
        else if (!strncmp(p, "expect", 6)) p += 6;
        else { i = e; continue; }
        while (*p == ' ') p++;
        const char *v = p, *ve = skip_int(p);
        char name[64];
        memcpy(name, task + i, e - i);
        name[e - i] = '\0';
        if (!ve || !file_has_define(ws, name))
            { i = e; continue; }
        snprintf(ex[n].call, sizeof(ex[n].call), "%.*s", (int)(close + 1 - (task + i)), task + i);
        ex[n].expect = strtol(v, NULL, 10);
        n++;
        i = (size_t)(ve - task);
    }
    return n;
}

static int any_main(const TASK_OPS_WORKSPACE *ws)
{
    for (int i = 0; i < ws->count; i++)
        if (is_c_source(ws->files[i].rel) && defines_main(ws->files[i].data))
            return 1;
    return 0;
}

/* 1 = every example holds (built from the workspace sources that have no
   main, plus a generated harness outside the workspace), 0 = not, -1 = no
   build. */
static int run_examples(const TASK_OPS_WORKSPACE *ws, const char *flags, const EXAMPLE *ex, int n)
{
    char hpath[TASK_OPS_MAX_PATH + 8], bin[TASK_OPS_MAX_PATH];
    temp_binary(bin, sizeof(bin));
    snprintf(hpath, sizeof(hpath), "%s_h.c", bin);
    FILE *h = fopen(hpath, "w");
    if (!h) return -1;
    for (int k = 0; k < n; k++) {
        char name[64];
        size_t nl = 0;
        while (nl < sizeof(name) - 1 && ident_char((unsigned char)ex[k].call[nl])) nl++;
        memcpy(name, ex[k].call, nl);
        name[nl] = '\0';
        int dup = 0;
        for (int j = 0; j < k; j++) dup |= !strncmp(ex[j].call, name, nl) && ex[j].call[nl] == '(';
        for (int i = 0; !dup && i < ws->count; i++) {
            size_t ll;
            long at = is_c_source(ws->files[i].rel) ? file_level_decl(ws->files[i].data, name, 1, &ll) : -1;
            if (at >= 0) { fprintf(h, "%.*s;\n", (int)ll, ws->files[i].data + at); break; }
        }
    }
    fprintf(h, "int main(void)\n{\n    return (1");
    for (int k = 0; k < n; k++) fprintf(h, " && (%s) == %ld", ex[k].call, ex[k].expect);
    fprintf(h, ") ? 0 : 1;\n}\n");
    fclose(h);
    char cmd[4096], objs[TASK_OPS_MAX_FILES][TASK_OPS_MAX_PATH + 16];
    int nobj = 0;
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    int res = -1;
    if (!r) { remove(hpath); return -1; }
    /* a source that defines main is compiled with its main renamed, so the
       functions it defines stay callable from the harness */
    for (int i = 0; i < ws->count && nobj < TASK_OPS_MAX_FILES; i++)
        if (is_c_source(ws->files[i].rel) && defines_main(ws->files[i].data) && !strchr(ws->files[i].rel, '"') &&
            strncmp(ws->files[i].rel, "test", 4) != 0) {
            snprintf(objs[nobj], sizeof(objs[nobj]), "%s_%d.o", bin, nobj);
            snprintf(cmd, sizeof(cmd), "gcc %s-w -Dmain=symbols_ws_main -c \"%s\" -o \"%s\"", flags, ws->files[i].rel, objs[nobj]);
            AgentShellResultInit(r);
            AgentShellExec(cmd, ws->root, 20000, r);
            nobj++;
            if (r->execution_failed || r->exit_code != 0) goto done;
        }
    size_t o = (size_t)snprintf(cmd, sizeof(cmd), "gcc %s-w -o \"%s\" \"%s\"", flags, bin, hpath);
    for (int i = 0; i < ws->count && o < sizeof(cmd); i++)
        if (is_c_source(ws->files[i].rel) && !defines_main(ws->files[i].data) && !strchr(ws->files[i].rel, '"') &&
            strncmp(ws->files[i].rel, "test", 4) != 0)
            o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, " \"%s\"", ws->files[i].rel);
    for (int k = 0; k < nobj && o < sizeof(cmd); k++)
        o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, " \"%s\"", objs[k]);
    {
        AgentShellResultInit(r);
        AgentShellExec(cmd, ws->root, 20000, r);
        if (!r->execution_failed && r->exit_code == 0) {
            char run_cmd[TASK_OPS_MAX_PATH + 8];
            snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", bin);
            AgentShellResultInit(r);
            AgentShellExec(run_cmd, ws->root, 15000, r);
            res = (!r->timed_out && r->exit_code == 0) ? 1 : 0;
        }
    }
done:
    free(r);
    for (int k = 0; k < nobj; k++) remove(objs[k]);
    remove(bin);
    remove(hpath);
    return res;
}

/* ------------------------------------------ relational operator search */

/* Observed state: the program builds but exits non-zero. Try one
   relational-operator swap at a time (in C sources, not preprocessor
   lines) and keep the swap that makes it exit 0. Tiers from the smallest
   change: 1 boundary (< <=, > >=), 2 direction (< >, <= >=), 3 the rest.
   Equality (== !=) is never swapped and test files (test*) are never
   edited: negating an assertion would "pass" by cheating. The first tier with exactly one working swap wins; two in the
   same tier = abstain. No task wording. Declared rule (tiers, 64 builds). */
static const char *const relops[] = {"<", "<=", ">", ">="};

static int relop_tier(const char *a, const char *b)
{
    if (!strcmp(a, b)) return 0;
    if (a[0] == b[0] && (a[0] == '<' || a[0] == '>')) return 1;
    if ((a[0] == '<' && b[0] == '>') || (a[0] == '>' && b[0] == '<'))
        return strlen(a) == strlen(b) ? 2 : 3;
    if ((!strcmp(a, "==") && !strcmp(b, "!=")) || (!strcmp(a, "!=") && !strcmp(b, "=="))) return 2;
    return 3;
}

typedef struct
{
    int    file;
    size_t at, len;
    char   to[24];
    char   pat[32];   /* induction pattern: primitive class @ site context */
} RELOP_HIT;

/* Phase 3b: the same search widened to the other single-token primitives,
   tried only after every relational tier found nothing, smallest change
   first: tier 4 a decimal integer literal +1 / -1, tier 5 binary + <-> -,
   tier 6 && <-> ||.
   A +/- is binary only when the token before it is a name, a number or a
   closing bracket. Same anchors, same oracle, same 64-build budget. */
static int prim_cands(const CTOK *t, int k, int tier, char out[2][24])
{
    const char *x = t[k].text;
    if (tier <= 3) {
        int is_rel = 0, n = 0;
        for (int r = 0; r < 4; r++) is_rel |= !strcmp(x, relops[r]);
        if (!is_rel) return 0;
        for (int r = 0; r < 4 && n < 2; r++)
            if (relop_tier(x, relops[r]) == tier) snprintf(out[n++], 24, "%s", relops[r]);
        return n;
    }
    if (tier == 6) {
        if (!strcmp(x, "&&")) { snprintf(out[0], 24, "||"); return 1; }
        if (!strcmp(x, "||")) { snprintf(out[0], 24, "&&"); return 1; }
        return 0;
    }
    if (tier == 5) {
        if ((!strcmp(x, "+") || !strcmp(x, "-")) && k > 0) {
            const char *p = t[k - 1].text;
            if (ident_char((unsigned char)p[0]) || p[0] == ')' || p[0] == ']') {
                snprintf(out[0], 24, "%s", x[0] == '+' ? "-" : "+");
                return 1;
            }
        }
        return 0;
    }
    if (tier == 4) {
        size_t l = strlen(x);
        if (l == 0 || l > 9) return 0;
        for (size_t i = 0; i < l; i++) if (!isdigit((unsigned char)x[i])) return 0;
        if (l > 1 && x[0] == '0') return 0;   /* octal or odd spelling: leave it */
        long v = strtol(x, NULL, 10);
        int n = 0;
        snprintf(out[n++], 24, "%ld", v + 1);
        if (v > 0) snprintf(out[n++], 24, "%ld", v - 1);
        return n;
    }
    return 0;
}

static int write_file(const char *root, const char *rel, const char *data);

/* Anchors: identifiers the task names that the workspace knows (code-shaped
   or a defined function). A swap is only tried on a line holding an anchor;
   no anchor = abstain. The mutation corpus showed that exit 0 alone picks a
   wrong swap about half the time (phase 2). */
static int line_has_anchor(const char *line, size_t ll, const TASK_TOKEN *an, int na)
{
    for (int a = 0; a < na; a++) {
        size_t al = strlen(an[a].text);
        for (size_t i = 0; i + al <= ll; i++)
            if (!strncmp(line + i, an[a].text, al) && (i == 0 || !ident_char((unsigned char)line[i - 1])) &&
                (i + al == ll || !ident_char((unsigned char)line[i + al])))
                return 1;
    }
    return 0;
}

/* main is the oracle when the program's own exit code is checked: its
   lines are never edited (the corpus caught "? 0 : 1" -> "? 0 : 0"). */
static int line_defines_main(const char *line, size_t ll)
{
    for (size_t i = 0; i + 4 <= ll; i++)
        if (!strncmp(line + i, "main", 4) && (i == 0 || !ident_char((unsigned char)line[i - 1]))) {
            size_t j = i + 4;
            while (j < ll && isspace((unsigned char)line[j])) j++;
            if (j < ll && line[j] == '(') return 1;
        }
    return 0;
}

/* ---- phase 4: induced operators ------------------------------------
   A pattern is "<primitive class>@<site>": class relop1/relop2/relop3
   (the relational tier), lit, arith or logic; site is the statement the
   token sits in, read from the line's first keyword before it: loop
   (for/while), if, return, else stmt. tools/induce_operators.py writes
   the table from labeled traces (exact restoration = positive); it is
   loaded only when SYMBOLS_OPERATORS names a file, otherwise the search
   behaves exactly as before. Demoted patterns (any non-exact repair in
   training) are never applied; when a tier has several verified
   candidates, a single promoted one is kept instead of abstaining. */
static const char *site_ctx(const CTOK *t, int k)
{
    for (int i = 0; i < k; i++) {
        if (!strcmp(t[i].text, "for") || !strcmp(t[i].text, "while")) return "loop";
        if (!strcmp(t[i].text, "if")) return "if";
        if (!strcmp(t[i].text, "return")) return "return";
    }
    return "stmt";
}

static const char *prim_class(int tier)
{
    static const char *const c[] = {"?", "relop1", "relop2", "relop3", "lit", "arith", "logic"};
    return tier >= 1 && tier <= 6 ? c[tier] : "?";
}

#define IOP_MAX 64
static struct { char pat[32]; int promoted; } iops[IOP_MAX];
static int niops = -1;
static char iops_path[512];

static void iops_load(void)
{
    const char *path = getenv("SYMBOLS_OPERATORS");
    char line[256];
    niops = 0;
    snprintf(iops_path, sizeof(iops_path), "%s", path ? path : "");
    if (!path || !*path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (niops < IOP_MAX && fgets(line, sizeof(line), f)) {
        char pat[32], st[16];
        if (line[0] == '#' || sscanf(line, "%31s %15s", pat, st) != 2) continue;
        if (strcmp(st, "promoted") && strcmp(st, "demoted")) continue;
        snprintf(iops[niops].pat, sizeof(iops[niops].pat), "%s", pat);
        iops[niops++].promoted = !strcmp(st, "promoted");
    }
    fclose(f);
}

/* 1 promoted, -1 demoted, 0 unknown (or no table) */
static int iop_status(const char *pat)
{
    const char *path = getenv("SYMBOLS_OPERATORS");
    if (niops < 0 || strcmp(iops_path, path ? path : "")) iops_load();
    for (int i = 0; i < niops; i++)
        if (!strcmp(iops[i].pat, pat)) return iops[i].promoted ? 1 : -1;
    return 0;
}

/* Phase 4b: oracle strength at the edit site. The enclosing function is
   the nearest column-0 head above the edit ("name(" on a line that is not
   a directive or a brace). Strength counts the distinct argument lists it
   is called with elsewhere in the workspace plus in stated examples:
   "o1" = one input or none (a single-value self-check, which lets a wrong
   swap still exit 0), "o2" = two or more. */
static void enclosing_name(const char *data, size_t at, char *out, size_t size)
{
    out[0] = '\0';
    size_t ls = at;
    while (1) {
        while (ls > 0 && data[ls - 1] != '\n') ls--;
        const char *l = data + ls;
        if (ident_char((unsigned char)*l) && *l != '#') {
            const char *par = l;
            while (*par && *par != '\n' && *par != '(') par++;
            if (*par == '(') {
                const char *e = par;
                while (e > l && isspace((unsigned char)e[-1])) e--;
                const char *b = e;
                while (b > l && ident_char((unsigned char)b[-1])) b--;
                if (e > b && (size_t)(e - b) < size) {
                    memcpy(out, b, (size_t)(e - b));
                    out[e - b] = '\0';
                    return;
                }
            }
        }
        if (ls == 0) return;
        ls--;
    }
}

static const char *oracle_strength(const TASK_OPS_WORKSPACE *ws, const char *fn, const EXAMPLE *ex, int nex)
{
    char seen[4][64];
    int ns = 0;
    size_t fl = strlen(fn);
    if (!fl) return "o1";
    for (int f = 0; f < ws->count && ns < 2; f++) {
        const TASK_OPS_FILE *F = &ws->files[f];
        if (!is_c_source(F->rel)) continue;
        for (size_t i = 0; i + fl < F->len && ns < 2; i++) {
            if (strncmp(F->data + i, fn, fl) || (i > 0 && ident_char((unsigned char)F->data[i - 1]))) continue;
            size_t j = i + fl;
            while (j < F->len && F->data[j] == ' ') j++;
            if (j >= F->len || F->data[j] != '(') continue;
            size_t ls = i;
            while (ls > 0 && F->data[ls - 1] != '\n') ls--;
            {   /* definition head: column-0 line, only type words before the name */
                int head = ident_char((unsigned char)F->data[ls]);
                for (size_t q = ls; head && q < i; q++)
                    if (!ident_char((unsigned char)F->data[q]) && F->data[q] != ' ' && F->data[q] != '*' && F->data[q] != '\t')
                        head = 0;
                if (head && i - ls >= 6 && !strncmp(F->data + ls, "return", 6)) head = 0;
                if (head) continue;
            }
            size_t k = j, depth = 0;
            for (; k < F->len; k++) {
                if (F->data[k] == '(') depth++;
                else if (F->data[k] == ')' && --depth == 0) break;
            }
            if (k >= F->len || k - j >= 63) continue;
            char args[64];
            memcpy(args, F->data + j, k - j + 1);
            args[k - j + 1] = '\0';
            int dup = 0;
            for (int q = 0; q < ns; q++) dup |= !strcmp(seen[q], args);
            if (!dup) snprintf(seen[ns++], 64, "%s", args);
        }
    }
    for (int e = 0; e < nex && ns < 2; e++)
        if (!strncmp(ex[e].call, fn, fl) && ex[e].call[fl] == '(') {
            int dup = 0;
            for (int q = 0; q < ns; q++) dup |= !strcmp(seen[q], ex[e].call + fl);
            if (!dup) snprintf(seen[ns++], 64, "%.63s", ex[e].call + fl);
        }
    return ns >= 2 ? "o2" : "o1";
}

static int relop_search(const TASK_OPS_WORKSPACE *ws, const char *flags, const char *task, RELOP_HIT *hit)
{
    int builds = 0;
    EXAMPLE ex[EX_MAX];
    int nex = task_examples(ws, task ? task : "", ex, EX_MAX), has_main = any_main(ws);
    if (!has_main && nex == 0)
        return 0;
    {   /* the observed state must fail before: own exit code or a stated example */
        int c0 = -1, r0 = -1;
        if (has_main) probe_out(ws, flags, &c0, &r0, NULL);
        int main_fails = has_main && c0 == 1 && r0 != 0 && r0 != 124;
        int ex_fails = nex > 0 && run_examples(ws, flags, ex, nex) == 0;
        if (!main_fails && !ex_fails)
            return 0;
        if (has_main && c0 != 1)
            return 0;
    }
    TASK_TOKEN an[64];
    int na = 0, nt = lex_code_tokens_g(task ? task : "", an, 64, ws);
    for (int k = 0; k < nt; k++)
        if (TaskOpsCountToken(ws, an[k].text) > 0)
            an[na++] = an[k];
    if (na == 0)
        return 0;
    for (int tier = 1; tier <= 6; tier++) {
        RELOP_HIT hits[8];
        int nh = 0;
        for (int f = 0; f < ws->count; f++) {
            const TASK_OPS_FILE *F = &ws->files[f];
            if (!is_c_source(F->rel) || !strncmp(F->rel, "test", 4) || strstr(F->rel, "/test"))
                continue;   /* never flip a test's own comparison to make it pass */
            const char *line = F->data;
            int depth = 0, in_fn = 0, in_main = 0;   /* inside the body of a function whose head holds an anchor / of main */
            int pending = 0;   /* head seen at depth 0, body brace not yet (Allman style: "{" on the next line) */
            while (line && *line) {
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl - line) : strlen(line);
                int anchored = line_has_anchor(line, ll, an, na);
                if (depth == 0) {
                    if (!pending) {
                        in_fn = anchored;
                        in_main = line_defines_main(line, ll);
                    } else {
                        in_fn |= anchored;
                        in_main |= line_defines_main(line, ll);
                    }
                }
                for (size_t q = 0; q < ll; q++)
                    depth += line[q] == '{' ? 1 : line[q] == '}' ? -1 : 0;
                if (depth < 0) depth = 0;
                int eligible = (anchored || in_fn) && !(has_main && in_main);
                if (depth == 0) {
                    const char *h0 = line;
                    while (h0 < line + ll && isspace((unsigned char)*h0)) h0++;
                    pending = (in_fn || in_main) && h0 < line + ll && *h0 != '#' && memchr(line, '(', ll) &&
                              !memchr(line, ';', ll) && !memchr(line, '{', ll) && !memchr(line, '}', ll);
                    if (!pending) {
                        if (!anchored) in_fn = 0;
                        in_main = 0;
                    }
                }
                const char *t0 = line;
                while (t0 < line + ll && isspace((unsigned char)*t0)) t0++;
                CTOK t[256];
                int n = !eligible ? 0 : (*t0 == '#' || (t0[0] == '/' && (t0[1] == '/' || t0[1] == '*'))) ? 0 : ctok_lex(line, ll, t, 256);
                for (int k = 0; k < n; k++) {
                    char cands[2][24];
                    int nc = prim_cands(t, k, tier, cands);
                    for (int r = 0; r < nc; r++) {
                        if (++builds > 64) return 0;
                        size_t at = (size_t)(line - F->data) + t[k].start, ol = strlen(t[k].text), nl2 = strlen(cands[r]);
                        char *cand = (char *)malloc(F->len - ol + nl2 + 1);
                        if (!cand) return 0;
                        memcpy(cand, F->data, at);
                        memcpy(cand + at, cands[r], nl2);
                        memcpy(cand + at + nl2, F->data + at + ol, F->len - at - ol + 1);
                        int c = -1, run = -1, exok = 1;
                        if (write_file(ws->root, F->rel, cand)) {
                            if (has_main) probe_out(ws, flags, &c, &run, NULL);
                            if (nex && (!has_main || (c == 1 && run == 0))) exok = run_examples(ws, flags, ex, nex) == 1;
                        }
                        write_file(ws->root, F->rel, F->data);
                        free(cand);
                        /* own exit code when there is a main, and every stated example */
                        if ((!has_main || (c == 1 && run == 0)) && exok && nh < 8) {
                            RELOP_HIT *h = &hits[nh++];
                            h->file = f;
                            h->at = at;
                            h->len = ol;
                            snprintf(h->to, sizeof(h->to), "%s", cands[r]);
                            char fn[64];
                            enclosing_name(F->data, at, fn, sizeof(fn));
                            snprintf(h->pat, sizeof(h->pat), "%s@%s/%s", prim_class(tier), site_ctx(t, k),
                                     oracle_strength(ws, fn, ex, nex));
                        }
                    }
                }
                line = nl ? nl + 1 : NULL;
            }
        }
        if (nh) {
            /* drop demoted patterns; one left = keep it; several = keep a
               single promoted one, else abstain (the pre-phase-4 rule) */
            int keep = -1, left = 0, prom = 0, pi = -1;
            for (int i = 0; i < nh; i++) {
                int stt = iop_status(hits[i].pat);
                if (stt < 0) continue;
                left++;
                keep = i;
                if (stt > 0) { prom++; pi = i; }
            }
            if (left == 1) { *hit = hits[keep]; return 1; }
            if (left > 1 && prom == 1) { *hit = hits[pi]; return 1; }
            return left ? left : -1;   /* -1: every verified candidate was demoted */
        }
    }
    return 0;
}


/* ------------------------------------------------ induction traces (phase 1) */

/* When SYMBOLS_TRACE names a file, every TaskOpsSolve attempt appends one
   TSV line: workspace hash, observed features, operator, verified, detail.
   Features are observed state only (build/run result and the first gcc
   diagnostic, with names and numbers masked), never task wording. No
   behavior change when the variable is unset. See docs/operator_induction.md. */
static unsigned long ws_hash(const TASK_OPS_WORKSPACE *ws)
{
    unsigned long h = 1469598103UL;
    for (int i = 0; i < ws->count; i++) {
        for (const char *c = ws->files[i].rel; *c; c++) h = (h ^ (unsigned char)*c) * 16777619UL;
        for (size_t k = 0; k < ws->files[i].len; k++) h = (h ^ (unsigned char)ws->files[i].data[k]) * 16777619UL;
    }
    return h & 0xffffffffUL;
}

static void first_diag_class(const TASK_OPS_WORKSPACE *ws, const char *flags, char *out, size_t size)
{
    char cmd[4096], srcs[3072];
    snprintf(out, size, "-");
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return;
    snprintf(cmd, sizeof(cmd), "gcc %s-Wall -fsyntax-only %s", flags, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r) return;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    const char *p = r->execution_failed ? NULL : strstr(r->stderr_buf, "error: ");
    if (!p && !r->execution_failed) p = strstr(r->stderr_buf, "warning: ");
    if (p) {
        size_t o = 0;
        int in_q = 0;
        for (const char *c = p; *c && *c != '\n' && o + 2 < size; c++) {
            unsigned char u = (unsigned char)*c;
            if (*c == '\'' || u == 0xe2) {             /* quoted name: mask */
                if (u == 0xe2) c += 2;
                if (!in_q && o + 2 < size) { out[o++] = 'X'; }
                in_q = !in_q;
                continue;
            }
            if (in_q) continue;
            if (isdigit(u)) { if (o == 0 || out[o - 1] != 'N') out[o++] = 'N'; continue; }
            out[o++] = (*c == '\t') ? ' ' : *c;
        }
        out[o] = '\0';
    }
    free(r);
}

static char trace_diag[160];   /* first diagnostic class, taken before any edit */

static void trace_begin(const TASK_OPS_WORKSPACE *ws, const char *flags)
{
    const char *path = getenv("SYMBOLS_TRACE");
    trace_diag[0] = '\0';
    if (path && *path)
        first_diag_class(ws, flags, trace_diag, sizeof(trace_diag));
}

static void trace_attempt(const TASK_OPS_WORKSPACE *ws, const char *flags, const TASK_OPS_REPORT *rep)
{
    const char *path = getenv("SYMBOLS_TRACE");
    (void)flags;
    if (!path || !*path)
        return;
    const char *diag = trace_diag[0] ? trace_diag : "-";
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    char detail[256];
    snprintf(detail, sizeof(detail), "%s", rep->op[0] ? rep->detail : rep->reason);
    for (char *c = detail; *c; c++) if (*c == '\t' || *c == '\n') *c = ' ';
    fprintf(f, "%08lx\tcompile=%d\trun=%d\tdiag=%s\top=%s\tverified=%d\t%s\n", ws_hash(ws), rep->compile_before,
            rep->run_before, diag, rep->op[0] ? rep->op : "none", rep->verified, detail);
    fclose(f);
}

/* ------------------------------------------------------- shell hardening */

/* The one shell script a shell rule applies to; a task that names script
   files restricts the candidates to those. -1 when none or ambiguous. */
static int shell_target(const TASK_OPS_WORKSPACE *ws, const char *task, char **out,
                        char *rule, size_t rsz, char *detail, size_t dsz)
{
    int hit = -1, named = 0;
    *out = NULL;
    for (int i = 0; i < ws->count; i++)
        if (ShellOpsIsScript(ws->files[i].rel, ws->files[i].data) && strstr(task, ws->files[i].rel))
            named++;
    for (int i = 0; i < ws->count; i++) {
        if (!ShellOpsIsScript(ws->files[i].rel, ws->files[i].data))
            continue;
        if (named && !strstr(task, ws->files[i].rel))
            continue;
        char r[32], d[128];
        char *o = ShellOpsApply(ws->files[i].data, task, r, sizeof(r), d, sizeof(d));
        if (!o)
            continue;
        if (!strcmp(r, "file_content")) {   /* the stated output must not be an existing file */
            const char *gt = strrchr(d, '>');
            char f[128];
            if (!gt || sscanf(gt + 1, " %127s", f) != 1 || ws_find_named(ws, f) >= 0) {
                free(o);
                continue;
            }
        }
        if (hit >= 0) {           /* ambiguous: abstain */
            free(o);
            free(*out);
            *out = NULL;
            return -1;
        }
        hit = i;
        *out = o;
        snprintf(rule, rsz, "%s", r);
        snprintf(detail, dsz, "%s", d);
    }
    return hit;
}

/* `sh -n` accepts the script; no shell available = not verified */
static int shell_syntax_ok(const char *root, const char *rel)
{
    if (strchr(rel, '"'))
        return 0;
    char cmd[TASK_OPS_MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "sh -n \"%s\"", rel);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, root, 10000, r);
    int ok = !r->execution_failed && !r->timed_out && r->exit_code == 0;
    free(r);
    return ok;
}


/* Stated-contract shell rules (arg_exit, file_content) are also run: all
   workspace text files are copied to a throwaway directory outside the
   tree, the edited script runs there once with HOME pointed at the copy
   and a 3 s timeout, and the stated contract is checked (exit code for the
   stated argument; the stated file holds the stated word). The copy is
   removed afterwards. SYMBOLS_SHELL_RUN=0 keeps the check static only. */
static void make_dir(const char *path);

static int sc_exec(const char *cmd, const char *cwd, int *code, int *timed_out)
{
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    int ok;
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, cwd, 3000, r);
    ok = !r->execution_failed;
    *code = r->exit_code;
    *timed_out = r->timed_out;
    free(r);
    return ok;
}

static void sc_rm_tree(const char *dir, int depth)
{
    char p[TASK_OPS_MAX_PATH * 2];
    if (depth > 16)
        return;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(p, sizeof(p), "%s\\*", dir);
    h = FindFirstFileA(p, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
                continue;
            snprintf(p, sizeof(p), "%s/%s", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                sc_rm_tree(p, depth + 1);
            else {
                SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
                remove(p);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    _rmdir(dir);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    if (d) {
        while ((e = readdir(d)) != NULL) {
            struct stat st;
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
            if (lstat(p, &st) == 0 && S_ISDIR(st.st_mode))
                sc_rm_tree(p, depth + 1);
            else
                remove(p);
        }
        closedir(d);
    }
    rmdir(dir);
#endif
}

static int sc_safe_name(const char *f)
{
    return f[0] && f[0] != '/' && f[0] != '\\' && !strstr(f, "..") && !strchr(f, '"') &&
           !strchr(f, ':') && !strchr(f, '$') && !strchr(f, '`');
}

static int shell_contract_run(const TASK_OPS_WORKSPACE *ws, int idx, const char *rule, const char *detail)
{
    const char *v = getenv("SYMBOLS_SHELL_RUN");
    char dir[TASK_OPS_MAX_PATH], cmd[TASK_OPS_MAX_PATH * 3], word[64] = "", file[128] = "";
    int code = -1, want = -1, to = 0, ok = 0;
    const char *rel = ws->files[idx].rel;
    if (v && !strcmp(v, "0"))
        return 1;
    if (!strcmp(rule, "arg_exit")) {
        if (sscanf(detail, "exit %d when $1 is %63s", &want, word) != 2)
            return 0;
    } else if (!strcmp(rule, "file_content")) {
        if (sscanf(detail, "echo %63s > %127s", word, file) != 2 || !sc_safe_name(file))
            return 0;
    } else
        return 1;
    if (!sc_safe_name(rel) || !sc_safe_name(word))
        return 0;
    temp_binary(dir, sizeof(dir));
    strncat(dir, "_shrun", sizeof(dir) - strlen(dir) - 1);
    if (strchr(dir, '"'))
        return 0;
    make_dir(dir);
    for (int i = 0; i < ws->count; i++) {
        char sub[TASK_OPS_MAX_PATH * 2];
        const char *r = ws->files[i].rel;
        for (const char *q = strchr(r, '/'); q; q = strchr(q + 1, '/')) {
            snprintf(sub, sizeof(sub), "%s/%.*s", dir, (int)(q - r), r);
            make_dir(sub);
        }
        if (!write_file(dir, r, ws->files[i].data))
            goto out;
    }
#ifdef _WIN32
    if (want >= 0)
        snprintf(cmd, sizeof(cmd), "sh \"%s\" \"%s\"", rel, word);
    else
        snprintf(cmd, sizeof(cmd), "sh \"%s\"", rel);
#else
    if (want >= 0)
        snprintf(cmd, sizeof(cmd), "HOME=\"%s\" sh \"%s\" \"%s\" </dev/null", dir, rel, word);
    else
        snprintf(cmd, sizeof(cmd), "HOME=\"%s\" sh \"%s\" </dev/null", dir, rel);
#endif
    if (!sc_exec(cmd, dir, &code, &to) || to)
        goto out;
    if (want >= 0)
        ok = code == want;
    else {
        char p[TASK_OPS_MAX_PATH * 2], buf[128];
        FILE *f;
        size_t n;
        snprintf(p, sizeof(p), "%s/%s", dir, file);
        f = fopen(p, "rb");
        if (f) {
            n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';
            while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
                buf[--n] = '\0';
            ok = !strcmp(buf, word);
        }
    }
out:
    sc_rm_tree(dir, 0);
    return ok;
}

/* --------------------------------------------------------- build repair */

/* sh -n evidence: a script the shell parser rejects; the one lowest-tier
   syntax candidate after which sh -n passes wins. Task wording not read. */
static int shell_syntax_target(const TASK_OPS_WORKSPACE *ws, char **out, char *rule, size_t rsz, char *detail, size_t dsz)
{
    int hit = -1, wins = 0;
    *out = NULL;
    for (int i = 0; i < ws->count; i++) {
        const TASK_OPS_FILE *F = &ws->files[i];
        if (!ShellOpsIsScript(F->rel, F->data) || F->len > 65536 || shell_syntax_ok(ws->root, F->rel))
            continue;
        static SHELL_CAND c[64];
        int n = ShellSyntaxCandidates(F->data, c, 64), best = 99, fw = 0, pick = -1;
        for (int tier = 1; tier <= 2 && fw == 0; tier++)
            for (int k = 0; k < n; k++) {
                if (c[k].tier != tier || !write_file(ws->root, F->rel, c[k].text))
                    continue;
                if (shell_syntax_ok(ws->root, F->rel)) {
                    fw++;
                    pick = k;
                    best = tier;
                }
            }
        write_file(ws->root, F->rel, F->data);   /* always restore */
        (void)best;
        if (fw == 1) {
            wins++;
            if (wins == 1) {
                hit = i;
                *out = c[pick].text;
                c[pick].text = NULL;
                snprintf(rule, rsz, "%s", c[pick].rule);
                snprintf(detail, dsz, "sh -n: %s", c[pick].detail);
            }
        } else if (fw > 1)
            wins += 2;   /* ambiguous inside one file */
        ShellSyntaxCandidatesFree(c, n);
    }
    if (wins != 1) {
        free(*out);
        *out = NULL;
        return -1;
    }
    return hit;
}

static int build_plan(const TASK_OPS_WORKSPACE *ws, const char *task, BUILD_EDIT *e)
{
    const char *rels[TASK_OPS_MAX_FILES], *datas[TASK_OPS_MAX_FILES];
    for (int i = 0; i < ws->count; i++) {
        rels[i] = ws->files[i].rel;
        datas[i] = ws->files[i].data;
    }
    return BuildOpsPlan(rels, datas, ws->count, task, e);
}

/* cmake configure of the workspace in a scratch dir; stdout+stderr into buf */
static int cmake_output(const TASK_OPS_WORKSPACE *ws, char *buf, size_t size);

static int run_ok(const char *cmd, const char *cwd, int timeout_ms, const char *must_not, const char *must)
{
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, cwd, timeout_ms, r);
    int ok = !r->execution_failed && !r->timed_out && r->exit_code == 0;
    if (ok && (must_not || must)) {
        char *all = (char *)malloc(r->stdout_len + r->stderr_len + 1);
        if (all) {
            memcpy(all, r->stdout_buf, r->stdout_len);
            memcpy(all + r->stdout_len, r->stderr_buf, r->stderr_len);
            all[r->stdout_len + r->stderr_len] = '\0';
            if (must_not && strstr(all, must_not)) ok = 0;
            if (must && !strstr(all, must)) ok = 0;
            free(all);
        } else
            ok = 0;
    }
    free(r);
    return ok;
}

static void make_dir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static int cmake_output(const TASK_OPS_WORKSPACE *ws, char *buf, size_t size)
{
    buf[0] = '\0';
    int has = 0;
    for (int i = 0; i < ws->count; i++)
        has |= !strcmp(ws->files[i].rel, "CMakeLists.txt");
    char tmp[TASK_OPS_MAX_PATH], cmd[TASK_OPS_MAX_PATH * 3];
    temp_binary(tmp, sizeof(tmp));
    if (!has || strchr(tmp, '"') || strchr(ws->root, '"'))
        return 0;
    snprintf(cmd, sizeof(cmd), "cmake -S \"%s\" -B \"%s\"", ws->root, tmp);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 60000, r);
    int ran = !r->execution_failed && !r->timed_out;
    snprintf(buf, size, "%.*s%.*s", (int)(r->stdout_len < size / 2 ? r->stdout_len : size / 2 - 1), r->stdout_buf,
             (int)(r->stderr_len < size / 2 ? r->stderr_len : size / 2 - 1), r->stderr_buf);
    free(r);
    snprintf(cmd, sizeof(cmd), "cmake -E rm -rf \"%s\"", tmp);
    run_ok(cmd, ws->root, 30000, NULL, NULL);
    return ran;
}

static int build_evidence_plan(const TASK_OPS_WORKSPACE *ws, BUILD_EDIT *e)
{
    static char out[16384];
    if (!cmake_output(ws, out, sizeof(out)))
        return 0;
    const char *rels[TASK_OPS_MAX_FILES], *datas[TASK_OPS_MAX_FILES];
    for (int i = 0; i < ws->count; i++) {
        rels[i] = ws->files[i].rel;
        datas[i] = ws->files[i].data;
    }
    return BuildOpsPlanEvidence(rels, datas, ws->count, out, e);
}

/* Evidence by really building: cmake configure (+build) out of tree,
   make -n (dry run), or the build script in a temp copy of the workspace
   that must pass on the real source and fail on a broken one. */
static int build_verified(const TASK_OPS_WORKSPACE *after, const BUILD_EDIT *e)
{
    const char *text = NULL;
    for (int i = 0; i < after->count; i++)
        if (!strcmp(after->files[i].rel, e->rel))
            text = after->files[i].data;
    if (!BuildOpsIntent(text, e))
        return 0;
    if (!strcmp(e->rule, "ci_ctest"))
        return 1;
    char tmp[TASK_OPS_MAX_PATH], cmd[TASK_OPS_MAX_PATH * 3];
    temp_binary(tmp, sizeof(tmp));
    if (strchr(tmp, '"') || strchr(after->root, '"'))
        return 0;
    int ok = 0;
    if (!strncmp(e->rule, "cmake_", 6)) {
        snprintf(cmd, sizeof(cmd), "cmake -S \"%s\" -B \"%s\"", after->root, tmp);
        ok = run_ok(cmd, after->root, 60000, "No project() command", NULL);
        if (ok && strcmp(e->rule, "cmake_project") != 0) {
            snprintf(cmd, sizeof(cmd), "cmake --build \"%s\"", tmp);
            ok = run_ok(cmd, after->root, 120000, NULL, NULL);
        }
        snprintf(cmd, sizeof(cmd), "cmake -E rm -rf \"%s\"", tmp);
        run_ok(cmd, after->root, 30000, NULL, NULL);
    } else if (!strcmp(e->rule, "make_dep")) {
        ok = run_ok("make -n all", after->root, 20000, NULL, e->expect);
    } else if (!strcmp(e->rule, "build_compile_step")) {
        make_dir(tmp);
        int wrote = 1;
        for (int i = 0; i < after->count; i++)
            if (strchr(after->files[i].rel, '/') == NULL)   /* flat copy of top-level files */
                wrote &= write_file(tmp, after->files[i].rel, after->files[i].data);
        if (wrote && !strchr(e->rel, '"')) {
            snprintf(cmd, sizeof(cmd), "sh \"%s\"", e->rel);
            int good = run_ok(cmd, tmp, 30000, NULL, NULL);
            write_file(tmp, e->expect, "int broken(\n");
            int bad_fails = !run_ok(cmd, tmp, 30000, NULL, NULL);
            ok = good && bad_fails;
        }
        snprintf(cmd, sizeof(cmd), "cmake -E rm -rf \"%s\"", tmp);
        if (!run_ok(cmd, after->root, 30000, NULL, NULL)) {
            for (int i = 0; i < after->count; i++) {
                char path[TASK_OPS_MAX_PATH * 2];
                snprintf(path, sizeof(path), "%s/%s", tmp, after->files[i].rel);
                remove(path);
            }
            remove(tmp);
        }
    }
    return ok;
}

/* The one C source a c_fix rule applies to; -1 when none or ambiguous. */
static int cfix_target(const TASK_OPS_WORKSPACE *ws, const char *task, char **out,
                       char *rule, size_t rsz, char *detail, size_t dsz)
{
    int hit = -1;
    *out = NULL;
    for (int i = 0; i < ws->count; i++) {
        if (!is_c_source(ws->files[i].rel))
            continue;
        char r[32], d[128];
        char *o = CFixApply(ws->files[i].data, task, r, sizeof(r), d, sizeof(d));
        if (!o)
            continue;
        if (hit >= 0) {
            free(o);
            free(*out);
            *out = NULL;
            return -1;
        }
        hit = i;
        *out = o;
        snprintf(rule, rsz, "%s", r);
        snprintf(detail, dsz, "%s", d);
    }
    return hit;
}

/* The one C source a split request applies to; the new files must not exist. */
static int cfix_split_target(const TASK_OPS_WORKSPACE *ws, const char *task, CFIX_SPLIT *out)
{
    int hit = -1;
    for (int i = 0; i < ws->count; i++) {
        if (!is_c_source(ws->files[i].rel) || strchr(ws->files[i].rel, '/'))
            continue;
        CFIX_SPLIT s;
        if (!CFixSplit(ws->files[i].data, ws->files[i].rel, task, &s))
            continue;
        if (hit >= 0 || ws_find_named(ws, s.c_rel) >= 0 || ws_find_named(ws, s.h_rel) >= 0) {
            CFixSplitFree(&s);
            CFixSplitFree(out);
            return -1;
        }
        hit = i;
        *out = s;
    }
    return hit;
}

/* ------------------------------------------------------ evidence_fix
   Trigger: the program builds and its own run fails (non-zero exit, not a
   timeout). Every candidate single edit from CFixCandidates is written,
   built and run; the lowest tier with exactly one candidate that makes the
   program exit 0 wins. Several winners in that tier: abstain. Test files
   are never edited. Task wording is not consulted. */
#define EV_MAX_CANDS 64
/* gcc -Wall -Wextra -O1 diagnostics for the whole program (stderr text) */
static void warn_text(const TASK_OPS_WORKSPACE *ws, const char *flags, char *buf, size_t size)
{
    char cmd[4096], srcs[3072], bin[TASK_OPS_MAX_PATH];
    buf[0] = '\0';
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return;
    temp_binary(bin, sizeof(bin));
    snprintf(cmd, sizeof(cmd), "gcc %s-Wall -Wextra -O1 -o \"%s\" %s", flags, bin, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    snprintf(buf, size, "%.*s", (int)(r->stderr_len < size - 1 ? r->stderr_len : size - 1), r->stderr_buf);
    free(r);
    remove(bin);
}

/* compiler evidence: exactly one identifier gcc calls undeclared, with no
   "did you mean" hint anywhere, in exactly one file; that file gets the
   declare_local edit for it. Returns the file index or -1. */
static int undeclared_target(const TASK_OPS_WORKSPACE *ws, const char *flags, char **out, char *detail, size_t dsz)
{
    static char w[16384];
    *out = NULL;
    warn_text(ws, flags, w, sizeof(w));
    {   /* UTF-8 locales quote with U+2018/U+2019: fold them to ' */
        char *o = w;
        for (const char *p = w; *p;) {
            if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 && ((unsigned char)p[2] == 0x98 || (unsigned char)p[2] == 0x99)) {
                *o++ = '\'';
                p += 3;
            } else
                *o++ = *p++;
        }
        *o = '\0';
    }
    if (!w[0] || strstr(w, "did you mean"))
        return -1;
    char name[64] = "", file[TASK_OPS_MAX_PATH] = "";
    int names = 0;
    for (const char *p = strstr(w, "' undeclared (first use in this function)"); p; p = strstr(p + 1, "' undeclared (first use in this function)")) {
        const char *q = p;
        while (q > w && q[-1] != '\'' && q[-1] != '\n') q--;
        if (q == w || q[-1] != '\'' || p - q <= 0 || p - q >= 63)
            return -1;
        char nm[64];
        memcpy(nm, q, (size_t)(p - q));
        nm[p - q] = '\0';
        const char *ls = q;   /* file of this diagnostic: start of line up to ':' */
        while (ls > w && ls[-1] != '\n') ls--;
        const char *colon = strchr(ls, ':');
        if (!colon || colon > q || (size_t)(colon - ls) >= sizeof(file))
            return -1;
        char f[TASK_OPS_MAX_PATH];
        memcpy(f, ls, (size_t)(colon - ls));
        f[colon - ls] = '\0';
        if (!names) {
            snprintf(name, sizeof(name), "%s", nm);
            snprintf(file, sizeof(file), "%s", f);
            names = 1;
        } else if (strcmp(name, nm) || strcmp(file, f))
            return -1;   /* several names or files: abstain */
    }
    if (!names)
        return -1;
    for (int i = 0; i < ws->count; i++) {
        const char *rel = ws->files[i].rel;
        size_t rl = strlen(rel), fl = strlen(file);
        if (!is_c_source(rel) || !strncmp(rel, "test", 4) || strstr(rel, "/test") || fl < rl || strcmp(file + fl - rl, rel) ||
            (fl > rl && file[fl - rl - 1] != '/' && file[fl - rl - 1] != '\\'))
            continue;
        char *o = CFixDeclareUndeclared(ws->files[i].data, name, detail, dsz);
        if (!o)
            return -1;
        *out = o;
        return i;
    }
    return -1;
}

/* 1-based line range of main()'s definition, head through closing brace
   (Allman heads included); lo > hi when there is none */
static void main_body_lines(const char *src, int *lo, int *hi)
{
    int ln = 1, depth = 0, head = 0, open = 0;
    *lo = 0;
    *hi = -1;
    for (const char *line = src; line && *line; ln++) {
        const char *nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) : strlen(line);
        const char *semi = memchr(line, ';', ll), *brace = memchr(line, '{', ll);
        if (depth == 0 && !head && !open && line_defines_main(line, ll) && (!semi || (brace && brace < semi))) {
            head = 1;
            *lo = ln;
        }
        for (size_t q = 0; q < ll; q++) {
            if (line[q] == '{') { depth++; if (head) { open = 1; head = 0; } }
            else if (line[q] == '}' && depth > 0) depth--;
        }
        if (open && depth == 0) {
            *hi = ln;
            return;
        }
        line = nl ? nl + 1 : NULL;
    }
    if (*lo) *hi = ln;   /* unterminated: to the end */
}

/* does 1-based line ln of src hold a return statement? */
static int line_has_return(const char *src, int ln)
{
    const char *line = src;
    for (int i = 1; i < ln && line; i++) {
        line = strchr(line, '\n');
        if (line) line++;
    }
    if (!line) return 0;
    const char *nl = strchr(line, '\n');
    size_t ll = nl ? (size_t)(nl - line) : strlen(line);
    for (size_t i = 0; i + 6 <= ll; i++)
        if (!strncmp(line + i, "return", 6) && (i == 0 || !ident_char((unsigned char)line[i - 1])) &&
            (i + 6 == ll || !ident_char((unsigned char)line[i + 6])))
            return 1;
    return 0;
}

/* does src define a function other than main (a depth-0 head with "(" and
   no ";" whose body brace follows on that line or the next)? */
static int defines_other_fn(const char *src)
{
    int depth = 0, pend = 0;
    for (const char *line = src; line && *line;) {
        const char *nl = strchr(line, '\n');
        size_t ll = nl ? (size_t)(nl - line) : strlen(line);
        const char *t = line;
        while (t < line + ll && isspace((unsigned char)*t)) t++;
        int brace_first = t < line + ll && *t == '{';
        if (depth == 0 && pend && brace_first)
            return 1;
        pend = 0;
        if (depth == 0 && t < line + ll && *t != '#' && memchr(line, '(', ll) && !memchr(line, ';', ll) &&
            !line_defines_main(line, ll)) {
            const char *ob = memchr(line, '{', ll);
            if (ob && ob > (const char *)memchr(line, '(', ll))
                return 1;
            if (!ob)
                pend = 1;
        }
        for (size_t q = 0; q < ll; q++)
            depth += line[q] == '{' ? 1 : (line[q] == '}' && depth > 0) ? -1 : 0;
        line = nl ? nl + 1 : NULL;
    }
    return 0;
}

/* does the program take input the probe run cannot supply (argv use,
   stdin reads)? Then exit 0 on the bare run is not the task's criterion. */
static int reads_input(const TASK_OPS_WORKSPACE *ws)
{
    static const char *const pats[] = {"argv[", "argc", "scanf(", "getchar(", "stdin", "getline(", "read(0", "getenv(", NULL};
    for (int i = 0; i < ws->count; i++) {
        if (!is_c_source(ws->files[i].rel))
            continue;
        for (int k = 0; pats[k]; k++)
            if (strstr(ws->files[i].data, pats[k]))
                return 1;
    }
    return 0;
}

/* other evidence the bare run does not cover: test sources, build or test
   scripts, or more than one main (several programs) */
static int other_criteria(const TASK_OPS_WORKSPACE *ws)
{
    int mains = 0;
    for (int i = 0; i < ws->count; i++) {
        const char *rel = ws->files[i].rel, *base = strrchr(rel, '/');
        base = base ? base + 1 : rel;
        if (!strncmp(base, "test", 4) || strstr(rel, "tests/") || !strcmp(base, "Makefile") || !strcmp(base, "CMakeLists.txt") ||
            ShellOpsIsScript(rel, ws->files[i].data) || (strlen(base) > 3 && !strcmp(base + strlen(base) - 3, ".py")))
            return 1;
        if (is_c_source(rel)) {
            const char *d = ws->files[i].data;
            for (const char *line = d; line && *line;) {
                const char *nl = strchr(line, '\n');
                size_t ll = nl ? (size_t)(nl - line) : strlen(line);
                const char *semi = memchr(line, ';', ll), *brace = memchr(line, '{', ll);
                if (line_defines_main(line, ll) && (!semi || (brace && brace < semi)))
                    mains++;   /* a definition, not a prototype */
                line = nl ? nl + 1 : NULL;
            }
        }
    }
    return mains != 1;
}

static int evidence_search(const TASK_OPS_WORKSPACE *ws, const char *flags, int run_before, int *file, char **out,
                           char *rule, size_t rsz, char *detail, size_t dsz, int *tried)
{
    /* The bare run fails (non-zero exit) and that run is the only criterion
       in sight: no input it cannot supply, no tests, scripts or second
       program. Every candidate single edit is built and run; the edit is
       kept only when it is the one candidate in ANY tier that makes the
       program exit 0, and the program's stdout is unchanged (a changed
       output is behavior this run cannot judge). A passing run is never
       edited (a warning alone is not evidence of the task). */
    *file = -1;
    *out = NULL;
    *tried = 0;
    if (run_before == 0 || reads_input(ws) || other_criteria(ws))
        return 0;
    static char out0[1024], out1[1024];
    snprintf(out0, sizeof(out0), "%s", probe_stdout[0]);
    int wins = 0, others = 0;
    for (int f = 0; f < ws->count; f++)
        if (is_c_source(ws->files[f].rel) && !strncmp(ws->files[f].rel + strlen(ws->files[f].rel) - 2, ".c", 2))
            others |= defines_other_fn(ws->files[f].data);
    for (int f = 0; f < ws->count; f++) {
        const TASK_OPS_FILE *F = &ws->files[f];
        if (!is_c_source(F->rel) || !strncmp(F->rel, "test", 4) || strstr(F->rel, "/test") || F->len > 65536)
            continue;
        int main_lo = 0, main_hi = -1;
        main_body_lines(F->data, &main_lo, &main_hi);
        static CFIX_CAND c[EV_MAX_CANDS];
        int n = CFixCandidates(F->data, c, EV_MAX_CANDS);
        for (int k = 0; k < n; k++) {
            int line = atoi(c[k].detail + 5);   /* "line N: ..." */
            if (line >= main_lo && line <= main_hi && (others || line_has_return(F->data, line)))
                continue;   /* main is the oracle: never edited when other functions exist, and its return never */
            if (!write_file(ws->root, F->rel, c[k].text))
                continue;
            int cc = -1, rr = -1;
            out1[0] = '\0';
            probe_out(ws, flags, &cc, &rr, out1);
            (*tried)++;
            if (cc == 1 && rr == 0) {
                wins++;
                if (wins == 1 && !strcmp(out0, out1)) {
                    *file = f;
                    *out = c[k].text;
                    c[k].text = NULL;
                    snprintf(rule, rsz, "%s", c[k].rule);
                    snprintf(detail, dsz, "%s", c[k].detail);
                } else if (wins == 1)
                    wins = 99;   /* output changed: unjudgeable, abstain */
            }
        }
        CFixCandidatesFree(c, n);
        write_file(ws->root, F->rel, F->data);   /* always restore */
    }
    if (wins != 1) {
        free(*out);
        *out = NULL;
        *file = -1;
        return wins > 1 ? wins : 0;
    }
    return 1;
}



/* shell_contract: run the script (with text in place of file idx) in a
   throwaway copy with the stated invocation; 1 when every stated
   expectation holds */
static int shc_check(const TASK_OPS_WORKSPACE *ws, int idx, const char *text, const SH_CONTRACT *c)
{
    char dir[TASK_OPS_MAX_PATH], cmd[TASK_OPS_MAX_PATH * 4];
    int ok = 0;
    temp_binary(dir, sizeof(dir));
    strncat(dir, "_shc", sizeof(dir) - strlen(dir) - 1);
    if (strchr(dir, '"'))
        return 0;
    make_dir(dir);
    for (int i = 0; i < ws->count; i++) {
        char sub[TASK_OPS_MAX_PATH * 2];
        const char *r = ws->files[i].rel;
        for (const char *q = strchr(r, '/'); q; q = strchr(q + 1, '/')) {
            snprintf(sub, sizeof(sub), "%s/%.*s", dir, (int)(q - r), r);
            make_dir(sub);
        }
        if (!write_file(dir, r, i == idx ? text : ws->files[i].data))
            goto out;
    }
    {
        size_t o = 0;
#ifdef _WIN32
        o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, "sh \"%s\"", c->script);
#else
        o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, "HOME=\"%s\" sh \"%s\"", dir, c->script);
#endif
        for (int a = 0; a < c->nargs && o < sizeof(cmd); a++)
            o += (size_t)snprintf(cmd + o, sizeof(cmd) - o, " \"%s\"", c->args[a]);
#ifndef _WIN32
        if (o < sizeof(cmd)) snprintf(cmd + o, sizeof(cmd) - o, " </dev/null");
#endif
    }
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        goto out;
    AgentShellResultInit(r);
    AgentShellExec(cmd, dir, 3000, r);
    if (!r->execution_failed && !r->timed_out) {
        ok = 1;
        if (c->exit_want == -2) ok = r->exit_code != 0;
        else if (c->exit_want >= 0) ok = r->exit_code == c->exit_want;
        if (ok && c->has_out) {
            size_t n = r->stdout_len;
            while (n && (r->stdout_buf[n - 1] == '\n' || r->stdout_buf[n - 1] == '\r')) n--;
            ok = n == strlen(c->out) && !strncmp(r->stdout_buf, c->out, n);
        }
        if (ok && c->has_file) {
            char p[TASK_OPS_MAX_PATH * 2], buf[128];
            snprintf(p, sizeof(p), "%s/%s", dir, c->file);
            FILE *f = fopen(p, "rb");
            ok = 0;
            if (f) {
                size_t n = fread(buf, 1, sizeof(buf) - 1, f);
                fclose(f);
                buf[n] = '\0';
                while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
                ok = !strcmp(buf, c->word);
            }
        }
    }
    free(r);
out:
    sc_rm_tree(dir, 0);
    return ok;
}

static SH_CONTRACT g_shc;
static int g_shc_idx = -1;

static int shell_contract_target(const TASK_OPS_WORKSPACE *ws, const char *task, char **next,
                                 char *rule, size_t rsz, char *detail, size_t dsz)
{
    const char *v = getenv("SYMBOLS_SHELL_RUN");
    g_shc_idx = -1;
    if ((v && !strcmp(v, "0")) || !ShellContractParse(task, &g_shc))
        return -1;
    int idx = -1;
    for (int i = 0; i < ws->count; i++)
        if (!strcmp(ws->files[i].rel, g_shc.script))
            idx = i;
    if (idx < 0 || ws->files[idx].len > 65536 || shc_check(ws, idx, ws->files[idx].data, &g_shc))
        return -1;   /* no such script, or it already meets the contract */
    static SH_CAND c[48];
    int n = ShellContractCandidates(ws->files[idx].data, &g_shc, c, 48), best = -1, tier = 99, dup = 0;
    for (int k = 0; k < n; k++) {
        if (c[k].tier > tier || !shc_check(ws, idx, c[k].text, &g_shc))
            continue;
        if (c[k].tier < tier) { tier = c[k].tier; best = k; dup = 0; }
        else dup = 1;
    }
    if (best >= 0 && !dup) {
        next[idx] = c[best].text;
        c[best].text = NULL;
        snprintf(rule, rsz, "%s", c[best].rule);
        snprintf(detail, dsz, "%s", c[best].detail);
        g_shc_idx = idx;
    } else
        idx = -1;
    ShellContractFree(c, n);
    return idx;
}

/* last compile_repair attempt, for the abstain shape: candidates generated
   and candidates that built (-1 = not attempted) */
static int g_cr_cands = -1, g_cr_built = -1;

/* compile_repair: the first compiler (or linker) error yields single-edit
   candidates; each is compiled on a scratch copy outside the tree, and only
   the one candidate of the lowest tier that builds (and, when the program
   runs, exits 0) is kept. */
static int compile_repair_target(const TASK_OPS_WORKSPACE *ws, const char *flags, char **next,
                                 char *rule, size_t rsz, char *detail, size_t dsz)
{
    char cmd[4096], srcs[3072], bin[TASK_OPS_MAX_PATH];
    char *diag = NULL;
    int touched = 0;
    g_cr_cands = g_cr_built = -1;
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return 0;
    temp_binary(bin, sizeof(bin));
    snprintf(cmd, sizeof(cmd), "gcc %s-o \"%s\" %s", flags, bin, srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return 0;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    if (!r->execution_failed && r->exit_code != 0) {
        diag = (char *)malloc(r->stderr_len + r->stdout_len + 1);
        if (diag) {
            memcpy(diag, r->stderr_buf, r->stderr_len);
            memcpy(diag + r->stderr_len, r->stdout_buf, r->stdout_len);
            diag[r->stderr_len + r->stdout_len] = '\0';
        }
    }
    free(r);
    remove(bin);
    if (!diag)
        return 0;
    CR_CAND *c = (CR_CAND *)calloc(32, sizeof(CR_CAND));
    TASK_OPS_WORKSPACE *tw = (TASK_OPS_WORKSPACE *)malloc(sizeof(*tw));
    int n = c ? CompileRepairCandidates(ws, diag, c, 32) : 0;
    free(diag);
    g_cr_cands = n;
    g_cr_built = 0;
    int ok[32] = {0};
    for (int k = 0; k < n && tw; k++) {
        char dir[TASK_OPS_MAX_PATH];
        char *tmp[TASK_OPS_MAX_FILES] = {0};
        int wrote = 1, comp = -1, run = -1;
        memcpy(tw, ws, sizeof(*tw));
        temp_binary(dir, sizeof(dir));
        strncat(dir, "_cr", sizeof(dir) - strlen(dir) - 1);
        snprintf(tw->root, sizeof(tw->root), "%s", dir);
        make_dir(dir);
        for (int i = 0; i < ws->count; i++) {
            const char *data = ws->files[i].data;
            if (c[k].file == i)
                data = c[k].text;
            else if (c[k].file < 0 && count_token_in(data, c[k].from) > 0)
                data = tmp[i] = replace_token(data, c[k].from, c[k].to);
            tw->files[i].data = (char *)data;
            char sub[TASK_OPS_MAX_PATH * 2];
            const char *rr = ws->files[i].rel;
            for (const char *q = strchr(rr, '/'); q; q = strchr(q + 1, '/')) {
                snprintf(sub, sizeof(sub), "%s/%.*s", dir, (int)(q - rr), rr);
                make_dir(sub);
            }
            if (!data || !write_file(dir, rr, data))
                wrote = 0;
        }
        if (wrote)
            probe_out(tw, flags, &comp, &run, NULL);
        ok[k] = !(wrote && comp == 1) ? 0 : run <= 0 ? 1 : 2;   /* 2 = builds, run fails */
        sc_rm_tree(dir, 0);
        for (int i = 0; i < ws->count; i++)
            free(tmp[i]);
    }
    free(tw);
    for (int k = 0; k < n; k++)
        g_cr_built += ok[k] != 0;
    int best = -1, tier = 99, dup = 0;
    for (int k = 0; k < n; k++)
        if (ok[k]) {
            if (c[k].tier < tier) { tier = c[k].tier; best = k; dup = 0; }
            else if (c[k].tier == tier) dup = 1;
        }
    /* uniqueness is judged on building alone; the run only vetoes */
    if (best >= 0 && !dup && ok[best] == 1) {
        if (c[best].file >= 0) {
            next[c[best].file] = c[best].text;
            c[best].text = NULL;
            touched = 1;
        } else
            for (int i = 0; i < ws->count; i++)
                if (count_token_in(ws->files[i].data, c[best].from) > 0) {
                    next[i] = replace_token(ws->files[i].data, c[best].from, c[best].to);
                    touched++;
                }
        snprintf(rule, rsz, "%s", c[best].rule);
        snprintf(detail, dsz, "%s", c[best].detail);
    }
    if (c) CompileRepairFree(c, n);
    free(c);
    return touched;
}

enum { OP_TEST, OP_RENAME, OP_FRAGMENT, OP_LITERAL, OP_DECLARE, OP_FIXIT, OP_DOCSYNC, OP_DEADFN, OP_BRACE, OP_RELOP, OP_SHELL, OP_BUILD, OP_CFIX, OP_CREPAIR, OP_SHCONTRACT, OP_EVIDENCE, OP_COUNT };
static const char *const op_names[OP_COUNT] = {
    "author_test", "rename_symbol", "stated_fragment", "literal_to_constant", "declare_implicit", "compiler_fixit", "doc_sync", "remove_dead_function", "unmatched_brace", "relop_search", "shell_harden", "build_repair", "c_fix", "compile_repair", "shell_contract", "evidence_fix"
};

/* The run-based evidence search (a candidate edit kept because the bare
   program then exits 0) is opt-in: an exit code alone cannot tell the
   intended fix from another edit that also passes, and the blind batch
   showed one. SYMBOLS_EVIDENCE_RUN=1 turns it back on. */
static int evidence_run_enabled(void)
{
    const char *v = getenv("SYMBOLS_EVIDENCE_RUN");
    return v && v[0] && strcmp(v, "0") != 0;
}

static int mem_enabled(void)
{
    const char *v = getenv("SYMBOLS_TASK_OPS_MEMORY");
    return v && v[0] && strcmp(v, "0") != 0;
}

static unsigned long long fnv_add(unsigned long long h, const char *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void mem_key(const TASK_OPS_WORKSPACE *ws, const char *task, char *out, size_t out_size)
{
    unsigned long long h = 1469598103934665603ULL;
    h = fnv_add(h, task, strlen(task) + 1);
    for (int i = 0; i < ws->count; i++) {
        h = fnv_add(h, ws->files[i].rel, strlen(ws->files[i].rel) + 1);
        h = fnv_add(h, ws->files[i].data, ws->files[i].len);
    }
    snprintf(out, out_size, "%016llx", h);
}

static void mem_path(const char *root, char *out, size_t out_size, int dir_only)
{
    snprintf(out, out_size, "%s/.symbols%s", root, dir_only ? "" : "/task_ops_memory.tsv");
}

static void mem_recall(const char *root, const char *key, int skip[OP_COUNT], int net[OP_COUNT])
{
    memset(skip, 0, sizeof(int) * OP_COUNT);
    memset(net, 0, sizeof(int) * OP_COUNT);
    char path[TASK_OPS_MAX_PATH * 2], line[256];
    mem_path(root, path, sizeof(path), 0);
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *t1 = strchr(line, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (!t2)
            continue;
        *t1 = *t2 = '\0';
        int kept = !strncmp(t2 + 1, "kept", 4);
        for (int o = 0; o < OP_COUNT; o++)
            if (!strcmp(t1 + 1, op_names[o])) {
                net[o] += kept ? 1 : -1;
                if (!kept && !strcmp(line, key))
                    skip[o] = 1;
            }
    }
    fclose(f);
}

static void mem_record(const char *root, const char *key, const char *op, int kept)
{
    char path[TASK_OPS_MAX_PATH * 2];
    mem_path(root, path, sizeof(path), 1);
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
    mem_path(root, path, sizeof(path), 0);
    FILE *f = fopen(path, "ab");
    if (!f)
        return;
    fprintf(f, "%s\t%s\t%s\n", key, op, kept ? "kept" : "rolled_back");
    fclose(f);
}

static int write_file(const char *root, const char *rel, const char *data)
{
    char path[TASK_OPS_MAX_PATH * 2];
    snprintf(path, sizeof(path), "%s/%s", root, rel);
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    size_t n = strlen(data);
    int ok = fwrite(data, 1, n, f) == n;
    return (fclose(f) == 0) && ok;
}

/* "no operator preconditions hold" plus a fixed-format workspace shape:
   only 0/1/-1 flags, never names, so the reason stays safe to count on a
   blind bank. c = compile probe (-1 none, 0 fail, 1 ok), run = probe exit
   (-1 not run, 0 ok, 1 nonzero), then presence of shell scripts, build
   files, docs, tests and a git repository. */

/* Fixed-vocabulary class of the first compiler error when the workspace
   does not compile (name-free, safe to count on a blind bank): the first
   "error:" line of gcc -fsyntax-only, or "link" when syntax passes. nerr is
   bucketed 1 / 2 (2-3) / 4 (4+); nc is 1 or 2 (several .c files). */
static const char *diag_class_of(const char *line)
{
    static const struct { const char *needle, *cls; } map[] = {
        {"No such file", "missing_header"}, {"implicit declaration", "implicit"},
        {"undeclared", "undeclared"}, {"unknown type name", "unknown_type"},
        {"conflicting types", "conflicting_types"}, {"too few arguments", "arity"},
        {"too many arguments", "arity"}, {"has no member", "no_member"},
        {"redefinition", "redefinition"}, {"incompatible", "incompatible"},
        {"invalid operands", "invalid_operands"}, {"lvalue", "lvalue"},
        {"storage size", "incomplete_type"}, {"incomplete type", "incomplete_type"},
        {"expected", "expected"}, {"return", "return"},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strstr(line, map[i].needle))
            return map[i].cls;
    return "other";
}

static void diag_shape(const TASK_OPS_WORKSPACE *ws, char *out, size_t size)
{
    char srcs[3072], cmd[3200];
    int nc = 0, nerr = 0;
    const char *cls = "none";
    out[0] = '\0';
    for (int i = 0; i < ws->count; i++) {
        size_t n = strlen(ws->files[i].rel);
        nc += n > 2 && !strcmp(ws->files[i].rel + n - 2, ".c");
    }
    if (c_sources(ws, srcs, sizeof(srcs)) == 0)
        return;
    snprintf(cmd, sizeof(cmd), "gcc -fsyntax-only %s", srcs);
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return;
    AgentShellResultInit(r);
    AgentShellExec(cmd, ws->root, 20000, r);
    if (!r->execution_failed && !r->timed_out) {
        if (r->exit_code == 0)
            cls = "link";
        else {
            const char *b = r->stderr_buf;
            for (const char *p = b; p && *p; ) {
                const char *e = strchr(p, '\n');
                size_t len = e ? (size_t)(e - p) : strlen(p);
                char line[512];
                snprintf(line, sizeof(line), "%.*s", (int)(len < 511 ? len : 511), p);
                if (strstr(line, "error:") || strstr(line, "fatal error:")) {
                    if (nerr++ == 0)
                        cls = diag_class_of(strstr(line, "error:"));
                }
                p = e ? e + 1 : NULL;
            }
        }
    }
    free(r);
    char crs[32] = "";
    if (g_cr_cands >= 0)   /* compile_repair: candidates / built, bucketed 0, 1, 2+ */
        snprintf(crs, sizeof(crs), " cr=%d cb=%d", g_cr_cands > 1 ? 2 : g_cr_cands, g_cr_built > 1 ? 2 : g_cr_built);
    snprintf(out, size, " [diag=%s nerr=%d nc=%d%s]", cls, cls[0] == 'l' || nerr < 2 ? 1 : nerr < 4 ? 2 : 4, nc > 1 ? 2 : 1, crs);
}

static void abstain_shape(const TASK_OPS_WORKSPACE *ws, TASK_OPS_REPORT *rep)
{
    int sh = 0, mk = 0, doc = 0, test = 0, git = 0, i;
    char head[TASK_OPS_MAX_PATH + 16];
    FILE *f;
    for (i = 0; i < ws->count; i++) {
        const char *rel = ws->files[i].rel;
        const char *base = strrchr(rel, '/');
        size_t n = strlen(rel);
        base = base ? base + 1 : rel;
        if ((n > 3 && !strcmp(rel + n - 3, ".sh")) || !strncmp(ws->files[i].data, "#!/bin/sh", 9) ||
            !strncmp(ws->files[i].data, "#!/usr/bin/env bash", 19) || !strncmp(ws->files[i].data, "#!/bin/bash", 11))
            sh = 1;
        if (!strcmp(base, "Makefile") || !strcmp(base, "CMakeLists.txt"))
            mk = 1;
        if (is_doc_file(rel))
            doc = 1;
        if (!strncmp(base, "test", 4) || strstr(rel, "tests/"))
            test = 1;
    }
    snprintf(head, sizeof(head), "%s/.git/HEAD", ws->root);
    if ((f = fopen(head, "rb")) != NULL) {
        git = 1;
        fclose(f);
    }
    char diag[96] = "";
    if (rep->compile_before == 0)
        diag_shape(ws, diag, sizeof(diag));
    snprintf(rep->reason, sizeof(rep->reason),
             "no operator preconditions hold [c=%d run=%d sh=%d mk=%d doc=%d test=%d git=%d]%s",
             rep->compile_before < 0 ? -1 : rep->compile_before > 0,
             rep->run_before < 0 ? -1 : rep->run_before != 0, sh, mk, doc, test, git, diag);
}

int TaskOpsSolve(const char *workspace, const char *task, TASK_OPS_REPORT *rep)
{
    g_cr_cands = g_cr_built = -1;
    TASK_OPS_REPORT local;
    if (!rep)
        rep = &local;
    memset(rep, 0, sizeof(*rep));
    rep->compile_before = rep->compile_after = -1;
    rep->run_before = rep->run_after = -1;
    if (!workspace || !task)
        return 0;

    /* repository state first: a merge in progress or a broken HEAD is a git
       problem, and file edits must not paper over it */
    GIT_OPS_RESULT gres;
    int grc = GitOpsSolve(workspace, task, &gres);
    if (grc >= 0) {
        snprintf(rep->op, sizeof(rep->op), "%s", gres.op);
        snprintf(rep->detail, sizeof(rep->detail), "%s", gres.detail);
        snprintf(rep->reason, sizeof(rep->reason), "%s", gres.reason);
        rep->candidates = 1;
        rep->applied = grc == 1;
        rep->verified = grc == 1;
        return grc == 1;
    }

    TASK_OPS_WORKSPACE *ws = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*ws));
    char **next = (char **)calloc(TASK_OPS_MAX_FILES, sizeof(char *));
    NEW_FILE created, created2;   /* created2: a c_fix split's header */
    memset(&created, 0, sizeof(created));
    memset(&created2, 0, sizeof(created2));
    CFIX_SPLIT split;
    memset(&split, 0, sizeof(split));
    if (!ws || !next) { free(ws); free(next); return 0; }
    TaskOpsLoadWorkspace(workspace, ws);

    char flags[512];
    task_flags(task, flags, sizeof(flags));
    probe_out(ws, flags, &rep->compile_before, &rep->run_before, probe_stdout[0]);
    trace_begin(ws, flags);

    /* recall (learn step, opt-in) */
    int use_mem = mem_enabled(), skip[OP_COUNT] = {0}, net[OP_COUNT] = {0}, order[OP_COUNT];
    int shell_file = -1;
    BUILD_EDIT bedit;
    char cfix_rule[32] = "", cfix_detail[128] = "";
    int cfix_file = -1;
    memset(&bedit, 0, sizeof(bedit));
    char *next_shell = NULL, shell_rule[32] = "", shell_detail[128] = "";
    char key[32] = {0};
    for (int o = 0; o < OP_COUNT; o++)
        order[o] = o;
    if (use_mem) {
        mem_key(ws, task, key, sizeof(key));
        mem_recall(ws->root, key, skip, net);
        for (int i = 1; i < OP_COUNT; i++)   /* stable insertion sort by net success */
            for (int j = i; j > 0 && net[order[j]] > net[order[j - 1]]; j--) {
                int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
                rep->memory_reordered = 1;
            }
    }
    {   /* evidence_fix is the last resort: every wording-anchored operator
           (and a relop abstention on a demoted pattern) comes first */
        int w = 0;
        for (int o = 0; o < OP_COUNT; o++)
            if (order[o] != OP_EVIDENCE) order[w++] = order[o];
        order[w] = OP_EVIDENCE;
    }
    if (rep->compile_before == 0) {
        /* verified single-edit repairs from the workspace outrank the
           compiler's own guesses (which may point at a library name) */
        int fx = -1, cr = -1;
        for (int o = 0; o < OP_COUNT; o++) {
            if (order[o] == OP_FIXIT) fx = o;
            if (order[o] == OP_CREPAIR) cr = o;
        }
        if (fx >= 0 && cr > fx) {
            for (int o = cr; o > fx; o--) order[o] = order[o - 1];
            order[fx] = OP_CREPAIR;
        }
    }

    /* reason: first operator (in that order) whose preconditions hold */
    char a[128] = {0}, b[128] = {0};
    int touched = 0;
    int renames = TaskOpsFindRename(ws, task, a, sizeof(a), b, sizeof(b));
    char y_text[128] = {0};
    int uses_before = 0, lit_file = -1, frags = 0, docs = 0;
    char da[128] = {0}, db[128] = {0}, dead[128] = {0};
    int deads = 0, relops_found = 0, evidence_wins = 0, evidence_tried = 0;
    RELOP_HIT rhit;
    TEST_PLAN tplan;
    LIT_PLAN lit;
    char *lit_out = NULL;
    FRAG_HIT hit;
    rep->candidates = renames;
    int crn = 0, crt = 0;
    for (int oi = 0; oi < OP_COUNT && !touched; oi++) {
        int op = order[oi];
        if (skip[op]) {
            rep->memory_skipped++;
            continue;
        }
        if (op == OP_RENAME && renames == 1) {
            snprintf(rep->op, sizeof(rep->op), "rename_symbol");
            for (int i = 0; i < ws->count; i++)
                if (count_token_in(ws->files[i].data, a) > 0) {
                    next[i] = replace_token(ws->files[i].data, a, b);
                    touched++;
                }
            snprintf(rep->detail, sizeof(rep->detail), "%.100s -> %.100s in %d file(s)", a, b, touched);
        } else if (op == OP_FRAGMENT &&
                   (frags = find_fragment_edit(ws, task, y_text, sizeof(y_text), &hit)) == 1) {
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "stated_fragment");
            const TASK_OPS_FILE *f = &ws->files[hit.file];
            size_t yl = strlen(y_text);
            char *out = (char *)malloc(f->len - (hit.to - hit.from) + yl + 1);
            if (out) {
                memcpy(out, f->data, hit.from);
                memcpy(out + hit.from, y_text, yl);
                memcpy(out + hit.from + yl, f->data + hit.to, f->len - hit.to + 1);
                next[hit.file] = out;
                touched = 1;
            }
            snprintf(rep->detail, sizeof(rep->detail), "'%.*s' -> '%.100s' in %.100s",
                     (int)(hit.to - hit.from > 80 ? 80 : hit.to - hit.from), f->data + hit.from,
                     y_text, f->rel);
        } else if (op == OP_LITERAL &&
                   find_literal_plan(ws, task, &lit) == 1 &&
                   (lit_out = apply_literal_plan(ws, &lit, &lit_file)) != NULL) {
            next[lit_file] = lit_out;
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "literal_to_constant");
            snprintf(rep->detail, sizeof(rep->detail), "%s%.60s%s -> %.60s in %.60s", lit.is_string ? "\"" : "",
                     lit.lit, lit.is_string ? "\"" : "", lit.name, ws->files[lit_file].rel);
        } else if (op == OP_DECLARE && rep->compile_before >= 0 &&
                   (touched = declare_implicit(ws, flags, task, next, &created, rep->detail, sizeof(rep->detail), &uses_before)) > 0) {
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "declare_implicit");
        } else if (op == OP_FIXIT && rep->compile_before == 0) {
            int fixes = 0;
            touched = compiler_fixits(ws, flags, next, &fixes);
            if (touched) {
                rep->candidates = 1;
                snprintf(rep->op, sizeof(rep->op), "compiler_fixit");
                snprintf(rep->detail, sizeof(rep->detail), "%d fix-it(s) in %d file(s)", fixes, touched);
            }
        } else if (op == OP_DOCSYNC && (docs = find_doc_sync(ws, task, da, sizeof(da), db, sizeof(db))) == 1) {
            int sh = ds_shape(da);
            for (int i = 0; i < ws->count; i++)
                if (is_doc_file(ws->files[i].rel) && ds_in_file(ws->files[i].data, da, sh) > 0) {
                    next[i] = sh == DS_IDENT ? replace_token(ws->files[i].data, da, db) : ds_replace(ws->files[i].data, da, db);
                    touched++;
                }
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "doc_sync");
            snprintf(rep->detail, sizeof(rep->detail), "%.100s -> %.100s in %d doc file(s)", da, db, touched);
        } else if (op == OP_DEADFN && (deads = find_dead_function(ws, task, dead, sizeof(dead))) == 1) {
            for (int i = 0; i < ws->count; i++)
                if (is_c_source(ws->files[i].rel) && count_token_in(ws->files[i].data, dead) > 0) {
                    next[i] = df_apply(ws->files[i].data, dead);
                    touched++;
                }
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "remove_dead_function");
            snprintf(rep->detail, sizeof(rep->detail), "%.100s removed from %d file(s)", dead, touched);
        } else if (op == OP_SHELL && ((shell_file = shell_target(ws, task, &next_shell, shell_rule, sizeof(shell_rule), shell_detail, sizeof(shell_detail))) >= 0 ||
                                      (shell_file = shell_syntax_target(ws, &next_shell, shell_rule, sizeof(shell_rule), shell_detail, sizeof(shell_detail))) >= 0)) {
            next[shell_file] = next_shell;
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "shell_harden");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.60s in %.120s", shell_rule, shell_detail, ws->files[shell_file].rel);
        } else if (op == OP_BUILD && (build_plan(ws, task, &bedit) || build_evidence_plan(ws, &bedit))) {
            if (bedit.file >= 0)
                next[bedit.file] = bedit.text;
            else {
                snprintf(created.rel, sizeof(created.rel), "%s", bedit.rel);
                created.data = bedit.text;
            }
            bedit.text = NULL;   /* owned by next[] / created now */
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "build_repair");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.120s in %.100s", bedit.rule, bedit.detail, bedit.rel);
        } else if (op == OP_CFIX && (cfix_file = cfix_target(ws, task, &next_shell, cfix_rule, sizeof(cfix_rule), cfix_detail, sizeof(cfix_detail))) >= 0) {
            next[cfix_file] = next_shell;
            next_shell = NULL;
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "c_fix");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.80s in %.120s", cfix_rule, cfix_detail, ws->files[cfix_file].rel);
        } else if (op == OP_CREPAIR && rep->compile_before == 0 &&
                   named_files_exist(ws, ws, task, &crn, &crt) &&   /* a named file it cannot create */
                   (touched = compile_repair_target(ws, flags, next, cfix_rule, sizeof(cfix_rule), cfix_detail, sizeof(cfix_detail))) > 0) {
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "compile_repair");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.160s", cfix_rule, cfix_detail);
        } else if (op == OP_SHCONTRACT &&
                   (shell_file = shell_contract_target(ws, task, next, shell_rule, sizeof(shell_rule), shell_detail, sizeof(shell_detail))) >= 0) {
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "shell_contract");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.80s in %.100s", shell_rule, shell_detail, ws->files[shell_file].rel);
        } else if (op == OP_CFIX && (cfix_file = cfix_split_target(ws, task, &split)) >= 0) {
            next[cfix_file] = split.new_src;
            snprintf(created.rel, sizeof(created.rel), "%s", split.c_rel);
            created.data = split.c_text;
            snprintf(created2.rel, sizeof(created2.rel), "%s", split.h_rel);
            created2.data = split.h_text;
            split.new_src = split.c_text = split.h_text = NULL;   /* owned by next[] / created now */
            touched = 1;
            rep->candidates = 1;
            snprintf(cfix_rule, sizeof(cfix_rule), "split_function");
            snprintf(rep->op, sizeof(rep->op), "c_fix");
            snprintf(rep->detail, sizeof(rep->detail), "split_function: %.140s", split.detail);
        } else if (op == OP_TEST && find_test_plan(ws, task, &tplan)) {
            size_t cap = 512 + strlen(tplan.call);
            created.data = (char *)malloc(cap);
            if (created.data) {
                snprintf(created.rel, sizeof(created.rel), "%s", tplan.test_rel);
                snprintf(created.data, cap, "#include \"%s\"\n\nint main(void)\n{\n    return %s == %ld ? 0 : 1;\n}\n",
                         tplan.header, tplan.call, tplan.expect);
                touched = 1;
                rep->candidates = 1;
                snprintf(rep->op, sizeof(rep->op), "author_test");
                snprintf(rep->detail, sizeof(rep->detail), "%.60s: %.120s == %ld", tplan.test_rel, tplan.call, tplan.expect);
            }
        } else if (op == OP_EVIDENCE && relops_found >= 0 && rep->compile_before == 0 &&
                   (cfix_file = undeclared_target(ws, flags, &next_shell, cfix_detail, sizeof(cfix_detail))) >= 0) {
            next[cfix_file] = next_shell;
            next_shell = NULL;
            touched = 1;
            rep->candidates = 1;
            snprintf(cfix_rule, sizeof(cfix_rule), "undeclared_local");
            snprintf(rep->op, sizeof(rep->op), "evidence_fix");
            snprintf(rep->detail, sizeof(rep->detail), "undeclared_local: %.80s in %.100s (gcc: undeclared)", cfix_detail, ws->files[cfix_file].rel);
        } else if (op == OP_EVIDENCE && evidence_run_enabled() && relops_found >= 0 && rep->compile_before == 1 &&
                   rep->run_before >= 0 && rep->run_before != 124 &&
                   (evidence_wins = evidence_search(ws, flags, rep->run_before, &cfix_file, &next_shell, cfix_rule, sizeof(cfix_rule),
                                                    cfix_detail, sizeof(cfix_detail), &evidence_tried)) == 1) {
            next[cfix_file] = next_shell;
            next_shell = NULL;
            touched = 1;
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "evidence_fix");
            snprintf(rep->detail, sizeof(rep->detail), "%s: %.80s in %.100s (%d candidates run)", cfix_rule, cfix_detail,
                     ws->files[cfix_file].rel, evidence_tried);
        } else if (op == OP_RELOP &&
                   (relops_found = relop_search(ws, flags, task, &rhit)) == 1) {
            const TASK_OPS_FILE *F = &ws->files[rhit.file];
            size_t tl = strlen(rhit.to);
            char *out = (char *)malloc(F->len - rhit.len + tl + 1);
            if (out) {
                memcpy(out, F->data, rhit.at);
                memcpy(out + rhit.at, rhit.to, tl);
                memcpy(out + rhit.at + tl, F->data + rhit.at + rhit.len, F->len - rhit.at - rhit.len + 1);
                next[rhit.file] = out;
                touched = 1;
                rep->candidates = 1;
                snprintf(rep->op, sizeof(rep->op), "relop_search");
                snprintf(rep->detail, sizeof(rep->detail), "'%.*s' -> '%s' in %.100s [%s%s]", (int)rhit.len, F->data + rhit.at,
                         rhit.to, F->rel, rhit.pat, iop_status(rhit.pat) > 0 ? " induced" : "");
            }
        } else if (op == OP_BRACE && rep->compile_before == 0) {
            int bf = -1;
            size_t bat = 0;
            if (find_unmatched_brace(ws, &bf, &bat) == 1 && (next[bf] = ub_apply(ws->files[bf].data, bat)) != NULL) {
                touched = 1;
                rep->candidates = 1;
                snprintf(rep->op, sizeof(rep->op), "unmatched_brace");
                snprintf(rep->detail, sizeof(rep->detail), "stray '}' removed in %.100s", ws->files[bf].rel);
            }
        }
    }
    if (!touched) {
        if (evidence_wins > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d single edits make the failing program exit 0", evidence_wins);
        else if (relops_found > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d relational swaps make it exit 0", relops_found);
        else if (relops_found < 0)
            snprintf(rep->reason, sizeof(rep->reason), "abstain: every verified swap matches a demoted induced pattern");
        else if (frags > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d places fit the stated fragment", frags);
        else if (deads > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d dead functions named", deads);
        else if (docs > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d doc terms fit", docs);
        else if (renames > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d rename candidates", renames);
        else
            abstain_shape(ws, rep);
        trace_attempt(ws, flags, rep);
        free(created.data);
        free(created2.data);
        TaskOpsFreeWorkspace(ws);
        free(ws);
        free(next);
        return 0;
    }

    /* act */
    int ok = 1;
    for (int i = 0; i < ws->count; i++)
        if (next[i] && !write_file(ws->root, ws->files[i].rel, next[i]))
            ok = 0;
    if (created.data && !write_file(ws->root, created.rel, created.data))
        ok = 0;
    if (created2.data && !write_file(ws->root, created2.rel, created2.data))
        ok = 0;
    rep->applied = touched;

    /* verify: operator intent holds and the agent's own probe did not
       regress (a compiler fix-it must take the build from failing to ok) */
    int verified = 0;
    TASK_OPS_WORKSPACE *after = ok ? (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*after)) : NULL;
    if (after) {
        TaskOpsLoadWorkspace(workspace, after);
        probe_out(after, flags, &rep->compile_after, &rep->run_after, probe_stdout[1]);
        int intent;
        if (!strcmp(rep->op, "rename_symbol"))
            intent = TaskOpsCountToken(after, a) == 0 && TaskOpsCountToken(after, b) > 0;
        else if (!strcmp(rep->op, "stated_fragment")) {
            CTOK y[FRAG_MAX_TOK];
            int ny = ctok_lex(y_text, strlen(y_text), y, FRAG_MAX_TOK);
            intent = ny > 0 && ws_contains_tokens(after, y, ny);
        } else if (!strcmp(rep->op, "literal_to_constant")) {
            int f2, files2;
            intent = literal_uses(after, &lit, &f2, &files2) == 0 && TaskOpsCountToken(after, lit.name) >= 2;
        } else if (!strcmp(rep->op, "author_test")) {
            intent = run_test_plan(after, &tplan, flags);
        } else if (!strcmp(rep->op, "remove_dead_function")) {
            intent = TaskOpsCountToken(after, dead) == 0 && rep->compile_after == 1;
        } else if (!strcmp(rep->op, "doc_sync")) {
            int sh = ds_shape(da);
            intent = ds_in(after, da, sh, 1) == 0 && ds_in(after, db, sh, 1) > 0;
        } else if (!strcmp(rep->op, "relop_search")) {
            EXAMPLE vex[EX_MAX];
            int vn = task_examples(after, task, vex, EX_MAX), vm = any_main(after);
            intent = (!vm || (rep->compile_after == 1 && rep->run_after == 0)) &&
                     (vn == 0 || run_examples(after, flags, vex, vn) == 1);
        } else if (!strcmp(rep->op, "shell_harden")) {
            intent = ShellOpsIntent(after->files[shell_file].data, shell_rule) &&
                     shell_syntax_ok(after->root, after->files[shell_file].rel) &&
                     shell_contract_run(after, shell_file, shell_rule, shell_detail);
        } else if (!strcmp(rep->op, "build_repair")) {
            intent = build_verified(after, &bedit);
        } else if (!strcmp(rep->op, "c_fix")) {
            intent = rep->compile_after == 1 && rep->run_after == 0 &&
                     (strcmp(cfix_rule, "goto_return") != 0 || TaskOpsCountToken(after, "goto") == 0) &&
                     (strcmp(cfix_rule, "split_function") != 0 ||
                      (ws_find_named(after, created.rel) >= 0 && ws_find_named(after, created2.rel) >= 0));
        } else if (!strcmp(rep->op, "shell_contract")) {
            intent = g_shc_idx >= 0 && g_shc_idx < after->count &&
                     shc_check(after, g_shc_idx, after->files[g_shc_idx].data, &g_shc);
        } else if (!strcmp(rep->op, "compile_repair")) {
            intent = rep->compile_after == 1 && rep->run_after <= 0;
        } else if (!strcmp(rep->op, "evidence_fix")) {
            intent = rep->compile_after == 1 && rep->run_after == 0;
        } else if (!strcmp(rep->op, "declare_implicit")) {
            IMPLICIT_USE left[DECL_MAX];
            intent = rep->compile_after == 1 && implicit_uses(after, flags, left, DECL_MAX) < uses_before;
        } else
            intent = rep->compile_after == 1;
        int no_regress = rep->compile_after >= rep->compile_before &&
                         (rep->run_before != 0 || rep->run_after == 0);
        /* scope: named files must exist, and when the task names files the
           edit must touch at least one of them (ws and after list the same
           files in the same order unless a file was created) */
        int named = 0, named_touched = 0;
        /* refactors must preserve observable behavior: same stdout */
        if ((!strcmp(rep->op, "rename_symbol") || !strcmp(rep->op, "literal_to_constant") ||
             !strcmp(rep->op, "remove_dead_function")) &&
            rep->run_before == 0 && strcmp(probe_stdout[0], probe_stdout[1]) != 0)
            no_regress = 0;
        int guard_names_missing = (!strcmp(rep->op, "shell_harden") &&
                                   (!strcmp(shell_rule, "file_guard") || !strcmp(shell_rule, "file_content"))) ||
                                  (!strcmp(rep->op, "build_repair") && !strcmp(bedit.rule, "cmake_missing_source")) ||
                                  (!strcmp(rep->op, "shell_contract") && g_shc.has_file);   /* the stated output file */
        if ((!guard_names_missing && !named_files_exist(ws, after, task, &named, &named_touched)) ||
            (named > 0 && named_touched == 0 && strcmp(rep->op, "doc_sync") != 0))
            intent = 0;   /* doc_sync edits docs only; named sources are where B was read */
        if (!strcmp(rep->op, "author_test"))
            no_regress = 1;   /* sources untouched; the test's own build and run is the evidence */
        verified = intent && no_regress;
        if (!verified)
            snprintf(rep->reason, sizeof(rep->reason),
                     "verify failed: intent=%d compile %d->%d run %d->%d", intent,
                     rep->compile_before, rep->compile_after, rep->run_before, rep->run_after);
        TaskOpsFreeWorkspace(after);
        free(after);
    } else {
        snprintf(rep->reason, sizeof(rep->reason), "write failed");
    }

    /* roll back unless verified */
    if (!verified)
        for (int i = 0; i < ws->count; i++)
            if (next[i])
                write_file(ws->root, ws->files[i].rel, ws->files[i].data);
    if (!verified && created.data) {
        char path[TASK_OPS_MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/%s", ws->root, created.rel);
        remove(path);
    }
    if (!verified && created2.data) {
        char path[TASK_OPS_MAX_PATH * 2];
        snprintf(path, sizeof(path), "%s/%s", ws->root, created2.rel);
        remove(path);
    }
    free(created.data);
    free(created2.data);
    /* learn: remember what this operator did on this workspace+task */
    if (use_mem && rep->op[0])
        mem_record(ws->root, key, rep->op, verified);
    for (int i = 0; i < TASK_OPS_MAX_FILES; i++)
        free(next[i]);
    free(next);
    rep->verified = verified;
    trace_attempt(ws, flags, rep);
    TaskOpsFreeWorkspace(ws);
    free(ws);
    return verified;
}
