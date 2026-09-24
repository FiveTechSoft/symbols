/* test_git_ops.c: repository-state operators, verified with git itself.
 * Synthetic repositories only (no engineering-bank task is used here). */
#include "git_ops.h"
#include "task_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void test_union(void)
{
    char *o = GitOpsUnionConflict("<<<<<<< HEAD\nalpha\n=======\nbeta\n>>>>>>> feature-b\n");
    CHECK(o && !strcmp(o, "alpha\nbeta\n"));
    free(o);
    o = GitOpsUnionConflict("top\n<<<<<<< HEAD\nx\nsame\n=======\nsame\ny\n>>>>>>> b\nend\n");
    CHECK(o && !strcmp(o, "top\nx\nsame\ny\nend\n"));
    free(o);
    /* diff3 style: the base section is dropped */
    o = GitOpsUnionConflict("<<<<<<< ours\na\n||||||| base\nbase\n=======\nb\n>>>>>>> theirs\n");
    CHECK(o && !strcmp(o, "a\nb\n"));
    free(o);
    CHECK(GitOpsUnionConflict("no conflict here\n") == NULL);
    CHECK(GitOpsUnionConflict("<<<<<<< HEAD\nalpha\n") == NULL);           /* unterminated */
    CHECK(GitOpsUnionConflict("a\n=======\nb\n") == NULL);                 /* stray marker */
}

#ifndef _WIN32
#include <unistd.h>

static char base_dir[256];

static int sh(const char *dir, const char *script)
{
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t "
             "GIT_COMMITTER_EMAIL=t@t GIT_CONFIG_NOSYSTEM=1 HOME='%s' && { %s ; } >/dev/null 2>&1",
             dir, base_dir, script);
    return system(cmd);
}

static const char *repo(const char *name, const char *script)
{
    static char d[512];
    snprintf(d, sizeof(d), "%s/%s", base_dir, name);
    char mk[600];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", d);
    if (system(mk) != 0 || sh(d, "git init -q -b main") != 0 || sh(d, script) != 0)
        printf("setup failed for %s\n", name);
    return d;
}

static char *slurp(const char *dir, const char *rel)
{
    char p[800];
    snprintf(p, sizeof(p), "%s/%s", dir, rel);
    FILE *f = fopen(p, "rb");
    if (!f)
        return NULL;
    static char b[4096];
    size_t n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[n] = '\0';
    return b;
}

#define MERGE_SETUP \
    "echo base > c.txt && git add . && git commit -qm base && " \
    "git checkout -qb fa && echo alpha > c.txt && git commit -qam a && " \
    "git checkout -q main && git checkout -qb fb && echo beta > c.txt && git commit -qam b && " \
    "git checkout -q main && git merge -q fa -m ma && ! git merge -q fb"

#define CALC_GOOD "printf '#include <stdio.h>\\nint main(void) { printf(\"ok\\\\n\"); return 0; }\\n' > calc.c"
#define CALC_BAD  "printf '#include <stdio.h>\\nint main(void) { printf(\"ok\\\\n\"); return 0 }\\n' > calc.c"

static void test_repos(void)
{
    GIT_OPS_RESULT r;
    const char *d;
    char *s;
    int gcc = system("gcc --version >/dev/null 2>&1") == 0;

    /* merge, keep both: resolved and committed as a two-parent merge */
    d = repo("m1", MERGE_SETUP);
    CHECK(GitOpsSolve(d, "Resolve the conflict in c.txt keeping both lines and finish the merge.", &r) == 1);
    CHECK(!strcmp(r.op, "resolve_merge") && r.verified);
    s = slurp(d, "c.txt");
    CHECK(s && !strcmp(s, "alpha\nbeta\n"));
    CHECK(sh(d, "! git rev-parse -q --verify MERGE_HEAD && git rev-parse -q --verify HEAD^2") == 0);
    CHECK(sh(d, "test -z \"$(git status --porcelain)\"") == 0);

    /* merge without "both": no guess, conflict left exactly as it was */
    d = repo("m2", MERGE_SETUP);
    CHECK(GitOpsSolve(d, "Fix the merge in c.txt.", &r) == 0);
    CHECK(!r.verified && strstr(r.reason, "both"));
    CHECK(sh(d, "git rev-parse -q --verify MERGE_HEAD && grep -q '^<<<<<<<' c.txt") == 0);

    /* restore a file HEAD deleted and the task names */
    d = repo("r1", "printf 'x\\ny\\n' > notes.txt && echo k > keep.txt && git add . && git commit -qm add && "
                   "git rm -q notes.txt && git commit -qm drop");
    CHECK(GitOpsSolve(d, "The last commit deleted notes.txt by mistake. Restore it.", &r) == 1);
    s = slurp(d, "notes.txt");
    CHECK(s && !strcmp(s, "x\ny\n"));
    CHECK(sh(d, "git ls-files --error-unmatch notes.txt") == 0);

    /* a deleted file the task does not name is not touched */
    d = repo("r2", "echo x > notes.txt && echo k > keep.txt && git add . && git commit -qm add && "
                   "git rm -q notes.txt && git commit -qm drop");
    CHECK(GitOpsSolve(d, "Restore readme.md from the previous release.", &r) == -1);
    CHECK(slurp(d, "notes.txt") == NULL);

    /* named but not asked to restore */
    d = repo("r3", "echo x > notes.txt && echo k > keep.txt && git add . && git commit -qm add && "
                   "git rm -q notes.txt && git commit -qm drop");
    CHECK(GitOpsSolve(d, "Explain why notes.txt was removed.", &r) == 0);
    CHECK(slurp(d, "notes.txt") == NULL);

    if (gcc) {
        /* revert a commit that broke the build: history kept, tree back */
        d = repo("v1", CALC_GOOD " && git add . && git commit -qm good && " CALC_BAD " && git commit -qam wip");
        CHECK(GitOpsSolve(d, "The last commit left calc.c uncompilable. Undo that commit with git.", &r) == 1);
        CHECK(!strcmp(r.op, "revert_head"));
        CHECK(sh(d, "test \"$(git rev-list --count HEAD)\" = 3 && "
                    "test \"$(git rev-parse HEAD^{tree})\" = \"$(git rev-parse HEAD~2^{tree})\"") == 0);

        /* HEAD builds fine: no evidence, nothing reverted */
        d = repo("v2", CALC_GOOD " && git add . && git commit -qm good && echo '/* c */' >> calc.c && "
                       "git commit -qam more");
        CHECK(GitOpsSolve(d, "Undo the last commit.", &r) == 0);
        CHECK(sh(d, "test \"$(git rev-list --count HEAD)\" = 2") == 0);

        /* routed through TaskOpsSolve before any file operator */
        d = repo("v3", CALC_GOOD " && git add . && git commit -qm good && " CALC_BAD " && git commit -qam wip");
        TASK_OPS_REPORT rep;
        CHECK(TaskOpsSolve(d, "The last commit broke calc.c. Revert that commit.", &rep) == 1);
        CHECK(!strcmp(rep.op, "revert_head") && rep.verified);
    }

    /* not a repository: no git operator */
    d = repo("plain", "rm -rf .git && echo x > a.txt");
    CHECK(GitOpsSolve(d, "Restore a.txt and undo the last commit.", &r) == -1);
}
#endif

int main(void)
{
    test_union();
#ifndef _WIN32
    if (system("git --version >/dev/null 2>&1") == 0) {
        snprintf(base_dir, sizeof(base_dir), "/tmp/test_git_ops_%d", (int)getpid());
        char c[300];
        snprintf(c, sizeof(c), "rm -rf '%s' && mkdir -p '%s'", base_dir, base_dir);
        /* the operators commit in this process: give it a fixed identity
           and no user config (CI runners have none) */
        setenv("GIT_AUTHOR_NAME", "t", 1);
        setenv("GIT_AUTHOR_EMAIL", "t@t", 1);
        setenv("GIT_COMMITTER_NAME", "t", 1);
        setenv("GIT_COMMITTER_EMAIL", "t@t", 1);
        setenv("GIT_CONFIG_NOSYSTEM", "1", 1);
        setenv("HOME", base_dir, 1);
        setenv("XDG_CONFIG_HOME", base_dir, 1);
        if (system(c) == 0)
            test_repos();
        snprintf(c, sizeof(c), "rm -rf '%s'", base_dir);
        (void)system(c);
    } else
        printf("git not found: repository tests skipped\n");
#endif
    printf(fails ? "test_git_ops: %d failure(s)\n" : "test_git_ops: ok\n", fails);
    return fails ? 1 : 0;
}
