# M1 exit gap reading (Phase 1: Structured filesystem)

Reading of the five Phase 1 exit criteria in `ROADMAP.md` against the tests on `master`, per platform,
with the open gaps named. It is a reading of sources and of CI results already recorded in
[core-m0-m1-exit-audit.md](core-m0-m1-exit-audit.md); nothing here is a new measurement. M1 is not declared.
The declaration is Antonio's. Last measured CI state recorded for the M1 cells: linux 171/171,
msvc and asan-msvc 184/184 (run 37211667043, `5d0455f`; 181 at m134), one runner per platform (`windows-latest`, NTFS,
one account; one Linux runner), cooperating writers, no power-loss claim. A ctest Passed line hides the
test output, so statuses come from ctest pass lines and the sources.

Status words: **shown** (named test, named run), **partial**, **not shown**.

| # | Criterion | Linux | Windows |
|---|---|---|---|
| C1 | `..`, absolute, symlink escape, rename races fail closed | **Shown** for the listed cells: `test_fs_adversarial`, `test_fs_read`, `test_fs_race_parent` (parent flipped to a symlink under writers, mutant `test_fs_race_parent_mc`). | **Shown** for the listed cells: `test_fs_win_symlink_leaf` (m101), `test_fs_win_fileparent`, fuzz fixed pass. **Not shown:** 8.3 aliases (`test_fs_win_aliases_short` is Skipped), per-directory case flag, other accounts, a rename race against a swapped parent (the POSIX `test_fs_race_parent` has no Windows counterpart). |
| C2 | Interrupted multi-file create and replace commit fully or restore the original bytes | **Shown** for the listed cells: `test_fs_batch`, `test_fs_batch_replace`, `test_fs_replace`. | **Partial.** Crash-point matrices and mutants in `test_fs_win_batch_create`, `test_fs_win_batch_replace`, `test_fs_win_replace` (orphan sweep since `530adeb`, run 37186804952). Open: single-file calls do not sweep; an unshared `.fsrp-` pin is kept by design; no crash during the sweep was injected; a foreign inode at a target is refused, not restored (same on POSIX). |
| C3 | Unified-diff behaviour on the structured API, no regression | **Shown** for the listed cells: `test_agent_patch`, `test_agent_patch_eol` (cells A to S, lone-CR mutant killed), `test_agent_patch_fs` T1 to T9. | **Shown for the listed cells, partial against POSIX parity.** `test_agent_patch_fs_win` T1 to T8, `test_agent_patch_ntfs_win` N1 to N6, `test_agent_patch_eol` cells. Crash phases 0 to 6 through `AgentPatch` shown since m142; T9 analogue and the diff-apply mutant are the gaps below. |
| C4 | Separator, case, permission, long path, newline, locked file fixtures | **Shown** for the six aspects within limits (`test_fs_c4_posix` and others). | **Shown** for the six aspects on the listed cells (`test_fs_win_case`, long-path, locked-file and EOL tests), not "met". Mutants only for permission. |
| C5 | Fuzzing of malformed paths and manifests: no out-of-workspace write | **Shown** for path operations and manifests with logged seeds: default, 1, 7, 12648430 at 400 iterations. | **Shown for the same operations with fewer iterations on the default seed** (see correction). Seeds 1, 7, 12648430 at 400. |

## C3 on Windows: gaps against POSIX parity

1. **Single-replace crash matrix through `AgentPatch`: shown since m137 to m142 (one runner).**
   `tests/test_agent_patch_crash_win.c` kills the writer at every `FsReplaceFile` phase 0 to 6
   (`FS_WIN_REPLACE_CRASH`) through `PatchApplyAtomic`, runs `PatchRecover` twice, and asserts bytes before and after
   recovery, no leftover names, and that the same plan applies again when the file is OLD. First run (m137, run
   37207473987, msvc and asan): red on 6 cells, phases 0 to 2: `.fsrp-` names left (1, 2 and 3) and the re-apply
   DENIED because the old pin made the target a two-link file (m138, run 37208117758: links 2, rc 0, io_diag
   "denied: file changed, is a link, or an Fs transaction is pending"). Fixes: m139 (run 37208824012) runs the
   exact-shape orphan sweep in the single-file recovery path when no journal exists; phases 0 and 2 became clean.
   Phase 1 kept one leftover; m140 (run 37209710843) showed it was links 1, size 11, the NEW replacement bytes (the
   unpublished stage). m141 (run 37210756786, second dispatch; the first was refused by the apply gate on a
   `test_fs_race_parent_mc` INCONCLUSIVE, see below) gives the single-file stage its own prefix `.fsrs-`, which the
   sweep deletes whatever its link count; the `.fsrp-` rule (shared deleted, unshared kept) is unchanged. m142
   (run 37211667043): all of `test_agent_patch_crash_win` (marker "agentpatch crash phases ran: 7"), mutant 10 (sweep
   skipped, must fail exactly the 6 cells) and mutant 11 (unshared `.fsrs-` kept, must fail exactly the phase 1
   leftover cell) Passed on msvc and asan, 184 of 184; linux 171 of 171. A mutant test exits 0 only on the exact
   expected failing set, so Passed means it was killed by those cells; the printed "killed by N cell(s)" line is
   hidden by ctest and was not read. Limits: Windows only, `windows-latest`, one account, process kill (not power
   loss), cooperating writers; an in-flight journal written by a build that named the stage `.fsrp-` is accepted
   by the validator but that case is not tested.
2. **T9.** The POSIX case plants a `.fstxn.lock` with mode 0644 and expects a fail-closed apply. Windows has no mode
   bits, so a port is not a translation. The nearest Windows question is what `AgentPatch` does when `.fstxn.lock`
   is not a plain file (a directory, a symlink, a hard-linked file, or held open by another process). That is not
   tested through `AgentPatch`. **Not shown.**
3. **Mutant.** A Windows mutant of the `AgentPatch` apply itself is NOT DONE. Mutants 10 and 11 above are mutants of
   the orphan sweep, killed through the `AgentPatch` crash matrix, not of the diff-apply logic.
4. **Printed values.** The values printed by T6 in `test_agent_patch_fs_win` were not read from a log. The values of
   `test_agent_patch_crash_win` were read from the red runs and the m138 and m140 diagnostics (msvc log), not from the
   final green run (ctest hides them).

## C5: scope gaps (both platforms unless stated)

- **Correction to the second-version table.** On Windows the default seed runs 150 iterations
  (`iters=150` in the Windows `main` of `tests/test_fs_fuzz_ops.c`), not 400. Only seeds 1, 7 and 12648430
  run 400 there, through `FS_FUZZ_ITERS=400` in the CMake registration. The POSIX default is 400. The
  earlier wording "default, 1, 7, 12648430 at 400 iterations" is right for POSIX and too strong for the Windows default seed.
- **Journal byte fuzzing: not shown.** No test corrupts or truncates a `.fstxn.*` journal, marker or commit file at
  random and checks that recovery fails closed. Recovery is tested at fixed crash points and with a foreign inode.
- **Content fuzzing: not shown.** The generated inputs are paths and manifests (counts, duplicate targets,
  traversal, kinds). File contents, EOL mixes and patch text are not fuzzed here.
- **Seeds:** four logged seeds. The criterion text does not say how many are enough; that is Antonio's call.
- **Windows symlink leaf** is a fixed pass, not in the random pool.
- **Windows `AgentPatch`-level fuzz:** not shown. The fuzz drives `Fs*` operations, not `PatchApplyAtomic`.

## Not shown, whole reading

Power-loss durability (deferred); other runners, filesystems, accounts, inherited ACLs; 8.3 aliases;
non-cooperating writers; recovery of an intent left by the old file-parent bug (NOT DONE); one runner per platform.

## Not decided here

Whether one runner and cooperating writers are enough scope, how much fuzzing satisfies C5, whether the foreign-inode
refusal satisfies C2, and the M1 declaration itself, are Antonio's decisions.

## Gate note: `test_fs_race_parent_mc`

The apply workflow runs the Linux ctest before pushing. On the first dispatch of m141 (run 37210249769) it refused the
patch: `test_fs_race_parent_mc` printed "FS_PARENT_FOLLOW_MUTANT INCONCLUSIVE, not a surviving mutant: no outside touch
in 30005 ms (swaps 663, writer calls OK 15 10, swapper exit 0)" and the test exited non-zero. The identical bytes passed
on the rerun. m141 touches no file of that test. Treated as a timing result of the C1 mutant on that runner; not shown
fixed, tally: one occurrence in the apply gate.
