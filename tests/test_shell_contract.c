/* test_shell_contract.c: the stated contract is read from the task and
   single-edit candidates are generated; current-state clauses are skipped. */
#include "shell_contract.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int has(SH_CAND *c, int n, const char *rule, const char *needle)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(c[i].rule, rule) && (!needle || strstr(c[i].text, needle)))
            return 1;
    return 0;
}

int main(void)
{
    SH_CONTRACT k;
    SH_CAND c[48];
    int n;

    CHECK(ShellContractParse("Invoking `sh run.sh bad` must return status 4 (currently 1); other arguments keep returning 0.", &k));
    CHECK(!strcmp(k.script, "run.sh") && k.nargs == 1 && !strcmp(k.args[0], "bad") && k.exit_want == 4 && !k.has_out);
    n = ShellContractCandidates("#!/bin/sh\nif [ \"$1\" = bad ]; then\n    exit 1\nfi\n", &k, c, 48);
    CHECK(has(c, n, "exit_literal", "    exit 4\n"));
    ShellContractFree(c, n);

    CHECK(ShellContractParse("When a step fails the script must stop and exit non-zero; right now `sh run.sh` keeps going and prints `continued`.", &k));
    CHECK(k.exit_want == -2 && !k.has_out);   /* the "right now" clause is not a contract */

    CHECK(ShellContractParse("Running `sh run.sh \"x   y\"` must print the argument exactly as given.", &k));
    CHECK(k.has_out && !strcmp(k.out, "x   y"));
    n = ShellContractCandidates("#!/bin/sh\nname=$1\necho $name\n", &k, c, 48);
    CHECK(has(c, n, "quote_expansion", "echo \"$name\"") && !has(c, n, "echo_literal", NULL));   /* expansions never become constants */
    ShellContractFree(c, n);

    CHECK(ShellContractParse("After `sh run.sh`, the file status.txt must exist and contain the single line `READY`.", &k));
    CHECK(k.has_file && !strcmp(k.file, "status.txt") && !strcmp(k.word, "READY"));

    CHECK(ShellContractParse("`sh run.sh` fails with \"greeting: not found\"; it must print `hi`.", &k));
    n = ShellContractCandidates("#!/bin/sh\ngreeting = \"hi\"\necho \"$greeting\"\n", &k, c, 48);
    CHECK(has(c, n, "assign_space", "greeting=\"hi\""));
    ShellContractFree(c, n);

    CHECK(ShellContractParse("Under dash, `sh run.sh yes` must print `on`, but it prints an error.", &k));
    n = ShellContractCandidates("if [ \"$1\" == yes ]; then echo on; fi\nif [\"$1\" = x]; then :; fi\n", &k, c, 48);
    CHECK(has(c, n, "eq_test", "\"$1\" = yes") && has(c, n, "bracket_space", "[ \"$1\" = x ]"));
    ShellContractFree(c, n);

    /* no invocation, two scripts, or no expectation: nothing to check */
    CHECK(!ShellContractParse("Make run.sh print `ok`.", &k));
    CHECK(!ShellContractParse("`sh a.sh` must print `x` and `sh b.sh` must print `y`.", &k));
    CHECK(!ShellContractParse("Clean up `sh run.sh` a bit.", &k));

    printf("%s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
