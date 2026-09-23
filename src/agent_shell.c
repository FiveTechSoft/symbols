/* ============================================================
   agent_shell.c: High-Performance Cross-Platform Shell Execution Engine
                  (Windows, Linux, macOS).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "agent_shell.h"
#include "command_policy.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#endif

/* ============================================================
   Helper Functions & Initialization
   ============================================================ */

void AgentShellResultInit(SHELL_EXEC_RESULT *res)
{
    if (!res)
        return;
    memset(res, 0, sizeof(*res));
    res->exit_code = -1;
}

SHELL_BACKEND AgentShellDetectBackend(void)
{
#ifdef _WIN32
    return SHELL_BACKEND_CMD;
#elif defined(__APPLE__)
    return SHELL_BACKEND_ZSH;
#else
    return SHELL_BACKEND_BASH;
#endif
}

const char *AgentShellBackendName(SHELL_BACKEND backend)
{
    switch (backend)
    {
    case SHELL_BACKEND_CMD:
        return "cmd.exe (Windows)";
    case SHELL_BACKEND_POWERSHELL:
        return "powershell.exe (Windows)";
    case SHELL_BACKEND_SH:
        return "/bin/sh (POSIX)";
    case SHELL_BACKEND_BASH:
        return "/bin/bash (Linux)";
    case SHELL_BACKEND_ZSH:
        return "/bin/zsh (macOS)";
    default:
        return "auto";
    }
}

int AgentShellWrapCommand(SHELL_BACKEND backend,
                          const char *cmd,
                          char *out_cmd,
                          size_t out_cmd_size)
{
    if (!cmd || !out_cmd || out_cmd_size == 0)
        return 0;

    switch (backend)
    {
    case SHELL_BACKEND_POWERSHELL:
        return snprintf(out_cmd, out_cmd_size,
                        "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"%s\"",
                        cmd);

    case SHELL_BACKEND_CMD:
        return snprintf(out_cmd, out_cmd_size, "cmd.exe /c \"%s\"", cmd);

    case SHELL_BACKEND_BASH:
        return snprintf(out_cmd, out_cmd_size, "/bin/bash -c \"%s\"", cmd);

    case SHELL_BACKEND_ZSH:
        return snprintf(out_cmd, out_cmd_size, "/bin/zsh -c \"%s\"", cmd);

    case SHELL_BACKEND_SH:
    default:
#ifdef _WIN32
        return snprintf(out_cmd, out_cmd_size, "cmd.exe /c \"%s\"", cmd);
#else
        return snprintf(out_cmd, out_cmd_size, "/bin/sh -c \"%s\"", cmd);
#endif
    }
}

/* ============================================================
   Platform Implementation: Windows (Win32)
   ============================================================ */

#ifdef _WIN32

static void AgentShellAppendCapture(char *dst,
                                    size_t *stored_len,
                                    size_t *total_len,
                                    bool *truncated,
                                    const char *src,
                                    size_t n)
{
    size_t capacity = SHELL_BUFFER_MAX - 1;
    size_t copy_n = 0;
    *total_len += n;
    if (*stored_len < capacity)
    {
        copy_n = capacity - *stored_len;
        if (copy_n > n)
            copy_n = n;
        memcpy(dst + *stored_len, src, copy_n);
        *stored_len += copy_n;
    }
    if (copy_n < n)
        *truncated = true;
}

static void AgentShellDrainWinPipe(HANDLE pipe,
                                   char *dst,
                                   size_t *stored_len,
                                   size_t *total_len,
                                   bool *truncated)
{
    char scratch[8192];
    for (;;)
    {
        DWORD available = 0;
        DWORD read_n = 0;
        DWORD want;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL) || available == 0)
            break;
        want = available > (DWORD)sizeof(scratch) ? (DWORD)sizeof(scratch) : available;
        if (!ReadFile(pipe, scratch, want, &read_n, NULL) || read_n == 0)
            break;
        AgentShellAppendCapture(dst, stored_len, total_len, truncated,
                                scratch, (size_t)read_n);
    }
}

int AgentShellExecExplicit(SHELL_BACKEND backend,
                           const char *cmd_line,
                           const char *working_dir,
                           uint32_t timeout_ms,
                           SHELL_EXEC_RESULT *out_result)
{
    SECURITY_ATTRIBUTES sa;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limit;
    LARGE_INTEGER freq, t_start, t_end;
    HANDLE hOutRead = NULL, hOutWrite = NULL;
    HANDLE hErrRead = NULL, hErrWrite = NULL;
    HANDLE hJob = NULL;
    char wrapped_cmd[4096];
    const char *work_dir;
    DWORD timeout;
    DWORD start_tick;
    BOOL process_finished = FALSE;
    BOOL ok;

    if (!cmd_line || !out_result)
        return 0;

    AgentShellResultInit(out_result);
    if (backend == SHELL_BACKEND_AUTO)
        backend = AgentShellDetectBackend();
    strncpy(out_result->backend_name, AgentShellBackendName(backend),
            sizeof(out_result->backend_name) - 1);

    work_dir = (working_dir && working_dir[0] != '\0') ? working_dir : NULL;
    if (work_dir)
    {
        DWORD attrs = GetFileAttributesA(work_dir);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            out_result->execution_failed = true;
            return 0;
        }
    }

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0) ||
        !SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0))
        goto launch_failure;
    if (!CreatePipe(&hErrRead, &hErrWrite, &sa, 0) ||
        !SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0))
        goto launch_failure;

    hJob = CreateJobObjectA(NULL, NULL);
    if (!hJob)
        goto launch_failure;
    memset(&job_limit, 0, sizeof(job_limit));
    job_limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                 &job_limit, sizeof(job_limit)))
        goto launch_failure;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutWrite;
    si.hStdError = hErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    memset(&pi, 0, sizeof(pi));

    if (AgentShellWrapCommand(backend, cmd_line, wrapped_cmd,
                              sizeof(wrapped_cmd)) <= 0)
        goto launch_failure;

    /* Suspended creation closes the race where descendants could escape the job. */
    ok = CreateProcessA(NULL, wrapped_cmd, NULL, NULL, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        NULL, work_dir, &si, &pi);
    if (!ok)
        goto launch_failure;
    if (!AssignProcessToJobObject(hJob, pi.hProcess))
    {
        TerminateProcess(pi.hProcess, 125);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        goto launch_failure;
    }
    if (ResumeThread(pi.hThread) == (DWORD)-1)
    {
        TerminateJobObject(hJob, 125);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        goto launch_failure;
    }

    CloseHandle(hOutWrite); hOutWrite = NULL;
    CloseHandle(hErrWrite); hErrWrite = NULL;

    timeout = timeout_ms > 0 ? timeout_ms : DEFAULT_SHELL_TIMEOUT_MS;
    start_tick = GetTickCount();
    while (!process_finished)
    {
        AgentShellDrainWinPipe(hOutRead, out_result->stdout_buf,
                               &out_result->stdout_len,
                               &out_result->stdout_total_len,
                               &out_result->stdout_truncated);
        AgentShellDrainWinPipe(hErrRead, out_result->stderr_buf,
                               &out_result->stderr_len,
                               &out_result->stderr_total_len,
                               &out_result->stderr_truncated);

        if (WaitForSingleObject(pi.hProcess, 5) == WAIT_OBJECT_0)
            process_finished = TRUE;
        else if (GetTickCount() - start_tick >= timeout)
        {
            /* The job contains the shell and every descendant it created. */
            TerminateJobObject(hJob, 124);
            WaitForSingleObject(pi.hProcess, INFINITE);
            out_result->timed_out = true;
            out_result->exit_code = 124;
            process_finished = TRUE;
        }
    }

    AgentShellDrainWinPipe(hOutRead, out_result->stdout_buf,
                           &out_result->stdout_len,
                           &out_result->stdout_total_len,
                           &out_result->stdout_truncated);
    AgentShellDrainWinPipe(hErrRead, out_result->stderr_buf,
                           &out_result->stderr_len,
                           &out_result->stderr_total_len,
                           &out_result->stderr_truncated);
    out_result->stdout_buf[out_result->stdout_len] = '\0';
    out_result->stderr_buf[out_result->stderr_len] = '\0';

    if (!out_result->timed_out)
    {
        DWORD ec = 0;
        GetExitCodeProcess(pi.hProcess, &ec);
        out_result->exit_code = (int)ec;
    }

    CloseHandle(hOutRead);
    CloseHandle(hErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hJob);
    QueryPerformanceCounter(&t_end);
    out_result->wall_clock_ms =
        (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 /
        (double)freq.QuadPart;
    return 1;

launch_failure:
    if (hOutRead) CloseHandle(hOutRead);
    if (hOutWrite) CloseHandle(hOutWrite);
    if (hErrRead) CloseHandle(hErrRead);
    if (hErrWrite) CloseHandle(hErrWrite);
    if (hJob) CloseHandle(hJob);
    out_result->execution_failed = true;
    out_result->exit_code = -1;
    return 0;
}

#else

/* ============================================================
   Platform Implementation: POSIX (Linux, macOS)
   ============================================================ */

static double AgentShellElapsedMs(const struct timespec *start,
                                  const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static void AgentShellAppendCapture(char *dst,
                                    size_t *stored_len,
                                    size_t *total_len,
                                    bool *truncated,
                                    const char *src,
                                    size_t n)
{
    size_t capacity = SHELL_BUFFER_MAX - 1;
    size_t copy_n = 0;
    *total_len += n;
    if (*stored_len < capacity)
    {
        copy_n = capacity - *stored_len;
        if (copy_n > n)
            copy_n = n;
        memcpy(dst + *stored_len, src, copy_n);
        *stored_len += copy_n;
    }
    if (copy_n < n)
        *truncated = true;
}

static void AgentShellDrainFd(struct pollfd *pfd,
                              char *dst,
                              size_t *stored_len,
                              size_t *total_len,
                              bool *truncated,
                              int *active_pipes)
{
    char scratch[8192];
    for (;;)
    {
        ssize_t n = read(pfd->fd, scratch, sizeof(scratch));
        if (n > 0)
        {
            AgentShellAppendCapture(dst, stored_len, total_len, truncated,
                                    scratch, (size_t)n);
            continue;
        }
        if (n == 0)
        {
            close(pfd->fd);
            pfd->fd = -1;
            (*active_pipes)--;
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            close(pfd->fd);
            pfd->fd = -1;
            (*active_pipes)--;
        }
        break;
    }
}

int AgentShellExecExplicit(SHELL_BACKEND backend,
                           const char *cmd_line,
                           const char *working_dir,
                           uint32_t timeout_ms,
                           SHELL_EXEC_RESULT *out_result)
{
    struct timespec ts_start, ts_now, ts_end;
    struct stat work_stat;
    struct pollfd pfd[2];
    uint32_t timeout;
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    int active_pipes = 2;
    int status = 0;
    int child_reaped = 0;
    int timed_out = 0;
    pid_t pid;

    if (!cmd_line || !out_result)
        return 0;

    AgentShellResultInit(out_result);
    if (backend == SHELL_BACKEND_AUTO)
        backend = AgentShellDetectBackend();
    strncpy(out_result->backend_name, AgentShellBackendName(backend),
            sizeof(out_result->backend_name) - 1);

    /* Validate before fork. The child checks chdir again to close the race. */
    if (working_dir && working_dir[0] != '\0' &&
        (stat(working_dir, &work_stat) != 0 || !S_ISDIR(work_stat.st_mode)))
    {
        out_result->execution_failed = true;
        return 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0)
        goto launch_failure;

    pid = fork();
    if (pid < 0)
        goto launch_failure;

    if (pid == 0)
    {
        /* A dedicated group lets the parent terminate the complete tree. */
        if (setpgid(0, 0) != 0)
            _exit(125);
        if (working_dir && working_dir[0] != '\0' && chdir(working_dir) != 0)
            _exit(126);
        if (dup2(out_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(err_pipe[1], STDERR_FILENO) < 0)
            _exit(125);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);

        if (backend == SHELL_BACKEND_ZSH)
            execlp("zsh", "zsh", "-c", cmd_line, (char *)NULL);
        else if (backend == SHELL_BACKEND_BASH)
            execlp("bash", "bash", "-c", cmd_line, (char *)NULL);
        else
            execlp("sh", "sh", "-c", cmd_line, (char *)NULL);
        _exit(127);
    }

    /* Harmless if the child won the race; required before any timeout kill. */
    if (setpgid(pid, pid) != 0 && errno != EACCES && errno != ESRCH)
    {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        goto launch_failure_parent;
    }

    close(out_pipe[1]); out_pipe[1] = -1;
    close(err_pipe[1]); err_pipe[1] = -1;
    fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, fcntl(err_pipe[0], F_GETFL, 0) | O_NONBLOCK);
    pfd[0].fd = out_pipe[0]; pfd[0].events = POLLIN; pfd[0].revents = 0;
    pfd[1].fd = err_pipe[0]; pfd[1].events = POLLIN; pfd[1].revents = 0;
    timeout = timeout_ms > 0 ? timeout_ms : DEFAULT_SHELL_TIMEOUT_MS;

    while (!child_reaped || active_pipes > 0)
    {
        int poll_timeout = 10;
        int pr;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        if (!timed_out && AgentShellElapsedMs(&ts_start, &ts_now) >= timeout)
        {
            kill(-pid, SIGKILL);
            timed_out = 1;
            out_result->timed_out = true;
            out_result->exit_code = 124;
        }

        if (active_pipes > 0)
        {
            pr = poll(pfd, 2, poll_timeout);
            if (pr < 0 && errno != EINTR)
                break;
            if (pfd[0].fd >= 0 &&
                (pfd[0].revents & (POLLIN | POLLHUP | POLLERR)))
                AgentShellDrainFd(&pfd[0], out_result->stdout_buf,
                                  &out_result->stdout_len,
                                  &out_result->stdout_total_len,
                                  &out_result->stdout_truncated, &active_pipes);
            if (pfd[1].fd >= 0 &&
                (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)))
                AgentShellDrainFd(&pfd[1], out_result->stderr_buf,
                                  &out_result->stderr_len,
                                  &out_result->stderr_total_len,
                                  &out_result->stderr_truncated, &active_pipes);
        }
        else
        {
            struct timespec pause_time = {0, 1000000L};
            nanosleep(&pause_time, NULL);
        }

        if (!child_reaped)
        {
            pid_t wr = waitpid(pid, &status, WNOHANG);
            if (wr == pid)
                child_reaped = 1;
            else if (wr < 0 && errno != EINTR)
                child_reaped = 1;
        }
    }

    if (!child_reaped)
        waitpid(pid, &status, 0);
    if (pfd[0].fd >= 0) close(pfd[0].fd);
    if (pfd[1].fd >= 0) close(pfd[1].fd);
    out_result->stdout_buf[out_result->stdout_len] = '\0';
    out_result->stderr_buf[out_result->stderr_len] = '\0';

    if (!timed_out)
    {
        if (WIFEXITED(status))
            out_result->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            out_result->exit_code = 128 + WTERMSIG(status);
        else
            out_result->exit_code = -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    out_result->wall_clock_ms = AgentShellElapsedMs(&ts_start, &ts_end);
    return 1;

launch_failure:
    if (out_pipe[0] >= 0) close(out_pipe[0]);
    if (out_pipe[1] >= 0) close(out_pipe[1]);
    if (err_pipe[0] >= 0) close(err_pipe[0]);
    if (err_pipe[1] >= 0) close(err_pipe[1]);
    out_result->execution_failed = true;
    return 0;

launch_failure_parent:
    if (out_pipe[0] >= 0) close(out_pipe[0]);
    if (out_pipe[1] >= 0) close(out_pipe[1]);
    if (err_pipe[0] >= 0) close(err_pipe[0]);
    if (err_pipe[1] >= 0) close(err_pipe[1]);
    out_result->execution_failed = true;
    return 0;
}

#endif

int AgentShellExec(const char *cmd_line,
                   const char *working_dir,
                   uint32_t timeout_ms,
                   SHELL_EXEC_RESULT *out_result)
{
    return AgentShellExecExplicit(SHELL_BACKEND_AUTO, cmd_line, working_dir, timeout_ms, out_result);
}

int AgentShellExecGuarded(const char *cmd_line,
                          const char *working_dir,
                          uint32_t timeout_ms,
                          SHELL_EXEC_RESULT *out_result)
{
    char why[160];
    POLICY_CLASS c = CommandPolicyClassify(cmd_line, why, sizeof(why));
    const char *grant = getenv("SYMBOLS_ALLOW_DESTRUCTIVE");
    if (!CommandPolicyAllowed(c) && !(grant && !strcmp(grant, "1"))) {
        AgentShellResultInit(out_result);
        out_result->execution_failed = true;
        out_result->exit_code = 126;
        int n = snprintf(out_result->stderr_buf, sizeof(out_result->stderr_buf),
                         "refused by command policy (%s): %s\n", CommandPolicyName(c), why[0] ? why : "no reason");
        out_result->stderr_len = n > 0 ? (size_t)n : 0;
        out_result->stderr_total_len = out_result->stderr_len;
        return 1;
    }
    return AgentShellExec(cmd_line, working_dir, timeout_ms, out_result);
}
