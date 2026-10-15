#ifndef SHELL_HANDLE_BOUNDARY_WIN_H
#define SHELL_HANDLE_BOUNDARY_WIN_H
#include <windows.h>
#include "agent_shell.h"
int PrivateAgentShellExec(const char *, const char *, uint32_t, SHELL_EXEC_RESULT *);
void ShellBoundaryReset(void);
int ShellBoundaryReport(int call, DWORD outer_before, DWORD outer_after);
#endif
