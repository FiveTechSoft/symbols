/* shell_contract.h: repair a shell script against the behaviour the task
 * states, by searching single edits and running each one.
 *
 * Perceive (task text, no ids): the one quoted invocation `sh S ARGS` or
 * `bash S ARGS` and, in clauses that describe the wanted behaviour (clauses
 * that describe the current behaviour - "currently", "right now",
 * "instead", "fails with", "but it" - and parenthesised text are skipped):
 *   stdout   - "print(s) `X`" / "stdout ... `X`" / "output ... `X`", or
 *              "print the argument" (= the invocation's last argument)
 *   exit     - "status N" / "exit N" / "exit code N", or "non-zero"
 *   file     - "file F ... contain ... `W`"
 * Operate: single-edit candidates on the script:
 *   tier 1 eq_test (== -> = in [ ]), bracket_space ([x -> [ x, x] -> x ]),
 *          assign_space (v = x -> v=x), quote_expansion (echo/printf args),
 *          fail_closed (set -e, only when a non-zero exit is wanted)
 *   tier 2 exit_literal (one exit K -> the wanted N), echo_literal (an echo
 *          whose argument has no expansion prints the wanted X),
 *          default_param ($1 -> ${1:-X} when the invocation passes no
 *          argument), file_write (echo W > F appended when nothing writes F)
 * Verify (caller): each candidate runs in a throwaway copy; only the one
 * candidate of the lowest tier meeting every stated expectation is kept.
 */
#ifndef SHELL_CONTRACT_H
#define SHELL_CONTRACT_H

#include <stddef.h>

typedef struct {
    char script[128];
    char args[4][96];
    int  nargs;
    int  has_out;  char out[128];
    int  exit_want;        /* -1 none, -2 non-zero, else the code */
    int  has_file; char file[96]; char word[96];
} SH_CONTRACT;

typedef struct {
    char *text;
    int  tier;
    char rule[24];
    char detail[120];
} SH_CAND;

/* 1 when an invocation and at least one expectation were read */
int  ShellContractParse(const char *task, SH_CONTRACT *c);
int  ShellContractCandidates(const char *script, const SH_CONTRACT *c, SH_CAND *out, int max);
void ShellContractFree(SH_CAND *c, int n);

#endif
