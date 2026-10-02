#ifndef AGENT_GIT_H
#define AGENT_GIT_H

#include <stdbool.h>
#include <stddef.h>

#define GIT_HEAD_MAX 65
#define GIT_BRANCH_MAX 256
#define GIT_ERROR_MAX 256

typedef enum
{
    GIT_INSPECT_OK = 0,
    GIT_INSPECT_NOT_REPOSITORY,
    GIT_INSPECT_COMMAND_FAILED,
    GIT_INSPECT_OUTPUT_TRUNCATED,
    GIT_INSPECT_MALFORMED_OUTPUT
} GIT_INSPECT_STATUS;

typedef struct
{
    char head[GIT_HEAD_MAX];
    char branch[GIT_BRANCH_MAX];
    bool detached_head;
    unsigned staged_paths;
    unsigned unstaged_paths;
    unsigned untracked_paths;
    unsigned ignored_paths;
    unsigned conflicted_paths;
    /* Filled only by a remote check (require_remote_in_sync). */
    char upstream[GIT_BRANCH_MAX];  /* e.g. origin/main */
    char remote_head[GIT_HEAD_MAX]; /* tip of that branch on the remote */
} GIT_REPOSITORY_STATE;

typedef struct
{
    const char *expected_head;
    const char *expected_branch;
    bool require_clean;
    bool allow_detached_head;
    /* Opt-in, read-only: ask the upstream remote for its branch tip (git
       ls-remote, no fetch, no ref or tree change) and refuse unless that tip
       is HEAD or an ancestor of HEAD. A remote that moved, diverged, is
       unreachable or whose tip is unknown locally fails closed. */
    bool require_remote_in_sync;
} GIT_PRECONDITIONS;

typedef enum
{
    GIT_PREFLIGHT_READY = 0,
    GIT_PREFLIGHT_NOT_REPOSITORY,
    GIT_PREFLIGHT_INSPECTION_FAILED,
    GIT_PREFLIGHT_STALE_HEAD,
    GIT_PREFLIGHT_WRONG_BRANCH,
    GIT_PREFLIGHT_DETACHED_HEAD,
    GIT_PREFLIGHT_CONFLICTS,
    GIT_PREFLIGHT_DIRTY_TREE,
    GIT_PREFLIGHT_NO_UPSTREAM,
    GIT_PREFLIGHT_REMOTE_ADVANCED
} GIT_PREFLIGHT_STATUS;

/* Inspect repository facts through fixed, read-only Git commands. No caller
   input is interpolated into a command line. Truncated or malformed output
   fails closed. */
GIT_INSPECT_STATUS AgentGitInspect(const char *working_dir,
                                   GIT_REPOSITORY_STATE *out,
                                   char *error,
                                   size_t error_size);

/* Check action-driving repository invariants against one fresh snapshot. */
GIT_PREFLIGHT_STATUS AgentGitPreflight(const char *working_dir,
                                       const GIT_PRECONDITIONS *required,
                                       GIT_REPOSITORY_STATE *observed,
                                       char *error,
                                       size_t error_size);

const char *AgentGitPreflightStatusName(GIT_PREFLIGHT_STATUS status);

#endif
