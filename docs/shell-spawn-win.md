# Native Windows invalid-image launch failure

`test_shell_spawn_win` tests one actual OS spawn failure through the normal
`AgentShellExec` implementation, without a hook, mock or production change.

The parent copies itself as `runner.exe` into a private temporary application
directory. A copied system `cmd.exe` is the positive control: a normal shell
command must create a witness there. A separate directory contains a 64-byte
invalid `cmd.exe`. Windows searches the loaded application's directory first
for this unqualified module name when `lpApplicationName` is NULL, as described
by [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa).
A direct OS probe must fail with error 193 before the shell API is tested.
Unexpected search or loader behavior is a failure, never a skip.

The frozen expectations are in `tests/shell_spawn_win_gold.json`. Three API
calls start from poisoned result bytes and must report return 0, launch failure,
exit -1, no timeout, empty captures and totals, no truncation and no witness.
A before/after native handle count must not increase. The same failure oracle
must reject the successful positive control with exactly mask 5 (status and
witness). Each private runner is bounded by 30 seconds inside a kill-on-close
Job Object. Only declared private files are deleted; other files make the
cleanup fail. There is no non-Windows stub registration.

This does not cover every launch failure or introduce stable error categories.
It does not prove a phase exit, ASAN execution before CI, or efficacy of any
solver. The existing invalid-cwd and overlong-command tests are unchanged.
