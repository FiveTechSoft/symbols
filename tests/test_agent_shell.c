/* ============================================================
   test_agent_shell.c: High-Performance Cross-Platform Shell Execution
                       Engine Unit and Integration Tests.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#endif
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


/* ============================================================
   Adversarial process-control regression tests
   ============================================================ */
static void test_invalid_working_directory_fails_closed(void)
{
    SHELL_EXEC_RESULT res;
    const char *effect = "agent_shell_invalid_cwd_effect.tmp";
    int ok;
    remove(effect);
#ifdef _WIN32
    ok = AgentShellExec("echo SHOULD_NOT_RUN>agent_shell_invalid_cwd_effect.tmp",
                        "agent_shell_cwd_that_does_not_exist", 1000, &res);
#else
    ok = AgentShellExec("printf SHOULD_NOT_RUN > agent_shell_invalid_cwd_effect.tmp",
                        "agent_shell_cwd_that_does_not_exist", 1000, &res);
#endif
    TEST_ASSERT(ok == 0, "Invalid cwd is rejected before process launch");
    TEST_ASSERT(res.execution_failed, "Invalid cwd marks execution_failed");
    TEST_ASSERT(fopen(effect, "rb") == NULL, "Invalid cwd command produced no side effect");
    remove(effect);
}

static void test_large_and_interleaved_output(void)
{
    SHELL_EXEC_RESULT res;
    int ok;
#ifdef _WIN32
    const char *large_cmd =
        "powershell.exe -NoProfile -Command \"[Console]::Out.Write('O'*100000); [Console]::Error.Write('E'*100000)\"";
    const char *mixed_cmd =
        "powershell.exe -NoProfile -Command \"1..5000 | %% { [Console]::Out.WriteLine('OUT'); [Console]::Error.WriteLine('ERR') }\"";
#else
    const char *large_cmd =
        "head -c 100000 /dev/zero | tr '\\0' O; head -c 100000 /dev/zero | tr '\\0' E >&2";
    const char *mixed_cmd =
        "i=0; while [ $i -lt 5000 ]; do echo OUT; echo ERR >&2; i=$((i+1)); done";
#endif
    ok = AgentShellExec(large_cmd, ".", 10000, &res);
    TEST_ASSERT(ok == 1 && res.exit_code == 0 && !res.timed_out,
                "Large dual-stream output completes without false timeout");
    TEST_ASSERT(res.stdout_len == SHELL_BUFFER_MAX - 1 && res.stderr_len == SHELL_BUFFER_MAX - 1,
                "Large output fills bounded capture buffers");
    TEST_ASSERT(res.stdout_total_len == 100000 && res.stderr_total_len == 100000,
                "Large output reports exact total byte counts");
    TEST_ASSERT(res.stdout_truncated && res.stderr_truncated,
                "Large output explicitly signals both stream truncations");

    ok = AgentShellExec(mixed_cmd, ".", 10000, &res);
    TEST_ASSERT(ok == 1 && res.exit_code == 0 && !res.timed_out,
                "Interleaved stdout/stderr completes without deadlock");
    TEST_ASSERT(res.stdout_total_len >= 20000 && res.stderr_total_len >= 20000,
                "Interleaved output drains both streams");
    TEST_ASSERT(strstr(res.stdout_buf, "OUT") != NULL && strstr(res.stderr_buf, "ERR") != NULL,
                "Interleaved output remains separated by stream");
}

static void test_timeout_terminates_process_tree(void)
{
    SHELL_EXEC_RESULT res;
#ifdef _WIN32
    int ok = AgentShellExec(
        "start /b powershell.exe -NoProfile -Command \"Start-Sleep -Seconds 30\" & ping 127.0.0.1 -n 30 >nul",
        ".", 200, &res);
    TEST_ASSERT(ok == 1 && res.timed_out && res.exit_code == 124,
                "Windows timeout terminates the assigned Job Object");
#else
    const char *pid_file = "agent_shell_background.pid";
    FILE *f;
    long child_pid = -1;
    int alive;
    int ok;
    remove(pid_file);
    ok = AgentShellExec("sleep 30 & echo $! > agent_shell_background.pid; wait",
                        ".", 200, &res);
    TEST_ASSERT(ok == 1 && res.timed_out && res.exit_code == 124,
                "POSIX timeout marks process tree termination");
    f = fopen(pid_file, "rb");
    if (f)
    {
        (void)fscanf(f, "%ld", &child_pid);
        fclose(f);
    }
    alive = child_pid > 0 && (kill((pid_t)child_pid, 0) == 0 || errno != ESRCH);
#if defined(__linux__)
    /* A killed orphan can briefly remain as a zombie until init reaps it. */
    if (alive)
    {
        char proc_path[64];
        char proc_line[256];
        char state = '\0';
        FILE *proc;
        snprintf(proc_path, sizeof(proc_path), "/proc/%ld/stat", child_pid);
        proc = fopen(proc_path, "rb");
        if (proc && fgets(proc_line, sizeof(proc_line), proc))
        {
            char *after_name = strrchr(proc_line, ')');
            if (after_name && after_name[1] == ' ')
                state = after_name[2];
        }
        if (proc)
            fclose(proc);
        if (state == 'Z')
            alive = 0;
    }
#endif
    TEST_ASSERT(child_pid > 0, "Background child PID was recorded before timeout");
    TEST_ASSERT(!alive, "Background child does not survive process-group timeout");
    remove(pid_file);
#endif
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
    test_invalid_working_directory_fails_closed();
    test_large_and_interleaved_output();
    test_timeout_terminates_process_tree();
    test_abductive_diagnostic_integration();
    test_swe_bench_shell_integration();

    printf("\n======================================================================\n");
    printf("  TEST RESULTS: %d passed, %d failed\n", g_tests_passed, g_tests_run - g_tests_passed);
    printf("======================================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
