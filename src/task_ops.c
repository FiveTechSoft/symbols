/*
 * task_ops.c - see include/task_ops.h.
 */
#include "task_ops.h"
#include "agent_shell.h"
#include "code_graph.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
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
static int lex_code_tokens(const char *task, TASK_TOKEN *out, int max)
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
        if (!code_shaped(word, quoted))
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
    int n = lex_code_tokens(task ? task : "", tok, 64);
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

static void probe(const TASK_OPS_WORKSPACE *ws, const char *flags, int *compile, int *run)
{
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
        AgentShellExec(run_cmd, ws->root, 5000, r);
        *run = r->timed_out ? 124 : r->exit_code;
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

/* Returns the number of distinct best locations (1 = usable); fills y_text,
   hit. */
static int find_fragment_edit(const TASK_OPS_WORKSPACE *ws, const char *task, char *y_text,
                              size_t y_size, FRAG_HIT *out)
{
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

/* ------------------------------------------------------------------ act */

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

int TaskOpsSolve(const char *workspace, const char *task, TASK_OPS_REPORT *rep)
{
    TASK_OPS_REPORT local;
    if (!rep)
        rep = &local;
    memset(rep, 0, sizeof(*rep));
    rep->compile_before = rep->compile_after = -1;
    rep->run_before = rep->run_after = -1;
    if (!workspace || !task)
        return 0;

    TASK_OPS_WORKSPACE *ws = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*ws));
    char **next = (char **)calloc(TASK_OPS_MAX_FILES, sizeof(char *));
    if (!ws || !next) { free(ws); free(next); return 0; }
    TaskOpsLoadWorkspace(workspace, ws);

    char flags[512];
    task_flags(task, flags, sizeof(flags));
    probe(ws, flags, &rep->compile_before, &rep->run_before);

    /* reason: first operator whose preconditions hold */
    char a[128], b[128];
    int touched = 0;
    int renames = TaskOpsFindRename(ws, task, a, sizeof(a), b, sizeof(b));
    rep->candidates = renames;
    if (renames == 1) {
        snprintf(rep->op, sizeof(rep->op), "rename_symbol");
        for (int i = 0; i < ws->count; i++)
            if (count_token_in(ws->files[i].data, a) > 0) {
                next[i] = replace_token(ws->files[i].data, a, b);
                touched++;
            }
        snprintf(rep->detail, sizeof(rep->detail), "%.100s -> %.100s in %d file(s)", a, b, touched);
    }
    char y_text[128] = {0};
    FRAG_HIT hit;
    int frags = renames == 1 ? 0 : find_fragment_edit(ws, task, y_text, sizeof(y_text), &hit);
    if (frags == 1) {
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
    } else if (renames != 1 && rep->compile_before == 0) {
        int fixes = 0;
        touched = compiler_fixits(ws, flags, next, &fixes);
        if (touched) {
            rep->candidates = 1;
            snprintf(rep->op, sizeof(rep->op), "compiler_fixit");
            snprintf(rep->detail, sizeof(rep->detail), "%d fix-it(s) in %d file(s)", fixes, touched);
        }
    }
    if (!touched) {
        if (frags > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d places fit the stated fragment", frags);
        else if (renames > 1)
            snprintf(rep->reason, sizeof(rep->reason), "ambiguous: %d rename candidates", renames);
        else
            snprintf(rep->reason, sizeof(rep->reason), "no operator preconditions hold");
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
    rep->applied = touched;

    /* verify: operator intent holds and the agent's own probe did not
       regress (a compiler fix-it must take the build from failing to ok) */
    int verified = 0;
    TASK_OPS_WORKSPACE *after = ok ? (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*after)) : NULL;
    if (after) {
        TaskOpsLoadWorkspace(workspace, after);
        probe(after, flags, &rep->compile_after, &rep->run_after);
        int intent;
        if (!strcmp(rep->op, "rename_symbol"))
            intent = TaskOpsCountToken(after, a) == 0 && TaskOpsCountToken(after, b) > 0;
        else if (!strcmp(rep->op, "stated_fragment")) {
            CTOK y[FRAG_MAX_TOK];
            int ny = ctok_lex(y_text, strlen(y_text), y, FRAG_MAX_TOK);
            intent = ny > 0 && ws_contains_tokens(after, y, ny);
        } else
            intent = rep->compile_after == 1;
        int no_regress = rep->compile_after >= rep->compile_before &&
                         (rep->run_before != 0 || rep->run_after == 0);
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
    for (int i = 0; i < TASK_OPS_MAX_FILES; i++)
        free(next[i]);
    free(next);
    rep->verified = verified;
    TaskOpsFreeWorkspace(ws);
    free(ws);
    return verified;
}
