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
static void probe(const TASK_OPS_WORKSPACE *ws, const char *flags, int *compile, int *run)
{
    char cmd[4096], srcs[3072] = {0}, bin[TASK_OPS_MAX_PATH];
    size_t used = 0;
    *compile = -1;
    *run = -1;
    for (int i = 0; i < ws->count; i++) {
        size_t n = strlen(ws->files[i].rel);
        if (n < 3 || strcmp(ws->files[i].rel + n - 2, ".c") != 0)
            continue;
        if (strchr(ws->files[i].rel, '"') || used + n + 4 >= sizeof(srcs))
            continue;
        used += (size_t)snprintf(srcs + used, sizeof(srcs) - used, "\"%s\" ", ws->files[i].rel);
    }
    if (used == 0)
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
    if (!ws)
        return 0;
    TaskOpsLoadWorkspace(workspace, ws);

    char a[128], b[128];
    rep->candidates = TaskOpsFindRename(ws, task, a, sizeof(a), b, sizeof(b));
    if (rep->candidates != 1) {
        snprintf(rep->reason, sizeof(rep->reason),
                 rep->candidates ? "ambiguous: %d rename candidates" : "no operator preconditions hold",
                 rep->candidates);
        TaskOpsFreeWorkspace(ws);
        free(ws);
        return 0;
    }

    char flags[512];
    task_flags(task, flags, sizeof(flags));
    probe(ws, flags, &rep->compile_before, &rep->run_before);

    /* act: rewrite every file that mentions A; originals stay in ws */
    char *next[TASK_OPS_MAX_FILES] = {0};
    int touched = 0, ok = 1;
    for (int i = 0; i < ws->count && ok; i++) {
        if (count_token_in(ws->files[i].data, a) == 0)
            continue;
        next[i] = replace_token(ws->files[i].data, a, b);
        ok = next[i] && write_file(ws->root, ws->files[i].rel, next[i]);
        touched++;
    }
    rep->applied = touched;
    snprintf(rep->op, sizeof(rep->op), "rename_symbol");
    snprintf(rep->detail, sizeof(rep->detail), "%.100s -> %.100s in %d file(s)", a, b, touched);

    /* verify: intent holds and the agent's own probe did not regress */
    int verified = 0;
    if (ok) {
        TASK_OPS_WORKSPACE *after = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*after));
        if (after) {
            TaskOpsLoadWorkspace(workspace, after);
            int intent = TaskOpsCountToken(after, a) == 0 && TaskOpsCountToken(after, b) > 0;
            probe(after, flags, &rep->compile_after, &rep->run_after);
            int no_regress = rep->compile_after >= rep->compile_before &&
                             (rep->run_before != 0 || rep->run_after == 0);
            verified = intent && no_regress;
            if (!verified)
                snprintf(rep->reason, sizeof(rep->reason),
                         "verify failed: intent=%d compile %d->%d run %d->%d", intent,
                         rep->compile_before, rep->compile_after, rep->run_before, rep->run_after);
            TaskOpsFreeWorkspace(after);
            free(after);
        }
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
    rep->verified = verified;
    TaskOpsFreeWorkspace(ws);
    free(ws);
    return verified;
}
