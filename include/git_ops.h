/* git_ops.h: repository-state operators, verified with git itself.
 *
 * Perceive: the workspace is the top of a git work tree; git reports the
 * state (merge in progress, files deleted by HEAD, HEAD failing to build).
 * Operate: one operator per call, only when the state and the task agree:
 *   resolve_merge   - a merge stopped on conflicts and the task asks to keep
 *                     both sides: each hunk becomes ours then theirs (lines
 *                     already present are not repeated), git add, commit
 *   restore_deleted - HEAD deleted a file the task names and it is absent
 *                     from the work tree: git checkout HEAD~1 -- FILE
 *   revert_head     - the task asks to undo/revert the last commit, the tree
 *                     is clean, HEAD's C sources fail to compile and
 *                     HEAD~1's compile: git revert --no-edit HEAD (history
 *                     is kept; nothing is reset)
 * Verify: git state after the operation (MERGE_HEAD gone, clean status, no
 * markers; blob equals HEAD~1's and is tracked; HEAD tree equals the old
 * HEAD~1 tree and the sources compile). Anything else rolls back.
 * Abstain: return -1 when no operator's preconditions hold, so the file
 * operators run as before.
 */
#ifndef GIT_OPS_H
#define GIT_OPS_H

#include <stddef.h>

typedef struct {
    char op[32];
    char detail[256];
    char reason[256];
    int  verified;
} GIT_OPS_RESULT;

/* 1 = operator applied and verified, 0 = a git operator matched but was
   not kept (state restored), -1 = no git operator applies */
int GitOpsSolve(const char *root, const char *task, GIT_OPS_RESULT *out);

/* Pure text step of resolve_merge: malloc'd text with each conflict hunk
   replaced by its ours lines then the theirs lines not already there; NULL
   when the text has no well-formed hunk. */
char *GitOpsUnionConflict(const char *text);

#endif
