/* server_taskops.c: see server_taskops.h. No task ids, file names or answers live here. */
#include "server_taskops.h"
#include "task_ops.h"
#include "git_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define sto_mkdir(p) _mkdir(p)
#define sto_rmdir(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define sto_mkdir(p) mkdir(p, 0755)
#define sto_rmdir(p) rmdir(p)
#endif

static void mkdirs_for(const char *path)
{
    char buf[1200];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *c = buf + 1; *c; c++)
        if (*c == '/' || *c == '\\') {
            char k = *c;
            *c = '\0';
            sto_mkdir(buf);
            *c = k;
        }
}

static int write_text(const char *path, const char *data)
{
    FILE *f;
    mkdirs_for(path);
    f = fopen(path, "wb");
    if (!f)
        return 0;
    size_t n = strlen(data);
    int ok = fwrite(data, 1, n, f) == n;
    return (fclose(f) == 0) && ok;
}

/* remove the scratch tree: every file we know of, then the directories on
   their paths, deepest first */
static void rm_scratch(const char *root, const TASK_OPS_WORKSPACE *ws)
{
    char p[1024];
    for (int i = 0; i < ws->count; i++) {
        snprintf(p, sizeof(p), "%s/%s", root, ws->files[i].rel);
        remove(p);
        for (char *c = strrchr(p, '/'); c && c > p + strlen(root); c = strrchr(p, '/')) {
            *c = '\0';
            sto_rmdir(p);
        }
    }
    sto_rmdir(root);
}

static int find_rel(const TASK_OPS_WORKSPACE *ws, const char *rel)
{
    for (int i = 0; i < ws->count; i++)
        if (!strcmp(ws->files[i].rel, rel))
            return i;
    return -1;
}

static int count_occ(const char *hay, const char *needle)
{
    int n = 0;
    size_t nl = strlen(needle);
    if (!nl)
        return 0;
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p++)
        n++;
    return n;
}

int StoMinimalHunk(const char *a, const char *b, char **old_text, char **new_text)
{
    size_t al = strlen(a), bl = strlen(b), pre = 0, suf = 0;
    *old_text = *new_text = NULL;
    if (!strcmp(a, b))
        return 0;
    while (pre < al && pre < bl && a[pre] == b[pre]) pre++;
    while (suf < al - pre && suf < bl - pre && a[al - 1 - suf] == b[bl - 1 - suf]) suf++;
    /* widen to whole lines */
    size_t s = pre, ea = al - suf, eb = bl - suf;
    while (s > 0 && a[s - 1] != '\n') s--;
    while (ea < al && a[ea] != '\n') { ea++; eb++; }
    if (ea < al) { ea++; eb++; }
    for (;;) {
        size_t on = ea - s, nn = eb - s;
        char *o = (char *)malloc(on + 1), *n = (char *)malloc(nn + 1);
        if (!o || !n) { free(o); free(n); return 0; }
        memcpy(o, a + s, on); o[on] = '\0';
        memcpy(n, b + s, nn); n[nn] = '\0';
        if (on > 0 && count_occ(a, o) == 1) {
            *old_text = o;
            *new_text = n;
            return 1;
        }
        free(o);
        free(n);
        if (s == 0 && ea >= al)
            return 0;   /* empty original or not unique even as a whole */
        /* one more line of context on each side */
        if (s > 0) { s--; while (s > 0 && a[s - 1] != '\n') s--; }
        if (ea < al) { while (ea < al && a[ea] != '\n') { ea++; eb++; } if (ea < al) { ea++; eb++; } }
    }
}

/* binary-safe recursive copy / removal, used for repositories (.git) */
static int copy_file(const char *a, const char *b)
{
    FILE *in = fopen(a, "rb"), *out;
    char buf[65536];
    size_t n;
    int ok = 1;
    if (!in)
        return 0;
    out = fopen(b, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    return (fclose(out) == 0) && ok;
}

static int tree_op(const char *src, const char *dst, int depth)
{
    char a[1200], b[1200];
    int ok = 1;
    if (depth > 24)
        return 0;
    if (dst)
        sto_mkdir(dst);
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(a, sizeof(a), "%s\\*", src);
    h = FindFirstFileA(a, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return dst == NULL;
    do {
        const char *nm = fd.cFileName;
        if (!strcmp(nm, ".") || !strcmp(nm, ".."))
            continue;
        snprintf(a, sizeof(a), "%s/%s", src, nm);
        if (dst) snprintf(b, sizeof(b), "%s/%s", dst, nm);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ok &= tree_op(a, dst ? b : NULL, depth + 1);
        else if (dst)
            ok &= copy_file(a, b);
        else {
            SetFileAttributesA(a, FILE_ATTRIBUTE_NORMAL);
            remove(a);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(src);
    struct dirent *e;
    if (!d)
        return dst == NULL;
    while ((e = readdir(d)) != NULL) {
        struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        snprintf(a, sizeof(a), "%s/%s", src, e->d_name);
        if (dst) snprintf(b, sizeof(b), "%s/%s", dst, e->d_name);
        if (lstat(a, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            ok &= tree_op(a, dst ? b : NULL, depth + 1);
        else if (dst) {
            if (S_ISREG(st.st_mode))
                ok &= copy_file(a, b);
        } else
            remove(a);
    }
    closedir(d);
#endif
    if (!dst)
        sto_rmdir(src);
    return ok;
}

static int is_git_repo(const char *root)
{
    char p[700];
    FILE *f;
    snprintf(p, sizeof(p), "%s/.git/HEAD", root);
    f = fopen(p, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Git operators on a full copy; 1 = planned, 0 = matched but abstained,
   -1 = no git operator applies (text operators run as before). */
static int plan_git(const char *workdir, const char *root, const TASK_OPS_WORKSPACE *orig,
                    const char *task, size_t max_arg, StoPlan *p)
{
    GIT_OPS_RESULT g;
    int rc;
    memset(&g, 0, sizeof(g));
    if (!tree_op(workdir, root, 0)) {
        tree_op(root, NULL, 0);
        return -1;
    }
    rc = GitOpsSolve(root, task, &g);
    snprintf(p->op, sizeof(p->op), "%s", g.op);
    snprintf(p->detail, sizeof(p->detail), "%s", g.detail);
    if (rc != 1 || !g.verified) {
        snprintf(p->reason, sizeof(p->reason), "%s", g.reason[0] ? g.reason : "no operator preconditions hold");
        tree_op(root, NULL, 0);
        return rc == 0 ? 0 : -1;
    }
    if (!strcmp(g.op, "restore_deleted")) {
        char f[300];
        if (sscanf(g.detail, "%299s restored", f) != 1 || strchr(f, '\'')) { rc = 0; goto out; }
        snprintf(p->bash, sizeof(p->bash), "git checkout HEAD~1 -- '%s'", f);
    } else if (!strcmp(g.op, "revert_head")) {
        snprintf(p->bash, sizeof(p->bash), "git revert --no-edit HEAD");
    } else if (!strcmp(g.op, "resolve_merge")) {
        size_t used = (size_t)snprintf(p->bash, sizeof(p->bash), "git add");
        TASK_OPS_WORKSPACE *after = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*after));
        if (!after || !TaskOpsLoadWorkspace(root, after)) { free(after); rc = 0; goto out; }
        for (int i = 0; i < after->count && rc == 1; i++) {
            int j = find_rel(orig, after->files[i].rel);
            StoHunk *h;
            if (j < 0 || !strcmp(orig->files[j].data, after->files[i].data))
                continue;
            if (p->nhunks >= STO_MAX_HUNKS || strchr(after->files[i].rel, '\'')) { rc = 0; break; }
            h = &p->hunks[p->nhunks];
            snprintf(h->rel, sizeof(h->rel), "%s", after->files[i].rel);
            if (!StoMinimalHunk(orig->files[j].data, after->files[i].data, &h->old_text, &h->new_text) ||
                strlen(h->old_text) + strlen(h->new_text) > max_arg) {
                p->nhunks++;
                rc = 0;
                break;
            }
            p->nhunks++;
            used += (size_t)snprintf(p->bash + used, used < sizeof(p->bash) ? sizeof(p->bash) - used : 0, " '%s'", h->rel);
        }
        TaskOpsFreeWorkspace(after);
        free(after);
        if (rc == 1 && (p->nhunks == 0 || used + 32 >= sizeof(p->bash)))
            rc = 0;
        if (rc == 1)
            snprintf(p->bash + used, sizeof(p->bash) - used, " && git commit --no-edit");
    } else
        rc = 0;
out:
    tree_op(root, NULL, 0);
    if (rc != 1) {
        snprintf(p->reason, sizeof(p->reason), "git operator %s verified but cannot be handed back", g.op);
        StoPlanFree(p);
        p->bash[0] = '\0';
        return 0;
    }
    return 1;
}

/* The client may hand the request over as a quoted, escaped string
   ("... \"RESULT: \" ..."): drop the outer quotes and undo \" and \\. */
static void unwrap_request(const char *task, char *req, size_t size)
{
    size_t tl = strlen(task), o = 0;
    if (tl >= 2 && task[0] == '"' && task[tl - 1] == '"') {
        for (size_t i = 1; i + 1 < tl && o + 1 < size; i++) {
            if (task[i] == '\\' && i + 2 < tl && (task[i + 1] == '"' || task[i + 1] == '\\'))
                i++;
            req[o++] = task[i];
        }
        req[o] = '\0';
    } else
        snprintf(req, size, "%s", task);
}

void StoPlanFree(StoPlan *p)
{
    for (int i = 0; i < p->nhunks; i++) {
        free(p->hunks[i].old_text);
        free(p->hunks[i].new_text);
    }
    p->nhunks = 0;
}

int StoPlanTask(const char *workdir, const char *task, size_t max_arg, StoPlan *p)
{
    static unsigned counter;
    char root[600];
    const char *t = getenv("TMPDIR");
    int kept = 0, ok = 0;
    TASK_OPS_REPORT rep;
    TASK_OPS_WORKSPACE *orig = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*orig));
    TASK_OPS_WORKSPACE *after = (TASK_OPS_WORKSPACE *)calloc(1, sizeof(*after));
    memset(p, 0, sizeof(*p));
    memset(&rep, 0, sizeof(rep));
    if (!orig || !after) {
        free(orig); free(after);
        snprintf(p->reason, sizeof(p->reason), "out of memory");
        return 0;
    }
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = "/tmp";
    counter++;
#ifdef _WIN32
    snprintf(root, sizeof(root), "%s/sto_%lu_%u", t, (unsigned long)GetCurrentProcessId(), counter);
#else
    snprintf(root, sizeof(root), "%s/sto_%lu_%lu_%u", t, (unsigned long)getpid(), (unsigned long)time(NULL), counter);
#endif
    TaskOpsLoadWorkspace(workdir, orig);
    if (is_git_repo(workdir)) {
        char req0[4096];
        unwrap_request(task, req0, sizeof(req0));
        int g = plan_git(workdir, root, orig, req0, max_arg, p);
        if (g >= 0) {
            ok = g;
            goto done;
        }
        memset(p, 0, sizeof(*p));
    }
    if (orig->count == 0) {
        snprintf(p->reason, sizeof(p->reason), "no readable text files in the workspace");
        goto done;
    }
    sto_mkdir(root);
    for (int i = 0; i < orig->count; i++) {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s", root, orig->files[i].rel);
        if (!write_text(path, orig->files[i].data)) {
            snprintf(p->reason, sizeof(p->reason), "could not stage a scratch copy");
            goto clean;
        }
    }
    {   /* the client may hand the request over wrapped in quotes */
        char req[4096];
        unwrap_request(task, req, sizeof(req));
        kept = TaskOpsSolve(root, req, &rep);
    }
    snprintf(p->op, sizeof(p->op), "%s", rep.op);
    snprintf(p->detail, sizeof(p->detail), "%s", rep.detail);
    if (!kept || !rep.verified) {
        snprintf(p->reason, sizeof(p->reason), "%s", rep.reason[0] ? rep.reason : "no operator preconditions hold");
        goto clean;
    }
    if (!TaskOpsLoadWorkspace(root, after)) {
        snprintf(p->reason, sizeof(p->reason), "could not read the verified scratch copy");
        goto clean;
    }
    for (int i = 0; i < orig->count; i++)
        if (find_rel(after, orig->files[i].rel) < 0) {
            snprintf(p->reason, sizeof(p->reason), "the verified edit deletes a file; not handed back");
            goto clean;
        }
    for (int i = 0; i < after->count; i++) {
        int j = find_rel(orig, after->files[i].rel);
        StoHunk *h;
        if (j >= 0 && !strcmp(orig->files[j].data, after->files[i].data))
            continue;
        if (p->nhunks >= STO_MAX_HUNKS) {
            snprintf(p->reason, sizeof(p->reason), "the verified edit touches too many files");
            goto clean;
        }
        h = &p->hunks[p->nhunks];
        snprintf(h->rel, sizeof(h->rel), "%s", after->files[i].rel);
        if (j < 0) {
            h->created = 1;
            h->new_text = strdup(after->files[i].data);
        } else if (!StoMinimalHunk(orig->files[j].data, after->files[i].data, &h->old_text, &h->new_text)) {
            snprintf(p->reason, sizeof(p->reason), "no unique hunk for a changed file");
            goto clean;
        }
        p->nhunks++;
        if (!h->new_text || (h->old_text ? strlen(h->old_text) : 0) + strlen(h->new_text) > max_arg) {
            snprintf(p->reason, sizeof(p->reason), "a hunk is too large for one tool call");
            goto clean;
        }
    }
    if (p->nhunks == 0) {
        snprintf(p->reason, sizeof(p->reason), "operator kept no textual change");
        goto clean;
    }
    ok = 1;
clean:
    rm_scratch(root, after);
    rm_scratch(root, orig);
done:
    if (!ok)
        StoPlanFree(p);
    TaskOpsFreeWorkspace(orig);
    TaskOpsFreeWorkspace(after);
    free(orig);
    free(after);
    return ok;
}
