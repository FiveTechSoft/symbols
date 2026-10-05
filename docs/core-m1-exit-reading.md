# M1 exit gap reading (Phase 1: Structured filesystem)

Reading of the five Phase 1 exit criteria in `ROADMAP.md` against the tests on `master`, per platform,
with the open gaps named. It is a reading of sources and of CI results already recorded in
[core-m0-m1-exit-audit.md](core-m0-m1-exit-audit.md); nothing here is a new measurement. M1 is not declared.
The declaration is Antonio's. Last measured CI state recorded for the M1 cells: linux 181/181,
msvc and asan-msvc 196/196, `ast-inspect-windows-ninja` 10 tests OK (run 37247707282, `15de1a7`, m181; 186 at `b9e0c8f`, 185 at m146, 184 at m142, 181 at m134), one runner per platform (`windows-latest`, NTFS,
one account; one Linux runner), cooperating writers, no power-loss claim. A ctest Passed line hides the
test output, so statuses come from ctest pass lines and the sources.

Status words: **shown** (named test, named run), **partial**, **not shown**.

| # | Criterion | Linux | Windows |
|---|---|---|---|
| C1 | `..`, absolute, symlink escape, rename races fail closed | **Shown** for the listed cells: `test_fs_adversarial`, `test_fs_read`, `test_fs_race_parent` (parent flipped to a symlink under writers, mutant `test_fs_race_parent_mc`). | **Shown** for the listed cells: `test_fs_win_symlink_leaf` (m101), `test_fs_win_fileparent`, fuzz fixed pass. 8.3 short names: **shown for one cell set** (the short name of the `.fstxn.lock` control file is refused by all 9 operations, `test_fs_win_aliases_short`, see the 8.3 bullet below; the ordinary-file alias is accepted, not refused). Since m204 the 8.3 alias of any reserved-prefix file is refused by a final-name check in `wc_open` (`test_fs_win_lock_alias`; before m204 it was accepted, a defect found in m203). The alias of a reserved-prefix directory as a path component was accepted until m207 (`test_fs_win_reserved_alias`, found in m206). Per-directory case flag: **shown for a flagged subdirectory, a flagged workspace root and two-entry batches in a flagged directory** (names that differ only by case are different entries, the library acts on the exact-case entry, a batch naming two spellings is refused whole; see the section "C1 on Windows: per-directory case flag" below). **Not shown:** other 8.3 cases (a reserved journal file reached through its alias, directories with short names, a volume where short names are off), other accounts, a parent swapped to a junction with the writers and limits of `test_fs_win_race` cell E and `test_fs_win_race_swap` (m209) only, other operations, other interleavings and other swap targets are not covered (see the section "C1 on Windows: parent swapped to a junction" below). |
| C2 | Interrupted multi-file create and replace commit fully or restore the original bytes | **Shown** for the listed cells: `test_fs_batch`, `test_fs_batch_replace`, `test_fs_replace`. | **Partial.** Crash-point matrices and mutants in `test_fs_win_batch_create`, `test_fs_win_batch_replace`, `test_fs_win_replace` (orphan sweep since `530adeb`, run 37186804952). Open: single-file create, copy, move and remove recovery does not sweep pre-intent orphans (same as POSIX, which has no sweep; single-file replace recovery sweeps since m139; see the bullet "Single-file calls and the sweep" below); an unshared `.fsrp-` pin is kept by design; the sweep killed after its first removal (m184 to m186) and, through chained kills, after its 2nd to 6th removal in `FsBatchRecover` mode (m211, run 37318504276; the other two entry points are not chained); a foreign inode at a target is refused, not restored (same on POSIX). |
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
   188/188 in run 37227572528 at `ed21782`, one run, not repeated, so not shown stable. A holder killed from outside while another process waits is shown by `sc_killholder` in
   `tests/test_fs_win_batch_replace.c` (m191, run 37265496715, `8da58de`, msvc and asan 196 of 196, the test Passed in
   5.85 s and 10.41 s): the holder is a batch replace parked by `FS_WIN_BATCH_HOLD=k` (journal written, lock held, k = 0, 1, 2),
   the waiter is a second process in `FsBatchRecover`; the waiter was still running 1500 ms after its start, the holder was
   killed with `TerminateProcess`, the waiter then finished within 20 s with status OK, the tree was back to the old bytes
   with no journal and no orphan name, and the batch then worked. Mutant 13 (test only, `wc_lock` takes no lock) was killed.
   Limits of that cell: the 1500 ms check alone cannot tell a waiter blocked in `LockFileEx` from one that started slowly
   (the mutant kill is what shows it bites); only an `FsBatchRecover` waiter, not a waiter inside `FsBatchCreate`,
   `FsBatchReplace` or `PatchApplyAtomic`; one park point per k; one runner; a process kill is not power loss. Not tried:
   power loss, other runners; the lock cases are sequential in one process; the POSIX mode check has no Windows
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
- **Journal byte fuzzing: shown for six POSIX journals (replace, create, remove, move, batch create, batch
  replace), with measured limits. Shown for six Windows journals (replace, create, remove, move, batch create, batch replace), with measured limits, m171 to m181.**
  `tests/test_fs_journal_fuzz.c` (m163 to m168) damages the journals with bit flips, set bytes, truncation,
  appended bytes, path-field rewrites, deleted journal files and stage or temp-name decoys, at every crash point
  of each kind, 350 iterations per kind, seeds 0, 1, 7, 12648430 and the test default 1786707969. A run passes
  when recovery refuses (and leaves the workspace unchanged) or returns OK with the old or the new state (all
  targets or none for the batches), nothing outside the workspace touched and no journal, stage or `.fstxn-`
  name left. Design: [core-m1-c5-journal-fuzz-proposal.md](core-m1-c5-journal-fuzz-proposal.md).
  **Integrity trailer (m168).** Since m168 every POSIX journal and marker record file is the record plus an 8 byte
  trailer (magic and CRC-32). Plain damage to a record is refused. A record without a trailer (written by a build
  before m168) is still accepted unchecked, so an interrupted older operation can be recovered; the legacy cells
  of the fuzz cut the trailer at every crash point of every kind and require recovery OK. The 8 byte commit
  markers (batch, remove, move) have no CRC trailer and need none: recovery requires a regular file with one link, size
  exactly 8 and the value of the live journal's inode number, so any damage that changes the size or the value is refused
  (measured since m183, see "Journal marker fuzz" below). **A CRC detects accidental damage only. A writer that rewrites a
  field and recomputes the CRC is not detected: that is the cooperating-writer model, not authentication.**
  Measured, with the status words that apply:
  - **F1, fixed (m164).** Replace recovery left the `.fstxn-<hex>` and `.fstxn-c<hex>` temporary links. It
    now removes them only when they are provably the journal's own links. The earlier m163 statement that
    no journal or stage name is left after recovery was true only for the names that test checked; it did
    not look for `.fstxn-` names. The fuzz now does, for all kinds.
  - **F2, F4, F6, F7: found by the fuzz before m168; now refused for plain damage, still open for a writer that
    recomputes the CRC.** Before m168 a damaged name field that was still a valid name was followed:
    F2 (remove, target field) restored the file under that name; F4 (move, source field, crash 22) left the new
    name and the real source as two links of one inode; F6 (batch create, item name, crash 6) returned OK with
    the first target still published, so the batch was half applied and the claim "all targets or none" was NOT
    met; F7 (batch replace, one hex digit of a stage name, crash 61) left the real new stage as an orphan link.
    F4 was found by the test-default seed (iteration 7, move, bit flip, crash 22), not by seeds 0, 1, 7 and
    12648430, so a suite with only those seeds would have hidden it. Each shape is a pinned cell run twice: plain
    damage must be refused with the workspace unchanged (measured since m168); with the CRC recomputed the old
    outcome is asserted, and it still holds, so these four are limits of the cooperating-writer model.
  - **F5, limit, not met, outside the crash model.** If the batch journal file is deleted while the batch is half
    published (create crash 6, replace crash 61), recovery has no record, returns OK and leaves the half applied
    state with no journal. The contract "all targets or none" is NOT met there. The oracle allows it for the
    delete class at those two crash points only (26 hits on seed 0, measured locally). The CRC does not change it.
  - **F3, limit, outside the crash model.** Deleting the move journal at crash point 22 leaves src and dst as
    the same inode. The oracle allows this for the delete class only.
  - **Mutants.** Exact kills: replace stage and replace temp (m163), replace temp links (m164, now mask 1 with
    the setbyte class ignored: it only fails when a random set byte writes the value already there), remove stage
    identity and move stage identity (m165). The stage and temp mutants are exact over the decoy classes only: the
    random byte classes of the mutated kind are ignored (`FS_JFUZZ_MUTANT_IGNORE`), because they can add
    seed-dependent failures. The stage decoy classes recompute the CRC on purpose; otherwise the CRC would refuse
    the record before the identity check the mutant drops, and every stage mutant would survive (measured before
    the change). There is no create mutant (a create-stage and a create-temp mutant both survived on all four
    seeds: a rewritten stage field changes the name the record temp link is derived from, so the record is refused
    before the stage identity check; that check is not independently exercised by the current classes). There is
    no batch mutant: none was both cheap and killable.
  - **Not shown:** damage to
    two journal files at once except the Windows replace pair-mismatch cell.
    Criterion 5 stays partial.
  - **Windows journals: read (source reading at a3e89a2), and the replace journal fuzzed (m171 to m175).**
    The five Windows records already carry a 32 bit FNV-1a checksum over the whole struct except the checksum
    field, checked on read: create, remove/move and replace in `src/fs_create_win.inc` (structs lines 30 to 56,
    `wc_checksum`, `wo_checksum`, `wr_checksum` lines 119 to 148, checks at lines 344, 376, 408); batch create and
    batch replace in `src/fs_batch_win.inc` (structs lines 16 to 32, `wb_sum` lines 35 to 44, checks at lines 74
    and 91). Records are also checked for magic, version, size, name shape and file ids. The Windows batch commit
    marker `.fstxn.commit` is a byte copy of the journal, compared with `memcmp` (`src/fs_batch_win.inc` lines
    154 to 156 and 319). FNV-1a, like the POSIX CRC, detects accidental damage only, not a writer that recomputes
    it. `FsWinBatchTestMutate` (`src/fs_batch_win.inc` lines 957 to 989) is a fixed list of 18 mutations of batch
    records, not random byte fuzz; the Windows batch journals have been fuzzed by bytes since m180 (bullet below).
  - **Windows replace journal fuzz (`tests/test_fs_win_journal_fuzz.c`, m171 to m175): measured, one runner
    (`windows-latest`), replace only, real process kills at crash phases 3 to 6.** 9 damage classes (bit flip,
    set byte, truncate, append, sealed name fields, sealed scalar fields, sealed shape, deleted journal files,
    intent and marker that disagree), 20 iterations per phase and class, seeds 1786707969 and 12648430, 660
    cells each. "Sealed" means the checksum is recomputed, so only the semantic checks can refuse the record.
    Measured in the m174 run (job `build-test-msvc`): **a sealed damaged name field
    is NOT followed on Windows replace.** The target, old pin and new pin fields were refused, a renamed stage
    was refused where the stage link exists and harmless where it does not, a renamed rollback name was harmless,
    and every scalar, shape and pair cell was refused: no miss in 2 x 660 cells outside the deleted-journal
    class. So the POSIX shapes F2, F4, F6 and F7 did not appear on Windows replace. This says nothing about the
    Windows batch journals (create, remove, move and batch are in the next bullets). Their record checks differ.
  - **Windows replace, journal deleted after the publish: limit, outside the crash model, measured.** With the
    intent deleted at crash phases 4 and 5, or both the intent and the marker deleted at phase 6, recovery
    returns OK, the new bytes are in place, and exactly one `.fsrp-` pin is left in the root (46 of 660 cells,
    the same count on both seeds, m174). It is the old pin: `wb_sweep_orphans` keeps an unshared `.fsrp-`
    on purpose, because it may be the last link to bytes someone still needs (comment above `wb_sweep_orphans` in
    `src/fs_batch_win.inc`; `include/fs_replace.h` says a pin left after record removal "is never deleted
    automatically"). Same class as the POSIX limit F5 (a record deleted after the publish leaves nothing to
    check). Data is not lost, names are not clean. Since m175 the test pins these cells: OK, new state, exactly
    one leftover name, a `.fsrp-` pin whose bytes are the old bytes. That the pin holds the old bytes was an
    inference from the code until the m175 run; the m175 CI result is reported separately.
  - **Windows create journal fuzz (`tests/test_fs_win_journal_fuzz_create.c`, m176 and m177): measured, one
    runner (`windows-latest`, msvc and asan jobs), create only, real process kills at crash points 3 to 6
    (`FS_WIN_JOURNAL_CRASH`).** Same 9 classes and seeds as the replace fuzz, 660 cells per seed, 584 of them
    held as predicted in both seeds: every unsealed damage, scalar (except a mode flip, which is followed and
    only changes the read-only bit), shape and pair-mismatch cell was refused, a record naming the decoy was
    refused, and the other name cells matched their predictions. The first run (m176, strict oracle) missed in
    6 table lines, 76 cells per seed, identical on msvc and asan and on both seeds; they are the cells below.
    **Resealed name fields ARE followed in two places, by a writer that recomputes the checksum in both journal
    files (outside the cooperating-writer model, the same stance as the POSIX CRC: it detects accidental
    damage, not forgery):** (1) at crash point 3, a stage name resealed to a missing name is accepted (the
    lookup of a missing name returns OK, `wc_lookup`, `src/fs_create_win.inc` lines 237 to 247), so recovery
    rolls back, returns OK and the real `.fst-` stage is left (5 cells per seed); (2) at crash point 6, a pin
    name resealed to a missing name in both files makes recovery return OK with the right bytes, but the real
    `.fsp-` pin stays as a second hard link of the finished file (5 cells per seed). Both are untracked leftovers
    with the right bytes, not lost data. **Journal deleted: limit, outside the crash model, measured.** With the
    intent deleted at crash point 3, recovery returns OK and leaves the stage and the pin (2 names, 20 cells per
    seed); with the intent deleted at points 4 and 5, or both files deleted at point 6, it returns OK and the
    finished file keeps a second link, the pin (46 cells per seed). With nothing to recover from the code does
    nothing, the same class as POSIX F5 and as the Windows replace limit above. Since m177 the test pins all
    these cells: state, exact number and kind of leftover names, their bytes and their link counts. Why the pin
    existence is not required by the code is a reading, not a measurement: after the commit marker a missing
    pin can be a legitimate state (a crash between the commit and the pin removal), so "the pin must exist"
    is not a safe rule without a deeper change. Crash points 1 and 2 (stage, or stage and pin, with no journal)
    are not covered: recovery returns OK without cleanup there by reading of the code (line 763), not by test.
  - **Windows remove and move journal fuzz (`tests/test_fs_win_journal_fuzz_ops.c`, m178 and m179): measured, one
    runner (`windows-latest`, msvc and asan jobs), real process kills at the points that leave a journal
    (`FS_WIN_OP_CRASH`: remove 1, 3, 4; move 1, 2, 3, 4).** Same 9 classes and seeds, 1160 cells per seed (500
    remove, 660 move). This recovery is stricter than the create one: it requires the pin, checks the pin's
    link count against the source and target actually present, and looks every name up by file id
    (`wo_recover_locked`, `src/fs_create_win.inc` lines 568 to 650). 1040 cells per seed held as predicted: all
    unsealed damage, scalar (except `target_parent` on remove, a field remove does not use), shape and
    pair-mismatch cells were refused, and so were a source or target resealed to the decoy, a target resealed
    to a missing leaf (except move point 1, where no target exists yet and recovery is correctly OK), and a
    resealed pin name. The first run (m178, strict oracle) missed in 9 table lines, 120 cells per seed, identical
    on msvc and asan and on both seeds. **A resealed source name IS followed in two places (remove point 3,
    move point 3), by a writer that recomputes the checksum in both journal files (outside the cooperating-writer
    model, same stance as the POSIX CRC: it detects accidental damage, not forgery):** the rollback relinks
    the pin under the name in the record (`wc_set_name` at `src/fs_create_win.inc` line 626), recovery returns
    OK, the file is back under the resealed name (the test reads that name from inside/ since m179), the original
    path stays empty, and for move the target link is removed (4 cells per seed each). **Journal deleted: limit,
    outside the crash model, measured, the same class as POSIX F5.** With the intent deleted at point 1 the
    source keeps the pin as a second link (remove and move, 20 cells each); move at point 2 leaves both the
    source and the target, 3 links each (20); at point 3 remove leaves the file only as the `.fsrm-` pin and move
    leaves it at the target with the pin as a second link (20 each); with both files deleted at point 4 the pin
    stays (6 each). Since m179 the test pins all 9 groups with the exact outcome (file set of inside/, bytes,
    link counts, names of leftovers). Not covered: point 0 (pin only, no journal: recovery returns OK without
    cleanup, by reading of the code); the batch journals are in the next bullet.
  - **Windows batch journal fuzz (`tests/test_fs_win_journal_fuzz_batch.c`, m180 and m181): measured, one runner
    (`windows-latest`, msvc and asan jobs: the m180 tables are identical on both jobs and both seeds), real process kills at the
    points that leave a journal (`FS_WIN_BATCH_CRASH`: create 3 to 6, replace 24 to 27).** Same 9 classes, two
    seeds, 1848 cells per seed (28 per class and point), batches of TWO items: `FsBatchCreate` of inside/a (mode
    0644) and inside/b (mode 0444), and `FsBatchReplace` of two files. The oracle requires both targets to end in
    the same state (create: both absent or both new; replace: both old or both new), one link each, no stage, pin,
    rollback or journal name, the decoy unchanged, the read-only attribute on the 0444 target after a create, and
    a second recovery that is OK and changes nothing. This recovery runs an orphan sweep after a successful
    `FsBatchRecover` when no journal or marker exists (`wb_sweep_orphans`). The first run (m180) missed in 11
    table lines, 146 cells per seed, identical on both seeds. All other cells held as predicted: all unsealed
    damage (bit flip, set byte, truncate, append), every scalar field except the benign create mode flip (0444
    versus 0644 only changes the read-only bit), every shape cell, the pair mismatch, and the target resealed to the
    decoy were refused with the workspace unchanged; stage and rollback names resealed were harmless.
    **The weakest result: a resealed target name IS followed, by a writer that recomputes the checksum in both
    journal files (outside the cooperating-writer model, same stance as the POSIX CRC: it detects accidental
    damage, not a writer that recomputes it), and the batch is then NOT atomic.** Create, crash point 4 (first
    target published), item 0 resealed to a missing leaf: recovery returns OK, a stays published, b is absent
    (4 cells per seed). Create point 5 (all targets published), item 0 resealed: a stays, b absent; item 1
    resealed: b stays, a absent (4 cells each). A half applied batch is stated here as a limit, not hidden. **Journal
    deleted: limit, outside the crash model, measured, the same class as POSIX F5.** Create with the journal
    deleted at point 4: a is new, b absent (28 cells). Create at point 5 and with both files deleted at point 6: both
    targets are new but b, created with mode 0444, is NOT read-only (the attribute is only set on the
    marker path); 28 and 9 cells. Replace with the journal deleted at point 4 (item 0 replaced): a new, b old, and
    one `.fsrp-` pin holding the old bytes of a stays as a single link (28); at point 5 and with both files
    deleted at point 6: both new and two `.fsrp-` pins stay (28 and 9). The sweep keeps an unshared old pin by
    design, because it may be the last link to the old bytes (the same limit as the single replace). A resealed
    old pin name at replace point 6 (item 0 or 1) leaves the real `.fsrp-` of that item (2 cells each, one
    pin). Since m181 the test pins all 11 groups with the exact outcome (bytes of both targets, links, the
    read-only attribute, names and contents of leftovers). The first version of the oracle did not report the
    leftover pin of replace point 4: it stopped at the mixed state; the m181 oracle checks state, links and
    leftovers together. Not covered: crash points with no journal (1, 2, 21, 22, 23: by reading of the code),
    batches of more than two items, two journal files damaged differently except the pair-mismatch class, power
    loss, one runner.
  - **POSIX commit marker fuzz (`tests/test_fs_journal_fuzz_marker.c`, m183, third seed m184): measured, Linux CI, three seeds (1786707970, 12648430, 305419896; about 1.0 to 1.2 s each on linux).** The
    marker of remove (`.fstxn.rcommit`), move (`.fstxn.mcommit`), batch create and batch replace (`.fstxn.commit`) is
    8 bytes, the inode number of the live journal. Recovery requires a regular file, one link, size 8 and that value
    (`src/fs_read.c`: batch replace line 829, batch create 910, remove 1316, move 1527). The test kills the writer at every
    crash point of the kind (one point per kind leaves a marker: found by looking at the files), damages only the
    marker, then recovers in a forked child. Per kind 89 damage cells: 64 single bit flips, 8 set bytes, truncation to 0
    to 7 bytes, 4 appends (1, 8, 9, 16 bytes), eight zero bytes, the inode of an unrelated file, a stale inode (the marker
    file's own), a symlink in place of the marker, a second hard link; plus 2 control cells that are not damage (91 cells per kind) (the
    journal's own inode number written back in place, and the marker replaced by a new file holding the same 8 bytes). 364
    cells per seed, 0 failures: every damaged marker is refused (DENIED), the workspace is byte for byte unchanged, nothing
    outside it changed, the journal and the marker stay; the controls recover OK in the committed state with no journal or
    marker left and a second recovery OK. Mutants, run locally once and not in CI: dropping the value compare at each of the
    four sites makes exactly that kind fail with "a damaged marker was not refused" (20 failures printed for each, the print is
    capped). **This is not authentication:** a writer that writes the journal's inode number into the marker is accepted,
    the same cooperating-writer limit as a recomputed CRC. An earlier plan to add a CRC trailer to these markers was
    dropped (a retraction, m183): the inode compare is already stronger against accidental damage and the trailer would
    only change the on-disk size. Not covered: the replace marker (it embeds its record and has the trailer), a marker
    that is deleted (delete class of `test_fs_journal_fuzz`), Windows, power loss, one runner.
- **Windows orphan sweep killed after its first removal (`tests/test_fs_win_batch_replace.c` `sc_sweepcrash`, m184, m185, m186): measured, msvc and asan.**
  A writer killed at point 23 leaves orphan names and no journal. A second process is killed by test point 37 (exit
  117) right after the sweep removed its first name, once through each of `FsBatchRecover`, `FsBatchCreate` and
  `FsBatchReplace`. Asserted per mode: at least 4 orphan names before, exactly one fewer after, the three targets keep
  their old bytes, a later `FsBatchRecover` (twice) leaves a clean tree and the batch then works. Mutant 12 (the sweep
  stops after one removal) is killed. Read: run 37254495035 (m185, `922e410`), msvc and asan 196 of 196, the test
  Passed. The first run of the same cells (m184, run 37253372878) aborted on a loop error of mine (mutant 11 does not
  belong to this test) and left both Windows jobs red until m185. Limits: only the kill after the FIRST removal, one
  3-item replace batch as the source of the orphans, one runner, a process kill and not power loss. A kill after a later
  removal is not injected. Orphan names in the root before the killed sweep and after it: 9 and 8 in all three modes (read on msvc and asan, both printed this line in run 37260112093 of m188: the first guess of 6 and 5 was wrong, a 3-item batch leaves three names per item, the old `.fsrp-` pin, the `.fst-` stage and the new `.fsp-` pin). The test asserts that line.
- **Windows orphan sweep killed after later removals (`tests/test_fs_win_batch_replace.c` `sc_sweepchain`, m211, run 37318504276, commit 2a63783): measured as a pass of a pinned line, msvc and asan.**
  The crash point 37 fires on the first removal of a process, so a chain of `FsBatchRecover` children each killed at 37 removes one more orphan per child. The setup is the same as above (a writer killed at point 23, 9 orphan names). Prediction before the run: counts 9 8 7 6 5 4 3 3, that is 6 kills and then a finishing child leaving 3 names (the unshared `.fsrp-` pins kept by design); I put it at about 50 percent because I had not read which names are shared. It held: the CMake regex pins the line and the test Passed on msvc (204 of 204) and asan (204 of 204); the line is known from that regex pass, not read in the log. Asserted at every step: a killed child exits 117 and removed exactly one name, the three targets keep their old bytes; at the end the finishing child exits 200, `FsBatchRecover` twice is OK and changes nothing, the old bytes are intact and the batch works. So a sweep killed after its 2nd, 3rd up to 6th removal leaves a tree that the next recovery finishes. Limits: only the `FsBatchRecover` entry point is chained (a finishing `FsBatchCreate` or `FsBatchReplace` child would change the tree itself); one 3-item replace batch as the source of the orphans; no mutant for this cell (mutant 12 would change every child the same way); one runner; a process kill, not power loss.
- **Single-file calls and the sweep (read in m212, not measured here).** In `src/fs_create_win.inc`, `wr_recover_locked` (replace) runs the orphan sweep when there is no intent and no marker (m139), so single-file replace recovery sweeps. `wc_recover_locked` (create and copy) returns OK when it finds neither record and does not sweep; `FsRemoveRecover` and `FsMoveRecover` go through `wo_recover_locked`, which I did not read for this. `tests/test_fs_win_journal.c` says rows 1 and 2 (crash with a stage only, and stage plus pin, before the intent) leave untracked orphans that the test removes itself. On POSIX, `src/fs_read.c` has a comment that a crash at the create stage "may leave an orphan stage instead" (line 530), and a search of the file for sweep, orphan and readdir found no function that removes orphans. So the missing sweep for single-file create, copy, move and remove is parity with POSIX, a known limit and not a Windows-only defect; the earlier phrase "single-file calls do not sweep" was too broad for replace. Not measured: whether such an orphan is removed by a later batch call on Windows (the batch sweep matches the `.fst-` and `.fsp-` shapes, which I read but did not run for this case), and what the orphans cost (a stage holds the bytes of one file).

- **Content fuzzing: not shown.** The generated inputs are paths and manifests (counts, duplicate targets,
  traversal, kinds). File contents, EOL mixes and patch text are not fuzzed here.
- **Seeds:** four logged seeds. The criterion text does not say how many are enough; that is Antonio's call.
- **Windows symlink leaf** is a fixed pass, not in the random pool.
- **Windows `AgentPatch`-level fuzz:** not shown. The fuzz drives `Fs*` operations, not `PatchApplyAtomic`.

## C1 on Windows: 8.3 short names

- **8.3 short names, Windows (m193 to m195, runs 37269323016, 37271090475, 37273525033; msvc and asan-msvc 197 of 197 in the last run, the test Passed in 2.11 s and 2.96 s):**
  the runner volume makes no short name by default (`test_fs_win_aliases_short` was Skipped until m194). `test_fs_win_8dot3_probe` (m193) measured that
  `fsutil 8dot3name set X: 0` returns 0 on the runner and a file created afterwards then has a short name; the test restores the setting
  (`set X: 1`) through `atexit`, so also after a failed check, but not if the process is killed in between. `test_fs_win_aliases_short`
  (m194, m195) does the same, then runs 9 operations (create, copy target, copy source, move target, move source, replace, remove, batch create,
  batch replace) through the short name of the `.fstxn.lock` control file, each on a fresh fixture. Measured (the CMake regex asserts the
  line, so a pass is the line): all 9 refused with status DENIED and nothing changed (victim bytes, entry counts, `inside/f`, the lock
  still empty). Controls: through the short name of an ordinary file the library accepts the 5 operations that need the file and refuses the 4
  that need the target absent (status DENIED), so it has no alias check of its own; an empty ordinary file through its long name
  is accepted by the 5 operations that pass expected bytes "", so a zero-length expected value is not what refuses the lock cells.
  **Why the lock cells are refused was isolated in m203, see the next bullet.** The reserved-name check compares names only and `FSTXN~1.LOC` passes it
  (read in `wc_target_status` and `wc_reserved`). **Wrong predictions, named:** in m194 I predicted A (accepted)
  for copy source, move source, replace, remove and batch replace and R for the other four; all five A were R on msvc and asan
  (run 37271090475, master was red on msvc and asan for that run by design, linux 184 and ninja 10 green), because my reading of src ("no
  8.3 handling, so the alias gets through") missed what refuses the lock. m195 predicted all three groups (lock, ordinary alias, empty file) and the asserted line matched on msvc and asan.
  Limits: one runner (`windows-latest`), one volume, short names switched on by the test (it changes a volume setting of the CI runner
  temporarily); only the lock file here (the foreign reserved-prefix file is the next bullet), and only a single directory level; an alias accepted for
  an ordinary file is the same file inside the tree (not an escape), nothing here shows more.

- **Why the lock alias is refused, a finding and its repair (m203, run 37294782982, commit ddd236f; m204, run 37297634708, commit 89deb1b):** `test_fs_win_lock_alias` runs the
  five operations that pass expected bytes "" (copy source, move source, replace, remove, batch replace) on an empty target in three groups, one fresh fixture per
  cell, with 8.3 names switched on by the test as above. The CMake regex pins the line, so a pass is the line (I did not read the line itself in the CI log).
  m203 measured, and all my predictions matched: (1) an ordinary empty file reached by its long name while the test holds `LockFileEx` on byte 0 with another handle:
  all five refused with DENIED, file intact, so a held byte-range lock is enough to make these operations refuse, which fits the explanation for the lock file
  (this was shown on an ordinary file, not on the lock file itself); (2) a foreign empty `.fstxn.foreign` in the root through its long name: all five refused by
  name; (3) **the same foreign file through its 8.3 name was accepted by all five**: copy source and batch replace read it, move source moved it away, replace wrote
  `NEW!`, remove deleted it. That is a defect in the name-only reserved check: the 8.3 alias of a reserved-prefix file that nobody locks bypassed it (the lock file
  was protected only by its held lock). I stopped, the parent chose a repair, and m204 added `wc_final_reserved` and a check in `wc_open`: after a successful open of
  a leaf that is not itself reserved, the final name of the handle (`GetFinalPathNameByHandleW`, normalized, volume name none) is read, and a reserved leaf or an
  unreadable name returns DENIED. Library-internal opens use reserved names literally and skip it. Result in m204 (msvc 202 of 202, asan 202 of 202;
  linux 184, ninja 10): the foreign-alias group is now refused by all five (the line changed from `0e 0- 0n 0- 0n` to `3e` five times), the other groups and
  every other test are unchanged. My m204 predictions all matched, the named risk (a handle with an extra hard link under a reserved pin name reporting that name)
  did not happen on this runner. Limits: one runner, one volume, a foreign file with an empty body; **not covered by m204:** the 8.3 alias of a reserved-prefix directory used as a
  path component (parents are opened by `win_open` in `src/fs_read.c`, not `wc_open`; measured and repaired in m206 and m207, next bullet), a writer
  that sets arbitrary short names, other filesystems. A final name read from a handle is a check for cooperating use, not a defence against a hostile writer.
- **The other reserved prefixes and a reserved directory (m206, run 37303780972, commit 654eb94; m207, run 37305908222, commit 2d64aa2):** `test_fs_win_reserved_alias` puts an
  empty foreign file `<prefix>foreign` in the root for each of the nine prefixes of `wc_reserved` (`.fstxn.`, `.fst-`, `.fsp-`, `.fsj-`, `.fsrp-`, `.fsrs-`, `.fsrb-`, `.fsrm-`,
  `.fsmv-`; the names never equal a real control name, so the library does not read them as journals) and replaces it through its long name and through its 8.3 name; one fresh
  fixture per cell. The CMake regex pins the line, so a pass is the line (not read in the CI log). m206 measured, all predictions matched (msvc 203 of 203, asan 203 of 203):
  all nine prefixes are refused (DENIED, file untouched) through both names, so the m204 check covers every prefix, not only `.fstxn`. **Finding (the gap named above):** a
  directory `.fstxn.dir` holding `inner.txt` was refused through its long path but **accepted through the 8.3 name of the directory**, and the file inside was replaced
  (`dir=3o0n`). The segment check is name-only and the parent is opened by `win_open`, so the final-name check never saw it. The file inside is not a control file (the control
  files live in the root), so this bypassed the name rule for directories, not the lock or journal protection. I stopped, the parent chose a repair, and m207 added
  `wc_below_root_reserved` to `wc_parent` in `src/fs_create_win.inc`: the parent's normalized final path is compared with the workspace root's and each segment below the root goes
  through `wc_reserved`; anything unreadable, outside the root, or a segment of 256 characters or more is refused (fail closed). Result in m207 (msvc 203 of 203, asan 203 of 203;
  linux 184, ninja 10; my predictions matched): `dir=3o3o` (both paths refused) and the over-refusal control `dirord=0n0n` (an ordinary directory through its long path and its 8.3
  name is still accepted); every other test unchanged. Limits: one runner and volume, an empty or two-byte body; the check runs in `wc_parent`, so reads (`FsReadFile` and the read
  path through `win_open`) are not covered, and they do not mutate; a path whose final name does not start with the root's (a junction or mount point) is refused rather than
  followed; a writer that sets arbitrary short names or a hostile writer is not addressed, this is a check for cooperating use.

## C1 on Windows: per-directory case flag

- **Case-sensitive directory (m197, m198; runs 37277509454 and 37279224457):** `test_fs_win_case_probe` (m197, msvc and asan 198 of 198) measured on the runner that
  a default directory cannot hold `x.txt` and `X.txt`, that `fsutil file setCaseSensitiveInfo <dir> enable` returns 0, that the query says enabled and that the
  flagged directory then holds both. `test_fs_win_case_flag` (m198, regex corrected in m199) flags one scratch directory `d` and runs the library on it, one fresh
  fixture per cell. Measured line (asserted by the CMake regex): `case flag: read_xX=0U replace_xX=0on remove_xX=0o- copy_source_xX=0oo move_source_xX=0o-
  replace_zZ=0on remove_zZ=0o- copy_source_zZ=0oo move_source_zZ=0o- create_yY=0on copy_target_yY=0on move_target_yY=0on` (status number, then the lowercase and the
  uppercase entry: o unchanged, - missing, n changed or newly present). Read: with `x.txt` "lower" and `X.txt` "UPPER", a read of `d/X.txt` returns UPPER; replace, remove,
  copy source and move source of `d/X.txt` and of `d/Z.txt` (where `z.txt` and `Z.txt` both hold "SAME!", so a wrong entry would pass the expected-bytes check) act on the
  exact-case entry only, the other entry is unchanged. Create, copy target and move target onto `d/Y.txt` while `y.txt` exists succeed and create `Y.txt` beside it.
  So in a flagged directory the library treats names that differ only by case as different entries and never acts on the other one. **Wrong prediction, named:**
  I predicted the three target cells would be refused (a collision with `y.txt`); all three succeeded, so the test was red on msvc and asan in run 37279224457 by design
  (linux 184 and ninja 10 green) until the regex was corrected. This differs from the default directory, where an alias of an existing name is refused (`test_fs_win_case`).
  Limits of m198: only the flagged subdirectory `d`; the root and batches are covered by the two bullets below; one directory level; one runner (`windows-latest`);
  the flag is set by the test on a scratch directory with `fsutil`; a process kill, power loss and other volumes are not tried.
- **Flagged workspace root (m200, run 37284795139; commit 3b2d03e; msvc 200 of 200, asan 200 of 200, linux 184, ninja 10):** `test_fs_win_case_root` sets the flag on the empty
  scratch root before the library opens it, so the lock file `.fstxn.lock` lives in a case-sensitive directory. The CMake regex pins `root flag: foreign_lock=0ooo
  reserved_create=3- reserved_remove=3o root_replace_aA=0on root_remove_aA=0o-` (status number 3 is FS_READ_DENIED; the four letters after `foreign_lock=0` are the library's
  `.fstxn.lock`, the foreign `.FSTXN.LOCK`, and the created `a.txt`). The test passes only when the printed line matches, and it passed on msvc and asan, so these are
  the measured values; I did not read the line itself in the CI log (a passing ctest hides stdout), only the pass. Read: with a foreign 3-byte file `.FSTXN.LOCK`
  present, a create of `a.txt` succeeds, the library takes its own empty `.fstxn.lock` and the foreign file is unchanged. Create and remove of `.FSTXN.LOCK` (a reserved
  name in another case) are refused by name with DENIED and nothing changes (the foreign file keeps its bytes). Replace and remove of `A.txt` next to `a.txt` (equal bytes)
  act on `A.txt` only; `a.txt` is unchanged. The five predictions matched, none wrong (confidence was 55 percent). Limits: one fixture per cell, one runner, no other
  reserved names (the journals) and no crash in a flagged root.
- **Batches in a flagged directory (m201, run 37287889480; commit 0350e8d; msvc 201 of 201, asan 201 of 201, linux 184, ninja 10):** `test_fs_win_case_batch` pins
  `batch flag: rep_dup_xX=1ooo rep_X_xX=0onn rep_Z_zZ=0onn cre_dup_nN=1--- cre_Y_yY=0onn` (status, then lowercase entry, uppercase entry and `k.txt`; status 1 is
  FS_READ_INVALID; same caveat: matched by regex, passed, line not read in the log). Read: a replace batch naming both `d/x.txt` and `d/X.txt` (two real entries, different
  bytes) is refused whole with INVALID and all three files are unchanged, and a create batch naming `d/n.txt` and `d/N.txt` is refused with INVALID and creates nothing: the
  batch validator folds names, so two targets that differ only by case count as one, as `wb_conflict` reads. A replace batch of `d/X.txt` and `d/k.txt` next to `x.txt`
  succeeds and replaces `X.txt` and `k.txt` only; the same with equal bytes (`Z.txt`, `z.txt`) acts on `Z.txt` only. A create batch of `d/Y.txt` and `d/k.txt` next to an
  existing `y.txt` succeeds, `y.txt` is unchanged. So in a flagged directory a batch can name only one spelling, and it acts on the exact-case entry. The five predictions
  matched, none wrong (confidence 55 to 85 percent). Limits: two entries per batch, one fixture per cell, no crash or recovery of a batch in a flagged directory,
  one runner.

## C1 on Windows: parent swapped to a junction

Correction to an earlier sentence of this reading and of ROADMAP.md ("a rename race against a swapped parent has no Windows counterpart"). It was too broad.
From the source of `tests/test_fs_win_race.c` (read, m209 design), cell E already flips a directory `D` between a real directory and a junction to an outside
directory while writer threads call `FsCreateFile`, `FsReplaceFile`, `FsRemoveFile` and `FsBatchCreate` through `D/...`, checks the outside directory and has
guards (junctions seen at least 5 times, at least one writer call OK). Not covered there: `FsMoveFile`, `FsCopyFile` and `FsBatchReplace` through the swapped parent.

- **Move, copy and batch replace (m209, run 37311710086, commit a601c31):** `tests/test_fs_win_race_swap.c`, Windows only, written as a sibling of cell E (cell E and its
  diagnostic are unchanged). The outside directory holds `victim` ("VICTIM"), `r1` ("A1") and `r2` ("A2"). For 5 s a swapper flips `D` between a real directory and a junction
  to it while two writer threads loop: move and copy of `D/victim` to a root name (never OK in the real phase because `victim` does not exist in the real `D`, so an OK
  or a file in the root means the junction was followed); move and copy of a root file into `D` followed by a remove there; and (one thread) create `D/r1`, `D/r2`,
  `FsBatchReplace` of both to "B1" and "B2" (the outside bytes equal the expected ones, so a followed junction would change them), then remove. The main thread checks
  every 50 ms and at the end that the outside directory has exactly three entries with their bytes. Guards: junction phases verified at least 5 times, at least one writer
  call OK, no "bad" count (an OK or a root file from the out-of-D move or copy). The predictions, written before the first run (invariant holds, guards pass), matched:
  msvc 204 of 204 (`test_fs_win_race_swap` Passed, 5.12 s), asan 204 of 204, linux 184, ninja 10. The printed counts (calls, OK per operation, swaps, junctions) were
  **not read**: ctest hides stdout and `PASS_REGULAR_EXPRESSION` pins the ok line, which prints only after the three guards, so a pass is that line. I do not know how many
  of the move, copy and batch replace calls ran in a real-directory phase.
  Limits: an invariant over one 5 s budget, not a proof that no interleaving exists; one runner (`windows-latest`); cooperating writers only; one swap target (a junction to
  a directory outside the root); the root itself and a swapped component deeper than one level are not swapped; no crash or recovery during the swap.

## Not shown, whole reading

Power-loss durability (deferred); other runners, filesystems, accounts, inherited ACLs; 8.3 aliases other than the lock-file cell set in the 8.3 section above;
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
