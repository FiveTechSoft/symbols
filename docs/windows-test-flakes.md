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

A separate Linux Release 100-repeat run on 2026-09-26 found a transient
`test_agent_shell` child-liveness assertion failure on repetition 72. The
single immediate `kill(pid, 0)` check could see a child still exiting while
`/proc/<pid>` vanished before the next read; the test now waits a bounded
window for only gone/zombie, checking the PID start-time to avoid reuse.
This does not explain the earlier Windows ASan failure: Windows takes the
Job Object branch, and its earlier buffered output did not locate the check.
That Windows cause remains unknown. On POSIX, the timeout path currently
ignores the return from `kill(-pid, SIGKILL)` while marking the result timed
out; a future instrumentation pass should expose failed signaling. This slice
changes no production shell code and does not claim all Phase 0 exit gates.

## Windows MSVC Test 6 follow-up instrumentation

The native Phase 0 shell stress passed 100/100 on Windows Release at
`73f2dbe`, separately from the runner's intermittent Test 6 did-you-mean
failure. The shell liveness race therefore is not an explanation for Test 6.
Two MSVC Release Test 6 failures within a day, followed by unchanged passing
reruns, warrant a separate diagnosis. Test 6 invokes `gcc` through PATH on
Windows; GCC availability and compiler stderr on the failing run are unknown.
The test now prints failure-only evidence: `gcc --version` availability,
first failed build's exit/timeout/execution status and bounded stdout/stderr,
parsed diagnostic class/symbol/suggestion, whether the repair was generated,
then hunk/preflight/applicability stages. These fields are observation only;
they neither choose a repair nor weaken any assertion. A passing CI run does
not establish the cause. Wait for a failure with this evidence before a
production fix. Also known: local GCC ASan `test_commonsense` throughput can
fail the >500,000 triples/s assertion on an unmodified `73f2dbe` checkout;
one standalone baseline run measured 371,903/s. Do not treat that benchmark
as a comparator or runner correctness regression, and do not lower its gate.
