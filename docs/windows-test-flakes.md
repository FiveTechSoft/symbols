# Windows CI flakes: source-based hardening, not a reproduced fix

On 2026-09-26, `test_agent_runner` failed one of 40 internal checks on the
Windows MSVC Release job, then passed unchanged on rerun. The failed check was
an exact two-attempt assertion in the compiler did-you-mean case. The same
result reported a solved task and one applied compiler repair. The test did
not print the actual attempt count or shell status, so the source does not
establish whether this was an extra retry, a missing count, or why. The
fixture invokes `gcc` via PATH with a 10-second per-build timeout in the
runner. It uses a fixed filename in the CTest source-root working directory.
There is no evidence in this run that another test touched that filename; a
real Windows repro with diagnostics is needed before changing the runner or
weakening the assertion. The patch adds failure-only counts/status output.

`test_agent_shell` failed once under Windows ASan, then passed unchanged on
rerun. The CTest output showed Test 5's first three checks but stopped before
its fourth, and the process took 15.23 seconds. That output was buffered, so
it does **not** locate the failing check. Source inspection shows two later
PowerShell-heavy output tests with 10-second deadlines, originally the same
as trivial echo tests: one writes 100 KB to each of two streams and the other
writes 5,000 interleaved lines. The patch grants these bounded-output tests
60 seconds and makes the suite output unbuffered, leaving the distinct 150/200
ms timeout tests and their assertions intact. A later Windows CI run must show
whether this removes the flake; the patch is not evidence of a root cause.

Both tests run from the source root under CTest. The CI workflow calls CTest
without `-j`, so CTest itself is not parallel in these jobs, but runner image
load and spawned process timing can vary. The patch changes no production
execution, audit, candidate ordering, or decision-store behavior.
