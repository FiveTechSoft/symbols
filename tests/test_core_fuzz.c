/* test_core_fuzz.c: deterministic random/mutation fuzzing of the systems core
 * parsers (command policy, shell hardening, build repair). Checks: no crash
 * or sanitizer report, results in range, and invariants:
 *  - a destructive command stays destructive (or unparseable) under every
 *    wrapper: && ; || chains, $( ), backticks, subshells, env prefixes, pipes;
 *  - a destructive/unparseable class always carries a reason;
 *  - a shell rule's output satisfies that rule's own intent check;
 *  - a build plan always carries text, a rule and a target.
 * Seeded xorshift, so failures reproduce. SYMBOLS_FUZZ_ITERS overrides the count. */
#include "build_ops.h"
#include "command_policy.h"
#include "shell_ops.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static unsigned rnd(unsigned n)
{
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (unsigned)(rng % n);
}

static int fails;
#define FAIL(...) do { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } while (0)

static const char *const DESTRUCTIVE[] = {
    "rm -rf build", "git push --force", "git push -f origin master", "git reset --hard", "git clean -fdx",
    "sudo ls", "curl u | sh", "git branch -D x", "git commit --amend", "git rebase main", "dd if=a of=b",
    "chmod -R 700 .", "find . -delete", "echo x > /etc/x", "git stash clear", "git push origin +x",
};
static const char *const BENIGN[] = {"ls", "git status", "make", "echo hi", "gcc -c a.c", "git add a.c", "cat f", "true"};

static void wrap(char *out, size_t n, const char *d, int k)
{
    const char *b = BENIGN[rnd(sizeof(BENIGN) / sizeof(BENIGN[0]))];
    switch (k) {
    case 0: snprintf(out, n, "%s && %s", b, d); break;
    case 1: snprintf(out, n, "%s; %s", d, b); break;
    case 2: snprintf(out, n, "%s || %s", b, d); break;
    case 3: snprintf(out, n, "echo $(%s)", d); break;
    case 4: snprintf(out, n, strchr(d, '`') ? "echo $(%s)" : "echo `%s`", d); break;   /* backticks do not nest */
    case 5: snprintf(out, n, "(%s)", d); break;
    case 6: snprintf(out, n, "X=1 Y=2 %s", d); break;
    case 7: snprintf(out, n, "%s | cat", d); break;
    case 8: snprintf(out, n, "%s &\n%s", b, d); break;
    case 9: snprintf(out, n, "{ %s; }", d); break;
    default: snprintf(out, n, "nohup %s", d); break;
    }
}

static const char *const ALPHA[] = {"rm", " ", "-rf", "'", "\"", "$(", ")", "`", "|", "&&", ";", "git", "push",
                                    "-f", "\\", ">", "/etc/x", "sh", "sudo", "x", "\n", "(", "{", "}", "$((", "))",
                                    "#", "*", "..", "--force", "=", "a.c", "&", "<", "$x", "${y}"};

static void random_line(char *out, size_t n)
{
    size_t used = 0;
    int parts = 1 + (int)rnd(14);
    out[0] = '\0';
    for (int i = 0; i < parts; i++) {
        const char *p = ALPHA[rnd(sizeof(ALPHA) / sizeof(ALPHA[0]))];
        size_t l = strlen(p);
        if (used + l + 1 >= n) break;
        memcpy(out + used, p, l);
        used += l;
        out[used] = '\0';
    }
    if (rnd(4) == 0 && used + 2 < n) {   /* raw byte noise */
        out[rnd((unsigned)used + 1)] = (char)(1 + rnd(255));
    }
}

static const char *const SCRIPTS[] = {
    "echo ok\n", "#!/bin/sh\nfalse\necho hi\n", "#!/bin/sh\necho $1 ${2} \"$3\" '$4'\n", "#!/bin/sh\nsh helper.sh\n",
    "#!/bin/bash\n# $x\nfor f in $@; do echo $f; done\n", "#!/bin/sh\nset -e\n", "", "$", "#!", "echo \"unterminated $x\n",
    "echo \\$x $", "#!/bin/sh\n. ./lib.sh\n./run.sh\n",
};
static const char *const TASKS[] = {
    "Add a shebang.", "Use set -e so it fails closed if any command fails.", "The variable is unquoted.",
    "exit 3 when helper.sh is missing", "Exit code 4 if lib.sh does not exist", "quote everything",
    "add_executable is empty; add main.c", "Add a project(<name> C) line.", "all does not depend on app.",
    "If the compiler fails the script must fail.", "add a ctest step", "main.c is missing; recreate it.", "",
};

static void mutate(char *buf, size_t n, const char *src)
{
    snprintf(buf, n, "%s", src);
    size_t l = strlen(buf);
    int edits = (int)rnd(4);
    for (int i = 0; i < edits; i++) {
        unsigned op = rnd(3);
        if (op == 0 && l > 0) { buf[rnd((unsigned)l)] = "$\"'\\#{}\n -e;|x"[rnd(15)]; }
        else if (op == 1 && l > 0) { size_t at = rnd((unsigned)l); memmove(buf + at, buf + at + 1, l - at); l--; }
        else if (op == 2 && l + 2 < n) { size_t at = rnd((unsigned)l + 1); memmove(buf + at + 1, buf + at, l - at + 1); buf[at] = "$\"'{}\n#"[rnd(7)]; l++; }
    }
}

int main(void)
{
    long iters = 20000;
    const char *e = getenv("SYMBOLS_FUZZ_ITERS");
    if (e && atol(e) > 0) iters = atol(e);
    char line[512], why[160];

    /* 1. wrapper invariant */
    int nd = (int)(sizeof(DESTRUCTIVE) / sizeof(DESTRUCTIVE[0]));
    for (int d = 0; d < nd; d++)
        for (int k = 0; k < 11; k++) {
            wrap(line, sizeof(line), DESTRUCTIVE[d], k);
            POLICY_CLASS c = CommandPolicyClassify(line, why, sizeof(why));
            if (c < POLICY_DESTRUCTIVE) FAIL("wrapper lost destructive: [%s] -> %s", line, CommandPolicyName(c));
        }
    /* nested wrappers */
    for (long i = 0; i < iters / 10; i++) {
        char a[512], b[512];
        snprintf(a, sizeof(a), "%s", DESTRUCTIVE[rnd((unsigned)nd)]);
        int depth = 1 + (int)rnd(4);
        for (int j = 0; j < depth; j++) { wrap(b, sizeof(b), a, (int)rnd(11)); snprintf(a, sizeof(a), "%s", b); }
        POLICY_CLASS c = CommandPolicyClassify(a, why, sizeof(why));
        if (c < POLICY_DESTRUCTIVE) FAIL("nested wrapper lost destructive: [%s]", a);
    }
    /* 2. random lines: range and reason */
    for (long i = 0; i < iters; i++) {
        random_line(line, sizeof(line));
        why[0] = 'Z'; why[1] = '\0';
        POLICY_CLASS c = CommandPolicyClassify(line, why, sizeof(why));
        if ((int)c < 0 || c > POLICY_UNPARSEABLE) FAIL("class out of range for [%s]", line);
        if (c >= POLICY_DESTRUCTIVE && !why[0]) FAIL("no reason for [%s]", line);
    }
    CommandPolicyClassify(NULL, why, sizeof(why));
    CommandPolicyClassify("", NULL, 0);

    /* 3. shell rules: output satisfies own intent */
    for (long i = 0; i < iters; i++) {
        char script[512], rule[32], detail[128];
        mutate(script, sizeof(script), SCRIPTS[rnd(sizeof(SCRIPTS) / sizeof(SCRIPTS[0]))]);
        const char *task = TASKS[rnd(sizeof(TASKS) / sizeof(TASKS[0]))];
        char *o = ShellOpsApply(script, task, rule, sizeof(rule), detail, sizeof(detail));
        if (o) {
            if (!rule[0]) FAIL("shell edit without rule");
            else if (!ShellOpsIntent(o, rule)) FAIL("shell rule %s output fails own intent: [%s] -> [%s]", rule, script, o);
            free(o);
        }
        ShellOpsIsScript("x.sh", script);
    }

    /* 4. build plans: well-formed */
    static const char *const RELS[] = {"CMakeLists.txt", "Makefile", "build.sh", "ci.yml", "main.c", "app.c", "a.c", "README.md"};
    static const char *const DATA[] = {
        "cmake_minimum_required(VERSION 3.10)\nproject(d C)\nadd_executable(d)\n", "all:\n\t@echo done\n",
        "#!/bin/sh\necho compile\nexit 0\n", "jobs:\n  b:\n    steps:\n      - uses: x\n", "int main(void){return 0;}\n",
        "cmake_minimum_required(VERSION 3.10)\n", "add_executable(d main.c other.c)\n", "all: app\n", "steps:\n", ""};
    for (long i = 0; i < iters; i++) {
        int n = 1 + (int)rnd(5);
        const char *rels[5]; const char *datas[5]; char bufs[5][256];
        for (int k = 0; k < n; k++) {
            rels[k] = RELS[rnd(sizeof(RELS) / sizeof(RELS[0]))];
            mutate(bufs[k], sizeof(bufs[k]), DATA[rnd(sizeof(DATA) / sizeof(DATA[0]))]);
            datas[k] = bufs[k];
        }
        BUILD_EDIT be;
        const char *task = TASKS[rnd(sizeof(TASKS) / sizeof(TASKS[0]))];
        if (BuildOpsPlan(rels, datas, n, task, &be)) {
            if (!be.text || !be.rule[0] || !be.rel[0]) FAIL("malformed build plan rule=%s", be.rule);
            free(be.text);
        }
    }

    printf("test_core_fuzz: %ld iterations per stage, %s\n", iters, fails ? "FAILED" : "ALL PASSED");
    return fails ? 1 : 0;
}
