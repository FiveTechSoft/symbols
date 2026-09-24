/*
 * git_ops.c - repository-state operators (see git_ops.h). Every decision
 * comes from what git reports; the task text only has to agree with it.
 */
#include "git_ops.h"
#include "agent_shell.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GIT_OPS_MAX_LIST 16
#define GIT_OPS_PATH 256

/* ------------------------------------------------------------ helpers */

static SHELL_EXEC_RESULT *git_run(const char *root, const char *cmd)
{
    SHELL_EXEC_RESULT *r = (SHELL_EXEC_RESULT *)malloc(sizeof(*r));
    if (!r)
        return NULL;
    AgentShellResultInit(r);
    if (!AgentShellExec(cmd, root, 20000, r) || r->execution_failed || r->timed_out ||
        r->stdout_truncated) {
        free(r);
        return NULL;
    }
    return r;
}

/* exit code of cmd, -1 when it could not run */
static int git_ok(const char *root, const char *cmd)
{
    SHELL_EXEC_RESULT *r = git_run(root, cmd);
    int rc = r ? r->exit_code : -1;
    free(r);
    return rc;
}

/* stdout of a successful cmd (malloc'd), NULL otherwise */
static char *git_out(const char *root, const char *cmd)
{
    SHELL_EXEC_RESULT *r = git_run(root, cmd);
    char *s = NULL;
    if (r && r->exit_code == 0) {
        s = (char *)malloc(r->stdout_len + 1);
        if (s) {
            memcpy(s, r->stdout_buf, r->stdout_len);
            s[r->stdout_len] = '\0';
        }
    }
    free(r);
    return s;
}

static void trim(char *s)
{
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

/* plain relative path: no shell metacharacters, no spaces, no "..", not absolute */
static int safe_path(const char *p)
{
    if (!p[0] || p[0] == '/' || p[0] == '-' || strstr(p, ".."))
        return 0;
    for (const char *c = p; *c; c++)
        if (!(isalnum((unsigned char)*c) || strchr("._-/", *c)))
            return 0;
    return 1;
}

/* newline-separated git output -> list of safe paths; -1 on an unsafe one */
static int split_paths(char *s, char list[][GIT_OPS_PATH], int max)
{
    int n = 0;
    for (char *line = strtok(s, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        trim(line);
        if (!line[0])
            continue;
        if (!safe_path(line) || strlen(line) >= GIT_OPS_PATH || n >= max)
            return -1;
        snprintf(list[n++], GIT_OPS_PATH, "%s", line);
    }
    return n;
}

static void lower_copy(const char *in, char *out, size_t sz)
{
    size_t i = 0;
    for (; in && in[i] && i + 1 < sz; i++)
        out[i] = (char)tolower((unsigned char)in[i]);
    out[i] = '\0';
}

/* word w in lowercased text t, on identifier boundaries */
static int has_word(const char *t, const char *w)
{
    size_t n = strlen(w);
    for (const char *p = strstr(t, w); p; p = strstr(p + 1, w)) {
        int left = p == t || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
        int right = !(isalnum((unsigned char)p[n]) || p[n] == '_');
        if (left && right)
            return 1;
    }
    return 0;
}

/* the task names this path: whole path or its basename as a word */
static int task_names(const char *lt, const char *path)
{
    char lp[GIT_OPS_PATH];
    lower_copy(path, lp, sizeof(lp));
    const char *base = strrchr(lp, '/');
    return has_word(lt, lp) || (base && has_word(lt, base + 1));
}

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = n >= 0 ? (char *)malloc((size_t)n + 1) : NULL;
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b);
        b = NULL;
    }
    fclose(f);
    if (b) {
        b[n] = '\0';
        if (len)
            *len = (size_t)n;
    }
    return b;
}

static int write_all(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    int ok = fwrite(data, 1, len, f) == len;
    return fclose(f) == 0 && ok;
}

static int path_exists(const char *root, const char *rel)
{
    char p[GIT_OPS_PATH * 4];
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    FILE *f = fopen(p, "rb");
    if (f)
        fclose(f);
    return f != NULL;
}

static int has_markers(const char *s)
{
    for (const char *l = s; l && *l;) {
        if (!strncmp(l, "<<<<<<<", 7) || !strncmp(l, ">>>>>>>", 7) || !strncmp(l, "=======", 7))
            return 1;
        const char *nl = strchr(l, '\n');
        l = nl ? nl + 1 : NULL;
    }
    return 0;
}

/* parents of HEAD, from `git rev-list --parents` (no ^ syntax: cmd.exe eats it) */
static int head_parents(const char *root)
{
    char *s = git_out(root, "git rev-list --parents -n 1 HEAD");
    int n = -1;
    if (s) {
        n = 0;
        for (char *t = strtok(s, " \t\r\n"); t; t = strtok(NULL, " \t\r\n"))
            n++;
        n--;
    }
    free(s);
    return n;
}

static int status_clean(const char *root)
{
    char *s = git_out(root, "git status --porcelain --untracked-files=no");
    int clean = s != NULL;
    if (s) {
        trim(s);
        clean = s[0] == '\0';
    }
    free(s);
    return clean;
}

/* -------------------------------------------------- union conflict text */

typedef struct { const char *p; size_t n; } LINE;

static int split_lines(const char *s, LINE *out, int max)
{
    int n = 0;
    while (*s && n < max) {
        const char *nl = strchr(s, '\n');
        size_t len = nl ? (size_t)(nl - s) + 1 : strlen(s);
        out[n].p = s;
        out[n].n = len;
        n++;
        s += len;
    }
    return *s ? -1 : n;
}

static int line_is(const LINE *l, const char *mark)
{
    return l->n >= 7 && !strncmp(l->p, mark, 7);
}

static int same_text(const LINE *a, const LINE *b)
{
    size_t na = a->n, nb = b->n;
    while (na && (a->p[na - 1] == '\n' || a->p[na - 1] == '\r'))
        na--;
    while (nb && (b->p[nb - 1] == '\n' || b->p[nb - 1] == '\r'))
        nb--;
    return na == nb && !memcmp(a->p, b->p, na);
}

static void emit(char *out, size_t *o, const LINE *l)
{
    memcpy(out + *o, l->p, l->n);
    *o += l->n;
    if (l->p[l->n - 1] != '\n')
        out[(*o)++] = '\n';
}

char *GitOpsUnionConflict(const char *text)
{
    enum { MAXL = 20000 };
    if (!text)
        return NULL;
    LINE *L = (LINE *)malloc(sizeof(LINE) * MAXL);
    int n = L ? split_lines(text, L, MAXL) : -1;
    char *out = n > 0 ? (char *)malloc(strlen(text) + (size_t)n + 1) : NULL;
    if (!out) {
        free(L);
        return NULL;
    }
    size_t o = 0;
    int hunks = 0, bad = 0;
    for (int i = 0; i < n && !bad;) {
        if (!line_is(&L[i], "<<<<<<<")) {
            if (line_is(&L[i], "=======") || line_is(&L[i], ">>>>>>>") || line_is(&L[i], "|||||||"))
                bad = 1;
            emit(out, &o, &L[i++]);
            continue;
        }
        int ours = i + 1, base = -1, mid = -1, end = -1;
        for (int j = ours; j < n; j++) {
            if (line_is(&L[j], "<<<<<<<")) break;
            if (line_is(&L[j], "|||||||") && mid < 0 && base < 0) base = j;
            else if (line_is(&L[j], "=======") && mid < 0) mid = j;
            else if (line_is(&L[j], ">>>>>>>") && mid >= 0) { end = j; break; }
        }
        if (mid < 0 || end < 0) {
            bad = 1;
            break;
        }
        int ours_end = base >= 0 ? base : mid;
        for (int k = ours; k < ours_end; k++)
            emit(out, &o, &L[k]);
        for (int k = mid + 1; k < end; k++) {
            int dup = 0;
            for (int m = ours; m < ours_end && !dup; m++)
                dup = same_text(&L[k], &L[m]);
            if (!dup)
                emit(out, &o, &L[k]);
        }
        hunks++;
        i = end + 1;
    }
    free(L);
    if (bad || hunks == 0) {
        free(out);
        return NULL;
    }
    out[o] = '\0';
    return out;
}

/* ---------------------------------------------------------- operators */

static int resolve_merge(const char *root, const char *lt, GIT_OPS_RESULT *r)
{
    char list[GIT_OPS_MAX_LIST][GIT_OPS_PATH];
    char *u = git_out(root, "git diff --name-only --diff-filter=U");
    int n = u ? split_paths(u, list, GIT_OPS_MAX_LIST) : -1;
    free(u);
    snprintf(r->op, sizeof(r->op), "resolve_merge");
    if (n <= 0) {
        snprintf(r->reason, sizeof(r->reason), "merge in progress but no readable conflicted paths");
        return 0;
    }
    if (!has_word(lt, "both")) {
        snprintf(r->reason, sizeof(r->reason), "merge conflict: the task does not say to keep both sides");
        return 0;
    }
    char *orig[GIT_OPS_MAX_LIST] = {0};
    size_t olen[GIT_OPS_MAX_LIST] = {0};
    int ok = 1, w = 0;
    for (int i = 0; i < n && ok; i++) {
        char p[GIT_OPS_PATH * 4];
        snprintf(p, sizeof(p), "%s/%s", root, list[i]);
        orig[i] = read_all(p, &olen[i]);
        char *m = orig[i] ? GitOpsUnionConflict(orig[i]) : NULL;
        ok = m && write_all(p, m, strlen(m));
        free(m);
        w += ok;
    }
    char cmd[GIT_OPS_PATH + 32];
    for (int i = 0; i < n && ok; i++) {
        snprintf(cmd, sizeof(cmd), "git add -- %s", list[i]);
        ok = git_ok(root, cmd) == 0;
    }
    ok = ok && git_ok(root, "git commit --no-edit") == 0;
    /* verify with git: merge finished, clean tree, no markers left */
    if (ok) {
        ok = git_ok(root, "git rev-parse -q --verify MERGE_HEAD") != 0 && status_clean(root) &&
             head_parents(root) == 2;
        for (int i = 0; i < n && ok; i++) {
            snprintf(cmd, sizeof(cmd), "git show HEAD:%s", list[i]);
            char *s = git_out(root, cmd);
            ok = s && !has_markers(s);
            free(s);
        }
    }
    if (!ok) {
        /* put the conflicted files back as they were */
        for (int i = 0; i < w; i++) {
            char p[GIT_OPS_PATH * 4];
            snprintf(p, sizeof(p), "%s/%s", root, list[i]);
            if (git_ok(root, "git rev-parse -q --verify MERGE_HEAD") == 0) {
                snprintf(cmd, sizeof(cmd), "git checkout -m -- %s", list[i]);
                git_ok(root, cmd);
            }
            if (orig[i])
                write_all(p, orig[i], olen[i]);
        }
        snprintf(r->reason, sizeof(r->reason), "verify failed: merge not completed cleanly");
    } else
        snprintf(r->detail, sizeof(r->detail), "kept both sides in %d file%s, merge committed", n,
                 n == 1 ? "" : "s");
    for (int i = 0; i < n; i++)
        free(orig[i]);
    return ok;
}

static int restore_deleted(const char *root, const char *lt, GIT_OPS_RESULT *r)
{
    char list[GIT_OPS_MAX_LIST][GIT_OPS_PATH];
    char *d = git_out(root, "git diff --name-only --diff-filter=D HEAD~1 HEAD");
    int n = d ? split_paths(d, list, GIT_OPS_MAX_LIST) : -1;
    free(d);
    if (n <= 0)
        return -1;
    int pick = -1, named = 0;
    for (int i = 0; i < n; i++)
        if (!path_exists(root, list[i]) && task_names(lt, list[i])) {
            pick = i;
            named++;
        }
    if (named == 0)
        return -1;
    snprintf(r->op, sizeof(r->op), "restore_deleted");
    if (named > 1) {
        snprintf(r->reason, sizeof(r->reason), "ambiguous: %d deleted files named", named);
        return 0;
    }
    if (!(has_word(lt, "restore") || has_word(lt, "recover") || has_word(lt, "undelete") ||
          has_word(lt, "back"))) {
        snprintf(r->reason, sizeof(r->reason), "deleted file named but the task does not ask to restore it");
        return 0;
    }
    const char *f = list[pick];
    char cmd[GIT_OPS_PATH + 48];
    snprintf(cmd, sizeof(cmd), "git checkout HEAD~1 -- %s", f);
    int ok = git_ok(root, cmd) == 0;
    if (ok) {
        char p[GIT_OPS_PATH * 4];
        snprintf(p, sizeof(p), "%s/%s", root, f);
        size_t len = 0;
        char *now = read_all(p, &len);
        snprintf(cmd, sizeof(cmd), "git show HEAD~1:%s", f);
        char *want = git_out(root, cmd);
        snprintf(cmd, sizeof(cmd), "git ls-files --error-unmatch -- %s", f);
        ok = now && want && len == strlen(want) && !memcmp(now, want, len) && git_ok(root, cmd) == 0;
        free(now);
        free(want);
        if (!ok) {
            snprintf(cmd, sizeof(cmd), "git rm -q --cached -- %s", f);
            git_ok(root, cmd);
            remove(p);
        }
    }
    if (ok)
        snprintf(r->detail, sizeof(r->detail), "%s restored from HEAD~1 and staged", f);
    else
        snprintf(r->reason, sizeof(r->reason), "verify failed: %s not restored byte-for-byte", f);
    return ok;
}

/* gcc -fsyntax-only on one file; 1 ok, 0 fails, -1 no compiler */
static int syntax_ok(const char *root, const char *rel)
{
    char cmd[GIT_OPS_PATH * 2 + 64];
    snprintf(cmd, sizeof(cmd), "gcc -std=c11 -fsyntax-only -I. %s", rel);
    SHELL_EXEC_RESULT *x = git_run(root, cmd);
    int rc = x ? (x->exit_code == 0 ? 1 : (x->exit_code == 127 ? -1 : 0)) : -1;
    free(x);
    return rc;
}

static int revert_head(const char *root, const char *lt, GIT_OPS_RESULT *r)
{
    if (!((has_word(lt, "undo") || has_word(lt, "revert")) && has_word(lt, "commit")))
        return -1;
    snprintf(r->op, sizeof(r->op), "revert_head");
    if (!status_clean(root) || head_parents(root) != 1) {
        snprintf(r->reason, sizeof(r->reason), "revert: needs a clean tree and a single-parent HEAD");
        return 0;
    }
    char list[GIT_OPS_MAX_LIST][GIT_OPS_PATH];
    char *d = git_out(root, "git diff --name-only HEAD~1 HEAD");
    int n = d ? split_paths(d, list, GIT_OPS_MAX_LIST) : -1;
    free(d);
    /* evidence: every changed C source compiled at HEAD~1, one fails now */
    int c = 0, broken = 0, prev_ok = 1;
    char tmp[GIT_OPS_PATH * 4], cmd[GIT_OPS_PATH + 48];
    snprintf(tmp, sizeof(tmp), "%s/.git/symbols_prev.c", root);
    for (int i = 0; i < n && prev_ok; i++) {
        size_t L = strlen(list[i]);
        if (L < 2 || strcmp(list[i] + L - 2, ".c") || !path_exists(root, list[i]))
            continue;
        c++;
        int now = syntax_ok(root, list[i]);
        if (now < 0) { prev_ok = 0; break; }
        broken += now == 0;
        snprintf(cmd, sizeof(cmd), "git show HEAD~1:%s", list[i]);
        char *s = git_out(root, cmd);
        prev_ok = s && write_all(tmp, s, strlen(s)) && syntax_ok(root, ".git/symbols_prev.c") == 1;
        free(s);
    }
    remove(tmp);
    if (n <= 0 || c == 0 || broken == 0 || !prev_ok) {
        snprintf(r->reason, sizeof(r->reason),
                 "revert: no build evidence that HEAD broke what HEAD~1 had (c=%d broken=%d prev_ok=%d)", c,
                 broken, prev_ok);
        return 0;
    }
    char *want = git_out(root, "git rev-parse HEAD~1:");
    char *head = git_out(root, "git rev-parse HEAD");
    int ok = want && head && git_ok(root, "git revert --no-edit HEAD") == 0;
    if (ok) {
        char *got = git_out(root, "git rev-parse HEAD:");
        ok = got && !strcmp(got, want) && status_clean(root);
        for (int i = 0; i < n && ok; i++) {
            size_t L = strlen(list[i]);
            if (L >= 2 && !strcmp(list[i] + L - 2, ".c") && path_exists(root, list[i]))
                ok = syntax_ok(root, list[i]) == 1;
        }
        free(got);
        if (!ok && head) {
            trim(head);
            snprintf(cmd, sizeof(cmd), "git reset -q --hard %s", head);
            git_ok(root, cmd);
        }
    } else
        git_ok(root, "git revert --abort");
    if (ok)
        snprintf(r->detail, sizeof(r->detail), "reverted HEAD; tree equals HEAD~1 and %d C file%s compile", c,
                 c == 1 ? "" : "s");
    else
        snprintf(r->reason, sizeof(r->reason), "verify failed: revert did not restore HEAD~1's tree");
    free(want);
    free(head);
    return ok;
}

int GitOpsSolve(const char *root, const char *task, GIT_OPS_RESULT *out)
{
    GIT_OPS_RESULT local;
    GIT_OPS_RESULT *r = out ? out : &local;
    memset(r, 0, sizeof(*r));
    if (!root || !task)
        return -1;
    /* only the top of a work tree: the workspace must be the repository */
    char *top = git_out(root, "git rev-parse --show-cdup");
    if (!top)
        return -1;
    trim(top);
    int at_top = top[0] == '\0';
    free(top);
    if (!at_top)
        return -1;
    char lt[4096];
    lower_copy(task, lt, sizeof(lt));
    int rc;
    if (git_ok(root, "git rev-parse -q --verify MERGE_HEAD") == 0)
        rc = resolve_merge(root, lt, r);
    else if (git_ok(root, "git rev-parse -q --verify HEAD~1") != 0)
        return -1;
    else if ((rc = restore_deleted(root, lt, r)) < 0)
        rc = revert_head(root, lt, r);
    if (rc >= 0)
        r->verified = rc == 1;
    return rc;
}
