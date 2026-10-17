#include "agent_runner.h"
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
/* AgentRunnerSetGitGuard end to end: AgentRunnerSolveTask on a real Git repository. Predictions, written
   before the first run (no build or test command, so a solve that is allowed to edit is solved):
     R1 guard OFF (default), target has an uncommitted edit elsewhere: solved, the user's edit survives
        (this is the measured G3 behavior, unchanged)
     R2 guard ON, same dirty target: NOT solved, the file keeps the user's bytes
     R3 guard ON, clean repo: solved, the target has the fix
     R4 guard ON, only an unrelated file is dirty: solved
     R5 guard ON, workspace is not a Git work tree: NOT solved, file unchanged
     R6 guard OFF, workspace is not a Git work tree: solved (default behavior unchanged)
     R7 guard ON, nothing is committed by the solve: HEAD unchanged, target shows as modified */
#define S "test_agent_runner_git_guard_scratch"
static int run_n, pass_n;
/* The non-Git workspace must live outside this repository (the test runs inside the source checkout,
   where any subdirectory is already inside a Git work tree), so it goes under the system temp dir. */
static char plain[600];
static void RmTree(const char *p)
{
    char c[700];
#ifdef _WIN32
    snprintf(c, sizeof c, "if exist \"%s\" rmdir /s /q \"%s\"", p, p);
#else
    snprintf(c, sizeof c, "rm -rf \"%s\"", p);
#endif
    (void)system(c);
}
static void InitPlain(void)
{
    const char *t = getenv("TMPDIR");
    size_t i;
    if (!t || !*t) t = getenv("TEMP");
    if (!t || !*t) t = getenv("TMP");
    if (!t || !*t) t = "/tmp";
    snprintf(plain, sizeof plain, "%s/symguard_runner_plain_%ld", t, (long)time(NULL));
    for (i = 0; plain[i]; i++) if (plain[i] == '\\') plain[i] = '/';
}
#define CK(x, m) do { run_n++; if (x) { pass_n++; printf("  [PASS] %s\n", m); } else printf("  [FAIL] %s (line %d)\n", m, __LINE__); } while (0)
static void Run(const char *cwd, const char *cmd)
{
    SHELL_EXEC_RESULT r;
    if (!AgentShellExec(cmd, cwd, 20000, &r) || r.execution_failed || r.timed_out || r.exit_code != 0)
    { printf("    fixture command failed: %s\n", cmd); exit(2); }
}
static void Out(const char *cwd, const char *cmd, char *buf, size_t n)
{
    SHELL_EXEC_RESULT r;
    size_t k;
    if (!AgentShellExec(cmd, cwd, 20000, &r) || r.execution_failed || r.exit_code != 0) { printf("    out failed: %s\n", cmd); exit(2); }
    snprintf(buf, n, "%s", r.stdout_buf);
    k = strlen(buf);
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
}
static void Put(const char *p, const char *v)
{
    FILE *f = fopen(p, "wb");
    if (!f || fputs(v, f) < 0 || fclose(f) != 0) { printf("    put failed: %s\n", p); exit(2); }
}
static int Is(const char *p, const char *v)
{
    char b[2048]; size_t n; FILE *f = fopen(p, "rb");
    if (!f) return 0;
    n = fread(b, 1, sizeof b - 1, f); fclose(f); b[n] = 0;
    return strcmp(b, v) == 0;
}
#define BASE "int f(int a, int b) {\n    return a + b;\n}\n"
#define FIXED "int f(int a, int b) {\n    return a * b;\n}\n"
static void Fixture(int git)
{
    const char *d = git ? S : plain;
    char p1[700], p2[700];
    RmTree(S);
    RmTree(plain);
    MKDIR(d);
    snprintf(p1, sizeof p1, "%s/a.c", d);
    snprintf(p2, sizeof p2, "%s/other.txt", d);
    Put(p1, BASE);
    Put(p2, "o\n");
    if (git)
    {
        Run(S, "git init -q");
        Run(S, "git config user.name t");
        Run(S, "git config user.email t@example.invalid");
        Run(S, "git config core.autocrlf false");
        Run(S, "git add -A");
        Run(S, "git commit -q -m base");
    }
}
static int Solve(const char *wsdir, int guard, SWE_BENCH_RESULT *res)
{
    AGENT_RUNNER *r = AgentRunnerCreate(wsdir ? wsdir : S, 1);
    SWE_BENCH_TASK t;
    int rc;
    if (!r) { puts("runner create failed"); exit(2); }
    AgentRunnerSetGitGuard(r, guard != 0);
    memset(&t, 0, sizeof t);
    memset(res, 0, sizeof *res);
    strncpy(t.task_id, "GG-1", sizeof t.task_id - 1);
    strncpy(t.issue_description, "multiply instead of add", sizeof t.issue_description - 1);
    strncpy(t.target_symbol, "f", sizeof t.target_symbol - 1);
    strncpy(t.target_file, wsdir ? wsdir : S, sizeof t.target_file - 1);
    strncat(t.target_file, "/a.c", sizeof t.target_file - strlen(t.target_file) - 1);
    t.target_line = 2;
    strncpy(t.context_before, "int f(int a, int b) {\n", sizeof t.context_before - 1);
    strncpy(t.buggy_snippet, "    return a + b;\n", sizeof t.buggy_snippet - 1);
    strncpy(t.fixed_snippet, "    return a * b;\n", sizeof t.fixed_snippet - 1);
    strncpy(t.context_after, "}\n", sizeof t.context_after - 1);
    rc = AgentRunnerSolveTask(r, &t, res);
    AgentRunnerDestroy(r);
    return rc && res->is_solved;
}
int main(void)
{
    SWE_BENCH_RESULT res;
    InitPlain();
    char h1[128], h2[128], st[256];

    puts("R1 guard OFF, dirty target");
    Fixture(1);
    Put(S "/a.c", BASE "// USER\n");
    CK(Solve(NULL, 0, &res), "R1: solved with the guard off");
    CK(Is(S "/a.c", FIXED "// USER\n"), "R1: the user's uncommitted edit survives next to the fix");

    puts("R2 guard ON, dirty target");
    Fixture(1);
    Put(S "/a.c", BASE "// USER\n");
    CK(!Solve(NULL, 1, &res), "R2: not solved with the guard on");
    CK(Is(S "/a.c", BASE "// USER\n"), "R2: the file keeps the user's bytes");

    puts("R3 guard ON, clean repo");
    Fixture(1);
    CK(Solve(NULL, 1, &res), "R3: solved on a clean repo");
    CK(Is(S "/a.c", FIXED), "R3: the target has the fix");

    puts("R4 guard ON, unrelated dirty file");
    Fixture(1);
    Put(S "/other.txt", "o dirty\n");
    CK(Solve(NULL, 1, &res), "R4: solved with only an unrelated file dirty");
    CK(Is(S "/other.txt", "o dirty\n"), "R4: the unrelated file is untouched");

    puts("R5 guard ON, not a Git work tree");
    Fixture(0);
    { char f[700]; snprintf(f, sizeof f, "%s/a.c", plain);
    CK(!Solve(plain, 1, &res), "R5: not solved outside a Git work tree");
    CK(Is(f, BASE), "R5: the file is unchanged"); }

    puts("R6 guard OFF, not a Git work tree");
    Fixture(0);
    { char f[700]; snprintf(f, sizeof f, "%s/a.c", plain);
    CK(Solve(plain, 0, &res), "R6: solved with the guard off outside Git");
    CK(Is(f, FIXED), "R6: the target has the fix"); }

    puts("R7 nothing committed");
    Fixture(1);
    Out(S, "git rev-parse HEAD", h1, sizeof h1);
    CK(Solve(NULL, 1, &res), "R7: solved (setup)");
    Out(S, "git rev-parse HEAD", h2, sizeof h2);
    Out(S, "git status --porcelain a.c", st, sizeof st);
    CK(strcmp(h1, h2) == 0, "R7: HEAD unchanged");
    CK(strstr(st, "a.c") != NULL && st[0] != '\0', "R7: the target shows as modified, nothing committed");

    RmTree(S);
    RmTree(plain);
    printf("\n%d/%d passed\n", pass_n, run_n);
    return pass_n == run_n ? 0 : 1;
}
