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
    o = apply("#!/bin/sh\necho nope\n", "run.sh must print exactly HELLO_X (with newline).", rule);
    CHECK(o && !strcmp(rule, "stated_output") && !strcmp(o, "#!/bin/sh\necho HELLO_X\n"));
    free(o);
    CHECK(apply("echo a\necho b\n", "print exactly OK", rule) == NULL);          /* two echo lines: abstain */
    CHECK(apply("#!/bin/sh\necho OK\n", "print exactly OK", rule) == NULL);      /* already prints it */
    /* no rule stated */
    CHECK(apply("#!/bin/sh\necho $1\n", "Print a greeting.", rule) == NULL);


    {   /* syntax candidates (sh -n evidence); the caller verifies with sh -n */
        SHELL_CAND c[32];
        int n, found;
#define HAS(rule_, sub_) do { found = 0; for (int i = 0; i < n; i++) if (!strcmp(c[i].rule, rule_) && strstr(c[i].text, sub_)) found = 1; CHECK(found); } while (0)
        n = ShellSyntaxCandidates("#!/bin/sh\nif [ -f x ]\n  echo a\nfi\n", c, 32);
        HAS("missing_then", "if [ -f x ]; then\n");
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\nfor f in a b\n  echo $f\ndone\n", c, 32);
        HAS("missing_do", "for f in a b; do\n");
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\nif [ -f x ]; then\n  echo a\necho b\n", c, 32);
        HAS("close_block", "  echo a\nfi\necho b\n");
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\nif [ -f x ]\nthen\n  echo a\n", c, 32);
        HAS("close_block", "  echo a\nfi\n");
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\nif [ -f x ]; then\necho a\n", c, 32);    /* flat body: no close_block */
        found = 0; for (int i = 0; i < n; i++) found |= !strcmp(c[i].rule, "close_block");
        CHECK(!found);
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\necho \"hello\necho bye\n", c, 32);
        HAS("close_quote", "echo \"hello\"\n");
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\necho a\nfi\n", c, 32);
        HAS("stray_closer", "#!/bin/sh\necho a\n");
        for (int i = 0; i < n; i++) if (!strcmp(c[i].rule, "stray_closer")) CHECK(c[i].tier == 2);
        ShellSyntaxCandidatesFree(c, n);
        n = ShellSyntaxCandidates("#!/bin/sh\n# if then \"\necho 'x \" y'\n", c, 32);  /* comment and quoted text */
        CHECK(n == 0);
        ShellSyntaxCandidatesFree(c, n);
#undef HAS
        /* fuzz: deterministic mutations never crash or yield NULL/identical text */
        const char *seed = "#!/bin/sh\nset -e\nif [ -f \"$1\" ]; then\n  for f in a b; do\n    echo \"$f\"\n  done\nfi\ncase x in\n  x) echo y ;;\nesac\n";
        const char alpha[] = "\"'#;|&(){}\\\n \tfidoneesacthen";
        char buf[512];
        unsigned r = 777u;
        size_t len = strlen(seed);
        for (int it = 0; it < 3000; it++) {
            memcpy(buf, seed, len + 1);
            for (int e = 0; e < 1 + it % 5; e++) {
                r = r * 1103515245u + 12345u;
                size_t at = (r >> 8) % len;
                r = r * 1103515245u + 12345u;
                buf[at] = alpha[(r >> 8) % (sizeof alpha - 1)];
            }
            if (it % 9 == 0) buf[(r >> 4) % len] = '\0';
            n = ShellSyntaxCandidates(buf, c, 32);
            CHECK(n >= 0 && n <= 32);
            for (int i = 0; i < n; i++) CHECK(c[i].text && strcmp(c[i].text, buf) && c[i].tier >= 1 && c[i].tier <= 2);
            ShellSyntaxCandidatesFree(c, n);
        }
    }
    /* stated argument contract: one exit code, one quoted argument word */
    o = apply("#!/bin/sh\nexit 0\n", "run.sh should exit with status 3 when the argument is 'fail'.", rule);
    CHECK(o && !strcmp(rule, "arg_exit") && !strcmp(o, "#!/bin/sh\nif [ \"$1\" = \"fail\" ]; then\n    exit 3\nfi\nexit 0\n") &&
          ShellOpsIntent(o, rule));
    free(o);
    o = apply("#!/bin/sh\nset -e\necho hi\n", "Return exit code 4 if called with `stop`.", rule);
    CHECK(o && !strcmp(o, "#!/bin/sh\nset -e\nif [ \"$1\" = \"stop\" ]; then\n    exit 4\nfi\necho hi\n"));
    free(o);
    CHECK(apply("#!/bin/sh\nexit 0\n", "Exit 3 on 'fail' or 'error' arguments.", rule) == NULL);          /* two words */
    CHECK(apply("#!/bin/sh\nexit 0\n", "Exit 3 or exit 4 when given 'fail'.", rule) == NULL);             /* two codes */
    CHECK(apply("#!/bin/sh\ncase $1 in x) exit 1;; esac\n", "Exit 3 when given 'fail'.", rule) == NULL); /* reads $1 */
    CHECK(apply("#!/bin/sh\nexit 0\n", "Exit 3 when given 'lib.sh'.", rule) == NULL);                    /* file, not a word */
    /* stated file content: one output file, one content word */
    o = apply("#!/bin/sh\ntouch output.txt\n", "run.sh must create output.txt with content OK using a redirection.", rule);
    CHECK(o && !strcmp(rule, "file_content") && !strcmp(o, "#!/bin/sh\necho OK > output.txt\n") && ShellOpsIntent(o, rule));
    free(o);
    o = apply("#!/bin/sh\necho hi\n", "Write the word \"ready\" into result.txt.", rule);
    CHECK(o && !strcmp(o, "#!/bin/sh\necho hi\necho ready > result.txt\n"));
    free(o);
    CHECK(apply("#!/bin/sh\ntouch a.txt\n", "Create a.txt with DONE and b.log with OK.", rule) == NULL);   /* two files */
    CHECK(apply("#!/bin/sh\ncc main.c\n", "Compile main.c with GCC.", rule) == NULL);                     /* source file */
    CHECK(apply("#!/bin/sh\ntouch a.txt\n", "a.txt should hold the result with something useful.", rule) == NULL); /* no stated word */
    printf("test_shell_ops: %s\n", fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
