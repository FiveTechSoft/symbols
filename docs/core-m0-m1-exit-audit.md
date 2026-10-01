# Core exit audit: Phase 0 (M0) and Phase 1 (M1)

Audited commit: `94c04ce58985ecd1aa4efc319338baf4743d69c3`. The criteria are the "Exit criteria" lists of Phase 0 and Phase 1 in [ROADMAP.md](../ROADMAP.md). A phase closes only when every one of its criteria is shown on a final SHA. "Not shown" never means "met".

Status words: **shown** (a test and its CI result cover the whole criterion), **partial** (some of it is covered), **not shown** (no test found), **not met** (the repository contradicts the criterion).

## How this audit was done, and its limits

- It is a read of `tests/*.c`, `CMakeLists.txt`, `.github/workflows/ci.yml`, `include/agent_shell.h` and the sources named below. Nothing was executed for the audit itself.
- CI logs were not opened. The only CI evidence used is job-level success for the exact-SHA run [36885353984](https://github.com/FiveTechSoft/symbols/actions/runs/36885353984): apply, Linux, MSVC Release, MSVC ASan and the Windows Ninja AST job all succeeded. `ci.yml` runs plain `ctest` on the three build jobs, with no exclusions and no repetition. That does not show which individual tests ran or passed.
- "No test" means none was found in `tests/` or `CMakeLists.txt` at that commit. The Windows `test_fs_win_*` tests were read by assertion names only.
- The tests added with this document (below) were run locally on Linux only. They are not CI evidence until a CI run on the SHA that contains them says so, and they are POSIX-only.

## M0: Phase 0, safe shell execution

| # | Criterion | Status at the audited commit | What exists | Minimal test for the gap |
|---|---|---|---|---|
| 1 | Invalid `cwd` and spawn failures produce zero command side effects | Partial | Invalid `cwd` is tested on POSIX and Windows: return 0, `execution_failed`, and a witness file is not created. No spawn-failure test. The API has only a boolean `execution_failed`, no stable error categories (a Phase 0 deliverable) | Force launch failure and assert `execution_failed` and no side effect |
| 2 | At least 100,000 bytes written simultaneously to both streams, no deadlock or false timeout, Linux and Windows | Partial | The large-output test writes all of stdout and then all of stderr (sequential). The interleaved test writes about 20 KB per stream | Two concurrent writers of 100,000 bytes each, with exact totals and stream purity |
| 3 | Timeouts leave no surviving child or grandchild, both platforms | Not shown | POSIX checks one background child, no grandchild. Windows asserts only `timed_out` and exit 124, with no survivor check | POSIX: a child that starts a grandchild, check both PIDs. Windows: record child and grandchild PIDs and query them after the timeout |
| 4 | Truncation is distinguishable from timeout and ordinary failure | Shown at field level | `stdout_truncated`, `stderr_truncated`, `timed_out`, `exit_code` are separate fields. The large-output test checks truncated with no timeout and exit 0. No combined case | Truncation with a non-zero exit, and truncation then timeout |
| 5 | `test_agent_shell`, the full CTest suite, MSVC build/test and MSVC ASan pass on the same final SHA | Shown at job level only | Run 36885353984 succeeded on all jobs. Per-test results were not read. Two Windows flakes are on record in [windows-test-flakes.md](windows-test-flakes.md), and their hardening is source-based and unproven | Read the three `ctest` logs for that SHA and record a PASS line for `test_agent_shell` in each |
| 6 | The concurrency and timeout stress set repeated 100 times: no hang, orphan or nondeterministic status | Not shown | No repetition in CI or CMake | A dedicated manual run of `ctest -R test_agent_shell --repeat until-fail:100` with an orphan-process count afterward. Windows and ASan separately. This is a new CI run |

Summary: criterion 4 is shown, criterion 5 is shown at job level, criteria 1 and 2 are partial, criteria 3 and 6 are not shown.

### Tests added with this document (POSIX, local only)

Added to `tests/test_agent_shell.c` under `#ifndef _WIN32`. No production code changed.

- **Spawn failure (criterion 1, POSIX).** With the descriptor limit lowered so `pipe()` fails, `AgentShellExec` returns 0 with `execution_failed`, the command does not run, and no descriptor leaks. With `PATH` pointing at a missing directory, the shell cannot be executed. That case is reported as exit 127 from the forked child with `execution_failed` **not** set; the test pins that and the safety property that the command never ran. Whether such a failure should also set `execution_failed` is an API decision, not made here.
- **Concurrent dual-stream output (criterion 2, POSIX).** Two background writers produce 100,000 bytes each at once. The test asserts exit 0, no timeout, exact totals, both buffers full and flagged truncated, and each buffer containing only its own stream's byte.
- **Truncation combined (criterion 4).** 100,000 bytes on each stream with `exit 3` keeps exit 3, both truncation flags and exact totals. 100,000 bytes then a long sleep with a short deadline reports `timed_out`, exit 124 and stdout truncation.
- **Grandchild survival (criterion 3, POSIX).** A shell starts a child that starts a grandchild. After the timeout both recorded PIDs must be gone or zombie, using the same `/proc` start-time check as the existing child test.

Local result on Linux: 91 of 91 assertions pass, repeated 8 times with the same result. As a sensitivity check, replacing the process-group kill with a single-PID kill in a scratch build made the suite hang until killed (the surviving process holds the pipe open), so the grandchild test does not pass on that defect. That scratch change was reverted and is not part of this patch. Criteria 1, 2, 3 and 4 remain **partial** until a CI run on the resulting SHA covers them and the Windows side exists.

## M1: Phase 1, structured filesystem

Present in the repository: typed `stat`, `list`, `read`, `create`, `replace`, `move`, `copy`, `remove` (POSIX), a workspace-root capability, create-intent journal and recovery, read-only operation manifest and transaction planning, batch create, and Windows NTFS handle-relative create, replace, move and remove with a process-crash journal.

| # | Criterion | Status at the audited commit | What exists | Minimal test or decision for the gap |
|---|---|---|---|---|
| 1 | `..`, absolute path, symlink escape and rename races fail closed in adversarial fixtures | Partial, POSIX only | `test_fs_adversarial` and `test_fs_race` are wrapped in `#ifndef _WIN32` and do nothing on Windows. POSIX covers traversal, `C:`, backslash, `.` and empty segments across create, copy, move, remove and batch, symlink leaf and directory, renamed root, and fork races on create, copy and move | Port the adversarial set to Windows (`..`, drive paths, `\\?\`, junctions) or declare Windows outside this criterion |
| 2 | Interrupted multi-file mutations commit fully or restore the original bytes | Partial | Only multi-file create has a batch executor (`test_fs_batch`, POSIX only, crash at each of eight steps, foreign and malformed fixtures). `test_fs_txn_plan` is read-only planning. No mixed-operation executor. Windows batch is not shown | Product decision: narrow the criterion to batch create, or build and test a mixed executor (medium size) |
| 3 | Unified-diff behavior reimplemented on the structured filesystem API with no patch regression | Not met | `src/agent_patch.c` still opens files with `fopen` (reads and writes). No agent module calls the `Fs*` write API. `FsNameOk` and `FsReadSandbox` in `tool_executor.c` are unrelated sandbox name checks | Product decision: migrate `AgentPatch` writes to `FsReplaceFile` and `FsCreateFile` with expected bytes, keeping `test_agent_patch` and the patch fixtures green (medium size) |
| 4 | Linux and Windows fixtures cover separator, case, permission, long path, newline and locked-file behavior | Partial | Newline: `test_fs_read` (CRLF and LF). Long path and locked file: only `test_fs_create_windows`. Separator: backslash and `C:` in the POSIX adversarial test. No case-sensitivity test and no permission test on either platform. `test_fs_copy`, replace, move and remove are POSIX-only in CMake | A criterion by platform table with one test per empty cell. Case and permission have no test on any platform |
| 5 | Fuzzing malformed paths and operation manifests yields no out-of-workspace write | Partial | `test_fs_adversarial` runs 400 seeded iterations over six segments for create, copy and move, with an outside-file check. POSIX only. No replace or remove, no malformed manifest | Extend to replace, remove and manifests with a recorded seed and the same outside check, then run on Windows |

Summary: no M1 criterion is shown. Criteria 1, 2, 4 and 5 are partial and criterion 3 is not met. Windows adds weight to every partial: the adversarial and race tests run nothing there.

## Order of work, by cost

1. Read-only: record per-test PASS lines for the audited SHA (M0-5), and fill the criterion by platform table (M1-4).
2. Small tests, no new run beyond ordinary CI: the four POSIX tests above are in this change. Windows equivalents remain.
3. Medium: Windows survivor check (M0-3), Windows adversarial and wider fuzz (M1-1, M1-5), case and permission fixtures (M1-4).
4. Product decisions, not started: mixed-operation executor or a narrower criterion (M1-2), `AgentPatch` on the `Fs*` API (M1-3).
5. Dedicated run: the 100-repetition stress (M0-6).
