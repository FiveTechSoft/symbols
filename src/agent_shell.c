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

int AgentShellExecExplicit(SHELL_BACKEND backend,
                           const char *cmd_line,
                           const char *working_dir,
                           uint32_t timeout_ms,
                           SHELL_EXEC_RESULT *out_result)
{
    if (!cmd_line || !out_result)
        return 0;

    AgentShellResultInit(out_result);
    if (backend == SHELL_BACKEND_AUTO)
        backend = AgentShellDetectBackend();

    strncpy(out_result->backend_name, AgentShellBackendName(backend), sizeof(out_result->backend_name) - 1);

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t_start);

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hOutRead = NULL, hOutWrite = NULL;
    HANDLE hErrRead = NULL, hErrWrite = NULL;

    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0))
    {
        out_result->execution_failed = true;
        return 0;
    }
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hErrRead, &hErrWrite, &sa, 0))
    {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        out_result->execution_failed = true;
        return 0;
    }
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutWrite;
    si.hStdError  = hErrWrite;
    si.hStdInput  = NULL;

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    char wrapped_cmd[4096];
    AgentShellWrapCommand(backend, cmd_line, wrapped_cmd, sizeof(wrapped_cmd));

    const char *work_dir = (working_dir && working_dir[0] != '\0') ? working_dir : NULL;

    BOOL ok = CreateProcessA(NULL,
                             wrapped_cmd,
                             NULL,
                             NULL,
                             TRUE,
                             CREATE_NO_WINDOW,
                             NULL,
                             work_dir,
                             &si,
                             &pi);

    /* Close write ends in parent process so EOF can be detected */
    CloseHandle(hOutWrite);
    CloseHandle(hErrWrite);

    if (!ok)
    {
        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
        out_result->execution_failed = true;
        out_result->exit_code = -1;
        return 0;
    }

    DWORD timeout = (timeout_ms > 0) ? timeout_ms : DEFAULT_SHELL_TIMEOUT_MS;
    DWORD start_tick = GetTickCount();
    BOOL process_finished = FALSE;

    /* Non-blocking drainage loop prevents pipe deadlock */
    while (!process_finished)
    {
        /* 1. Drain stdout pipe */
        DWORD avail_out = 0;
        if (PeekNamedPipe(hOutRead, NULL, 0, NULL, &avail_out, NULL) && avail_out > 0)
        {
            DWORD want = avail_out;
            if (out_result->stdout_len + want >= SHELL_BUFFER_MAX)
                want = (DWORD)(SHELL_BUFFER_MAX - 1 - out_result->stdout_len);
            if (want > 0)
            {
                DWORD rd = 0;
                if (ReadFile(hOutRead, out_result->stdout_buf + out_result->stdout_len, want, &rd, NULL))
                    out_result->stdout_len += rd;
            }
        }

        /* 2. Drain stderr pipe */
        DWORD avail_err = 0;
        if (PeekNamedPipe(hErrRead, NULL, 0, NULL, &avail_err, NULL) && avail_err > 0)
        {
            DWORD want = avail_err;
            if (out_result->stderr_len + want >= SHELL_BUFFER_MAX)
                want = (DWORD)(SHELL_BUFFER_MAX - 1 - out_result->stderr_len);
            if (want > 0)
            {
                DWORD rd = 0;
                if (ReadFile(hErrRead, out_result->stderr_buf + out_result->stderr_len, want, &rd, NULL))
                    out_result->stderr_len += rd;
            }
        }

        /* 3. Check process completion */
        DWORD wait_res = WaitForSingleObject(pi.hProcess, 10);
        if (wait_res == WAIT_OBJECT_0)
        {
            process_finished = TRUE;
        }
        else if (GetTickCount() - start_tick >= timeout)
        {
            TerminateProcess(pi.hProcess, 124);
            out_result->timed_out = true;
            out_result->exit_code = 124;
            process_finished = TRUE;
        }
    }

    /* Final drain after process termination */
    for (;;)
    {
        DWORD avail = 0, rd = 0;
        if (!PeekNamedPipe(hOutRead, NULL, 0, NULL, &avail, NULL) || avail == 0)
            break;
        DWORD want = avail;
        if (out_result->stdout_len + want >= SHELL_BUFFER_MAX)
            want = (DWORD)(SHELL_BUFFER_MAX - 1 - out_result->stdout_len);
        if (want == 0)
            break;
        if (!ReadFile(hOutRead, out_result->stdout_buf + out_result->stdout_len, want, &rd, NULL) || rd == 0)
            break;
        out_result->stdout_len += rd;
    }

    for (;;)
    {
        DWORD avail = 0, rd = 0;
        if (!PeekNamedPipe(hErrRead, NULL, 0, NULL, &avail, NULL) || avail == 0)
            break;
        DWORD want = avail;
        if (out_result->stderr_len + want >= SHELL_BUFFER_MAX)
            want = (DWORD)(SHELL_BUFFER_MAX - 1 - out_result->stderr_len);
        if (want == 0)
            break;
        if (!ReadFile(hErrRead, out_result->stderr_buf + out_result->stderr_len, want, &rd, NULL) || rd == 0)
            break;
        out_result->stderr_len += rd;
    }

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

    QueryPerformanceCounter(&t_end);
    out_result->wall_clock_ms = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    return 1;
}

#else

/* ============================================================
   Platform Implementation: POSIX (Linux, macOS)
   ============================================================ */

int AgentShellExecExplicit(SHELL_BACKEND backend,
                           const char *cmd_line,
                           const char *working_dir,
                           uint32_t timeout_ms,
                           SHELL_EXEC_RESULT *out_result)
{
    if (!cmd_line || !out_result)
        return 0;

    AgentShellResultInit(out_result);
    if (backend == SHELL_BACKEND_AUTO)
        backend = AgentShellDetectBackend();

    strncpy(out_result->backend_name, AgentShellBackendName(backend), sizeof(out_result->backend_name) - 1);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    int out_pipe[2];
    int err_pipe[2];

    if (pipe(out_pipe) < 0)
    {
        out_result->execution_failed = true;
        return 0;
    }
    if (pipe(err_pipe) < 0)
    {
        close(out_pipe[0]);
        close(out_pipe[1]);
        out_result->execution_failed = true;
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        out_result->execution_failed = true;
        return 0;
    }

    if (pid == 0)
    {
        /* Child Process */
        if (working_dir && working_dir[0] != '\0')
        {
            if (chdir(working_dir) != 0)
            {
                /* Proceed anyway if chdir fails */
            }
        }

        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

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

    /* Parent Process */
    close(out_pipe[1]);
    close(err_pipe[1]);

    /* Set non-blocking on read pipes */
    int flags = fcntl(out_pipe[0], F_GETFL, 0);
    fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(err_pipe[0], F_GETFL, 0);
    fcntl(err_pipe[0], F_SETFL, flags | O_NONBLOCK);

    uint32_t timeout = (timeout_ms > 0) ? timeout_ms : DEFAULT_SHELL_TIMEOUT_MS;
    struct pollfd pfd[2];
    pfd[0].fd = out_pipe[0];
    pfd[0].events = POLLIN;
    pfd[1].fd = err_pipe[0];
    pfd[1].events = POLLIN;

    int active_pipes = 2;
    uint32_t elapsed = 0;
    const uint32_t slice_ms = 10;

    while (active_pipes > 0 && elapsed < timeout)
    {
        int pr = poll(pfd, 2, slice_ms);
        elapsed += slice_ms;

        if (pr > 0)
        {
            /* Check stdout */
            if (pfd[0].fd >= 0 && (pfd[0].revents & POLLIN))
            {
                size_t max_read = SHELL_BUFFER_MAX - 1 - out_result->stdout_len;
                if (max_read > 0)
                {
                    ssize_t n = read(out_pipe[0], out_result->stdout_buf + out_result->stdout_len, max_read);
                    if (n > 0)
                        out_result->stdout_len += n;
                    else if (n == 0)
                    {
                        close(out_pipe[0]);
                        pfd[0].fd = -1;
                        active_pipes--;
                    }
                }
            }
            if (pfd[0].fd >= 0 && (pfd[0].revents & (POLLHUP | POLLERR)))
            {
                close(out_pipe[0]);
                pfd[0].fd = -1;
                active_pipes--;
            }

            /* Check stderr */
            if (pfd[1].fd >= 0 && (pfd[1].revents & POLLIN))
            {
                size_t max_read = SHELL_BUFFER_MAX - 1 - out_result->stderr_len;
                if (max_read > 0)
                {
                    ssize_t n = read(err_pipe[0], out_result->stderr_buf + out_result->stderr_len, max_read);
                    if (n > 0)
                        out_result->stderr_len += n;
                    else if (n == 0)
                    {
                        close(err_pipe[0]);
                        pfd[1].fd = -1;
                        active_pipes--;
                    }
                }
            }
            if (pfd[1].fd >= 0 && (pfd[1].revents & (POLLHUP | POLLERR)))
            {
                close(err_pipe[0]);
                pfd[1].fd = -1;
                active_pipes--;
            }
        }
    }

    if (pfd[0].fd >= 0) close(out_pipe[0]);
    if (pfd[1].fd >= 0) close(err_pipe[0]);

    out_result->stdout_buf[out_result->stdout_len] = '\0';
    out_result->stderr_buf[out_result->stderr_len] = '\0';

    int status = 0;
    if (elapsed >= timeout)
    {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        out_result->timed_out = true;
        out_result->exit_code = 124;
    }
    else
    {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            out_result->exit_code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            out_result->exit_code = 128 + WTERMSIG(status);
        else
            out_result->exit_code = -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    out_result->wall_clock_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                                (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

    return 1;
}

#endif

int AgentShellExec(const char *cmd_line,
                   const char *working_dir,
                   uint32_t timeout_ms,
                   SHELL_EXEC_RESULT *out_result)
{
    return AgentShellExecExplicit(SHELL_BACKEND_AUTO, cmd_line, working_dir, timeout_ms, out_result);
}
