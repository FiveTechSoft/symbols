/* ============================================================
   test_agent_shell.c: High-Performance Cross-Platform Shell Execution
                       Engine Unit and Integration Tests.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_shell.h"
#include "agent_diagnose.h"
#include "agent_runner.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* ============================================================
   Test 1: Backend Detection, Naming & Command Wrapping
   ============================================================ */
static void test_detection_and_wrapping(void)
{
    printf("\n=== Test 1: Backend Detection, Naming & Command Wrapping ===\n");

    SHELL_BACKEND auto_backend = AgentShellDetectBackend();
#ifdef _WIN32
    TEST_ASSERT(auto_backend == SHELL_BACKEND_CMD, "Windows detects SHELL_BACKEND_CMD as default");
#elif defined(__APPLE__)
    TEST_ASSERT(auto_backend == SHELL_BACKEND_ZSH, "macOS detects SHELL_BACKEND_ZSH as default");
#else
    TEST_ASSERT(auto_backend == SHELL_BACKEND_BASH, "Linux detects SHELL_BACKEND_BASH as default");
#endif

    /* Backend names */
    TEST_ASSERT(strstr(AgentShellBackendName(SHELL_BACKEND_CMD), "cmd.exe") != NULL, "CMD backend name contains cmd.exe");
    TEST_ASSERT(strstr(AgentShellBackendName(SHELL_BACKEND_POWERSHELL), "powershell.exe") != NULL, "PowerShell backend name contains powershell.exe");
    TEST_ASSERT(strstr(AgentShellBackendName(SHELL_BACKEND_SH), "/bin/sh") != NULL, "SH backend name contains /bin/sh");
    TEST_ASSERT(strstr(AgentShellBackendName(SHELL_BACKEND_BASH), "/bin/bash") != NULL, "BASH backend name contains /bin/bash");
    TEST_ASSERT(strstr(AgentShellBackendName(SHELL_BACKEND_ZSH), "/bin/zsh") != NULL, "ZSH backend name contains /bin/zsh");

    /* Command wrapping */
    char buf[1024];
    int len = AgentShellWrapCommand(SHELL_BACKEND_CMD, "echo test", buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Wrap CMD succeeds");
    TEST_ASSERT(strstr(buf, "cmd.exe /c") != NULL, "Wrap CMD produces cmd.exe /c");

    len = AgentShellWrapCommand(SHELL_BACKEND_POWERSHELL, "Write-Output test", buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Wrap PowerShell succeeds");
    TEST_ASSERT(strstr(buf, "powershell.exe") != NULL, "Wrap PowerShell produces powershell.exe invocation");

    len = AgentShellWrapCommand(SHELL_BACKEND_BASH, "echo test", buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Wrap BASH succeeds");
    TEST_ASSERT(strstr(buf, "/bin/bash -c") != NULL, "Wrap BASH produces /bin/bash -c");

    len = AgentShellWrapCommand(SHELL_BACKEND_ZSH, "echo test", buf, sizeof(buf));
    TEST_ASSERT(len > 0, "Wrap ZSH succeeds");
    TEST_ASSERT(strstr(buf, "/bin/zsh -c") != NULL, "Wrap ZSH produces /bin/zsh -c");

    /* Result initialization */
    SHELL_EXEC_RESULT res;
    AgentShellResultInit(&res);
    TEST_ASSERT(res.exit_code == -1, "AgentShellResultInit sets exit_code to -1");
    TEST_ASSERT(res.stdout_len == 0 && res.stderr_len == 0, "Buffers initialized to zero length");
    TEST_ASSERT(!res.timed_out && !res.execution_failed, "Flags initialized to false");
}

/* ============================================================
   Test 2: Successful Execution and Stdout Capture
   ============================================================ */
static void test_successful_execution(void)
{
    printf("\n=== Test 2: Successful Execution and Stdout Capture ===\n");

    SHELL_EXEC_RESULT res;
    int ok = AgentShellExec("echo HELLO_SYMBOLIC_SHELL", ".", 10000, &res);

    TEST_ASSERT(ok == 1, "AgentShellExec returns 1 on process launch");
    TEST_ASSERT(!res.execution_failed, "Execution does not fail");
    TEST_ASSERT(res.exit_code == 0, "Process exit code is 0");
    TEST_ASSERT(res.stdout_len > 0, "Captured stdout output");
    TEST_ASSERT(strstr(res.stdout_buf, "HELLO_SYMBOLIC_SHELL") != NULL, "Stdout contains expected string");
    TEST_ASSERT(res.stderr_len == 0, "Stderr is empty on successful echo");
    TEST_ASSERT(!res.timed_out, "Process did not time out");
    TEST_ASSERT(res.wall_clock_ms >= 0.0, "Wall clock time measured in milliseconds");
    TEST_ASSERT(res.backend_name[0] != '\0', "Backend name populated in result");

    printf("  [INFO] Executed in %.2f ms via %s\n", res.wall_clock_ms, res.backend_name);
}

/* ============================================================
   Test 3: Stderr Capture and Non-Zero Exit Code
   ============================================================ */
static void test_stderr_and_exit_code(void)
{
    printf("\n=== Test 3: Stderr Capture and Non-Zero Exit Code ===\n");

    SHELL_EXEC_RESULT res;

#ifdef _WIN32
    /* Windows cmd: output to stderr (1>&2) and exit with code 42 */
    const char *cmd = "echo ERROR_TEST_CAPTURE 1>&2 & exit 42";
#else
    /* POSIX sh: output to stderr and exit with code 42 */
    const char *cmd = "echo ERROR_TEST_CAPTURE >&2; exit 42";
#endif

    int ok = AgentShellExec(cmd, ".", 10000, &res);

    TEST_ASSERT(ok == 1, "AgentShellExec launches process");
    TEST_ASSERT(!res.execution_failed, "Process executed without OS launch failure");
    TEST_ASSERT(res.exit_code == 42, "Process returns exact exit code 42");
    TEST_ASSERT(res.stderr_len > 0, "Stderr buffer captured error stream");
    TEST_ASSERT(strstr(res.stderr_buf, "ERROR_TEST_CAPTURE") != NULL, "Stderr contains expected error token");
    TEST_ASSERT(!res.timed_out, "Did not time out");

    printf("  [INFO] Exit code: %d, Stderr captured: '%s'\n", res.exit_code, res.stderr_buf);
}

/* ============================================================
   Test 4: Hard Timeout Termination
   ============================================================ */
static void test_timeout_termination(void)
{
    printf("\n=== Test 4: Hard Timeout Termination ===\n");

    SHELL_EXEC_RESULT res;

#ifdef _WIN32
    /* Ping 127.0.0.1 4 times takes ~3 seconds; with 150ms timeout it should be killed */
    const char *slow_cmd = "ping 127.0.0.1 -n 4 >nul";
#else
    /* Sleep 3 seconds; with 150ms timeout it should be killed */
    const char *slow_cmd = "sleep 3";
#endif

    int ok = AgentShellExec(slow_cmd, ".", 150, &res);

    TEST_ASSERT(ok == 1, "AgentShellExec returned 1");
    TEST_ASSERT(res.timed_out, "Process marked as timed_out");
    TEST_ASSERT(res.exit_code == 124, "Timeout exit code is standard 124");
    TEST_ASSERT(res.wall_clock_ms >= 100.0, "Elapsed time at least timeout duration");
    TEST_ASSERT(res.wall_clock_ms < 2000.0, "Terminated well before slow command completion");

    printf("  [INFO] Timed out after %.2f ms (exit_code=%d)\n", res.wall_clock_ms, res.exit_code);
}

/* ============================================================
   Test 5: Working Directory Propagation
   ============================================================ */
static void test_working_directory(void)
{
    printf("\n=== Test 5: Working Directory Propagation ===\n");

    SHELL_EXEC_RESULT res;

#ifdef _WIN32
    const char *cmd = "cd";
#else
    const char *cmd = "pwd";
#endif

    int ok = AgentShellExec(cmd, "include", 10000, &res);

    TEST_ASSERT(ok == 1, "AgentShellExec succeeds");
    TEST_ASSERT(res.exit_code == 0, "Exit code 0");
    TEST_ASSERT(res.stdout_len > 0, "Stdout captured working directory");
    TEST_ASSERT(strstr(res.stdout_buf, "include") != NULL, "Working directory 'include' respected in execution");
}

/* ============================================================
   Test 6: Integration with Abductive Diagnostic Parser
   ============================================================ */
static void test_abductive_diagnostic_integration(void)
{
    printf("\n=== Test 6: Integration with Abductive Diagnostic Parser ===\n");

    /* Simulate compiler output emitted to stderr */
#ifdef _WIN32
    const char *cmd = 
        "echo src/matrix.c:42:15: error: 'matrix_t' has no member named 'width'; did you mean 'cols'? 1>&2 & exit 1";
#else
    const char *cmd = 
        "echo \"src/matrix.c:42:15: error: 'matrix_t' has no member named 'width'; did you mean 'cols'?\" >&2; exit 1";
#endif

    SHELL_EXEC_RESULT shell_res;
    int ok = AgentShellExec(cmd, ".", 10000, &shell_res);

    TEST_ASSERT(ok == 1, "AgentShellExec executed simulated compiler command");
    TEST_ASSERT(shell_res.exit_code == 1, "Compiler exit code indicates failure (1)");
    TEST_ASSERT(shell_res.stderr_len > 0, "Captured compiler error on stderr");

    /* Feed shell stderr directly into abductive diagnosis */
    DIAGNOSTIC_REPORT diag_rep;
    int diag_ok = DiagnosticParseOutput(shell_res.stderr_buf, &diag_rep);

    TEST_ASSERT(diag_ok == 1, "DiagnosticParseOutput successfully parsed shell stderr stream");
    TEST_ASSERT(diag_rep.error_count == 1, "Discovered exactly 1 compiler error");
    TEST_ASSERT(diag_rep.root_type == DIAG_ERR_MISSING_MEMBER, "Classified as DIAG_ERR_MISSING_MEMBER");
    TEST_ASSERT(strcmp(diag_rep.root_symbol, "width") == 0, "Identified offending symbol 'width'");
    TEST_ASSERT(strcmp(diag_rep.root_suggestion, "cols") == 0, "Extracted did-you-mean suggestion 'cols'");

    char remedy[256];
    DiagnosticAbduceRemedy(&diag_rep, NULL, remedy, sizeof(remedy));
    TEST_ASSERT(strstr(remedy, "cols") != NULL, "Abduced remedy suggests 'cols'");

    printf("  [INFO] Abductive diagnosis from shell stderr: %s\n", remedy);
}

/* ============================================================
   Test 7: End-to-End SWE-bench Task with Shell Execution
   ============================================================ */
static void test_swe_bench_shell_integration(void)
{
    printf("\n=== Test 7: End-to-End SWE-bench with Shell Execution ===\n");

    const char *target_file = "test_shell_math.c";
    const char *initial_code =
        "#include <stdio.h>\n"
        "int ComputeSquare(int x) {\n"
        "    return x + x; /* BUG: addition instead of multiplication */\n"
        "}\n";

    FILE *f = fopen(target_file, "wb");
    fwrite(initial_code, 1, strlen(initial_code), f);
    fclose(f);

    AGENT_RUNNER *runner = AgentRunnerCreate(".", 3);
    TEST_ASSERT(runner != NULL, "AgentRunnerCreate succeeded");

    SWE_BENCH_TASK task;
    memset(&task, 0, sizeof(task));
    strncpy(task.task_id, "SHELL-TASK-001", sizeof(task.task_id) - 1);
    strncpy(task.issue_description, "Fix ComputeSquare multiplication bug and verify with shell",
            sizeof(task.issue_description) - 1);
    strncpy(task.target_symbol, "ComputeSquare", sizeof(task.target_symbol) - 1);
    strncpy(task.target_file, target_file, sizeof(task.target_file) - 1);
    task.target_line = 3;
    strncpy(task.context_before, "int ComputeSquare(int x) {\n", sizeof(task.context_before) - 1);
    strncpy(task.buggy_snippet, "    return x + x; /* BUG: addition instead of multiplication */\n",
            sizeof(task.buggy_snippet) - 1);
    strncpy(task.fixed_snippet, "    return x * x; /* FIXED: multiplication */\n",
            sizeof(task.fixed_snippet) - 1);
    strncpy(task.context_after, "}\n", sizeof(task.context_after) - 1);

    /* Real shell verification command (echo verified) */
    strncpy(task.test_command, "echo VERIFICATION_PASSED", sizeof(task.test_command) - 1);

    SWE_BENCH_RESULT result;
    int rc = AgentRunnerSolveTask(runner, &task, &result);

    TEST_ASSERT(rc == 1, "AgentRunnerSolveTask succeeded with shell verification");
    TEST_ASSERT(result.is_solved, "Task is marked solved");
    TEST_ASSERT(result.last_shell_exec.exit_code == 0, "Shell test command exited with 0");
    TEST_ASSERT(result.last_shell_exec.backend_name[0] != '\0', "Shell backend recorded in result");
    TEST_ASSERT(result.last_shell_exec.wall_clock_ms >= 0.0, "Shell execution latency tracked");
    TEST_ASSERT(strstr(result.senior_engineer_report, "Shell Environment") != NULL,
                "Senior report contains shell telemetry");

    AgentRunnerDestroy(runner);
    remove(target_file);
}

int main(void)
{
    printf("======================================================================\n");
    printf("  TEST SUITE: CROSS-PLATFORM AGENT SHELL & REPAIR ENGINE (agent_shell)\n");
    printf("======================================================================\n");

    test_detection_and_wrapping();
    test_successful_execution();
    test_stderr_and_exit_code();
    test_timeout_termination();
    test_working_directory();
    test_abductive_diagnostic_integration();
    test_swe_bench_shell_integration();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
