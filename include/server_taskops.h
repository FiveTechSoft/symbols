/* server_taskops.h: run the task operators (TaskOpsSolve) for a client
 * workspace without touching it, and hand back the verified result as
 * per-file edits the client applies itself.
 *
 * Perceive: the workspace's text files are copied into a scratch
 * directory. Operate + verify: TaskOpsSolve runs there with its own
 * verification (shell rules stay static: intent + sh -n). Hand back: for
 * each changed file one hunk (the smallest line range that differs,
 * widened until its old text is unique in the file), or the whole text
 * for a created file. Repository-state operators (git_ops) run on a full
 * copy including .git; their result comes back as the git command the
 * client runs (plus hunks for resolved conflicts). Abstain (0) when no edit was kept, a file was
 * deleted, or a hunk does not fit a tool call.
 */
#ifndef SERVER_TASKOPS_H
#define SERVER_TASKOPS_H

#include <stddef.h>

#define STO_MAX_HUNKS 16

typedef struct {
    char  rel[512];
    int   created;      /* 1 = new file: new_text is the whole file */
    char *old_text;     /* unique in the original file (NULL when created) */
    char *new_text;
} StoHunk;

typedef struct {
    char    op[32];
    char    detail[256];
    char    reason[256];
    int     nhunks;
    StoHunk hunks[STO_MAX_HUNKS];
    char    bash[480];    /* repository-state step run after the hunks ("" = none) */
} StoPlan;

/* 1 = verified edit planned; 0 = abstain (reason filled). max_arg bounds
   old_text + new_text so a hunk fits one client tool call. */
int  StoPlanTask(const char *workdir, const char *task, size_t max_arg, StoPlan *p);
void StoPlanFree(StoPlan *p);

/* Minimal unique hunk between two texts (exposed for tests). 1 on success. */
int  StoMinimalHunk(const char *before, const char *after, char **old_text, char **new_text);

#endif
