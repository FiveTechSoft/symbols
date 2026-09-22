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
            out->untracked_paths++;
        else if (line[0] == '!' && line[1] == ' ')
            out->ignored_paths++;
        else if (line[0] != '\0')
        {
            SetError(error, error_size, "Unknown Git status entry");
            return GIT_INSPECT_MALFORMED_OUTPUT;
        }
        line = strtok(NULL, "\n");
    }
    return GIT_INSPECT_OK;
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
    return GIT_PREFLIGHT_READY;
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
        default: return "unknown";
    }
}
