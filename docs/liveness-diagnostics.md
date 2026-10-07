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

## Bounded adaptive windows (L1b, D22)

The deferral above is historical. After the separately recorded S12 output-only #47 liveness failure, the parent approved the original L1b scope: POSIX #40 and Windows race-swap #48. #47 already extends from 5 s to a 30 s cap and failed despite that policy; it is not changed or claimed cured by L1b. Details and the limits of its phase counter are in [build_graph.md](build_graph.md#update-critical-measurement-locations-s12-d22).

L1b landed at `cafd684e780dda393529064dbb9043e7430a4ef8`, [run 37684920936](https://github.com/FiveTechSoft/symbols/actions/runs/37684920936), with binary readback equal to the approved five-file patch. Only tests and CMakeLists changed. POSIX keeps a 4 s minimum and the final floor of 50 OK per writer. Each writer publishes total OK through its own nonblocking pipe about every 500 ms after a completed batch. The parent consumes whole-long snapshots and continues in 50 ms checks until both snapshots reach 50 or the elapsed window reaches 30 s. Stale or dropped snapshots can prolong the run but cannot satisfy the unchanged final counts loaded after join. Windows race-swap keeps a 5 s minimum and extends only while zero OK, to the same 30 s cap; it reads the counter with InterlockedCompareExchange. The >=5 verified junction phases, outside invariants, bad-call guard and final success guard remain unchanged. REAL_HOLD_MS remains 30 ms.

The cap limits the decision to continue, not total wall time: the outside checks, scheduling and existing child/thread join can add time. The outside directory is checked in every extension iteration. POSIX escape-mutant stopping remains first outside touch or its existing 30 s cap, with INCONCLUSIVE exit 7 at that cap. No production code, retry policy or diagnostic-to-gate promotion changed. The existing output filters were not edited.

The native portable decision test checks fourteen boundary cells: ten for the 4 s budget and four additional 5 s boundaries. Two compile-time mutants have exact failing masks over the ten-cell set: omitting the floor condition, 0x54 (three extension cells); omitting the cap, 0x180 (two cap cells). This is a decision oracle, not scheduling or filesystem evidence.

### Local measurements and predictions

Before measuring, I predicted that three idle and three CPU-pressure samples would retain the floor in 4 s; CPU pressure might reduce throughput or imbalance writers without proving starvation. Baseline and candidate used GCC 11.4.0, -O3 -DNDEBUG and FS_CREATE_TEST_CRASH=1 in the Linux workspace, with the same production sources. CPU pressure was six arithmetic busy-loop processes, stopped and joined after the regime. Baseline ran before candidate, not interleaved; these samples cannot establish instrumentation neutrality or reproduce the CI runner.

Baseline idle OK pairs: 267/285, 339/350, 309/345. Baseline CPU: 436/410, 368/359, 345/339. All six exited 0 with zero outside mismatches; measured parent windows 4025-4044 ms. Writer CPU ranged 195-283 ms and calls 7722-9726; every completion bin had OK. Maximum batch duration was 32-42 ms except the first idle run, 336 ms for both writers. CPU pressure did not uniformly reduce successful calls, so that hypothesis did not hold. No floor failure or wedge was reproduced.

Candidate idle OK pairs: 332/292, 337/326, 353/318. Candidate CPU: 382/365, 390/382, 412/400. All six exited 0 with zero outside mismatches and no extension. A measurement-only compile with floor 600 extended to 7607 ms, snapshots 625/611 and final counts 636/615, exit 0 and zero outside mismatches. A measurement-only floor of 100000000 reached 30004 ms and failed exactly the final floor assertion, exit 1, final counts 2389/2458 and zero outside mismatches. Neither altered floor is in the patch. These were predicted extension/cap checks, not unexpected reds or a reproduced CI cause. The direct escape mutant was killed after 50 ms by one outside mismatch, exit 0 as its expected oracle. The complete local build and repository guard passed, its two mutants kept exact failing sets, and CTest #40, its escape mutant and the decision test passed 3/3. Windows was not built or measured locally.

### CI reading and remaining limits

All five L1b logs were read: apply 218 tests, zero skips, 235.44 s; Linux 218, zero skips, 223.42 s; MSVC 242 total with 16 skips, 651.05 s; ASan MSVC 242 total with 16 skips, 1198.79 s; Ninja 10 tests in 2.894 s. The decision test Passed in 0.00/0.00/0.01/0.06 s. #40 Passed in 4.04/4.03 s and Windows #48 in 5.13/5.29 s. Counts/skips and pass predictions held; no failure or sanitizer diagnostic was found in the recovered logs. No retry was needed. Passing CTest hides the decision mutant stdout and race counters. ASan step wall time was 20 m 55 s, a different measurement from CTest's 1198.79 s; no cause of the difference was inferred.

The separate Linux output-only passes had swaps 88/86/88 and OK pairs 485/529, 465/521, 457/534, zero outside mismatches, in 4.03/4.02/4.03 s. Its filter hides adaptive-window and snapshot lines. Windows #48's passing stdout is also hidden. CI therefore did not expose a measured extension, nor a reproduction cured by extension. #47, unchanged, Passed in the main MSVC/ASan suites (5.78/8.27 s); its three MSVC output-only iterations Passed in 5.82/5.90/5.87 s with calls 4902/4866/4788 and OK 3/6/2. These do not erase its S12 red or prove a cure.

More time can help a low-throughput window but need not fix a wedge. The original #40/#48 causes and the separate #47 cause remain unproved. One runner, cooperating writers, no power-loss durability and no general stability guarantee. No milestone declaration, criterion 5 closure, compiler-nightly promotion or production repair follows from these results.
