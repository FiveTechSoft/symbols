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

## First native red and bounded diagnostic revision

Run [586](https://github.com/FiveTechSoft/symbols/actions/runs/38794982933)
on `0ae3e9b147ff75a5647b1e57acac800f446cc957` failed the new MSVC and ASAN tests (30.16 s and 30.22 s):
the positive control and sensitivity mask 5 passed, but the invalid runner
reached its 30-second bound without a probe diagnostic. This is a fixture
failure with unconfirmed cause, not evidence that the shell API handled the
invalid image incorrectly.

The private runner now sets `SEM_FAILCRITICALERRORS` while preserving its other
inherited error-mode bits. No production process changes its error mode.
[Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode)
documents this flag as preventing critical-error dialogs from hanging an
application. A modal loader error is a hypothesis, not a confirmed diagnosis.
Diagnostics are flushed at private-runner entry and before/after the OS probe;
wait, assignment, resume and cleanup errors are explicit. The gold, expected
error 193, three failure calls, sensitivity mask 5 and 30-second bound are
unchanged. A second unexpected result remains red, never a redefined pass.
