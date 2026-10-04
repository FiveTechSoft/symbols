#include "agent_git.h"
#include "agent_shell.h"

#include <stdio.h>
#include <string.h>

static void SetError(char *out, size_t size, const char *text)
{
    if (out == NULL || size == 0)
        return;
    snprintf(out, size, "%s", text != NULL ? text : "");
}

static void TrimLine(char *text)
{
    size_t n;
    if (text == NULL)
        return;
    n = strlen(text);
    while (n > 0 && (text[n - 1] == '\r' || text[n - 1] == '\n'))
        text[--n] = '\0';
}

/* Fs* control artifacts (lock, intents, stage/pin files) live in the workspace
   root while AgentPatch edits files. They are internal, not user changes. */
static int IsFsControlEntry(const char *line)
{
    const char *path = line + 2; /* after "? " or "! " */
    const char *base = path, *p;
    size_t n;
    if (*base == '"')
        base++;
    for (p = base; *p; p++)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    if (!strncmp(base, ".fstxn", 6) || !strncmp(base, ".fsrp-", 6) || !strncmp(base, ".fsrs-", 6) ||
        !strncmp(base, ".fsrb-", 6) || !strncmp(base, ".fsrm-", 6) ||
        !strncmp(base, ".fsmv-", 6))
        return 1;
    n = strlen(base);
    if (n > 0 && base[n - 1] == '"')
        n--;
    return !strncmp(base, ".fs", 3) && n > 9 && !strncmp(base + n - 6, "-stage", 6);
}

static GIT_INSPECT_STATUS RunReadOnly(const char *command,
                                      const char *working_dir,
                                      SHELL_EXEC_RESULT *result,
                                      char *error,
                                      size_t error_size)
{
    if (!AgentShellExec(command, working_dir, 10000, result) ||
        result->execution_failed)
    {
        SetError(error, error_size, "Git process could not be started");
        return GIT_INSPECT_COMMAND_FAILED;
    }
    if (result->timed_out)
    {
        SetError(error, error_size, "Git inspection timed out");
        return GIT_INSPECT_COMMAND_FAILED;
    }
    if (result->stdout_truncated || result->stderr_truncated)
    {
        SetError(error, error_size, "Git inspection output was truncated");
        return GIT_INSPECT_OUTPUT_TRUNCATED;
    }
    return GIT_INSPECT_OK;
}

GIT_INSPECT_STATUS AgentGitInspect(const char *working_dir,
                                   GIT_REPOSITORY_STATE *out,
                                   char *error,
                                   size_t error_size)
{
    SHELL_EXEC_RESULT result;
    GIT_INSPECT_STATUS status;
    char status_text[SHELL_BUFFER_MAX];
    char *line;

    if (working_dir == NULL || working_dir[0] == '\0' || out == NULL)
    {
        SetError(error, error_size, "A working directory and output are required");
        return GIT_INSPECT_MALFORMED_OUTPUT;
    }
    memset(out, 0, sizeof(*out));
    SetError(error, error_size, "");

    status = RunReadOnly("git rev-parse --is-inside-work-tree", working_dir,
                         &result, error, error_size);
    if (status != GIT_INSPECT_OK)
        return status;
    TrimLine(result.stdout_buf);
    if (result.exit_code != 0 || strcmp(result.stdout_buf, "true") != 0)
    {
        SetError(error, error_size, "Working directory is not inside a Git work tree");
        return GIT_INSPECT_NOT_REPOSITORY;
    }

    status = RunReadOnly("git rev-parse --verify HEAD", working_dir,
                         &result, error, error_size);
    if (status != GIT_INSPECT_OK)
        return status;
    TrimLine(result.stdout_buf);
    if (result.exit_code != 0 || result.stdout_buf[0] == '\0' ||
        strlen(result.stdout_buf) >= sizeof(out->head))
    {
        SetError(error, error_size, "Repository has no verifiable HEAD");
        return GIT_INSPECT_MALFORMED_OUTPUT;
    }
    snprintf(out->head, sizeof(out->head), "%s", result.stdout_buf);

    status = RunReadOnly("git symbolic-ref --quiet --short HEAD", working_dir,
                         &result, error, error_size);
    if (status != GIT_INSPECT_OK)
        return status;
    TrimLine(result.stdout_buf);
    if (result.exit_code == 0)
    {
        if (result.stdout_buf[0] == '\0' || strlen(result.stdout_buf) >= sizeof(out->branch))
        {
            SetError(error, error_size, "Git returned an invalid branch name");
            return GIT_INSPECT_MALFORMED_OUTPUT;
        }
        snprintf(out->branch, sizeof(out->branch), "%s", result.stdout_buf);
    }
    else if (result.exit_code == 1)
    {
        out->detached_head = true;
    }
    else
    {
        SetError(error, error_size, "Git could not inspect the current branch");
        return GIT_INSPECT_COMMAND_FAILED;
    }

    status = RunReadOnly("git status --porcelain=v2 --untracked-files=all --ignored=matching",
                         working_dir, &result, error, error_size);
    if (status != GIT_INSPECT_OK)
        return status;
    if (result.exit_code != 0)
    {
        SetError(error, error_size, "Git could not inspect repository status");
        return GIT_INSPECT_COMMAND_FAILED;
    }
    snprintf(status_text, sizeof(status_text), "%s", result.stdout_buf);
    line = strtok(status_text, "\n");
    while (line != NULL)
    {
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\r')
            line[n - 1] = '\0';
        if (line[0] == '1' || line[0] == '2')
        {
            if (strlen(line) < 4 || line[1] != ' ')
            {
                SetError(error, error_size, "Malformed Git status entry");
                return GIT_INSPECT_MALFORMED_OUTPUT;
            }
            if (line[2] != '.')
                out->staged_paths++;
            if (line[3] != '.')
                out->unstaged_paths++;
        }
        else if (line[0] == 'u' && line[1] == ' ')
            out->conflicted_paths++;
        else if (line[0] == '?' && line[1] == ' ')
        {
            if (!IsFsControlEntry(line))
                out->untracked_paths++;
        }
        else if (line[0] == '!' && line[1] == ' ')
        {
            if (!IsFsControlEntry(line))
                out->ignored_paths++;
        }
        else if (line[0] != '\0')
        {
            SetError(error, error_size, "Unknown Git status entry");
            return GIT_INSPECT_MALFORMED_OUTPUT;
        }
        line = strtok(NULL, "\n");
    }
    return GIT_INSPECT_OK;
}

/* Remote and branch names come from Git itself; still accept only a plain
   charset before they reach a command line. */
static int SafeRefName(const char *s)
{
    size_t n = s ? strlen(s) : 0, i;
    if (n == 0 || n >= GIT_BRANCH_MAX || s[0] == '-' || s[0] == '/')
        return 0;
    for (i = 0; i < n; i++)
    {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '/'))
            return 0;
    }
    return strstr(s, "..") == NULL;
}

/* Read-only comparison with the upstream branch tip. Never fetches. */
static GIT_PREFLIGHT_STATUS RemoteCheck(const char *working_dir,
                                        GIT_REPOSITORY_STATE *observed,
                                        char *error, size_t error_size)
{
    SHELL_EXEC_RESULT result;
    char remotes[SHELL_BUFFER_MAX], command[512];
    char upstream[GIT_BRANCH_MAX], best[GIT_BRANCH_MAX] = "", *line;
    size_t i, hex;
    if (observed->detached_head || observed->branch[0] == '\0')
    {
        SetError(error, error_size, "A branch with an upstream is required");
        return GIT_PREFLIGHT_NO_UPSTREAM;
    }
    if (RunReadOnly("git rev-parse --abbrev-ref --symbolic-full-name @{upstream}",
                    working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    if (result.exit_code != 0)
    {
        SetError(error, error_size, "Current branch has no upstream");
        return GIT_PREFLIGHT_NO_UPSTREAM;
    }
    TrimLine(result.stdout_buf);
    if (!SafeRefName(result.stdout_buf))
    {
        SetError(error, error_size, "Upstream name is not a plain Git name");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    snprintf(upstream, sizeof(upstream), "%s", result.stdout_buf);
    snprintf(observed->upstream, sizeof(observed->upstream), "%s", upstream);
    if (RunReadOnly("git remote", working_dir, &result, error, error_size) != GIT_INSPECT_OK ||
        result.exit_code != 0)
    {
        if (error && error_size && !error[0])
            SetError(error, error_size, "Git could not list remotes");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    snprintf(remotes, sizeof(remotes), "%s", result.stdout_buf);
    for (line = strtok(remotes, "\n"); line; line = strtok(NULL, "\n"))
    {
        size_t n;
        TrimLine(line);
        n = strlen(line);
        if (n > 0 && strncmp(upstream, line, n) == 0 && upstream[n] == '/' &&
            n > strlen(best))
            snprintf(best, sizeof(best), "%s", line);
    }
    if (best[0] == '\0' || !SafeRefName(best) || upstream[strlen(best) + 1] == '\0')
    {
        SetError(error, error_size, "Upstream does not name a configured remote branch");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    snprintf(command, sizeof(command), "git ls-remote --exit-code %s refs/heads/%s",
             best, upstream + strlen(best) + 1);
    if (!SafeRefName(upstream + strlen(best) + 1) ||
        RunReadOnly(command, working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    if (result.exit_code != 0)
    {
        SetError(error, error_size, "Remote branch could not be read");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    for (hex = 0; result.stdout_buf[hex] &&
         ((result.stdout_buf[hex] >= '0' && result.stdout_buf[hex] <= '9') ||
          (result.stdout_buf[hex] >= 'a' && result.stdout_buf[hex] <= 'f')); hex++)
        ;
    if ((hex != 40 && hex != 64) || hex >= sizeof(observed->remote_head))
    {
        SetError(error, error_size, "Remote returned a malformed tip");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    for (i = 0; i < hex; i++)
        observed->remote_head[i] = result.stdout_buf[i];
    observed->remote_head[hex] = '\0';
    if (strcmp(observed->remote_head, observed->head) == 0)
        return GIT_PREFLIGHT_READY;
    snprintf(command, sizeof(command), "git merge-base --is-ancestor %s HEAD",
             observed->remote_head);
    if (RunReadOnly(command, working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    if (result.exit_code == 0)
        return GIT_PREFLIGHT_READY; /* local is ahead of the remote tip */
    SetError(error, error_size, "Remote branch moved or diverged from HEAD");
    return GIT_PREFLIGHT_REMOTE_ADVANCED;
}

GIT_PREFLIGHT_STATUS AgentGitPreflight(const char *working_dir,
                                       const GIT_PRECONDITIONS *required,
                                       GIT_REPOSITORY_STATE *observed,
                                       char *error,
                                       size_t error_size)
{
    GIT_INSPECT_STATUS inspected;
    if (required == NULL || observed == NULL)
    {
        SetError(error, error_size, "Preconditions and observed state are required");
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    }
    inspected = AgentGitInspect(working_dir, observed, error, error_size);
    if (inspected == GIT_INSPECT_NOT_REPOSITORY)
        return GIT_PREFLIGHT_NOT_REPOSITORY;
    if (inspected != GIT_INSPECT_OK)
        return GIT_PREFLIGHT_INSPECTION_FAILED;
    if (required->expected_head != NULL && required->expected_head[0] != '\0' &&
        strcmp(required->expected_head, observed->head) != 0)
    {
        SetError(error, error_size, "HEAD does not match the expected base");
        return GIT_PREFLIGHT_STALE_HEAD;
    }
    if (!required->allow_detached_head && observed->detached_head)
    {
        SetError(error, error_size, "Detached HEAD is not allowed");
        return GIT_PREFLIGHT_DETACHED_HEAD;
    }
    if (required->expected_branch != NULL && required->expected_branch[0] != '\0' &&
        strcmp(required->expected_branch, observed->branch) != 0)
    {
        SetError(error, error_size, "Current branch does not match the expected branch");
        return GIT_PREFLIGHT_WRONG_BRANCH;
    }
    if (observed->conflicted_paths > 0)
    {
        SetError(error, error_size, "Repository has unresolved conflicts");
        return GIT_PREFLIGHT_CONFLICTS;
    }
    if (required->require_clean &&
        (observed->staged_paths > 0 || observed->unstaged_paths > 0 ||
         observed->untracked_paths > 0))
    {
        SetError(error, error_size, "Repository has unreviewed changes");
        return GIT_PREFLIGHT_DIRTY_TREE;
    }
    if (required->require_remote_in_sync)
        return RemoteCheck(working_dir, observed, error, error_size);
    return GIT_PREFLIGHT_READY;
}

/* ---- Commit contract and "already applied" detection ------------------- */

#define EXPECTED_MAX 1024

static int ExpectedValid(const GIT_EXPECTED_CHANGE *expected, size_t count,
                         char *error, size_t error_size)
{
    size_t i, j;
    if (count > EXPECTED_MAX || (count > 0 && expected == NULL))
    {
        SetError(error, error_size, "Expected change list is invalid");
        return 0;
    }
    for (i = 0; i < count; i++)
    {
        const char *p = expected[i].path;
        if (p == NULL || *p == '\0' || *p == '/' || strchr(p, '\n') ||
            strchr(p, '\t') || strchr(p, '"') || strchr(p, '\r') ||
            (expected[i].status != 0 && expected[i].status != 'A' &&
             expected[i].status != 'M' && expected[i].status != 'D'))
        {
            SetError(error, error_size, "Expected change path or status is invalid");
            return 0;
        }
        for (j = 0; j < i; j++)
            if (strcmp(expected[j].path, p) == 0)
            {
                SetError(error, error_size, "Expected change list repeats a path");
                return 0;
            }
    }
    return 1;
}

static GIT_CHANGES_STATUS CompareChanges(const char *command,
                                         const char *working_dir,
                                         const GIT_EXPECTED_CHANGE *expected,
                                         size_t count,
                                         char *error,
                                         size_t error_size)
{
    SHELL_EXEC_RESULT result;
    unsigned char seen[EXPECTED_MAX];
    char text[SHELL_BUFFER_MAX], *line;
    size_t i, found = 0;
    memset(seen, 0, sizeof(seen));
    if (RunReadOnly(command, working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_CHANGES_INSPECTION_FAILED;
    if (result.exit_code != 0)
    {
        SetError(error, error_size, "Git could not list the changes");
        return GIT_CHANGES_INSPECTION_FAILED;
    }
    snprintf(text, sizeof(text), "%s", result.stdout_buf);
    for (line = strtok(text, "\n"); line; line = strtok(NULL, "\n"))
    {
        char status;
        const char *path;
        TrimLine(line);
        if (*line == '\0')
            continue;
        if (line[1] != '\t' || line[2] == '\0' || line[2] == '"')
        {
            SetError(error, error_size, "Git change line is malformed or quoted");
            return GIT_CHANGES_INSPECTION_FAILED;
        }
        status = line[0];
        path = line + 2;
        for (i = 0; i < count; i++)
            if (strcmp(expected[i].path, path) == 0)
                break;
        if (i == count)
        {
            snprintf(error, error_size, "Unexpected change: %c %.200s", status, path);
            return GIT_CHANGES_MISMATCH;
        }
        if (expected[i].status != 0 && expected[i].status != status)
        {
            snprintf(error, error_size, "Wrong change kind for %.200s: got %c, expected %c",
                     path, status, expected[i].status);
            return GIT_CHANGES_MISMATCH;
        }
        if (!seen[i])
        {
            seen[i] = 1;
            found++;
        }
    }
    if (found != count)
    {
        for (i = 0; i < count; i++)
            if (!seen[i])
                break;
        snprintf(error, error_size, "Expected change is missing: %.200s", expected[i].path);
        return GIT_CHANGES_MISMATCH;
    }
    SetError(error, error_size, "");
    return GIT_CHANGES_MATCH;
}

GIT_CHANGES_STATUS AgentGitStagedMatches(const char *working_dir,
                                         const GIT_EXPECTED_CHANGE *expected,
                                         size_t count,
                                         char *error,
                                         size_t error_size)
{
    if (!ExpectedValid(expected, count, error, error_size))
        return GIT_CHANGES_INSPECTION_FAILED;
    return CompareChanges("git -c core.quotepath=off diff --cached --name-status --no-renames",
                          working_dir, expected, count, error, error_size);
}

GIT_CHANGES_STATUS AgentGitHeadCommitMatches(const char *working_dir,
                                             const GIT_EXPECTED_CHANGE *expected,
                                             size_t count,
                                             char *error,
                                             size_t error_size)
{
    SHELL_EXEC_RESULT result;
    const char *p;
    unsigned words = 0;
    if (!ExpectedValid(expected, count, error, error_size))
        return GIT_CHANGES_INSPECTION_FAILED;
    if (RunReadOnly("git rev-list --parents -n 1 HEAD", working_dir, &result,
                    error, error_size) != GIT_INSPECT_OK)
        return GIT_CHANGES_INSPECTION_FAILED;
    if (result.exit_code != 0)
    {
        SetError(error, error_size, "Git could not read HEAD");
        return GIT_CHANGES_INSPECTION_FAILED;
    }
    TrimLine(result.stdout_buf);
    for (p = result.stdout_buf; *p; p++)
        if (*p == ' ')
            words++;
    if (words > 1)
    {
        SetError(error, error_size, "HEAD is a merge commit");
        return GIT_CHANGES_MISMATCH;
    }
    return CompareChanges("git -c core.quotepath=off diff-tree --root --no-commit-id "
                          "--name-status -r --no-renames HEAD",
                          working_dir, expected, count, error, error_size);
}

GIT_PATCH_STATE AgentGitPatchState(const char *working_dir,
                                   const char *patch_path,
                                   char *error,
                                   size_t error_size)
{
    SHELL_EXEC_RESULT result;
    char command[512];
    FILE *probe;
    if (!SafeRefName(patch_path) || strlen(patch_path) > 300)
    {
        SetError(error, error_size, "Patch path is not a plain relative name");
        return GIT_PATCH_CHECK_FAILED;
    }
    probe = fopen(patch_path, "rb");
    if (probe == NULL)
    {
        /* The path is relative to the caller, git runs in working_dir. */
        char joined[640];
        snprintf(joined, sizeof(joined), "%s/%s", working_dir ? working_dir : ".", patch_path);
        probe = fopen(joined, "rb");
    }
    if (probe == NULL)
    {
        SetError(error, error_size, "Patch file could not be read");
        return GIT_PATCH_CHECK_FAILED;
    }
    fclose(probe);
    snprintf(command, sizeof(command), "git apply --check %s", patch_path);
    if (RunReadOnly(command, working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_PATCH_CHECK_FAILED;
    if (result.exit_code == 0)
        return GIT_PATCH_NOT_APPLIED;
    if (result.exit_code != 1)
    {
        SetError(error, error_size, "Git could not parse the patch");
        return GIT_PATCH_CHECK_FAILED;
    }
#ifdef AGENT_GIT_PATCH_STATE_MUTANT
    /* Test only: forget the reverse check, so an applied patch is never recognised. */
    snprintf(command, sizeof(command), "git apply --check %s", patch_path);
#else
    snprintf(command, sizeof(command), "git apply --check --reverse %s", patch_path);
#endif
    if (RunReadOnly(command, working_dir, &result, error, error_size) != GIT_INSPECT_OK)
        return GIT_PATCH_CHECK_FAILED;
    if (result.exit_code == 0)
        return GIT_PATCH_ALREADY_APPLIED;
    if (result.exit_code != 1)
    {
        SetError(error, error_size, "Git could not parse the patch");
        return GIT_PATCH_CHECK_FAILED;
    }
    SetError(error, error_size, "Patch applies neither forward nor in reverse");
    return GIT_PATCH_NO_MATCH;
}

const char *AgentGitPreflightStatusName(GIT_PREFLIGHT_STATUS status)
{
    switch (status)
    {
        case GIT_PREFLIGHT_READY: return "ready";
        case GIT_PREFLIGHT_NOT_REPOSITORY: return "not_repository";
        case GIT_PREFLIGHT_INSPECTION_FAILED: return "inspection_failed";
        case GIT_PREFLIGHT_STALE_HEAD: return "stale_head";
        case GIT_PREFLIGHT_WRONG_BRANCH: return "wrong_branch";
        case GIT_PREFLIGHT_DETACHED_HEAD: return "detached_head";
        case GIT_PREFLIGHT_CONFLICTS: return "conflicts";
        case GIT_PREFLIGHT_DIRTY_TREE: return "dirty_tree";
        case GIT_PREFLIGHT_NO_UPSTREAM: return "no_upstream";
        case GIT_PREFLIGHT_REMOTE_ADVANCED: return "remote_advanced";
        default: return "unknown";
    }
}
