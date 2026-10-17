#include "agent_git.h"
#include "agent_shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMTREE(p) "if exist " p " rmdir /s /q " p
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMTREE(p) "rm -rf " p
#endif
/* AgentGitGuard*: the opt-in single-file guard behind AgentRunnerSetGitGuard. Real Git repository,
   nothing mocked. Predictions, written before the first run:
     U1 clean repo: Begin ok, Check ok
     U2 target has an uncommitted edit: Begin refuses (path_dirty)
     U3 only an unrelated file is dirty (modified and untracked): Begin ok, Check ok
     U4 HEAD moves by an unrelated commit after Begin: Check head_moved
     U5 target is edited after Begin: Check path_dirty
     U6 target is untracked (new file) at Begin: path_dirty
     U7 directory that is not a work tree: not_repository
     U8 paths git cannot be safely asked about (space, leading slash, "..") : inspection_failed
     U9 the guard changed nothing: HEAD equal and `git status --porcelain` identical before and after
   Mutant 1 (AGENT_GIT_GUARD_MUTANT=1) skips the HEAD comparison and must fail U4 only.
   Mutant 2 (=2) skips the target-dirty check and must fail U2, U5 and U6 (not U3).
   Non-mutant builds exit 1 on any failed cell. Mutant builds exit 0 when exactly the predicted cells
   failed and print "mutant killed by N cell(s)"; otherwise exit 1. */
/* Each build has its own scratch directory so the plain and mutant ctest entries can run in parallel. */
#ifdef AGENT_GIT_GUARD_MUTANT
#if AGENT_GIT_GUARD_MUTANT == 1
#define S "test_agent_git_guard_scratch_mc1"
#else
#define S "test_agent_git_guard_scratch_mc2"
#endif
#else
#define S "test_agent_git_guard_scratch"
#endif
static int run_n, pass_n, failed_mask;
static int cell_no;
#define CELL(bit, x, m) do { run_n++; if (x) { pass_n++; printf("  [PASS] %s\n", m); } else { failed_mask |= (1 << (bit)); printf("  [FAIL] %s (line %d)\n", m, __LINE__); } } while (0)
static int Run(const char *cwd, const char *cmd)
{
    SHELL_EXEC_RESULT r;
    if (!AgentShellExec(cmd, cwd, 20000, &r) || r.execution_failed || r.timed_out || r.exit_code != 0)
    {
        printf("    fixture command failed: %s (cwd %s)\n", cmd, cwd ? cwd : ".");
        exit(2);
    }
    return 1;
}
static void Out(const char *cwd, const char *cmd, char *buf, size_t n)
{
    SHELL_EXEC_RESULT r;
    if (!AgentShellExec(cmd, cwd, 20000, &r) || r.execution_failed || r.exit_code != 0) { printf("    out failed: %s\n", cmd); exit(2); }
    snprintf(buf, n, "%s", r.stdout_buf);
}
static void Put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (!f || fputs(text, f) < 0 || fclose(f) != 0) { printf("    put failed: %s\n", path); exit(2); }
}
static void Repo(void)
{
    (void)system(RMTREE(S));
    MKDIR(S);
    Put(S "/a.c", "line1\nint x = 1;\n");
    Put(S "/other.txt", "o\n");
    Run(S, "git init -q");
    Run(S, "git config user.name t");
    Run(S, "git config user.email t@example.invalid");
    Run(S, "git config core.autocrlf false");
    Run(S, "git add -A");
    Run(S, "git commit -q -m base");
}
int main(void)
{
    GIT_GUARD g;
    char err[GIT_ERROR_MAX], s1[4096], s2[4096], h1[128], h2[128];

    puts("U1 clean");
    Repo();
    CELL(0, AgentGitGuardBegin(S, "a.c", &g, err, sizeof err) == GIT_GUARD_OK, "U1: Begin ok on a clean repo");
    CELL(1, AgentGitGuardCheck(S, "a.c", &g, err, sizeof err) == GIT_GUARD_OK, "U1: Check ok on a clean repo");

    puts("U2 target dirty at Begin");
    Repo();
    Put(S "/a.c", "line1\nint x = 1;\nUSER\n");
    CELL(2, AgentGitGuardBegin(S, "a.c", &g, err, sizeof err) == GIT_GUARD_PATH_DIRTY, "U2: Begin refuses a dirty target");

    puts("U3 unrelated dirty");
    Repo();
    Put(S "/other.txt", "o dirty\n");
    Put(S "/new_untracked.txt", "u\n");
    CELL(3, AgentGitGuardBegin(S, "a.c", &g, err, sizeof err) == GIT_GUARD_OK, "U3: Begin ok with unrelated dirty files");
    CELL(4, AgentGitGuardCheck(S, "a.c", &g, err, sizeof err) == GIT_GUARD_OK, "U3: Check ok with unrelated dirty files");

    puts("U4 HEAD moves");
    Repo();
    AgentGitGuardBegin(S, "a.c", &g, err, sizeof err);
    Put(S "/other.txt", "o2\n");
    Run(S, "git add -A");
    Run(S, "git commit -q -m moved");
    CELL(5, AgentGitGuardCheck(S, "a.c", &g, err, sizeof err) == GIT_GUARD_HEAD_MOVED, "U4: Check refuses a HEAD that moved");

    puts("U5 target edited after Begin");
    Repo();
    CELL(6, AgentGitGuardBegin(S, "a.c", &g, err, sizeof err) == GIT_GUARD_OK, "U5: Begin ok (setup)");
    Put(S "/a.c", "line1\nint x = 1;\nUSER\n");
    CELL(7, AgentGitGuardCheck(S, "a.c", &g, err, sizeof err) == GIT_GUARD_PATH_DIRTY, "U5: Check refuses a target edited after Begin");

    puts("U6 untracked target");
    Repo();
    Put(S "/fresh.c", "int y;\n");
    CELL(8, AgentGitGuardBegin(S, "fresh.c", &g, err, sizeof err) == GIT_GUARD_PATH_DIRTY, "U6: Begin refuses an untracked target");

    puts("U7 not a work tree");
    {
        /* Outside this repository: the scratch dir sits inside the source checkout, so use the temp dir. */
        char d[700], c[760];
        const char *t = getenv("TMPDIR");
        size_t i;
        if (!t || !*t) t = getenv("TEMP");
        if (!t || !*t) t = getenv("TMP");
        if (!t || !*t) t = "/tmp";
        snprintf(d, sizeof d, "%s/symguard_plain_%s", t, S);
        for (i = 0; d[i]; i++) if (d[i] == '\\') d[i] = '/';
        MKDIR(d);
        CELL(9, AgentGitGuardBegin(d, "a.c", &g, err, sizeof err) == GIT_GUARD_NOT_REPOSITORY, "U7: a plain directory outside any repository is not_repository");
#ifdef _WIN32
        snprintf(c, sizeof c, "if exist \"%s\" rmdir /s /q \"%s\"", d, d);
#else
        snprintf(c, sizeof c, "rm -rf \"%s\"", d);
#endif
        (void)system(c);
    }

    puts("U8 unsafe paths");
    Repo();
    CELL(10, AgentGitGuardBegin(S, "a b.c", &g, err, sizeof err) == GIT_GUARD_INSPECTION_FAILED, "U8: a path with a space fails closed");
    CELL(11, AgentGitGuardBegin(S, "/a.c", &g, err, sizeof err) == GIT_GUARD_INSPECTION_FAILED, "U8: a leading slash fails closed");
    CELL(12, AgentGitGuardBegin(S, "../a.c", &g, err, sizeof err) == GIT_GUARD_INSPECTION_FAILED, "U8: a .. path fails closed");

    puts("U9 read only");
    Repo();
    Put(S "/other.txt", "o dirty\n");
    Out(S, "git rev-parse HEAD", h1, sizeof h1);
    Out(S, "git status --porcelain", s1, sizeof s1);
    AgentGitGuardBegin(S, "a.c", &g, err, sizeof err);
    AgentGitGuardCheck(S, "a.c", &g, err, sizeof err);
    Out(S, "git rev-parse HEAD", h2, sizeof h2);
    Out(S, "git status --porcelain", s2, sizeof s2);
    CELL(13, strcmp(h1, h2) == 0 && strcmp(s1, s2) == 0, "U9: HEAD and git status unchanged by the guard");

    (void)system(RMTREE(S));
    (void)cell_no;
    printf("\n%d/%d passed\n", pass_n, run_n);
#if defined(AGENT_GIT_GUARD_MUTANT) && AGENT_GIT_GUARD_MUTANT == 1
    {
        int want = 1 << 5;
        if (failed_mask == want) { puts("mutant killed by 1 cell(s)"); return 0; }
        printf("MUTANT SURVIVED or wrong cells failed (mask 0x%x, want 0x%x)\n", failed_mask, want);
        return 1;
    }
#elif defined(AGENT_GIT_GUARD_MUTANT) && AGENT_GIT_GUARD_MUTANT == 2
    {
        int want = (1 << 2) | (1 << 7) | (1 << 8);
        if (failed_mask == want) { puts("mutant killed by 3 cell(s)"); return 0; }
        printf("MUTANT SURVIVED or wrong cells failed (mask 0x%x, want 0x%x)\n", failed_mask, want);
        return 1;
    }
#else
    return failed_mask == 0 ? 0 : 1;
#endif
}
