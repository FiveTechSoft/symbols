/* test_shell_ops.c: shell hardening rules apply, verify, and abstain. */
#include "shell_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static char *apply(const char *d, const char *task, char *rule)
{
    char detail[128];
    return ShellOpsApply(d, task, rule, 32, detail, sizeof(detail));
}

int main(void)
{
    char rule[32];
    char *o;

    CHECK(ShellOpsIsScript("x.sh", "echo"));
    CHECK(ShellOpsIsScript("build", "#!/bin/bash\necho"));
    CHECK(!ShellOpsIsScript("main.c", "int main;"));

    o = apply("echo ok\n", "Add a shebang line.", rule);
    CHECK(o && !strcmp(rule, "shebang") && !strcmp(o, "#!/bin/sh\necho ok\n") && ShellOpsIntent(o, rule));
    free(o);
    CHECK(apply("#!/bin/sh\necho ok\n", "Add a shebang line.", rule) == NULL);   /* already there */

    o = apply("#!/bin/sh\nfalse\necho hi\n", "Make it stop on error with set -e.", rule);
    CHECK(o && !strcmp(rule, "fail_closed") && !strcmp(o, "#!/bin/sh\nset -e\nfalse\necho hi\n"));
    free(o);
    CHECK(apply("#!/bin/sh\nset -eu\nfalse\n", "use set -e", rule) == NULL);        /* already fail-closed */

    o = apply("#!/bin/sh\n# $1 in comment\necho $1 \"$2\" '$3' ${HOME}\n", "The variable is unquoted.", rule);
    CHECK(o && !strcmp(rule, "quote_vars") &&
          !strcmp(o, "#!/bin/sh\n# $1 in comment\necho \"$1\" \"$2\" '$3' \"${HOME}\"\n") && ShellOpsIntent(o, rule));
    free(o);
    CHECK(!ShellOpsIntent("echo $x\n", "quote_vars"));

    o = apply("#!/bin/sh\nsh helper.sh\n", "Exit 3 when helper.sh is missing.", rule);
    CHECK(o && !strcmp(rule, "file_guard") && strstr(o, "if [ -f helper.sh ]; then\n    sh helper.sh\nelse\n    exit 3\nfi"));
    free(o);
    /* abstain: two run lines, no stated code, conflicting codes, file not named */
    CHECK(apply("sh a.sh\nsh a.sh\n", "exit 3 when a.sh is missing", rule) == NULL);
    CHECK(apply("sh a.sh\n", "fail when a.sh is missing", rule) == NULL);
    CHECK(apply("sh a.sh\n", "exit 3 or exit 4 when a.sh is missing", rule) == NULL);
    CHECK(apply("sh a.sh\n", "exit 3 when the helper is missing", rule) == NULL);
    /* found by test_core_fuzz: a lone trailing $ and ${ with quotes are not expansions */
    CHECK(apply("echo $", "quote the variables", rule) == NULL);
    o = apply("echo $1 ${\"}\n", "The variable is unquoted.", rule);
    CHECK(o && !strcmp(o, "echo \"$1\" ${\"}\n"));
    free(o);
    /* no rule stated */
    CHECK(apply("#!/bin/sh\necho $1\n", "Print a greeting.", rule) == NULL);

    printf("test_shell_ops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
