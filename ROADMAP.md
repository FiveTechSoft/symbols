# Symbolic LLM Engineering Roadmap

This roadmap turns the current native C11 engine into a dependable engineering agent through measured capability gates. It is ordered by dependency, not by calendar date. A phase exits only when its evidence is reproducible in CI on the final commit.

## Where we are

Snapshot: master at `ce46d28` (m225) before this docs patch (last change under `src/`: `b1fc280`, m224, `PatchVerifyAgainstBuffer` and `PatchApplyAtomic` share one routine and refuse a hunk that is not found exactly once at its own step; see the status update dated 2026-10-06 below); the QEMU and containment line stays parked at `3ab774506024ded0a9cf097b304565489d79d218`. This section is the entry point for anyone (Antonio, Mimo, OpenCode) who needs to know where the project stands and where it goes next. The phase sections below remain the detailed contract. Sizes are S, M, L and dependencies are stated; there are no dates.

How to read status words: **landed** means code or docs are on `master` with the cited evidence. **Exit not declared** means a phase has landed pieces but nobody has recorded that every one of its exit criteria below holds on a final SHA. A phase closes only when all of its own exit criteria are shown on the final commit. Absence of a recorded check is reported here as "not shown", never as "met".

### The core, and the core-first rule

The core is the deterministic verifier and its engineering operators: bounded shell execution, structured filesystem and Git, build/test understanding, and the verified repair loop (perceive, test, correct, consolidate verified experience, abstain). It follows the operating principles above, in particular principle 8 (memory + verification) and the domain orientation (shell, git and C first). The rule is: **the core comes before any new front.** A new front is only opened when it unblocks a core milestone or Antonio decides it.

### Milestones

| # | Milestone (phase) | Status | Evidence on `master` | Closes when | Remaining size and dependencies |
|---|---|---|---|---|---|
| M0 | Safe shell execution (Phase 0) | Foundation landed, exit not declared | Commit `ba90e1d4ba648e844c3e975ad8e870df38877c89` (70/70 `test_agent_shell`, bounded capture, process-tree timeout); command safety policy with 82 labeled commands and fuzzing, see [docs/core_triage.md](docs/core_triage.md); Windows flake hardening is source-based only, see [docs/windows-test-flakes.md](docs/windows-test-flakes.md) | All Phase 0 exit criteria hold on one final SHA, including the 100x stress repeat and the Linux/MSVC/ASan matrix | S. No dependency. The Windows flake fix is unproven until a later Windows run shows it |
| M1 | Structured filesystem (Phase 1) | Largely landed, exit not declared | `fs_*` modules and tests on `master`: read-only manifest and journal planning, POSIX create/replace/move/copy/remove with recovery, adversarial path and race tests, Windows NTFS handle-relative create with a process-crash journal (for example `0458caa`); crash-safe multi-file create and multi-file replace on POSIX (`FsBatchCreate`, `FsBatchReplace`) and, since 2026-10-03, on NTFS (`FsBatchCreate`, `FsBatchReplace`, `FsBatchRecover` in `src/fs_batch_win.inc`: hard-link pins, journal and marker, crash-point matrices, a foreign-process handle test and mutants, all green in CI runs 323 to 328; **partial**: one runner and one NTFS volume, no power-loss claim, orphan pin names after some crash points, a foreign inode at a target fails closed instead of being restored; evidence and non-claims in [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md)); no mixed-operation executor | All Phase 1 exit criteria hold, including path fuzzing with no out-of-workspace write and unified-diff behavior reimplemented on the filesystem API (neither is shown as met here) | M. Depends on M0. Power-loss durability stays explicitly deferred |
| M2 | Safe Git (Phase 2) | Partial, exit not declared | `git_ops` and `agent_git` modules with tests; the guarded apply-patch workflow gives a reproducible delivery boundary | All Phase 2 exit criteria hold; in particular apply-patch must use the Git contracts rather than open-coded assumptions (not shown as met) | M. Depends on M1 |
| M3 | Build and test intelligence (Phase 3) | Partial | `build_ops` repair rules verified by a real out-of-tree build; bank `build_ci` went 0/7 to 7/7 but those task texts were read before the rules were written, so that is development evidence | CMake File API and CTest metadata ingestion, affected-test selection with 100% recall and full-suite fallback, versioned benchmark runner (none of these found in the repository) | L. Depends on M2 |
| M4 | Clang AST and `compile_commands.json` (Phase 4) | Design closed, phase open | [docs/ast-design-spike.md](docs/ast-design-spike.md), [docs/ast-inspect.md](docs/ast-inspect.md): opt-in read-only libclang inspector; the native Windows Ninja job ran its tests for real only from `530cc26` (before that all tests were skipped); no edit authority | Phase 4 exit criteria; first consumer proposed is read-only impact reporting next to `CodeGraphComputeBlastRadius` | L. Depends on M3 metadata |
| M5 | Persistent workflows (Phase 5) | Not started | No dedicated deliverable on `master` | Phase 5 exit criteria | L. Depends on M2 and M3 |
| M6 | Verified episodic memory (Phase 6) | Slice landed, phase open | Atomic fact store at `1c9dd550241b8cd0aaebac118d8bdaa04bde6615`; opt-in shadow episode audit and offline replay gate ([docs/engineering-episodes.md](docs/engineering-episodes.md), [docs/episode-replay.md](docs/episode-replay.md)); the solver does not read the episode store | Phase 6 exit criteria (verified-only write, forgetting, invalidation, measured benefit) | L. Depends on M3 and a trustworthy verifier |
| M7 | Verified Reflexion-style loop (Phase 7) | Minimal slice landed, phase open | Commit `06cc3ba`: analogue tasks 0/3 reflection-off versus 2/3 on; Mimo's counts-only blind rerun on `27a3b21` found no gain in solved tasks (2/24, zero wrong edits); see [COORDINATION.md](COORDINATION.md) | Phase 7 exit criteria, not the slice | L. Depends on M6 |

### What is measured and what it says

- The engineering bank (59 tasks, 9 categories) was last recorded at 49/59 for both the CLI and OpenCode, with 0 wrong edits and the bank self-test at 59/59 (COORDINATION.md, September 25 entries). CTest was 113/113 locally in the same entries. The bank is partly development-visible.
- Mimo's blind batch, counts only, was 2/24 on `27a3b21` with zero wrong edits. The gap between the visible bank and the blind batch is the honest headline: **the verifier never makes a wrong edit, but its recall on unseen tasks is low.** Closing that gap, without weakening abstention, is the main core problem.
- The metrics block in `README.md` is a snapshot from commit `d6a0aad` (September 24). It is older than the numbers above. Read the status log in COORDINATION.md for newer counts until the block is regenerated.
- M1 criterion 4 (fixtures for separator, case, permission, long path, newline and locked file), Windows: **shown for the six aspects on the listed cells, not "met"**. One runner (`windows-latest`, NTFS), one account, cooperating writers only, no power-loss claim; the not-shown columns (for example other accounts, inherited ACLs, 8.3 aliases, symlink leaves, batch operations against foreign handles) are non-claims. Mutants exist only for the permission aspect. Evidence and per-aspect table: [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md), section "criterion 4 on Windows, final status per aspect (2026-10-04)", last run read 37169864690 (170 of 170 on msvc and asan-msvc, asan read through the total only). The M1 exit is still not declared: path fuzzing and diff-on-the-filesystem-API are not shown.
- M1 criterion 5 (path and manifest fuzz, no out-of-workspace write), Windows: **partial**. `test_fs_fuzz_ops` now runs the default seed plus three logged seeds (1, 7 and 12648430, 400 iterations) in CI. Two of them found a production bug on 2026-10-04: a create, copy or move whose parent component was a regular file left a pending transaction (no write outside the workspace). Fixed in `wc_parent` and covered by `test_fs_win_fileparent` and its mutant; green in run 37173202649 (176 of 176 on msvc and asan-msvc). Not shown: other seeds, journal and content fuzzing, recovery of the old pending state. Details in [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md).
- `AgentPatch` mixed line endings (2026-10-04): untouched lines of a file with both CRLF and LF were rewritten to CRLF (measured on Linux); fixed so each line keeps its ending, nine cells and a killed mutant on Linux. On Windows the same nine cells pass in CI (m90, `windows-latest`, one runner), shown for those cells only. `test_agent_runner` had a timing flake on Windows runners (three occurrences); a warm-up was added, **not shown fixed**.
- M1 exit reading, second version (2026-10-04, code at `a5c0400`, CI run 37184347825, all four jobs green: linux 170/170, msvc and asan-msvc 181/181): **the M1 exit condition ("All Phase 1 exit criteria hold") is NOT met.** Shown for the listed cells on POSIX: criteria 1, 2, 3, 4, 5. Shown for the listed cells on Windows (one runner): criteria 1, 4, and 5 for the listed operations and seeds. **Windows criterion 2 is partial**: orphan pin names left by the six listed batch crash points are swept by the next batch call since `530adeb` (CI run 37186804952, docs in m108); an unshared `.fsrp-` pin is kept by design, the single-file calls do not sweep, a crash during the sweep was not injected, and POSIX orphans were not measured. A foreign inode at a target makes recovery refuse and leave it untouched instead of restoring over it; that is the same on POSIX, so it is a reading of the criterion (cooperating writers), not a Windows difference. **Windows criterion 3 is partial against POSIX parity**: the single-replace crash matrix through `AgentPatch` exercises only crash point 4, and T9 is not ported. **Update (m143, run 37211667043, `5d0455f`, msvc and asan 184/184, linux 171/171):** the Windows single-replace crash matrix through `AgentPatch` now covers phases 0 to 6 (`test_agent_patch_crash_win`, mutants 10 and 11); T9 has no Windows reading yet, so Windows criterion 3 stays partial. **Update (m147, run 37215099208, `85bc2ff`, msvc and asan 185/185, linux 171/171):** the T9 analogue now has a Windows reading (`test_agent_patch_lock_win`): a directory, a symlink, a non-empty file and a held-open lock are refused and a plain empty lock applies; a hard-linked empty lock is accepted, measured and not asserted, and recorded as an accepted documented limit (main agent decision under the standing mandate, retractable by Antonio). Windows criterion 3 stays partial (no lock contention case, no power loss, one runner). **Update (m151, run 37219123908, `b9e0c8f`, msvc and asan 186/186, linux 171/171):** a Windows mutant of the diff-apply drift guard (mutant 12, `test_agent_patch_fs_win_mc`) is killed by exactly four cells; the Linux `test_fs_race_parent_mc` flake (three occurrences) has a measured cause, wedged writers, and a fix for the mutant build in m150, not shown fixed yet; the non-mutant test has the same wedge, an open measured gap. Criterion 5 has no journal or content fuzzing and only the logged seeds on both platforms. **Correction (m136):** the Windows default seed runs 150 iterations, not 400 as the wording of this reading and the audit implied; only seeds 1, 7 and 12648430 run 400 on Windows (POSIX default: 400). Details in [docs/core-m1-exit-reading.md](docs/core-m1-exit-reading.md). M1 is not declared; the declaration is Antonio's. Table, per-cell tests and the NOT SHOWN list: [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md), section "M1 exit reading, second version"; the first version (run 37177291280) is kept above it for the record.
- Status update (2026-10-04, master `ed21782`; last full CI run 37227572528 at `ed21782`: linux 171/171, msvc and asan-msvc 188/188, `ast-inspect-windows-ninja` 10 tests OK): the M1 exit condition stays **NOT met**. The second reading above (`a5c0400`, 181/181) is the last full reading and is superseded here only for its numbers. Since then: Windows criterion 3 is **partial against POSIX parity** (crash phases 0 to 6 and the lock cells L1 to L5 shown, L6 a hard-linked lock accepted as a documented limit, mutant 12 killed by exactly 4 cells; a second process waiting on the lock is shown for two processes on one runner: `test_agent_patch_contend_win` and its mutant target `test_agent_patch_contend_win_mc` passed in msvc and asan-msvc, 188/188, run 37227572528 at `ed21782` (one run, not repeated, so not shown stable; the first mutant run at m159 SURVIVED because the mutant re-read the target and `FsReplaceFile` compared again, and m160 staggered the sleeps; a waiter killed while holding the lock, power loss and other runners are not tried). `test_fs_race_parent_mc` had three INCONCLUSIVE runs since m141 plus one earlier; m150 and m154 narrow it and it is **not shown fixed**. The `ast-inspect-windows-ninja` job showed green with every test skipped from 2026-09-25 until m156; docs that cited it as a pass are corrected. One runner, cooperating writers, no power-loss claim. Details: [docs/core-m1-exit-reading.md](docs/core-m1-exit-reading.md).
- Status update (2026-10-05, master `15de1a7`, m181; run 37247707282: linux 181/181, msvc and asan-msvc 196/196, `ast-inspect-windows-ninja` 10 tests OK): the M1 exit condition stays **NOT met** and the declaration is Antonio's. Criterion 5, journal part: the journals are now byte fuzzed on both platforms, six POSIX journals (replace, create, remove, move, batch create, batch replace) and six Windows journals (replace, create, remove, move, batch create, batch replace), with a strict oracle, two seeds, real process kills at the crash points that leave a journal, and the outcomes that are NOT clean pinned as asserted limits with their exact result (`tests/test_fs_win_journal_fuzz*.c`, m171 to m181, [docs/core-m1-exit-reading.md](docs/core-m1-exit-reading.md)). The limits all sit outside the cooperating-writer model: a writer that recomputes the checksum can make recovery follow a rewritten name, and on the Windows batch create that leaves a half applied batch (the batch is NOT atomic against that writer); a journal deleted after the publish leaves leftover pins, a second link or a mixed state (the same class as POSIX F5). A CRC or checksum detects accidental damage, not a writer that recomputes it. **Remaining evidence for M1, as read in the exit reading, not as a plan:** (1) Windows criterion 2 partial: single-file create, copy, move and remove recovery does not sweep pre-intent orphans (read, not measured; POSIX has no sweep either, single-file replace recovery sweeps since m139), an unshared `.fsrp-` pin is kept by design, the sweep killed after its first removal (m184 to m186) and after its 2nd to 6th removal through chained `FsBatchRecover` kills (m211, run 37318504276; the other entry points are not chained), a foreign inode at a target is refused and not restored; (2) Windows criterion 3 partial against POSIX parity: a lock holder killed from outside while a second process waits is shown only for the batch replace and an `FsBatchRecover` waiter (m191, run 37265496715), power loss and other runners are not tried, the hard-linked lock is an accepted documented limit; (3) criterion 1 on Windows: 8.3 aliases shown only for the `.fstxn.lock` control file (9 operations refused with status DENIED, nothing changed; an ordinary empty file under a held byte-range lock is refused by the same operations (m203), which fits the lock as the reason; m203 also found that the 8.3 alias of a foreign reserved-prefix file was accepted, and m204 (run 37297634708) refuses it by a final-name check in `wc_open`, m206 and m207 (runs 37303780972 and 37305908222) showed all nine reserved prefixes refused through an alias and repaired the alias of a reserved directory used as a path component, which had been accepted; reads, arbitrary short names and other filesystems not covered; m194 and m195, runs 37271090475 and 37273525033, msvc and asan 197 of 197), other 8.3 cases not shown, per-directory case flag shown for a flagged subdirectory only (names that differ only by case are different entries and the library acts on the exact-case entry; a flagged workspace root and two-entry batches in a flagged directory shown in m200 and m201, runs 37284795139 and 37287889480: a foreign `.FSTXN.LOCK` is left alone, the reserved name in another case is refused by name, and a batch naming two spellings is refused whole; m197 and m198, runs 37277509454 and 37279224457), a parent swapped to a junction is covered for create, replace, remove, batch create (`test_fs_win_race` cell E) and move, copy, batch replace (`test_fs_win_race_swap`, m209, run 37311710086) as an invariant over a time budget on one runner, not a proof (see docs/core-m1-exit-reading.md, "C1 on Windows: parent swapped to a junction"); (4) criterion 5: Windows crash points with no journal (1, 2, 21, 22, 23) and batches of more than two items are covered by reading only, file contents and single-hunk `AgentPatch` plans are fuzzed on Windows since m218 and m220 (see the 2026-10-06 status update), plans of up to three disjoint hunks with one- or two-line regions are fuzzed since m225, hunk context strings are not, and how many seeds suffice is Antonio's call; (5) not shown for the whole reading: power-loss durability (deferred), other runners, filesystems and accounts, non-cooperating writers, recovery of an intent left by the old file-parent bug; (6) flakes recorded, not shown fixed: `test_fs_race_parent_mc` inconclusive runs and the `test_fs_win_race` E diagnostic (about 3 per 4600 calls in m173; passed in the m179 and m181 runs; failed once more in the m213 run, msvc job 111817148358; m214 extends its window up to 30 s while no writer call has succeeded, runs green, fix not shown, cause on Windows not measured); a foreign file named like a control file (`.fst-`, `.fsp-`, `.fsrs-` plus 32 hex) is deleted by the orphan sweep (m213, measured, no repair decided). M2 and the declarations are not touched by this update.
- Status update (2026-10-06, master `749cf49`, m223; run 37391350416, all four jobs read: apply 186, linux 186, msvc 210, asan-msvc 210, `ast-inspect-windows-ninja` 10 tests OK): the M1 exit condition stays **NOT met** and the declaration is Antonio's; this supersedes the 2026-10-05 counts above for numbers only. Since then, each with its run read: m218 (run 37345059480) a Windows content fuzz of `FsCreateFile`, `FsReplaceFile` and `FsCopyFile` with generated bytes, 4 seeds x 100 rounds; m220 (run 37351517071) a Windows `AgentPatch` fuzz for single-hunk plans, 4 seeds x 100 rounds, whose first asan attempt failed only in `test_commonsense` (a wall-clock throughput benchmark; green on re-run without a code change, one occurrence tracked, not fixed); m221 (run 37383864652) `AgentPatch` now refuses a target holding a NUL byte (before, a hunk before the NUL cut the file there, measured on Linux only), a refusal and not support, with `tests/test_agent_patch_nul.c` and a mutant that exits 0 only when exactly cells A, B, D, F and G fail; m222 (run 37388340967) renamed a test macro `OUT` that `windows.h` also defines. Criterion 5 stays **partial**: not shown are multi-hunk and multi-line `AgentPatch` plans, files near the 1 MiB limit, other runners and accounts, and power loss. Details and limits: [docs/core-m1-exit-reading.md](docs/core-m1-exit-reading.md). Lines printed by the new tests are known from the CTest regex pass, not read in the logs.
- Status update (2026-10-06, later, master `ce46d28`, m225; run 37397505793, all four jobs read: apply 189, linux 189, msvc 213, asan-msvc 213 of 213, `ast-inspect-windows-ninja` 10 tests OK): the M1 exit condition stays **NOT met**, the declaration is Antonio's. m224 (run 37395462405) closed a wrong-site edit in multi-hunk `AgentPatch` plans found by reading and probing on Linux: a plan that verified cleanly could edit a site the verifier had not checked. The sequential model is kept (the runner's missing-header repair relies on it) and made fail closed; the result still depends on the order of the hunks, and a plan that becomes ambiguous only after an earlier hunk is refused. m225 added a multi-hunk fuzz on POSIX and Windows. Details and limits: [docs/core-m1-exit-reading.md](docs/core-m1-exit-reading.md). Lines printed by tests are known from the regex pass, not read in the logs.
- Final-SHA CI exists: the apply workflow calls `ci.yml` for the exact commit it pushes (see "Closed: final-SHA CI gap"). Pages and bank workflows are separate and do not always run on a given SHA, so one SHA's green apply-plus-CI does not imply them.

### Landed side lines

- **Typed C contract line.** A strict, read-only data contract and static previews: [v1](docs/typed-c-contract-v1.md), [v2 Slice A verify without target execution](docs/typed-c-contract-v2-slice-a.md), [binary-safe preview](docs/typed-c-binary-preview-v1.md), and a CLI-only abstain-and-ask pilot ([docs/abstain-and-ask.md](docs/abstain-and-ask.md)). Free-form stdout parsing was rejected after independent gates, see [docs/c-stdout-contract-boundary.md](docs/c-stdout-contract-boundary.md). The conservative `shell_contract` guard stays held. **Target-code execution is not enabled:** an eligible v2 candidate still returns `probe_unavailable`.

### Parked: the QEMU and containment line

- **Why it exists.** Running a candidate's compiled program is the one step that needs a real isolation boundary ([docs/typed-c-slice-b-isolation-design.md](docs/typed-c-slice-b-isolation-design.md)). The line studied a disposable hosted runner VM as that boundary ([docs/qemu-runner-vm-contract.md](docs/qemu-runner-vm-contract.md), evidence in [docs/qemu-runner-vm-evidence-ledger.md](docs/qemu-runner-vm-evidence-ledger.md)).
- **Where it stops.** Documented up to `3ab774506024ded0a9cf097b304565489d79d218` ([docs/qemu-runner-vm-benign-scope-setup-design.md](docs/qemu-runner-vm-benign-scope-setup-design.md)). Everything on this line is no-boot metadata observation or documentation. All nine isolation gates are `not_proven`. No isolation, resource-limit or cleanup claim is made, and no candidate code has been run under it.
- **Why it is parked.** Its next step needs a verified, dedicated cgroup parent grant that does not exist, and the choice below is a product decision, not an engineering one.
- **Open fork, Antonio's decision, no urgency** (details in the design doc): A) GitHub-hosted runner under the current no-privilege rules, where a private cgroup path stays blocked without verified delegation; B) self-hosted hardware under Antonio's house rules, with persistent runner, network, storage and secret risk; C) the nine gates without a private cgroup, with the narrower claims that allows.
- **Unparking** happens only on Antonio's choice of A, B or C. Until then no work on this line is scheduled, and nothing on the core waits for it.

### What is missing for core v1

Proposed definition of v1, for Antonio to accept or change: **Phases 0 to 3 meet their exit criteria on a final SHA, the Phase 0-1 and Phase 2-3 columns of the measurable targets table hold, harmful edits stay at 0, and the blind batch shows a measured gain over its current 2/24.** Phases 4 to 7 and the Oracle vision stay beyond v1 except for the slices already landed.

In dependency order, with sizes:

1. Declare M0 and M1: run and record the remaining exit checks (path fuzzing, diff on the filesystem API). S to M. No dependency. The M0 stress repeat is no longer on this list: 100 of 100 iterations passed on Windows (run `36937340017`, commit `50ee995`) and on Linux (run `37115541002`, commit `f1c43cb`), within the limits stated in the M0 update of [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md); ASan and macOS are not covered, and M0 is not declared by that update.
2. M2: move apply-patch onto the Git contracts and close the remote-advance, stale-HEAD and interrupted-retry fixtures. M. After step 1.
3. M3: CMake File API and CTest ingestion, then conservative affected-test selection with full-suite fallback. L. After step 2.
4. Generalization of the repair operators on unseen tasks, without hand-tuning on the blind or held-out sets and without any wrong edit. L. Runs alongside steps 2 and 3, scored by Mimo's counts-only batch.
5. Regenerate the README metrics block so one source states current numbers. S.

Not on this list on purpose: new fronts, new languages, QEMU or any execution-isolation work, and the Oracle.

## Operating principles

This roadmap does not encode one behavior per situation. It builds mechanisms: **perceive** the repository and its context, **test** candidate changes against the world, **correct** without hiding failure, **consolidate** only verified experience, and **abstain** when evidence is insufficient. Each phase below strengthens specific mechanisms; each gate exists to prove them.

1. **Deterministic core.** Repository state, plans, actions, and verification are explicit data. A probabilistic model may propose work at the boundary, but it is never the source of truth for permissions, invariants, or success.
2. **Fail closed.** Unknown state, ambiguous paths, stale preconditions, unsupported syntax, and incomplete verification stop the affected action without partial side effects.
3. **Structured operations before shell commands.** Shell execution remains an escape hatch. Filesystem, Git, build, test, and code operations gain typed contracts and narrower capabilities.
4. **Evidence over claims.** Every milestone names its fixture, benchmark, invariant, or CI job. Local success is progress; cross-platform CI on the final SHA is the release gate.
5. **Reversible changes.** Mutations use dry runs, precondition checks, atomic replacement where possible, and explicit rollback or compensation.
6. **Facts and derived knowledge stay separate.** Source facts carry provenance. Heuristics, scores, diagnoses, and prior experiences are marked as derived and can be invalidated.
7. **Natural conversation, grounded in evidence.** symbols behaves as a technical partner, not a legacy command bot. It reports what it did, what it verified, and what it does not know in plain language, grounded in the real state of the work. Conversational quality is a measured capability with its own fixtures, never a cosmetic layer over canned responses.
8. **Memory + verification.** Memory is what makes learning accumulate; verification is what makes what accumulates true. Neither is enough alone: memory without verification gets poisoned (a false result is stored, recalled with confidence, and reinforced), and verification without memory repeats itself (every attempt starts from zero and makes the same mistakes). The loop between them is the point, and its order is fixed: perceive, test, correct, and only then consolidate; abstain when there is no verified evidence. Nothing enters memory as reusable experience before the real toolchain has checked it.

## Domain orientation

- **Current focus: shell, git, and C.** The core is built and proven on systems work first: safe shell execution, git operations, and C/C++ builds, diagnostics, and repair. The core comes before any new front.
- **Language-agnostic contracts.** Language knowledge stays behind contracts (toolchain, semantics, diagnostics, corpus, evaluations), so a later language is added by supplying those, never by rewriting the engine. No other language is in scope until the systems core meets its gates.
- **C/C++ is the substrate and the first domain.** The engine and its extensions stay on the C pipeline; its portability and memory-safety gates remain mandatory.

## Verified foundation

The repository already contains substantial symbolic and agent infrastructure. The next phases build on it rather than replacing it.

### Native engine and measured C components

- A hash-indexed relation store (expected-average, not worst-case constant time) with a 32-byte relation representation on the tested ABIs, snapshot persistence, provenance, rule reasoning, planning, diagnosis, and graph traversal.
- CCG realization, VSA operations, commonsense ingestion, and persona projection with dedicated unit tests and benchmarks documented in `README.md` and the completed milestones in the previous roadmap.
- Stress benchmarks for relations and embeddings exist, but this roadmap does not cite their timing or memory figures as evidence. The `bench_1m` summary counter is internally inconsistent, and timings depend on compiler, build type, OS, and hardware; see `README.md` section 4.5. Figures become evidence only after the harness is corrected and CI records the environment on the final SHA.

### Coding-agent operators

The codebase already has operators for:

- repository indexing and code-graph construction;
- blast-radius traversal and impact analysis;
- unified-diff parsing, preflight checks, atomic application, and rollback;
- STRIPS planning and replanning;
- compiler/linter diagnostic parsing and abductive suggestions;
- subprocess execution with captured stdout/stderr and timeouts;
- an `AgentRunner` pipeline that connects inspect, plan, patch, verify, retry, and report stages;
- a representative SWE-bench harness that verifies candidate patches rather than claiming unguided issue resolution.

These operators are useful primitives. Their presence does not yet prove reliable autonomous repository administration.

### Delivery workflow

`.github/workflows/apply-patch.yml` currently provides a guarded patch path: exact-base validation, patch validation, application, Ubuntu build and CTest, commit, and push. This gives changes a reproducible transaction boundary.

### Persistent episodic fact memory

Commit `1c9dd550241b8cd0aaebac118d8bdaa04bde6615` lands the atomic fact-memory substrate: crash-safe atomic replacement of the on-disk store, selective forgetting of individual facts, and honest failure reporting when persistence fails. A failed save never claims success and leaves the previous store byte-identical. Evidence: the guarded apply workflow plus the full Linux, MSVC, and MSVC-ASan matrix on the final SHA, and a live end-to-end proof covering learn, query, server restart, query, selective forget, immediate absence, second restart, continued absence, and read-only-filesystem failure honesty. This substrate is what Phase 7 stores its episodes through.

The shell hardening at commit `ba90e1d4ba648e844c3e975ad8e870df38877c89` adds three important invariants:

- an invalid working directory fails without running the command;
- concurrent large stdout and stderr are drained without a false timeout, with explicit truncation;
- timeout cleanup terminates the spawned process tree rather than leaving children alive.

Local evidence for that patch was 70/70 `test_agent_shell` assertions, 82/82 in the exercised suite, and repeated stress runs passing. The guarded apply workflow also passed its Ubuntu build and CTest gate.

### Closed: final-SHA CI gap

A push made by `github-actions[bot]` from the apply workflow does not trigger workflows configured only for `on: push`, so the Linux/MSVC/ASan matrix once did not run for the commit the apply workflow created. This is closed: `ci.yml` is now a reusable workflow (`workflow_call`), and the apply workflow calls it for the exact SHA it pushed, so every applied commit gets a Linux, MSVC, and MSVC ASan run on that SHA. A commit counts as verified only when that run is green.

## Dependency chain

```text
safe shell
    |
    v
structured filesystem
    |
    v
safe Git
    |
    v
build/test intelligence
    |
    v
Clang AST + compile_commands.json
    |
    v
persistent workflows
    |
    v
verified episodic memory
    |
    v
reflexion learning loop
```

Security capabilities, provenance, metrics, and cross-platform CI cut across every phase. Later phases may be prototyped early, but they do not exit before their dependencies.

### Sequencing decisions (September 2026)

These decisions change the order of work, not the dependency chain above. No pulled-forward item closes its phase early; each still has to meet its phase's exit criteria when that phase is reached.

- **Minimal Reflexion loop before full Phase 7 (landed).** A small slice of Phase 7 is in place: attempt, structured reflection ("tried X, failed because Y") with source and timestamp, read back on a retry of the same task. Only reflections whose feedback came from the real toolchain are stored or read back. It is measured on analogue tasks against a reflection-off baseline, under the same gates as any other change (bank runs with zero wrong edits, full test suite, CI green on the final SHA). The rest of Phase 7 (full episodic record, bounded cross-task retrieval, restart and forgetting exit criteria on the external benchmark) stays where it is.
- **AST design spike before Phase 4 proper (writeup complete).** The [design spike](docs/ast-design-spike.md) compares: tree-sitter versus libclang, the cost of each in the CMake build on Linux and MSVC, and which consumer comes first (candidate generation for the repair operators, or impact analysis). Its pilot and written recommendation change no core code. Phase 4 deliverables and exit criteria are unchanged, and a syntax-only parser is not treated as compiler-grounded semantics (see "Explicitly out of scope").

## Phase 0: Safe shell execution

**Goal:** make subprocess execution a bounded, observable primitive rather than an implicit source of repository state.

**Mechanisms:** test. Bounded, observable execution is what makes trying a candidate against reality safe.

### Deliverables

- Complete POSIX and Windows process-tree termination semantics.
- Bounded stdout/stderr capture with separate byte counts, explicit truncation flags, exit status, signal/exception status, elapsed time, timeout status, and spawn error.
- Validated `cwd`, environment allow/deny rules, command/argument separation, and no ambient shell interpretation unless explicitly requested.
- Stable error categories that callers can use without parsing prose.
- Exact-SHA CI entry point covering Ubuntu, MSVC, and MSVC ASan, reusable by `apply-patch` after commit creation.

### Exit criteria

- Invalid `cwd` and spawn failures produce zero command side effects.
- A fixture writing at least 100,000 bytes simultaneously to both streams completes without deadlock or false timeout on Linux and Windows.
- Timeout fixtures leave no surviving child or grandchild process on either platform.
- Truncation is distinguishable from timeout and ordinary command failure.
- `test_agent_shell`, the full CTest suite, MSVC build/test, and MSVC ASan pass on the same final SHA.
- Repeating the concurrency and timeout stress set 100 times produces no hang, orphan, or nondeterministic status.

## Phase 1: Structured filesystem

**Goal:** remove routine file mutation from ad hoc shell strings and enforce workspace boundaries as data.

**Mechanisms:** correct. Reversible, journaled mutation replaces destructive, ad hoc edits.

### Deliverables

- Typed operations for stat, list, read, create, replace, move, copy, and remove.
- A workspace-root capability with canonical path checks, symlink policy, and explicit handling for paths that do not yet exist.
- Atomic single-file replacement and a journaled multi-file transaction with rollback.
- Windows create, batch create and batch replace use NTFS-only handle-relative journals for process-termination recovery. Directory FlushFileBuffers, where accepted, is a best-effort ordering hint; neither Windows nor POSIX promises power-loss persistence here. Power-loss durability and atomic replace claims remain deferred until an NTFS-specific primitive and real fault-injection evidence support them.
- Encoding, newline, permission, file-kind, size, and binary/text metadata.
- Dry-run manifests that state every intended read, write, rename, and delete.

### Exit criteria

- Traversal through `..`, absolute-path escape, symlink escape, and rename races fail closed in adversarial fixtures.
- Interrupted multi-file create and replace batches either commit fully or restore the original byte-for-byte state. Mixed-operation batches (create, replace, move, remove in one transaction) are not part of this criterion; see the 2026-10-02 decision in [docs/core-m0-m1-exit-audit.md](docs/core-m0-m1-exit-audit.md).
- Existing unified-diff behavior is reimplemented on the structured filesystem API with no regression in patch fixtures.
- Linux and Windows fixtures cover separator, case, permission, long-path, newline, and locked-file behavior.
- Fuzzing malformed paths and operation manifests yields no out-of-workspace write.

## Phase 2: Safe Git

**Goal:** make repository inspection and change delivery explicit, race-aware, and reversible.

**Mechanisms:** correct. Changes become atomic, reviewable, and attributable commits guarded by race-aware preconditions.

### Deliverables

- Structured status, diff, log, branch, worktree, index, and object inspection.
- Preconditions for expected HEAD, clean/allowed-dirty paths, branch, remote, and changed-file allowlists.
- Atomic commit preparation with author/message policy and exact staged-content reporting.
- Safe fetch, branch creation, and push with non-fast-forward and remote-advance detection.
- Rebase/cherry-pick support only with explicit conflict states and abort paths. Force push is disabled by default.

### Exit criteria

- Dirty-tree, stale-HEAD, detached-HEAD, conflict, remote-advance, and submodule fixtures fail without losing user changes.
- A produced commit's tree exactly matches the reviewed mutation manifest.
- Retrying after an interrupted operation is idempotent or reports the already-completed result.
- No test path invokes destructive reset, clean, checkout, or force push implicitly.
- Apply-patch uses these Git contracts instead of open-coded repository assumptions.

## Phase 3: Build and test intelligence

**Goal:** understand what to build and test, then preserve complete evidence for the decision and result.

**Mechanisms:** perceive and test. Build and test structure becomes data, and every result becomes evidence with provenance.

### Deliverables

- CMake File API and CTest metadata ingestion for targets, sources, generated files, configurations, tests, labels, and dependencies.
- Build/test graph linked to changed files and code symbols.
- Structured compiler, linker, sanitizer, and test-result records with artifact paths and provenance.
- Conservative affected-test selection with an automatic full-suite fallback when coverage is unknown.
- Reproducible benchmark runner that records toolchain, OS, CPU, configuration, corpus/fixture version, repetitions, and distribution statistics.

### Exit criteria

- On a labeled fixture repository, affected-test selection has 100% recall. Precision is reported, not optimized at the cost of recall.
- Unknown generators, dynamic dependencies, or missing metadata select the full gate rather than guessing.
- Clean builds succeed with GCC, Clang, and MSVC in CI; sanitizer jobs report zero findings on the gated suite.
- Test reports distinguish not-run, skipped, passed, failed, timed out, and crashed.
- Performance claims in `README.md` are produced by versioned benchmark commands, with median and tail latency plus memory high-water mark.

A slice-3 opt-in, read-only AST inspection adapter is described in [docs/ast-inspect.md](docs/ast-inspect.md). It is not a Phase 4 exit-criterion claim and authorizes no edits. Native Windows execution remains unmeasured.

## Phase 4: Clang AST and `compile_commands.json`

**Goal:** replace lightweight C scanning with compiler-grounded semantics for C and C++ while keeping the graph auditable.

**Mechanisms:** perceive. Compiler-grounded semantics replace guessing about code structure.

### Deliverables

- Import of compilation databases with per-translation-unit flags, working directory, language mode, defines, include paths, and generated-source handling.
- Stable source identities for declarations, definitions, references, scopes, types, overloads, macros, includes, calls, and source ranges.
- Explicit representation of conditional-compilation variants and unresolved compiler states.
- Incremental invalidation keyed by file content, compile command, included-header closure, and parser version.
- Graph queries for callers/callees, references, type compatibility, include impact, and candidate edit ranges.

### Exit criteria

- A versioned C/C++ corpus covers nested scopes, shadowing, function pointers, macros, conditional compilation, headers, and overloads.
- Declaration/reference and call-edge precision and recall are measured against compiler-derived golden data; release requires no false-negative edge in the safety corpus.
- The engine refuses semantic edits for translation units that do not parse under their recorded compile command.
- No stale edge survives a source, flag, macro, or included-header change.
- Existing code-graph and SWE-bench verification fixtures either migrate to AST facts or remain clearly labeled as lexical fallback.

## Phase 5: Persistent workflows

**Goal:** execute long engineering tasks as resumable state machines with durable evidence and compensation.

**Mechanisms:** correct. Interruption and recovery leave no partial or duplicated effects.

### Deliverables

- Versioned workflow schema for steps, inputs, outputs, preconditions, capabilities, retries, timeouts, approvals, and compensation.
- Append-only event log plus checkpointed state, with stable operation IDs and artifact hashes.
- Recovery after process or machine interruption without duplicating commits, pushes, comments, or test dispatches.
- Final-SHA orchestration: pre-commit local gate, commit, reusable CI matrix, observed result, and explicit closeout.
- Human decision nodes for irreversible, ambiguous, or policy-sensitive actions.

### Exit criteria

- Fault injection after every state transition resumes to the same terminal result as an uninterrupted run.
- Replaying a completed workflow produces no duplicate external side effect.
- A failed post-commit CI gate leaves an exact failure record and a safe next action; it never reports completion.
- Every mutation is attributable to an input, capability, workflow step, and verification result.
- Workflow schema migrations preserve the ability to inspect prior runs.

## Phase 6: Verified episodic memory

**Goal:** reuse prior engineering experience without converting past guesses into current facts.

**Mechanisms:** consolidate and abstain. Verified experience becomes reusable knowledge; unverified experience never authorizes an action.

### Deliverables

- Episodes containing repository/version identity, problem signature, observed diagnostics, chosen operators, patch hash, tests, CI result, and final outcome.
- Separate stores for immutable observations, derived hypotheses, and validated outcomes.
- Retrieval based on structural context such as symbol/type/build/test similarity, with explicit confidence and incompatibility checks.
- Invalidation when toolchain, compile command, dependency, file, symbol, or test evidence changes.
- Negative episodes for failed or unsafe approaches.

**Vector search, if added, only proposes.** Today memory is triples plus an episodic TSV log, with small embeddings used only to rank or break ties; there is no vector database. If vector or similarity search is added in this phase, it acts only as a candidate generator: it may suggest episodes to consider, but every suggestion still passes the structural compatibility and evidence checks above, and only verified outcomes are written back. Similarity alone never authorizes an action or satisfies a gate.

### Exit criteria

- Only episodes with completed verification gates can influence automatic action selection.
- Retrieved advice always exposes its source episode and the differences from the current context.
- An incompatible or stale episode cannot silently authorize or satisfy a current gate.
- On a held-out repair corpus, memory improves median steps or time without reducing success rate, safety-gate recall, or determinism.
- Deleting or rebuilding memory changes efficiency only, never the truth of repository facts or verification results.

## Phase 7: Verified Reflexion-style learning loop

**Goal:** implement the complete learning loop from *Reflexion: Language Agents with Verbal Reinforcement Learning* (Noah Shinn, Federico Cassano, Edward Berman, Ashwin Gopinath, Karthik Narasimhan, Shunyu Yao, 2023; arXiv:2303.11366; https://arxiv.org/abs/2303.11366) with deterministic components: the agent attempts a task, the environment returns feedback, the agent writes a verbal reflection, stores it in episodic memory, retrieves it on the next attempt, and measurably improves. No weight updates anywhere, and nothing consolidates without external evidence.

**Mechanisms:** all five, in one loop. Perceive the task and workspace, test each attempt against the real toolchain, correct from the feedback, consolidate only verified episodes, abstain when no verified experience applies.

**Relation to the paper.** Reflexion reinforces a language agent through linguistic feedback kept in an episodic memory buffer instead of gradient updates. Symbols keeps that loop shape and replaces the probabilistic actor with the deterministic planner and operators; the evaluator is always the real toolchain: compiler, tests, sanitizers, and tools. This adaptation is a deliberate difference from the paper, not a claim of equivalence.

### Deliverables

- Bounded attempt loop: each task runs a capped number of attempts, and every attempt yields a candidate patch plus captured external feedback (compiler diagnostics, test results, sanitizer output, tool exit codes). Self-evaluation never substitutes for a real toolchain signal.
- Deterministic verbal reflection: each attempt is summarized from its diagnostics into structured text - what was tried, what the environment reported, why it failed, what to change on the next attempt.
- Persistent episodic record: every attempt stores task, attempt number, feedback, reflection, patch hash, outcome, and provenance (repository SHA, toolchain, fixture version) through the atomic fact-memory substrate, keeping its crash-safety and persistence-failure honesty guarantees.
- Bounded retrieval: the next attempt receives the most relevant prior episodes for the same task, plus explicit warnings when the context differs, with a hard cap on how much history may enter a decision.
- Negative episodes: failed approaches stay recorded as failures so they are not retried blindly; they inform avoidance, never authorization.
- Correction and forgetting: correcting or forgetting an episode stops its influence immediately and across restarts, reusing the selective-forgetting machinery.
- Abstention: when no episode passes its evidence gate, the loop reports that it has no verified experience instead of guessing.

### Exit criteria

- End-to-end run on the external C benchmark: attempt, compiler/test/ASan feedback, reflection, persisted episode, retrieval on the next attempt, verified live against the server and across a process restart.
- Measured improvement against a no-memory baseline: on a held-out benchmark split, the memory-enabled runner beats the same runner with memory disabled on success rate or median attempts, with no regression in safety-gate recall or determinism. Results are published with fixture versions and the final SHA.
- No consolidation without external evidence: an episode may influence a later attempt only if its feedback came from a real compiler, test, sanitizer, or tool run; internally generated confidence never promotes an episode.
- Forgetting restores the baseline: after a selective forget, behavior for that task matches the no-memory runner, immediately and after a restart.
- Honest abstention: with an empty or invalidated store, the runner states that it has no verified experience rather than fabricating recall.
- No weight updates: the implementation stores and retrieves episodes only; it changes no model parameters.

## Vision: the Oracle (knowledge beyond code, not scheduled)

For code, the reference that learning needs is free: the toolchain compiles or it does not, the program prints the stated output or it does not. For general questions there is no stdout to read, so something else has to say whether an answer is right. That component is the Oracle. It is a direction recorded here so it is not lost; it is not scheduled, it does not change the current focus on shell, git, and C, and it builds on principle 8 (Memory + verification).

**Principles.**

- **The Oracle asks the questions that help find the answers.** It does not hand down verdicts. Asked "is X true?", it answers "what evidence is there about X, who states it, and how much does it weigh?", and names what would settle the question.
- **Above all, know thyself** (*temet nosce*). The system knows what it knows, knows what it does not know, and does not lie to itself about the difference. Its own analogue results are a mirror it wrote itself; held-out and blind evaluations are where that self-image is checked.
- **The tribunal is always present.** Every claim is judged, not just stored: it faces evidence (what was run or observed), witnesses (sources, each with its authority), and precedent (verified memory), and it gets a verdict that states how sure the system is and why. No claim becomes an answer or a memory without facing the tribunal. For code the tribunal already sits with one judge, the real toolchain; the Oracle extends the same court to knowledge where no compiler can rule.

**Design direction (discussed, not implemented).**

- Every claim carries provenance: its sources and when they were observed.
- Truth levels, highest first: *executed* (confirmed by a real toolchain run or test), *corroborated* (two or more independent sources agree), *asserted* (one source states it; used only with the source visible), *contradicted* (sources disagree; the conflict is shown, no winner is picked), *unknown* (honest abstention).
- Source authority is explicit: the user's own correction outranks a curated corpus, which outranks ingested text.
- Only *executed* and *corroborated* claims are consolidated as reusable facts; lower levels are kept with their label. Before a reflection or episode is consolidated, it passes through the same check.
- Levels change with evidence: a claim that is later corroborated moves up; one that is contradicted moves down and invalidates what depended on it (the Phase 6 invalidation machinery).
- "I don't know" stays a correct answer, reported with the evidence and gaps that exist so far.

**Abstain and ask (typed CLI pilot).** Slice 1 shipped: only the exact `stdout-goal-missing` task with opt-in `--ask-missing-goal` can ask in a single input-free C workspace. Natural-language tasks never infer a question. The proposed slice 2 is a separate noninteractive key-bound answer-to-edit continuation (not shipped): a user-asserted normalized stdout goal enters a `C_CONTRACT` data field and can produce an edit only after unique throwaway compile/run and final verification. Its claim is reachability, not semantic correctness of the assertion; no durable episode yet. [`docs/abstain-and-ask.md`](docs/abstain-and-ask.md) records exact limits and the earlier rejected free-form question design.

**Memory hygiene (named piece, owner to be assigned in Phases 6-7).** Keeping memory clean has four layers:

1. *The door:* the tribunal. The cheapest cleanup is the one never needed: only verified experience is consolidated, and reflections are read back only when their source is the toolchain (already in place).
2. *The user:* `/forget` (`/olvida`) removes any fact immediately and across restarts; the user is the highest authority (already in place).
3. *Invalidation on contradiction:* when a claim is contradicted it moves down a level and invalidates what depended on it (Phase 6 invalidation, designed, not implemented).
4. *The periodic sweep* (informally, the tribunal's janitor): a scheduled pass that ages *asserted* claims that were never corroborated, removes reflections whose task or workspace no longer exists, and lists accumulated contradictions awaiting resolution. It reuses the Phase 6 invalidation and Phase 7 forgetting machinery, reports what it changed, and never deletes a verified fact on its own; removing or demoting one needs new evidence or the user. Not implemented.

## Measurable targets per phase

Phases are measured with the same metrics the README publishes (Section 2.1, regenerated by `tools/metrics.py`). A phase is never closed on an estimate: its target has to appear in a real run on the final commit. These targets are an initial proposal and will be adjusted with data.

| Metric | Today (see README) | Phase 0-1 | Phase 2-3 | Phase 4-5 | Phase 6-7 |
|---|---|---|---|---|---|
| Agent: C repair, evaluation (resolved) | 12/25 | ≥ 12/25 | ≥ 15/25 | ≥ 18/25 | ≥ 20/25 |
| Harmful edits (any suite) | 0 | 0 | 0 | 0 | 0 |
| Varied engineering task bank (resolved, hidden split) | blind 2/24 (Mimo, counts only; public bank 27/56, see README) | bank exists (≥ 50 tasks, ≥ 8 categories) | ≥ 20% | ≥ 40% | ≥ 60% |
| Grounded QA: precision | see README | ≥ 85% | ≥ 90% | ≥ 95% | ≥ 95% |
| Grounded QA: recall | see README | ≥ 80% | ≥ 85% | ≥ 90% | ≥ 90% |
| Invented or wrong answers on the QA set | see README | ≤ 2 | ≤ 1 | 0 | 0 |
| Memory: probes when repeating already-learned commands | 0 | 0 | 0 | 0 | 0 |
| Memory: repeated tasks solved faster or with fewer attempts | not measured | not measured | measured | ≥ 50% | ≥ 80% |
| CI on the final SHA (Linux, MSVC, MSVC ASan) | fully green on recent `master` SHAs (sanitizer skips only throughput thresholds) | green except performance thresholds | fully green | fully green | fully green |
| Declared hand-written rules | see README | does not grow undeclared | decreasing | decreasing | decreasing |

## Cross-cutting release gates

Every phase must maintain these gates:

| Gate | Required evidence |
| :--- | :--- |
| Correctness | Versioned unit, integration, adversarial, and regression fixtures |
| Portability | Linux GCC/Clang and Windows MSVC on the same final SHA |
| Memory safety | ASan where supported, plus platform-appropriate runtime checks |
| Determinism | Same inputs and state produce the same plan, manifest, patch, and status |
| Fail-closed behavior | Unsupported or ambiguous state causes no partial mutation |
| Performance | Reproducible benchmark with environment, repetitions, median, tail, and memory |
| Provenance | Inputs, derived facts, artifacts, and verification outcomes remain traceable |
| Recovery | Interruption and retry do not duplicate effects or lose user work |

Benchmark thresholds belong beside versioned fixtures and should be tightened only from reproducible measurements. A single development-machine number is a baseline, not a portable service-level objective.

## Risks and controls

- **Platform semantic drift:** POSIX and Windows differ in process, path, locking, and signal behavior. Control: behavioral fixtures on both platforms, not preprocessor equivalence alone.
- **Time-of-check/time-of-use races:** filesystem, Git, and remote state may change after inspection. Control: content hashes, expected HEAD, atomic primitives, and preconditions checked immediately before mutation.
- **Graph staleness:** cached code or build edges can outlive their inputs. Control: dependency-based invalidation and refusal to use incomplete semantic state for mutation.
- **Benchmark overfitting:** small internal fixtures can reward narrow operators. Control: held-out corpora, adversarial variants, published fixture versions, and separate verification from generation.
- **False autonomy claims:** verifying a supplied candidate patch is not the same as resolving an issue from a natural-language report. Control: report proposal, selection, mutation, and verification success separately.
- **Capability creep:** a general shell or Git token can bypass narrower contracts. Control: least-privilege capabilities and a complete mutation manifest.
- **Memory poisoning:** an unverified episode can reinforce an earlier mistake. Control: verification-gated promotion, provenance, negative results, and invalidation.
- **Server handles one request at a time (known infrastructure gap):** `symbols-server` accepts and serves connections serially, so one slow request blocks every other client. Control until fixed: clients run sequentially and use timeouts; closing the gap needs a bounded worker model with per-request isolation, measured under the concurrency and timeout stress set before it is claimed.

## Explicitly out of scope for this roadmap

- A probabilistic LLM inside the trusted execution, permission, repository-state, or verification core.
- Premature general administration of arbitrary machines, networks, cloud accounts, or package ecosystems.
- Autonomous force push, history rewriting, secret handling, privilege escalation, or destructive cleanup by default.
- Treating a lexical scanner as compiler-equivalent C/C++ semantics.
- Claiming full SWE-bench issue resolution from candidate-patch verification results.
- Expanding to languages beyond C/C++ before their semantics, build/test evidence, and workflow recovery meet their gates.
- Maximizing conversational breadth at the expense of engineering reliability.
- Building a tool-protocol adapter (for example MCP) into the core. Such an adapter is a possible future distribution layer that would expose existing, already-gated operators to other agents; it adds no capability and no trust, and it is not scheduled in any phase.

## Next gate

Final-SHA CI is in place (see "Closed: final-SHA CI gap"). The current work, in order (see "Sequencing decisions"):

1. **Minimal Reflexion loop - landed as an early slice, not a Phase 7 exit.** `TaskOpsSolve` now caps attempts, reflects on real toolchain feedback and excludes a refuted edit; optional same-task reflection persistence and retrieval remain separate from the full Phase 7 episode and forgetting gates. See the September 25 [COORDINATION.md](COORDINATION.md) entry (commit `06cc3ba`: analogue reflection-off 0/3 versus on 2/3, bank CLI/OpenCode 49/59 each with zero wrong edits, CTest 113/113), and Mimo's subsequent counts-only blind rerun on `27a3b21` (2/24 passed, zero wrong edits, no gain in solved tasks). Later exact-SHA [CI run](https://github.com/FiveTechSoft/symbols/actions/runs/36226995102) passed Linux, MSVC, Windows Ninja AST and MSVC ASan, though it does not independently remeasure the Reflexion analogue. No full Phase 7 exit claim follows.
2. **AST design spike - writeup complete, Phase 4 open.** [Design-spike comparison and first-consumer recommendation](docs/ast-design-spike.md) records the optional libclang pilot, native Windows Ninja evidence (corrected: tests skipped before `530cc26`), tree-sitter's unmeasured status and the two rejected semantic vetoes. The first proposed consumer is read-only impact reporting adjacent to `CodeGraphComputeBlastRadius`; no edit path or phase status changes.

The phase gates above are unchanged: no phase exits before its dependencies and exit criteria are met, and new work does not weaken the shell invariants.

### Update: core-first reset

The QEMU and containment line is parked (see "Where we are"). The proposed next gate, pending Antonio's acceptance of the v1 definition, is the first item under "What is missing for core v1", then the rest in that order. The two items above stay as history and are not reopened by this update.
