/* ============================================================
   agent_shell.h: High-Performance Cross-Platform Shell Execution Engine
                  (Windows, Linux, macOS).
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.

   Capabilities:
     1. Unified Subprocess Execution: Windows (Win32 CreateProcessA),
        Linux (POSIX fork/pipe/poll/SIGKILL), macOS (Darwin zsh/sh).
     2. Hard Timeout Protection: Microsecond-precision timer with guaranteed
        process termination (prevents infinite loops in test runners).
     3. Dual Stream Capture: Discrete stdout and stderr buffers (up to 64KB)
        for surgical abductive compiler diagnostic parsing.
     4. High-Precision Telemetry: Nanosecond/microsecond wall-clock tracking.
   ============================================================ */

#ifndef AGENT_SHELL_H
#define AGENT_SHELL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define SHELL_BUFFER_MAX 65536
#define DEFAULT_SHELL_TIMEOUT_MS 10000

/* Native shell environment classification */
typedef enum
{
    SHELL_BACKEND_AUTO = 0,
    SHELL_BACKEND_CMD,         /* Windows cmd.exe */
    SHELL_BACKEND_POWERSHELL,  /* Windows powershell.exe */
    SHELL_BACKEND_SH,          /* POSIX /bin/sh */
    SHELL_BACKEND_BASH,        /* Linux /bin/bash */
    SHELL_BACKEND_ZSH          /* macOS /bin/zsh */
} SHELL_BACKEND;

/* Structured command execution result */
typedef struct
{
    int      exit_code;
    char     stdout_buf[SHELL_BUFFER_MAX];
    size_t   stdout_len;
    char     stderr_buf[SHELL_BUFFER_MAX];
    size_t   stderr_len;
    double   wall_clock_ms;
    bool     timed_out;
    bool     execution_failed;
    char     backend_name[32];
} SHELL_EXEC_RESULT;

/* Initialize an execution result structure */
void AgentShellResultInit(SHELL_EXEC_RESULT *res);

/* Detect the native shell backend appropriate for host OS */
SHELL_BACKEND AgentShellDetectBackend(void);

/* Get human-readable name of shell backend */
const char *AgentShellBackendName(SHELL_BACKEND backend);

/* Wrap raw command line in shell invocation */
int AgentShellWrapCommand(SHELL_BACKEND backend,
                          const char *cmd,
                          char *out_cmd,
                          size_t out_cmd_size);

/* Execute command using auto-detected native shell */
int AgentShellExec(const char *cmd_line,
                   const char *working_dir,
                   uint32_t timeout_ms,
                   SHELL_EXEC_RESULT *out_result);

/* Execute command using an explicit shell backend */
int AgentShellExecExplicit(SHELL_BACKEND backend,
                           const char *cmd_line,
                           const char *working_dir,
                           uint32_t timeout_ms,
                           SHELL_EXEC_RESULT *out_result);

#endif /* AGENT_SHELL_H */
