# M1 exit gap reading (Phase 1: Structured filesystem)

Reading of the five Phase 1 exit criteria in `ROADMAP.md` against the tests on `master`, per platform,
with the open gaps named. It is a reading of sources and of CI results already recorded in
[core-m0-m1-exit-audit.md](core-m0-m1-exit-audit.md); nothing here is a new measurement. M1 is not declared.
The declaration is Antonio's. Last measured CI state recorded for the M1 cells: linux 171/171,
msvc and asan-msvc 186/186 (run 37219123908, `b9e0c8f`; 185 at m146, 184 at m142, 181 at m134), one runner per platform (`windows-latest`, NTFS,
one account; one Linux runner), cooperating writers, no power-loss claim. A ctest Passed line hides the
test output, so statuses come from ctest pass lines and the sources.

Status words: **shown** (named test, named run), **partial**, **not shown**.

| # | Criterion | Linux | Windows |
|---|---|---|---|
| C1 | `..`, absolute, symlink escape, rename races fail closed | **Shown** for the listed cells: `test_fs_adversarial`, `test_fs_read`, `test_fs_race_parent` (parent flipped to a symlink under writers, mutant `test_fs_race_parent_mc`). | **Shown** for the listed cells: `test_fs_win_symlink_leaf` (m101), `test_fs_win_fileparent`, fuzz fixed pass. **Not shown:** 8.3 aliases (`test_fs_win_aliases_short` is Skipped), per-directory case flag, other accounts, a rename race against a swapped parent (the POSIX `test_fs_race_parent` has no Windows counterpart). |
| C2 | Interrupted multi-file create and replace commit fully or restore the original bytes | **Shown** for the listed cells: `test_fs_batch`, `test_fs_batch_replace`, `test_fs_replace`. | **Partial.** Crash-point matrices and mutants in `test_fs_win_batch_create`, `test_fs_win_batch_replace`, `test_fs_win_replace` (orphan sweep since `530adeb`, run 37186804952). Open: single-file calls do not sweep; an unshared `.fsrp-` pin is kept by design; no crash during the sweep was injected; a foreign inode at a target is refused, not restored (same on POSIX). |
| C3 | Unified-diff behaviour on the structured API, no regression | **Shown** for the listed cells: `test_agent_patch`, `test_agent_patch_eol` (cells A to S, lone-CR mutant killed), `test_agent_patch_fs` T1 to T9. | **Shown for the listed cells, partial against POSIX parity.** `test_agent_patch_fs_win` T1 to T8, `test_agent_patch_ntfs_win` N1 to N6, `test_agent_patch_eol` cells. Crash phases 0 to 6 through `AgentPatch` shown since m142; T9 analogue shown for cells L1 to L5 since m146 (`test_agent_patch_lock_win`); the hard-linked lock is an accepted documented limit, and a Windows mutant of the diff-apply drift guard (mutant 12, m149) is killed by exactly four cells; lock contention with a waiting process, power loss and other runners are the gaps below. |
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
2. **T9 analogue (m144 to m146).** The POSIX case plants a `.fstxn.lock` with mode 0644 and expects a fail-closed
   apply. Windows has no mode bits, so a port is not a translation; `tests/test_agent_patch_lock_win.c` asks the
   nearest question: what does `PatchApplyAtomic` do when `.fstxn.lock` is not a plain empty file. Final run 37215099208
   (`297c41d` to `85bc2ff`), msvc and asan 185 of 185, marker "agentpatch lock cells ran: 6" (hidden by ctest on a
   pass, so the pass line is the evidence). Two earlier reds were fixture bugs in the test, not source findings: m144
   (run 37213626113) counted the symlink target file as a leftover name, m145 (run 37214252790) printed 5 for a
   marker that expects 6. Measured on msvc (values read from the m145 log; the m146 log was not read, it passed):
   - L1 lock is a directory: apply returned 0, `a.c` stayed OLD, io_diag "denied: file changed, is a link, or an Fs
     transaction is pending", no leftover names, the directory untouched, the same plan applied after removal.
   - L2 lock is a file symlink (the runner could create one, flag `0x2`): same refusal, the link was not followed.
   - L3 lock is a non-empty file: same refusal, the file untouched.
   - L4 lock held open by another handle with no sharing: same refusal, no hang, applies once the holder closes.
   - L5 plain empty lock: applies (control).
   - L6 lock is a hard link to another empty file: **apply returned 1 and `a.c` became NEW.** This is a measured
     behaviour, not an asserted one: the test prints it and checks nothing. `wc_lock` checks the kind (file) and size
     (0) but no link count. **Decision (main agent, under the standing mandate, retractable by Antonio): accepted as a documented limit, no source change.** The lock is an empty file whose only function is mutual exclusion among cooperating writers; a hard link to it shares that lock object and exposes no user bytes; whoever can create hard links inside the workspace is outside the cooperating-writer model anyway. The test still asserts nothing for L6.
   Limits: one runner and one account; a second process waiting on the lock is shown by `test_agent_patch_contend_win` (m159, two
   processes on one runner): W1 a child waits behind a foreign `LockFileEx` and then applies, W2 two children with
   different patches give one applied and one refused. Its mutant target `test_agent_patch_contend_win_mc` SURVIVED on
   the first run (the mutant re-reads the target and `FsReplaceFile` compares again under the lock, and the two
   children woke together); with staggered sleeps (m160) it was killed by exactly the W2 cell, msvc and asan-msvc
   188/188 in run 37227572528 at `ed21782`, one run, not repeated, so not shown stable. Not tried: a waiter killed
   while holding the lock, power loss, other runners; the lock cases are sequential in one process; the POSIX mode check has no Windows
   equivalent, so this is parity of intent, not of mechanism. **Shown for L1 to L5, L6 measured only.**
3. **Mutant (m149).** Mutant 12 (`AGENT_PATCH_APPLY_MUTANT`, test-only, in `replace_target`): the drift guard is
   dropped, the bytes the target holds at replace time are passed as the expected bytes, so a change between the read
   and the replace is overwritten. Target `test_agent_patch_fs_win_mc` is `test_agent_patch_fs_win.c` compiled with the
   define; it exits 0 only if the failing cells are exactly four (T1 "apply refuses", T1 "external bytes kept", T1
   "diagnostic set, not applied", T8 "rollback refused, user edit kept") and prints "mutant killed by 4 cell(s)", matched
   by `PASS_REGULAR_EXPRESSION`. Run 37218295633 (`1e9ac22`): Passed on msvc and asan, 186 of 186; the printed line was
   not read (ctest hides it), the regex match is the evidence. Predicted before the run, held. Limits: this is one
   mutant of one rule (the conditional replace); mutants of the hunk matching, the EOL handling and the path rules are
   covered by the portable `test_agent_patch` and `test_agent_patch_eol` mutants, not by a Windows-only one. Mutants 10
   and 11 above are mutants of the orphan sweep.
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

Update (m151): the flake occurred twice more. Second: first dispatch of m148 (run 37216934858, apply gate): "no outside
touch in 30011 ms (swaps 663, writer calls OK 5 14)". Third: the CI Linux job of the m149 run (37218295633): "no outside
touch in 30004 ms (swaps 621, writer calls OK 9 5)". Tally: three occurrences since m141 (two in the apply gate, one in
a CI Linux job), plus the one recorded in the test header for run 37181356800. Cause, measured locally in my own
sandbox (gcc, not the CI runner) with a status histogram and a listing of the scratch directory: the writers are
wedged. A replace cut off by the directory swap leaves `.fstxn.replace`, `.fstxn.pcommit` and `.fsrp-` files in the
root, recovery refuses to restore over the swapped directory, and from then on about 99.9 percent of the writers'
calls return DENIED, so no call meets a symlink window. Reproduced under artificial load: 5 of 110 mutant runs
INCONCLUSIVE with the old code, 0 of 110 after m150. m150 (`b9e0c8f`, run 37219123908, test file only, mutant build only):
the swapper deletes those leftover files at the top of each cycle; the non-mutant build compiles to identical assembly.
In that run `test_fs_race_parent_mc` Passed in 0.10 s in both the apply gate and the CI Linux job (30.04 s when it
was INCONCLUSIVE). Status: **not shown fixed**; two clean runs since m150, several more are needed. Two attempts that
did not work are recorded for honesty: holding the symlink until a writer heartbeat advanced, and calling
`FsReplaceRecover` in the writers; the first did not remove the wedge, the second was worse locally.
Open measured gap, not fixed: the NON-mutant `test_fs_race_parent` has the same wedge. In 12 local runs, no load, each
writer made about 19000 calls in its 4 s window and only 0 to 21 returned OK. A symlinked `D` is refused by design for
about a third of each cycle. What its pass shows: no outside mismatch ever appeared, and the calls made in the first
milliseconds were real. What it does not show: four seconds of racing writes. See the m153 correction below for what
clearing the leftover files does to those counts.

Correction (m153), an error of mine in m151 and m152. Both state, from local variant builds, that deleting the leftover
files did not raise the writers' OK counts in the non-mutant build (8 to 38 OK of about 39000 calls, 9 to 23 with every
`.fstxn.*` except the lock deleted) and that why was unexplained. Those variants never ran their deletion: the
`unwedge()` call sat inside `#ifdef FS_PARENT_FOLLOW_MUTANT`, so the non-mutant build deleted nothing until the
end-of-run cleanup (an strace shows `.fstxn.replace` and `.fstxn.pcommit` unlinked only at the end). Those numbers and
the "unexplained" sentence are withdrawn; I have not re-checked the earlier variant files for the same defect, so
treat all of those variant numbers as unreliable. Redone with the call active in the non-mutant build, local sandbox
(gcc, not the CI runner), 6 runs each, 4 s: with no deletion each writer made 0 to 4 OK per operation (about 3300
DENIED per operation, the wedge); with the swapper deleting the leftover files each cycle, 26 to 96 OK per operation
per writer (about 250 to 400 per writer in total), about 480 to 1600 DENIED per operation, and all 6 runs ended
ALL PASS with 0 outside mismatches. So deleting the leftover files restores the writers' real calls by roughly 60 times
in the non-mutant build. This supports the m150 mechanism for the non-mutant build locally. It does not show it for the
mutant build, where every local run ended at the first kill (50 to 100 ms), nor on the CI runner, nor under load.
The non-mutant test itself is unchanged and still has the wedge; a change plus a floor on OK calls is the next patch.

Not a race_parent item, found while checking (m153): the CI job `ast-inspect-windows-ninja` is green, but in the runs
read (m152 run 37221273660 and the m150 run 37219123908) `pip install libclang==18.1.1 ninja==1.12.1` fails with
"No matching distribution found for ninja==1.12.1", the version check then raises PackageNotFoundError, the step still
passes, and all 10 tests of `tests/test_ast_inspect.py` and `test_c_source_qa.py` report skipped ("pinned libclang
18.1.1 not installed"): `OK (skipped=10)`. So that job has not exercised the AST tests in those runs, and its green is
not evidence of them. The fix is in `.github/workflows/ci.yml`, which is Antonio's; not touched here.
