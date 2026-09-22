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
    CHECK(Run(repo, "git merge --abort", 0), "fixture conflict aborted explicitly");

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
