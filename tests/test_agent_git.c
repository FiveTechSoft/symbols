#include "agent_git.h"
#include "agent_shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static int tests_run;
static int tests_passed;
#define CHECK(expr, message) do { tests_run++; if (expr) { tests_passed++; printf("  [PASS] %s\n", message); } else printf("  [FAIL] %s (line %d)\n", message, __LINE__); } while (0)

static int WriteFile(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    size_t n = strlen(text);
    if (f == NULL) return 0;
    if (fwrite(text, 1, n, f) != n) { fclose(f); return 0; }
    return fclose(f) == 0;
}

static int Run(const char *cwd, const char *command, int expected)
{
    SHELL_EXEC_RESULT result;
    if (!AgentShellExec(command, cwd, 10000, &result)) return 0;
    return !result.execution_failed && !result.timed_out && result.exit_code == expected;
}

/* M2: a refused preflight must leave the repository and the user's files exactly as they were. */
static int Out(const char *cwd, const char *cmd, char *buf, size_t n)
{
    SHELL_EXEC_RESULT r;
    size_t k;
    if (!AgentShellExec(cmd, cwd, 10000, &r) || r.execution_failed || r.exit_code != 0) return 0;
    snprintf(buf, n, "%s", r.stdout_buf);
    k = strlen(buf);
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    return 1;
}

static void FileBytes(const char *path, char *out, size_t n)
{
    FILE *f = fopen(path, "rb");
    size_t got = 0;
    if (f == NULL) { snprintf(out, n, "<absent>"); return; }
    got = fread(out, 1, n - 1, f);
    out[got] = 0;
    fclose(f);
}

/* HEAD, every ref, porcelain v2 (with untracked), index entries by stage (unmerged show as 1/2/3),
   and the bytes of the three user files. */
static int Snapshot(const char *repo, char *out, size_t n)
{
    char head[128] = "", refs[1024] = "", status[2048] = "", index[2048] = "";
    char a[256], b[256], c[256];
    if (!Out(repo, "git rev-parse HEAD", head, sizeof(head)) &&
        !Out(repo, "git symbolic-ref -q HEAD", head, sizeof(head))) return 0;
    if (!Out(repo, "git for-each-ref", refs, sizeof(refs)) ||
        !Out(repo, "git status --porcelain=v2 --branch --untracked-files=all", status, sizeof(status)) ||
        !Out(repo, "git ls-files -s", index, sizeof(index))) return 0;
    FileBytes("test_agent_git_repo/tracked.txt", a, sizeof(a));
    FileBytes("test_agent_git_repo/staged.txt", b, sizeof(b));
    FileBytes("test_agent_git_repo/untracked.txt", c, sizeof(c));
    snprintf(out, n, "HEAD:%s\nREFS:%s\nSTATUS:%s\nINDEX:%s\nTRACKED:%s\nSTAGED:%s\nUNTRACKED:%s\n",
             head, refs, status, index, a, b, c);
    return 1;
}

static char g_selfcheck_before[8192];

/* Text of one snapshot section (label 0..6), for the detection-power cells. */
static const char *kSnapLabels[] = {"HEAD:", "REFS:", "STATUS:", "INDEX:", "TRACKED:", "STAGED:", "UNTRACKED:"};
static void SnapSection(const char *snap, int label, char *out, size_t n)
{
    char key[32];
    const char *start, *end;
    size_t len;
    snprintf(key, sizeof(key), "%s%s", label == 0 ? "" : "\n", kSnapLabels[label]);
    start = strstr(snap, key);
    out[0] = 0;
    if (start == NULL) return;
    start += strlen(key);
    if (label < 6) {
        snprintf(key, sizeof(key), "\n%s", kSnapLabels[label + 1]);
        end = strstr(start, key);
    } else end = NULL;
    len = end ? (size_t)(end - start) : strlen(start);
    if (len >= n) len = n - 1;
    memcpy(out, start, len);
    out[len] = 0;
}

/* The snapshot must differ in exactly the named section when that dimension is changed. */
static int DiffersIn(const char *repo, int label)
{
    char after[8192], a[4096], b[4096];
    if (!Snapshot(repo, after, sizeof(after))) return 0;
    SnapSection(g_selfcheck_before, label, a, sizeof(a));
    SnapSection(after, label, b, sizeof(b));
    if (strcmp(a, b) == 0) printf("    section %s did not change\n", kSnapLabels[label]);
    return strcmp(a, b) != 0;
}

static int RefusedUntouched(const char *repo, const GIT_PRECONDITIONS *required,
                            GIT_PREFLIGHT_STATUS want, const char *message)
{
    GIT_REPOSITORY_STATE state;
    char err[512] = "", before[8192], after[8192];
    GIT_PREFLIGHT_STATUS got;
    int ok;
    if (!Snapshot(repo, before, sizeof(before))) { printf("    snapshot before failed\n"); return 0; }
    got = AgentGitPreflight(repo, required, &state, err, sizeof(err));
    if (!Snapshot(repo, after, sizeof(after))) { printf("    snapshot after failed\n"); return 0; }
    ok = got == want && strcmp(before, after) == 0;
    if (!ok) {
        printf("    %s: got status %d want %d, snapshots %s\n--- before\n%s--- after\n%s",
               message, (int)got, (int)want, strcmp(before, after) == 0 ? "equal" : "DIFFER", before, after);
    }
    return ok;
}

static void ResetFixture(void)
{
#ifdef _WIN32
    (void)system("if exist test_agent_git_repo rmdir /s /q test_agent_git_repo");
#else
    (void)system("rm -rf test_agent_git_repo");
#endif
    MKDIR("test_agent_git_repo");
}

int main(void)
{
    const char *repo = "test_agent_git_repo";
    GIT_REPOSITORY_STATE state;
    GIT_PRECONDITIONS required;
    GIT_PREFLIGHT_STATUS preflight;
    char error[GIT_ERROR_MAX];
    char base[GIT_HEAD_MAX];

    printf("=== Native Git inspection and fail-closed preflight ===\n");
    ResetFixture();
    CHECK(Run(repo, "git init -b main", 0) &&
          Run(repo, "git config user.name Fixture", 0) &&
          Run(repo, "git config user.email fixture@example.invalid", 0),
          "real fixture repository initialized with local merge identity");
    CHECK(WriteFile("test_agent_git_repo/.gitignore", "*.tmp\n"), "ignore fixture written");
    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "base\n"), "tracked fixture written");
    CHECK(Run(repo, "git add .gitignore tracked.txt", 0), "fixture files staged");
    CHECK(Run(repo, "git -c user.name=Fixture -c user.email=fixture@example.invalid commit -m base", 0), "base commit created");

    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK,
          "clean repository inspected");
    CHECK(strcmp(state.branch, "main") == 0 && !state.detached_head,
          "branch is structured state");
    CHECK(state.staged_paths == 0 && state.unstaged_paths == 0 &&
          state.untracked_paths == 0 && state.conflicted_paths == 0,
          "clean state has no changes or conflicts");
    snprintf(base, sizeof(base), "%s", state.head);

    memset(&required, 0, sizeof(required));
    required.expected_head = base;
    required.expected_branch = "main";
    required.require_clean = true;
    preflight = AgentGitPreflight(repo, &required, &state, error, sizeof(error));
    CHECK(preflight == GIT_PREFLIGHT_READY, "known clean base passes preflight");

    CHECK(WriteFile("test_agent_git_repo/cache.tmp", "ignored\n"), "ignored file created");
    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.ignored_paths == 1 && state.untracked_paths == 0,
          "ignored path is reported separately and is not dirty");
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_READY,
          "ignored-only state does not fail clean-tree preflight");

    CHECK(WriteFile("test_agent_git_repo/untracked.txt", "new\n"), "untracked file created");
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_DIRTY_TREE,
          "untracked path fails closed as dirty");
    CHECK(Run(repo, "git add untracked.txt", 0), "untracked file staged");
    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.staged_paths == 1, "staged path is reported");
    CHECK(Run(repo, "git reset --quiet", 0), "fixture staging reset explicitly");
    remove("test_agent_git_repo/untracked.txt");

    CHECK(WriteFile("test_agent_git_repo/.fstxn.lock", "") &&
          WriteFile("test_agent_git_repo/.fsrp-0123abcd", "x") &&
          WriteFile("test_agent_git_repo/.fstxn-0123abcd", "x"),
          "Fs* control artifacts created");
    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.untracked_paths == 0 &&
          AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_READY,
          "Fs* control artifacts are not user changes");
    CHECK(WriteFile("test_agent_git_repo/.fsrpx", "x") &&
          AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.untracked_paths == 1, "similar non-control name still counts as untracked");
    remove("test_agent_git_repo/.fsrpx");
    remove("test_agent_git_repo/.fstxn.lock");
    remove("test_agent_git_repo/.fsrp-0123abcd");
    remove("test_agent_git_repo/.fstxn-0123abcd");

    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "modified\n"), "tracked file modified");
    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.unstaged_paths == 1, "unstaged tracked path is reported");
    CHECK(Run(repo, "git checkout -- tracked.txt", 0), "fixture modification restored explicitly");

    required.expected_head = "0000000000000000000000000000000000000000";
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_STALE_HEAD,
          "stale expected HEAD fails closed");
    required.expected_head = base;
    required.expected_branch = "other";
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_WRONG_BRANCH,
          "unexpected branch fails closed");
    required.expected_branch = "main";

    CHECK(Run(repo, "git checkout --detach --quiet", 0), "fixture enters detached HEAD");
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_DETACHED_HEAD,
          "detached HEAD fails closed");
    CHECK(Run(repo, "git checkout main --quiet", 0), "fixture returns to main");

    CHECK(Run(repo, "git checkout -b side --quiet", 0), "conflict branch created");
    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "side\n"), "side content written");
    CHECK(Run(repo, "git add tracked.txt", 0) &&
          Run(repo, "git -c user.name=Fixture -c user.email=fixture@example.invalid commit -m side --quiet", 0),
          "side commit created");
    CHECK(Run(repo, "git checkout main --quiet", 0), "returned to main for conflict fixture");
    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "main\n"), "main content written");
    CHECK(Run(repo, "git add tracked.txt", 0) &&
          Run(repo, "git -c user.name=Fixture -c user.email=fixture@example.invalid commit -m main --quiet", 0),
          "main commit created");
    CHECK(Run(repo, "git merge side --no-edit", 1), "real merge conflict produced");
    required.expected_head = NULL;
    CHECK(AgentGitInspect(repo, &state, error, sizeof(error)) == GIT_INSPECT_OK &&
          state.conflicted_paths == 1, "unmerged path is structured conflict state");
    CHECK(AgentGitPreflight(repo, &required, &state, error, sizeof(error)) == GIT_PREFLIGHT_CONFLICTS,
          "conflict state abstains before dirty-tree handling");
    CHECK(RefusedUntouched(repo, &required, GIT_PREFLIGHT_CONFLICTS, "conflict"),
          "M2: refused conflict preflight changed no HEAD, ref, index entry or file byte");
    CHECK(Run(repo, "git merge --abort", 0), "fixture conflict aborted explicitly");

    /* M2 cells: user work in flight (modified + staged + untracked), then refusals on it. */
    CHECK(Run(repo, "git checkout main --quiet", 0), "M2 fixture on main");
    CHECK(Out(repo, "git rev-parse HEAD", base, sizeof(base)), "M2 base read");
    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "user edit, not saved anywhere else\n") &&
          WriteFile("test_agent_git_repo/staged.txt", "staged by the user\n") &&
          Run(repo, "git add staged.txt", 0) &&
          WriteFile("test_agent_git_repo/untracked.txt", "untracked by the user\n"),
          "M2 user work: modified, staged and untracked files");
    memset(&required, 0, sizeof(required));
    required.expected_head = base;
    required.expected_branch = "main";
    required.require_clean = true;
    CHECK(RefusedUntouched(repo, &required, GIT_PREFLIGHT_DIRTY_TREE, "dirty"),
          "M2: refused dirty preflight left HEAD, refs, index and all three user files untouched");
    required.expected_head = "0000000000000000000000000000000000000000";
    CHECK(RefusedUntouched(repo, &required, GIT_PREFLIGHT_STALE_HEAD, "stale"),
          "M2: refused stale-head preflight on the same work changed nothing");
    required.expected_head = base;
    required.expected_branch = "other";
    CHECK(RefusedUntouched(repo, &required, GIT_PREFLIGHT_WRONG_BRANCH, "wrong branch"),
          "M2: refused wrong-branch preflight on the same work changed nothing");
    required.expected_branch = "main";
    CHECK(Run(repo, "git checkout --detach --quiet", 0), "M2 fixture detached with the work in place");
    CHECK(RefusedUntouched(repo, &required, GIT_PREFLIGHT_DETACHED_HEAD, "detached"),
          "M2: refused detached preflight changed nothing");
    CHECK(Run(repo, "git checkout main --quiet", 0), "M2 fixture back on main");
    CHECK(Run(repo, "git reset --quiet", 0), "M2 staging reset explicitly");
    remove("test_agent_git_repo/staged.txt");
    remove("test_agent_git_repo/untracked.txt");
    CHECK(Run(repo, "git checkout -- tracked.txt", 0), "M2 user edit restored explicitly");

    /* Detection power: change each captured dimension in turn; the snapshot must see it. */
    CHECK(Snapshot(repo, g_selfcheck_before, sizeof(g_selfcheck_before)), "self-check: baseline snapshot");
    CHECK(Run(repo, "git branch selfcheck", 0) && DiffersIn(repo, 1) &&
          Run(repo, "git branch -D selfcheck --quiet", 0), "self-check: a new ref is seen (REFS)");
    CHECK(WriteFile("test_agent_git_repo/tracked.txt", "one byte differs\n") && DiffersIn(repo, 4) &&
          Run(repo, "git checkout -- tracked.txt", 0), "self-check: a changed file byte is seen (TRACKED)");
    CHECK(WriteFile("test_agent_git_repo/untracked.txt", "u\n") && DiffersIn(repo, 6) &&
          DiffersIn(repo, 2), "self-check: an untracked file is seen (UNTRACKED and STATUS)");
    remove("test_agent_git_repo/untracked.txt");
    CHECK(Run(repo, "git update-index --chmod=+x tracked.txt", 0) && DiffersIn(repo, 3) &&
          Run(repo, "git reset --quiet", 0), "self-check: an index entry change is seen (INDEX)");
    CHECK(Run(repo, "git " "-c user.name=Fixture -c user.email=fixture@example.invalid commit --allow-empty -m selfcheck --quiet", 0) &&
          DiffersIn(repo, 0) && Run(repo, "git reset --hard --quiet HEAD~1", 0), "self-check: a moved HEAD is seen (HEAD)");
    {
        char restored[8192];
        CHECK(Snapshot(repo, restored, sizeof(restored)) && strcmp(restored, g_selfcheck_before) == 0,
              "self-check: fixture restored to the baseline snapshot");
    }

    CHECK(AgentGitInspect(".", &state, error, sizeof(error)) == GIT_INSPECT_OK,
          "containing project repository remains inspectable");
    CHECK(AgentGitInspect("test_agent_git_repo/missing", &state, error, sizeof(error)) == GIT_INSPECT_COMMAND_FAILED,
          "missing working directory fails without a Git side effect");

    printf("\n%d/%d assertions passed\n", tests_passed, tests_run);
    ResetFixture();
#ifdef _WIN32
    (void)system("rmdir /s /q test_agent_git_repo");
#else
    (void)system("rm -rf test_agent_git_repo");
#endif
    return tests_passed == tests_run ? 0 : 1;
}
