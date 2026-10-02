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

/* Phase 2: commit contract and "already applied" detection. Read-only. */

/* One path a reviewed mutation manifest says will change. status is 'A'
   (added), 'M' (modified), 'D' (deleted), or 0 when only the path matters.
   Renames are not detected (git runs with --no-renames), so a rename is a
   'D' plus an 'A'. */
typedef struct
{
    const char *path;
    char status;
} GIT_EXPECTED_CHANGE;

typedef enum
{
    GIT_CHANGES_MATCH = 0,
    GIT_CHANGES_MISMATCH,
    GIT_CHANGES_INSPECTION_FAILED
} GIT_CHANGES_STATUS;

/* The index differs from HEAD in exactly the expected paths (and statuses
   where given): nothing extra, nothing missing. Unstaged and untracked files
   are not part of the index and are ignored here; AgentGitPreflight owns
   them. error names the first difference. Paths git would quote (quote,
   newline, tab) fail closed as INSPECTION_FAILED. */
GIT_CHANGES_STATUS AgentGitStagedMatches(const char *working_dir,
                                         const GIT_EXPECTED_CHANGE *expected,
                                         size_t count,
                                         char *error,
                                         size_t error_size);

/* The HEAD commit differs from its parent (or from the empty tree for a root
   commit) in exactly the expected paths. A merge commit never matches. */
GIT_CHANGES_STATUS AgentGitHeadCommitMatches(const char *working_dir,
                                             const GIT_EXPECTED_CHANGE *expected,
                                             size_t count,
                                             char *error,
                                             size_t error_size);

typedef enum
{
    GIT_PATCH_NOT_APPLIED = 0,   /* applies forward to the working tree */
    GIT_PATCH_ALREADY_APPLIED,   /* applies in reverse: its changes are present */
    GIT_PATCH_NO_MATCH,          /* applies neither way (diverged or partial) */
    GIT_PATCH_CHECK_FAILED       /* unreadable, corrupt or unsafe patch path */
} GIT_PATCH_STATE;

/* Tell a retry whether a patch is already present. Uses git apply --check,
   which writes nothing. Forward application wins when both directions apply.
   The patch path is interpolated into a command line, so only
   [A-Za-z0-9._/-] is accepted: relative, no leading '-' or '/', no "..". The check is against the
   working tree, so run it after a clean preflight. */
GIT_PATCH_STATE AgentGitPatchState(const char *working_dir,
                                   const char *patch_path,
                                   char *error,
                                   size_t error_size);

const char *AgentGitPreflightStatusName(GIT_PREFLIGHT_STATUS status);

#endif
