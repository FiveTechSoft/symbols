# Fixed-window liveness diagnostics (L1a, D17)

L1a landed in `2106170`, [run 37631983349](https://github.com/FiveTechSoft/symbols/actions/runs/37631983349), with binary readback equal to its approved patch. It changes only `tests/test_fs_race_parent.c` and `tests/test_fs_win_race_swap.c`. No production code, time budget, floor, pass/fail guard or retry policy changed. POSIX retains 4000 ms and 50 successful calls per writer; Windows race-swap retains 5000 ms, at least five verified junction phases and at least one successful call.

## Why measure

Two liveness failures occurred, with different symptoms and no demonstrated common cause:

- [S7](build_graph.md#update-include-spelling-normalization-s7-d17), run 37624506798: POSIX #40 had successful calls 74/44, floor 50 each, swaps 62, all three child exits 0. One Linux-only diagnostic rerun passed. Its three later output-only samples had swaps 88 and successful calls 492/516, 536/510, 565/509. They are separate executions, not counters from the passing suite.
- D15, [run 37608349354](https://github.com/FiveTechSoft/symbols/actions/runs/37608349354): Windows ASan #48 had calls 3096, successful calls 0, swaps 65, verified junction phases 65, bad 0. One ASan-only diagnostic rerun passed #48 in 5.20 s; passing counters were hidden. No sanitizer diagnostic was reported in the failure.

The POSIX floor comment says it was chosen from a local minimum of 224 across 27 runs and was not yet measured on CI. The Windows #48 failure met its junction guard but not its success guard. Neither red is erased by a subsequent pass. Timing/environment sensitivity is a hypothesis, not a measured cause.

## Telemetry, not adaptation

POSIX writers record elapsed and process CPU time, `getrusage` context-switch and block-IO counts, completed calls/OK by operation and status, 500 ms completion bins and maximum six-call batch duration. The parent prints the measured window, unchanged budget/floor, outside mismatch count and up to 20 final root dot names with their total. Bins use each writer's own start; bin 7 contains every completion at or after 3500 ms, including longer mutant runs. A six-call batch is assigned to its completion bin, not each call's individual completion time. The parent's elapsed window excludes child joining and final diagnostics.

Windows adds per-writer counted calls/OK, a status histogram, 500 ms completion bins, the existing wait's return value and thread exit codes, and post-join elapsed time. Counted calls are precisely the existing `count()` subset, not every library call. Bins use the parent's start; bin 9 includes all completions at or after 4500 ms. No new wait-result assertion was added; collecting a wait result does not fix or validate the pre-existing wait behavior.

Instrumentation costs time and can alter interleavings. CPU/context-switch counts do not measure runnable scheduling delay. Continued calls with low OK can mean refusals or contention, not starvation. A DENIED-heavy interval with no OK and leftover control files can suggest a wedge, but final filenames alone do not establish one. No periodic control-file snapshots or per-call latency trace were added. Windows does not record CPU or scheduling counters. Early failures before the final diagnostics may still omit them. No claim of complete cause separation is made.

Passing normal CTest hides stdout. The existing Linux output-only step's grep exposes swaps/OK and pass lines, not the new telemetry. Thus the new diagnostics are available in failure stdout, not measured successful CI telemetry. No workflow filter changed. The fixed-window baseline was retained locally; it is not a second CI target.

## Local measurement

Predictions recorded before measuring: idle baseline and instrumented copies reach the floor; CPU pressure may lower throughput and imbalance the writers; IO pressure may lower OK without demonstrating starvation. Wedge requires stalled progress with compatible states, not simply a small OK count. The prediction that CPU pressure would reduce OK did not hold in these samples.

Environment: two-core Linux workspace, GCC 11.4.0, direct `-O2` builds with `FS_CREATE_TEST_CRASH=1`, same production sources, unchanged stop/guards. Each regime used three repetitions of baseline and instrumented builds, alternating their order. CPU load was six arithmetic busy-loop processes. IO load was one process cycling 64 KiB `pwrite` plus `fsync` through a 64 MiB file on the same filesystem. Combined load used both. Workers stopped and joined after each regime. This is controlled local pressure, not a reproduction of the CI runner's load or filesystem.

An initial telemetry version completed 24 runs, all exit 0. Per-writer OK ranges:

| Regime | Fixed baseline | Initial telemetry |
| --- | --- | --- |
| Idle | 283-354 | 258-294 |
| CPU | 356-390 | 359-399 |
| IO | 303-345 | 311-370 |
| CPU + IO | 293-336 | 277-375 |

The final scheduler/status-instrumented series also completed 24 runs, all exit 0. The three OK pairs per build were:

| Regime | Fixed baseline | Final telemetry |
| --- | --- | --- |
| Idle | 325/321, 328/370, 325/324 | 332/340, 292/334, 319/332 |
| CPU | 378/372, 378/365, 331/327 | 391/390, 362/345, 385/391 |
| IO | 360/356, 309/324, 265/295 | 311/312, 317/274, 298/316 |
| CPU + IO | 338/358, 312/315, 323/292 | 340/340, 348/351, 306/291 |

Final telemetry per-writer calls were 9372-9654 idle, 8196-8400 CPU, 7956-8622 IO and 7728-8160 combined. Process CPU time ranges were 255-275, 198-221, 243-265 and 181-208 ms respectively, against 4023-4066 ms elapsed. Maximum six-call batch duration was 31-49 ms. Every completion bin had calls and OK. No sustained starvation, terminal progress plateau or wedge was shown, and neither CI failure was reproduced. Load reduced completed calls, but successful calls did not uniformly fall. Small, sequential race samples cannot establish instrumentation neutrality.

A final Release-like `-O3 -DNDEBUG` smoke passed: swaps 88, successful calls 306/327, measured window 4030 ms, zero outside mismatches. The existing POSIX escape mutant was killed after 50 ms, with two outside mismatches, swaps 2 and successful calls 9/12. These are one smoke run each, not a Release load series. The matrix predates the final bounded filename-printing change; this smoke covers that final diff. Windows was not built or measured locally.

## CI and decision

All five L1a attempt-1 job logs were read: apply 212/212 in 259.17 s; Linux 212/212 in 189.13 s; MSVC 236 total with 15 skips in 397.78 s; MSVC ASan 236 total with 15 skips in 1220.04 s; Ninja AST corpus 10 tests in 2.675 s. #40 passed in 4.04 s in apply and 4.03 s in Linux. #48 passed in 5.13 s under MSVC and 5.18 s under ASan. No failure or sanitizer diagnostic was found in the inspected Tests logs. The predicted counts/skips held. Successful suite counters and new diagnostics were hidden.

The Linux output-only step separately ran #40 three times: swaps 88 each, successful calls 483/480, 462/459 and 459/422; pass times 4.05, 4.03 and 4.03 s. These are not the main-suite counters. No retry was needed for L1a.

Decision: L1b adaptation is deferred without a date until an insufficient-liveness failure is reproduced with interval/status/CPU evidence. Do not lower floors, extend windows or change guards on the basis of these green samples. More time may not fix a wedge. The causes of the two original failures remain unknown. This is diagnostics, not a stability declaration, a milestone declaration or evidence on other runners/filesystems. Cooperating-writer and no-power-loss limits remain; compiler nightly promotion and CI output-filter changes are outside this block.
