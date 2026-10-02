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
4. Product decisions: M1-2 decided on 2026-10-02 (narrower criterion, see the decision section below); not started: `AgentPatch` on the `Fs*` API (M1-3).
5. Dedicated run: the 100-repetition stress (M0-6).

## Update: M1 criterion 5 fuzz (`tests/test_fs_fuzz_ops.c`, POSIX)

Corrections to the table above, read from `master` after the audited commit:
- `test_fs_adversarial` has a Windows port (the `#else` branch: the 20 malformed strings across create, copy, move, remove and manifest, the 400 generated strings and the outside check). Criteria 1 and 5 are no longer "POSIX only" for create, copy, move and remove. Replace, batch and the other operations below are still not covered on Windows.
- `FsBatchReplace` (multi-file replace, POSIX) exists next to `FsBatchCreate`; see `docs/agent-patch-fs.md`.

What the new test covers, with a recorded seed (`0x20d1854`, override with `FS_FUZZ_SEED` and `FS_FUZZ_ITERS`; a failure prints seed, iteration, operation and path):
- Generated paths of one to three segments from a pool of `..`, `.`, empty, `safe`, `sub`, backslash, `C:`, a root symlink to a directory outside the workspace, a root symlink to a file outside, control-file names, a control byte, a leading slash. For each: replace (four expected images, including the outside file's bytes so a followed symlink would succeed), remove, create, move (both directions), copy (both directions), batch create and batch replace with the bad path as second entry, `FsTxnPlan`, then `FsBatchRecover`.
- After every operation the test requires: the operation was rejected; the workspace tree (names, kinds, modes, bytes, link targets), a directory outside the workspace and a file outside are identical to the baseline; no journal, stage or marker remains. A positive control first shows that the detector sees a real replace and a real batch replace.
- Manifests: a table of malformed requests (count 0 and 65, null arguments, bad kinds, wrong source or target arity, empty, absolute, control byte, backslash, colon, `..`, duplicate, prefix overlap, move onto itself, missing parent, file as parent, symlink component and leaf, over 1024 bytes, `FsTxnPlan` without and with a wrong expected image) and 300 random manifests where any accepted plan must name only safe paths and change nothing.
- Measured locally on Linux: 5808 rejected operations per default run, about 0.2 s (0.45 s under ASan and UBSan); 100 seeds of 600 iterations and one 20,000-iteration ASan run found no violation.

Not covered, by design: Windows for these operations (the test prints SKIP there, which is not coverage), concurrent races (`test_fs_race`), journal byte fuzzing (fixed malformed-journal cases exist in `test_fs_batch` and `test_fs_batch_replace`), content fuzzing.

Observations, not bugs: control names such as `.fstxn.commit` and `.fsrp-*` are reserved at the workspace root only; inside a subdirectory they are ordinary file names. Mutation check: dropping the reserved-name check or the manifest's target validation makes the test fail; dropping only the manifest's colon or leading-slash check does not, because `FsReadStat` rejects those paths again later (redundant defense).
Criterion 5 therefore reads: shown on POSIX for these operations with a recorded seed; not shown on Windows for replace, batch and the rest. Superseded for Windows by the 2026-10-02 update at the end of this document.

## Update: M1 criterion 4 on POSIX (`tests/test_fs_c4_posix.c`)

The new test pins behaviour measured on Linux. It is not a claim about other filesystems (macOS and Windows are usually case-insensitive) and it adds no Windows coverage.

- **Case.** `Safe.txt`, `safe.txt` and `SAFE.TXT` are three objects; an exact duplicate is refused; replacing one leaves the others; a manifest with `New.txt` and `new.txt` plans both creates.
- **Permissions** (skipped when euid is 0). In a `0500` directory, create, replace, remove, move out, move in, copy in, batch create and batch replace all fail and leave the directory byte-for-byte as it was; recovery then leaves the workspace root clean, and after `chmod 0700` the work resumes. A `0444` file is protected by its directory, not by its own bits: replace (mode stays `0444`), copy, move and remove succeed. A `0000` file cannot be read, so read, replace, copy and remove are all DENIED and nothing changes. A directory without `x` denies stat and create.
- **Path length.** A 254 or 255 byte component works for create, replace and remove; 256 and 257 are refused with no leftover. A path of 1022 or 1023 bytes works for create, replace and remove; 1024 and 1025 are INVALID.
- **Newline.** CRLF, LF, mixed, no trailing newline and a NUL-bearing payload survive create, copy, replace, move, remove and batch replace byte for byte; the reported newline kind and the binary flag match. A near-miss expected image (LF where the file has CRLF) is refused.
- **Locks.** A child holding `flock` on `.fstxn.lock` makes create and replace wait; both complete after the release and land exactly once. Only the order is asserted (the holder's release is observed before the call returns), never a duration. A lock file with mode `0644` makes create and replace DENIED. A `flock` on a target file is not honoured: replace does not wait for it. That is declared out of scope in the Fs* comments (cooperative workspace lock only).

Observation found while writing this test: a refused operation in a read-only directory used to return DENIED (or IO for some paths) and leave its journal and stage files in the workspace root, and a pending intent blocked later writers until the explicit recovery call ran. No target was ever changed, and recovery restored a clean root. Follow-up patch P1 changes create, copy, remove and move: a failure after the journal and before the source name is touched is now rolled back in process, with the same code a crash replay uses (create removes only its own byte-identical intent and its own stage). The original error is returned, the root is clean and the next writer works with no recovery call. If the rollback itself fails the journal stays and the call returns IO (fail closed). Patch P2 does the same for replace (journal present, target still the old inode) and batch create (any failure before the commit marker, including a partial publication). `FsBatchReplace` already behaved this way. All eight cases in the read-only-directory table now require a clean root with no recovery call. A failure after a name became visible keeps IO plus explicit recovery, except that move now also rolls back when only the new target link exists and the source is intact.

Criterion by platform (shown / partial / not shown):

| Aspect | Linux | Windows |
|---|---|---|
| Separator | partial: `test_fs_adversarial`, `test_fs_fuzz_ops` (backslash, `C:`) | partial: the `test_fs_adversarial` Windows port exists; not read by this audit beyond a green job |
| Case | shown: `test_fs_c4_posix` (case-sensitive, pinned) | partial: `test_fs_create_windows` has case alias checks; not read beyond a green job |
| Permission | shown: `test_fs_c4_posix` (0444, 0500 dir, 0000, no-x dir, lock mode) | partial: `test_fs_create_windows` read-only attribute only |
| Long path | shown: `test_fs_c4_posix` (255 component, 1023 path, edges refused) | partial: `test_fs_create_windows`, 240 byte component only; no total-path limit test |
| Newline | shown: `test_fs_c4_posix`, `test_fs_read` | partial: `test_fs_read` CRLF read; no write-op byte test |
| Locked file | partial: workspace lock order and unsafe lock mode shown; per-file foreign locks are not honoured by design | partial: `test_fs_create_windows` opens the file with sharing denied; replace and remove are not covered |

Criterion 4 therefore reads: shown on Linux for the six aspects within the limits above (one aspect, locked file, only for the workspace lock); partial on Windows; not shown on any other filesystem.

## Update: Phase 2 step 1, remote-advance preflight (`tests/test_agent_git_remote.c`, POSIX)

`AgentGitPreflight` has a new opt-in precondition, `require_remote_in_sync`, run last (after dirty, stale and conflict checks). It finds the upstream of the current branch, asks the remote for the branch tip with `git ls-remote --exit-code` (no fetch, no ref, index or tree change) and compares it with HEAD:

| Remote tip | Status |
|---|---|
| equals HEAD, or is an ancestor of HEAD (local ahead) | `ready` |
| not an ancestor of HEAD (remote advanced, diverged, or unknown object) | `remote_advanced` |
| branch has no upstream, or HEAD is detached with detached allowed | `no_upstream` |
| remote unreachable, name not a plain Git name, malformed answer | `inspection_failed` (fails closed) |

The default stays off, so callers that do not set the field behave as before (the test pins this). Local tracking refs are not trusted for this, since they are only as fresh as the last fetch.

The test uses a bare local remote and two clones. Every case also checks that HEAD, `refs/remotes`, index and working tree are identical before and after the check.

Phase 2 status after this step (shown / partial / not shown / not met):

| Criterion | Status |
|---|---|
| dirty, stale, detached, conflict | shown (`test_agent_git`) |
| remote advance | shown on a local file remote only; no network remote, no authentication failure case |
| submodule | shown on POSIX with a `file://` submodule (`test_agent_git_submodule`, see below); no network submodule, no nested submodule, no Windows |
| commit equals manifest | not shown |
| idempotent retry ("already applied") | not shown |
| apply-patch on the Git contracts | not met |

### Bounded finding: `src/git_ops.c` (conflict solver)

Not changed by this step. The solver is reached from `task_ops.c:3972` and `server_taskops.c:216`, which is the conflict-solving path and not the patch delivery path. It runs three history or tree rewriting commands:

- line 389: `git checkout HEAD~1 -- <file>` overwrites the working copy of one file with its parent version.
- line 346: `git checkout -m -- <file>` re-creates the conflicted merge state of one file while a merge is in progress (`MERGE_HEAD` present).
- line 477: `git reset -q --hard <head>` on the failure branch of a revert, resetting the whole tree and index to the saved head.

Line 477 is the broad one: it discards uncommitted work in the repository. It is outside the delivery contracts and is registered here as a known exception for criterion 4 of Phase 2, to be reviewed on its own.

## Decision: M1 criterion 2 narrowed (2026-10-02)

Criterion 2 now reads: interrupted multi-file **create and replace** batches either commit fully or restore the original bytes byte-for-byte. The planned mixed-operation executor (M1-2b) is dropped as planned work.

Reason: no consumer needs a mixed batch today, and a mixed executor would add a recovery surface (journal formats, crash points, foreign-state handling) that nothing justifies yet. If a real consumer appears, it is built then, with its own crash tests.

What the narrowed criterion rests on, unchanged by this decision: `test_fs_batch` and `test_fs_fuzz_ops` (POSIX) and the in-process rollback tests for batch create and batch replace. Windows batch create and replace remain not shown (`FsBatch*` returns unsupported there). The earlier criterion-2 row in the table above records the state before this decision.

## Update: Phase 2 submodule states (`tests/test_agent_git_submodule.c`, POSIX)

No production change: `AgentGitInspect` already reads submodule state from `git status --porcelain=v2`, where a submodule is an ordinary entry with a `160000` mode. The test pins that behaviour with real repositories (a library repo used as a `file://` submodule of a super repo; the fixture allows the file protocol with `-c protocol.file.allow=always` on that one command only).

| State of the submodule | Preflight |
|---|---|
| just added (`.gitmodules` and pointer staged) | `dirty_tree`, 2 staged |
| at the recorded commit | `ready` |
| tracked file edited inside it | `dirty_tree`, 1 unstaged |
| untracked file inside it | `dirty_tree`, 1 unstaged |
| moved past the recorded commit, pointer not staged | `dirty_tree`, 1 unstaged |
| pointer staged | `dirty_tree`, 1 staged |
| pointer committed | `ready` |
| checked out behind the recorded commit | `dirty_tree`, 1 unstaged |
| uninitialised (plain clone without `--recurse-submodules`) | `ready`: there is nothing to compare, so a caller that needs the submodule content must check initialisation itself |

Not covered: network submodules and credentials, nested submodules, a submodule whose remote advanced (the remote-advance check looks at the super repo only), Windows.

## Update: M1 criterion 5 fuzz, Windows slice (`tests/test_fs_fuzz_ops.c`, `#else` branch)

The Windows branch of `test_fs_fuzz_ops` was an empty skip. It now runs the confinement fuzz on NTFS: seeded malformed paths against replace, remove, create, move and copy, the manifest and transaction-plan tables, and the random manifests. A rejected operation must leave the workspace tree, a directory outside it and a file outside it unchanged (names, kind, read-only attribute, size, bytes) and leave no journal, stage or marker behind. Reproduce with `FS_FUZZ_SEED` and `FS_FUZZ_ITERS`.

Differences from the POSIX branch:

- Modes are only the read-only attribute. There is no symlink fixture; the directory link is a junction made with `mklink /J`.
- The path pool adds the Windows control names (`.fstxn.intent`, `.fstxn.remove`, `.fstxn.move`, `.fstxn.rcommit`, `.fstxn.mcommit`, `.fst-stage`, `.fsrm-stage`, `.fsmv-stage`) and Win32 aliases (`NUL`, `con.txt`, a trailing dot, a trailing space), which the writers are expected to refuse.
- `FsBatchCreate`, `FsBatchReplace` and `FsBatchRecover` are unsupported stubs on Windows. The test asserts the stub status, so it fails when batch support arrives. It makes no claim about batch behaviour on Windows.
- Default iterations are 150 and 100 (POSIX: 400 and 300) until the CI cost is measured.

Status: written without a Windows toolchain (syntax-checked only against stubs). The Windows cell of criterion 5 stays "not shown" until the CI jobs that build and run it are green; the measured CI cost is recorded in the commit closeout, not here.

## Finding: Windows writers accepted Win32 device names (fixed here)

The Windows fuzz slice (commit `a89cbd8`, run 37036245164) failed on `build-test-msvc` and `asan-msvc` with `FAIL operation with a generated path was accepted (seed=34412628 iter=7 op=create path=[NUL])`: `FsCreateFile(root, "NUL", ...)` returned OK. `wc_target_status` in `src/fs_create_win.inc` rejected a trailing dot or space, `:`, `\`, control characters and over-long segments, but not device names. The manifest layer already refused them (`valid_component` in `src/fs_manifest.c`), so only the direct writers had the gap.

Fix: `wc_device_name` refuses CON, PRN, AUX, NUL, COM1-9 and LPT1-9 in every path component, in any case, with any extension and with trailing spaces before the extension (`NUL .txt`). It sits in `wc_target_status`, which every writer entry point calls (create, copy, remove, move, replace), so one check covers all of them. The result is `FS_READ_INVALID`, like the other malformed-name cases. It is slightly wider than the manifest rule on one point: the manifest does not trim spaces before the extension.

Not covered: `CONIN$`, `CONOUT$` and the superscript digit forms of COM and LPT. `tests/test_fs_win_device_names.c` (Windows only, runs only in CI) tries every writer entry point with each device name as source, destination and directory component, checks the listing is unchanged, and checks that near-miss names (`NULL`, `COM0`, `COM10`, `LPT0`, `CONSOLE`, `nul1`, `xNUL`) still work. The logic of `wc_device_name` alone was also run on Linux against the same name lists. The fuzz test is unchanged and keeps exercising the aliases.

## Update: criterion 5 Windows cell after CI (2026-10-02)

Run 37037694490 (commit `a154d40`) is green on all four jobs, including `build-test-msvc` and `asan-msvc`, which run the Windows branch of `test_fs_fuzz_ops` and `test_fs_win_device_names`. The earlier red (run 37036245164) was the device-name gap fixed in that commit.

Criterion 5 on Windows: **partial**. Shown by CI with a recorded default seed (150 iterations, 100 random manifests): replace, remove, create, move and copy with malformed paths, the manifest and transaction-plan tables, and random manifests. Not shown: batch (the Windows batch functions are unsupported stubs, asserted as such), the iteration count of the POSIX run (400 and 300), other seeds, and `CONIN$`, `CONOUT$` and superscript COM and LPT names. The per-test pass counts and run time were not read from the CI logs; the job status is what was read.
