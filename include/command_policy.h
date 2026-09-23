/* command_policy.h: fail-closed safety classification of shell command lines.
 *
 * Every command the agent would run is split into simple commands (on ; & |
 * && || newlines and inside $( ) and backticks), each is tokenized with
 * POSIX quoting, and the worst class wins:
 *   READ         - inspects state only (git status/diff/log/show, ls, cat...)
 *   WRITE        - changes the workspace in a recoverable way (build, git add/commit)
 *   DESTRUCTIVE  - loses work or history, or reaches outside the workspace
 *                  (force push, reset --hard, clean -f, rm -r, sudo, curl|sh...)
 *   UNPARSEABLE  - unbalanced quotes or other syntax the classifier cannot
 *                  read; treated like DESTRUCTIVE by callers
 * Unknown programs are WRITE: they may change files, never silently allowed
 * as READ. No command is executed here.
 */
#ifndef COMMAND_POLICY_H
#define COMMAND_POLICY_H

#include <stddef.h>

typedef enum
{
    POLICY_READ = 0,
    POLICY_WRITE = 1,
    POLICY_DESTRUCTIVE = 2,
    POLICY_UNPARSEABLE = 3
} POLICY_CLASS;

POLICY_CLASS CommandPolicyClassify(const char *cmdline, char *reason, size_t reason_size);
const char  *CommandPolicyName(POLICY_CLASS c);
/* 1 when the class may run without an explicit destructive grant */
int          CommandPolicyAllowed(POLICY_CLASS c);

#endif
