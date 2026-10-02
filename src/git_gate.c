#include "git_gate.h"
#include <stdlib.h>
#include <string.h>

static void Err(char *e, size_t n, const char *fmt, const char *a)
{
    if (e == NULL || n == 0)
        return;
    snprintf(e, n, fmt, a ? a : "");
}

static bool SafePath(const char *p)
{
    size_t i, n = strlen(p);
    if (n == 0 || n >= GIT_GATE_PATH_MAX || p[0] == '/' || p[0] == '-')
        return false;
    for (i = 0; i < n; i++)
    {
        char c = p[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '/' || c == '-'))
            return false;
    }
    return strstr(p, "..") == NULL;
}

static bool Add(GIT_GATE_MANIFEST *m, const char *path, char status, char *e, size_t en)
{
    size_t i;
    if (!SafePath(path))
    {
        Err(e, en, "Unsupported path in the patch headers: %s", path);
        return false;
    }
    for (i = 0; i < m->count; i++)
        if (strcmp(m->path[i], path) == 0)
        {
            Err(e, en, "Path appears twice in the patch: %s", path);
            return false;
        }
    if (m->count >= GIT_GATE_MAX_PATHS)
    {
        Err(e, en, "Patch changes more than the supported number of paths%s", "");
        return false;
    }
    snprintf(m->path[m->count], GIT_GATE_PATH_MAX, "%s", path);
    m->status[m->count++] = status;
    return true;
}

typedef struct
{
    bool open, is_new, is_del;
    char a[GIT_GATE_PATH_MAX], b[GIT_GATE_PATH_MAX];
    char ren_from[GIT_GATE_PATH_MAX], ren_to[GIT_GATE_PATH_MAX], copy_to[GIT_GATE_PATH_MAX];
} BLOCK;

static bool Finish(BLOCK *k, GIT_GATE_MANIFEST *m, char *e, size_t en)
{
    if (!k->open)
        return true;
    k->open = false;
    if (k->ren_from[0] || k->ren_to[0])
    {
        if (!k->ren_from[0] || !k->ren_to[0])
        {
            Err(e, en, "Incomplete rename header%s", "");
            return false;
        }
        return Add(m, k->ren_from, 'D', e, en) && Add(m, k->ren_to, 'A', e, en);
    }
    if (k->copy_to[0])
        return Add(m, k->copy_to, 'A', e, en);
    if (strcmp(k->a, k->b) != 0)
    {
        Err(e, en, "Paths differ without a rename header: %s", k->b);
        return false;
    }
    return Add(m, k->b, k->is_new ? 'A' : k->is_del ? 'D' : 'M', e, en);
}

static bool StartsWith(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

bool GitGateDeriveManifest(const char *patch_file, GIT_GATE_MANIFEST *out, char *error, size_t error_size)
{
    FILE *f;
    char *buf = NULL, *line, *next;
    long size;
    BLOCK k;
    bool in_header = false, ok = true;

    if (out == NULL || patch_file == NULL)
    {
        Err(error, error_size, "A patch file and an output are required%s", "");
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(&k, 0, sizeof(k));
    f = fopen(patch_file, "rb");
    if (f == NULL)
    {
        Err(error, error_size, "Patch file could not be read: %s", patch_file);
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || size > 2097152L || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        Err(error, error_size, "Patch file has an unsupported size%s", "");
        return false;
    }
    buf = (char *)malloc((size_t)size + 1);
    if (buf == NULL || fread(buf, 1, (size_t)size, f) != (size_t)size)
    {
        fclose(f);
        free(buf);
        Err(error, error_size, "Patch file could not be read%s", "");
        return false;
    }
    fclose(f);
    buf[size] = '\0';
    for (line = buf; ok && line != NULL && *line; line = next)
    {
        next = strchr(line, '\n');
        if (next)
            *next++ = '\0';
        if (StartsWith(line, "diff --git "))
        {
            char *sp;
            ok = Finish(&k, out, error, error_size);
            if (!ok)
                break;
            memset(&k, 0, sizeof(k));
            k.open = true;
            in_header = true;
            if (!StartsWith(line + 11, "a/") || (sp = strchr(line + 13, ' ')) == NULL ||
                !StartsWith(sp + 1, "b/") || strchr(sp + 1, ' ') != NULL)
            {
                Err(error, error_size, "Unsupported diff header: %s", line);
                ok = false;
                break;
            }
            *sp = '\0';
            if (strlen(line + 13) >= sizeof(k.a) || strlen(sp + 3) >= sizeof(k.b))
            {
                Err(error, error_size, "Path is too long%s", "");
                ok = false;
                break;
            }
            snprintf(k.a, sizeof(k.a), "%s", line + 13);
            snprintf(k.b, sizeof(k.b), "%s", sp + 3);
            continue;
        }
        if (!in_header)
            continue;
        if (StartsWith(line, "@@ ") || StartsWith(line, "GIT binary patch") || StartsWith(line, "Binary files "))
        {
            in_header = false;
            continue;
        }
        if (StartsWith(line, "new file mode "))
            k.is_new = true;
        else if (StartsWith(line, "deleted file mode "))
            k.is_del = true;
        else if (StartsWith(line, "rename from "))
            snprintf(k.ren_from, sizeof(k.ren_from), "%s", line + 12);
        else if (StartsWith(line, "rename to "))
            snprintf(k.ren_to, sizeof(k.ren_to), "%s", line + 10);
        else if (StartsWith(line, "copy to "))
            snprintf(k.copy_to, sizeof(k.copy_to), "%s", line + 8);
    }
    if (ok)
        ok = Finish(&k, out, error, error_size);
    free(buf);
    if (ok && out->count == 0)
    {
        Err(error, error_size, "The patch changes no files%s", "");
        return false;
    }
    return ok;
}

static const char *Opt(int argc, char **argv, const char *name)
{
    int i;
    for (i = 0; i + 1 < argc; i++)
        if (strcmp(argv[i], name) == 0)
            return argv[i + 1];
    return NULL;
}

static bool Flag(int argc, char **argv, const char *name)
{
    int i;
    for (i = 0; i < argc; i++)
        if (strcmp(argv[i], name) == 0)
            return true;
    return false;
}

/* The patch operand is the first argument after the subcommand that is not an
   option or an option value. */
static const char *PatchArg(int argc, char **argv)
{
    int i;
    for (i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--dir") == 0)
        {
            i++;
            continue;
        }
        if (argv[i][0] == '-')
            return NULL;
        return argv[i];
    }
    return NULL;
}

static int Usage(FILE *err)
{
    fputs("usage: symbols_git_gate preflight --expected-head SHA [--branch NAME] [--remote-sync] [--dir D]\n"
          "       symbols_git_gate patch-state|verify-staged|verify-head PATCH [--dir D]\n",
          err);
    return GIT_GATE_USAGE;
}

int GitGateRun(int argc, char **argv, FILE *out, FILE *err)
{
    const char *cmd, *dir, *patch;
    char error[GIT_ERROR_MAX] = "";

    if (argc < 2)
        return Usage(err);
    cmd = argv[1];
    dir = Opt(argc, argv, "--dir");
    if (dir == NULL)
        dir = ".";

    if (strcmp(cmd, "preflight") == 0)
    {
        GIT_PRECONDITIONS pre;
        GIT_REPOSITORY_STATE st;
        GIT_PREFLIGHT_STATUS s;
        const char *head = Opt(argc, argv, "--expected-head");
        if (head == NULL || head[0] == '\0')
            return Usage(err);
        memset(&pre, 0, sizeof(pre));
        pre.expected_head = head;
        pre.expected_branch = Opt(argc, argv, "--branch");
        pre.require_clean = true;
        pre.allow_detached_head = false;
        pre.require_remote_in_sync = Flag(argc, argv, "--remote-sync");
        s = AgentGitPreflight(dir, &pre, &st, error, sizeof(error));
        fprintf(out, "preflight: %s\n", AgentGitPreflightStatusName(s));
        if (s != GIT_PREFLIGHT_READY)
        {
            fprintf(err, "%s\n", error);
            return 10 + (int)s;
        }
        return 0;
    }
    if (strcmp(cmd, "patch-state") == 0)
    {
        GIT_PATCH_STATE s;
        patch = PatchArg(argc, argv);
        if (patch == NULL)
            return Usage(err);
        s = AgentGitPatchState(dir, patch, error, sizeof(error));
        switch (s)
        {
        case GIT_PATCH_NOT_APPLIED:
            fputs("patch-state: not applied\n", out);
            return 0;
        case GIT_PATCH_ALREADY_APPLIED:
            fputs("patch-state: already applied\n", out);
            return 3;
        case GIT_PATCH_NO_MATCH:
            fputs("patch-state: applies neither way\n", out);
            fprintf(err, "%s\n", error);
            return 4;
        default:
            fputs("patch-state: check failed\n", out);
            fprintf(err, "%s\n", error);
            return 5;
        }
    }
    if (strcmp(cmd, "verify-staged") == 0 || strcmp(cmd, "verify-head") == 0)
    {
        GIT_GATE_MANIFEST *m;
        GIT_EXPECTED_CHANGE *exp;
        GIT_CHANGES_STATUS s;
        char joined[1024];
        size_t i;
        patch = PatchArg(argc, argv);
        if (patch == NULL)
            return Usage(err);
        snprintf(joined, sizeof(joined), "%s/%s", dir, patch);
        m = (GIT_GATE_MANIFEST *)malloc(sizeof(*m));
        exp = (GIT_EXPECTED_CHANGE *)malloc(sizeof(*exp) * GIT_GATE_MAX_PATHS);
        if (m == NULL || exp == NULL)
        {
            free(m);
            free(exp);
            fputs("out of memory\n", err);
            return 6;
        }
        if (!GitGateDeriveManifest(joined, m, error, sizeof(error)))
        {
            fprintf(out, "%s: manifest not derived\n", cmd);
            fprintf(err, "%s\n", error);
            free(m);
            free(exp);
            return 6;
        }
        for (i = 0; i < m->count; i++)
        {
            exp[i].path = m->path[i];
            exp[i].status = m->status[i];
        }
        s = cmd[7] == 's' ? AgentGitStagedMatches(dir, exp, m->count, error, sizeof(error))
                          : AgentGitHeadCommitMatches(dir, exp, m->count, error, sizeof(error));
        fprintf(out, "%s: %s (%u paths)\n", cmd,
                s == GIT_CHANGES_MATCH ? "match" : s == GIT_CHANGES_MISMATCH ? "mismatch" : "inspection failed",
                (unsigned)m->count);
        if (s != GIT_CHANGES_MATCH)
            fprintf(err, "%s\n", error);
        free(m);
        free(exp);
        return (int)s;
    }
    return Usage(err);
}
