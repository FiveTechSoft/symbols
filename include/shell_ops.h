/* shell_ops.h: deterministic hardening operators for POSIX shell scripts.
 *
 * Perceive: a script is a workspace file ending in .sh or starting with a
 * #!/bin/sh or #!/bin/bash (or env sh/bash) line.
 * Operate: one general rule per call, chosen from what the task states:
 *   shebang      - add #!/bin/sh when the script has no #! line
 *   fail_closed  - add set -e after the #! line when it is absent
 *   quote_vars   - double-quote bare parameter expansions ($x, $1, ${x})
 *   file_guard   - wrap a command that sources/runs a named missing file in
 *                  if [ -f F ]; then ...; else exit N; fi
 * Verify (caller): the rule's static intent holds and `sh -n` accepts the
 * result; scripts are never executed by the agent.
 * Abstain: return NULL when no rule's preconditions hold or more than one
 * target is possible.
 */
#ifndef SHELL_OPS_H
#define SHELL_OPS_H

#include <stddef.h>

int   ShellOpsIsScript(const char *rel, const char *data);
/* returns a malloc'd new text, rule name in rule[]; NULL = abstain */
char *ShellOpsApply(const char *data, const char *task, char *rule, size_t rule_size,
                    char *detail, size_t detail_size);
/* static intent of rule on the edited text (1 holds, 0 not) */
int   ShellOpsIntent(const char *data, const char *rule);

#endif
