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
} GIT_REPOSITORY_STATE;

typedef struct
{
    const char *expected_head;
    const char *expected_branch;
    bool require_clean;
    bool allow_detached_head;
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
    GIT_PREFLIGHT_DIRTY_TREE
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
