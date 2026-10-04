# M1 criterion 5: journal fuzzing proposal (design only, nothing implemented)

Status: PROPOSAL, written 2026-10-04. No code, no test, no CMake change comes with this file. It sizes
the gap named in [core-m1-exit-reading.md](core-m1-exit-reading.md) section "C5: scope gaps" and ROADMAP
"Where we are" (criterion 5 is **partial**): "Journal byte fuzzing: not shown". Whether this proposal is
worth doing, and how much fuzzing satisfies C5, is Antonio's call.

## What is not covered today

- `tests/test_fs_fuzz_ops.c` fuzzes paths and manifests and checks confinement and no-change. It never
  corrupts a journal.
- Recovery is tested at fixed crash points (`FS_CREATE_TEST_CRASH`, `tests/test_fs_win_journal.c`,
  `tests/test_agent_patch_crash_win.c`) and with a foreign inode. No test flips, truncates, extends or
  zero-fills a `.fstxn.*` record or a `.fsrp-` / `.fsrb-` stage at random.
- Recovery entry points in `src/fs_read.c` read these names: `.fstxn.intent`, `.fstxn.batch`,
  `.fstxn.commit`, `.fstxn.remove`, `.fstxn.rcommit`, `.fstxn.move`, `.fstxn.mcommit`, `.fstxn.replace`,
  `.fstxn.pcommit`, and the stages `.fsrp-*` and `.fsrb-*`. Recovery functions: `FsCreateRecover`,
  `FsBatchRecover`, `FsRemoveRecover`, `FsMoveRecover`, `FsReplaceRecover`.

## Claim the test would make (and not make)

Claim: for a journal that is damaged after a valid crash point, recovery FAILS CLOSED: it either
(a) returns a refusal and leaves every workspace and outside byte unchanged, or (b) returns OK and the
workspace then equals the state before the operation or the state after it, never a mix; and in both cases
nothing outside the workspace is written, read-only files stay as they are, and the process does not crash or
hang (ASan job and a 30 s child timeout).

Not claimed: power-loss durability, non-cooperating writers, other filesystems, recovery of an intent left
by the old file-parent bug (still NOT DONE), content or patch-text fuzzing (a separate gap).

## Design

1. Source of valid journals. Reuse the existing crash hook: a child process runs one operation with
   `FS_CREATE_TEST_CRASH` set to a phase, exits at that phase, and leaves a real journal. The parent copies the
   journal files (names above) to a corpus directory. Nothing is hand-built, so the layouts stay in sync with src.
2. Mutations on the copy, seeded and logged (`FS_FUZZ_SEED`, `FS_FUZZ_ITERS`, same convention as
   `test_fs_fuzz_ops`): single bit flip; byte set to 0x00 or 0xFF; truncate to a random length including 0
   and length minus 1; append random bytes; swap two records (an intent from one phase with a commit marker from
   another); delete one of the pair (record without marker, marker without record); replace a path field
   with `..`, an absolute path, a symlink name, a `.fstxn.lock` or `.fsrp-` name.
3. Oracle per iteration: snapshot the tree and a sentinel file and directory outside (same walker as the
   path fuzz), run the recovery function for that operation kind, then assert the claim above. Record seed,
   iteration, journal kind, phase and mutation in the failure line.
4. Mutants to prove the fuzzer can fail (same rule as the lock and crash tests: exit 0 only on the exact
   predicted failing cells, `PASS_REGULAR_EXPRESSION`, no `WILL_FAIL`): (i) recovery accepts a target path
   without the confinement check; (ii) recovery trusts a record whose commit marker is missing. A fuzzer that
   cannot be killed by a known-bad recovery is not evidence.
5. Order: POSIX first (gcc here, measurable in the sandbox and in the linux and asan jobs), then Windows,
   which is CI-only (no Windows compiler in the sandbox); Windows needs the same rules for its journal
   (`tests/test_fs_win_journal.c` shows the crash-phase recipe).

## Size

- POSIX test, one new file about 400 to 500 lines plus CMake: M. Two patches (test, then mutants and docs).
- Windows port: M. Each Windows red costs a CI cycle of about 10 minutes, and two of my recent Windows-side changes (m155 then m156, m159 then m160)
  needed a second round, so plan for 2 to 3 rounds.
- A finding in `src/fs_read.c` is likely to be possible: this is the first time damaged journals are tried.
  If one shows up, the rule is STOP and report, the repair is the main agent's and Antonio's choice,
  and it is not part of the size above (S to L depending on the finding).
- Total: M for POSIX alone, M to L with Windows and one repair.

## Predictions, written before anything is run (all unmeasured)

- Most mutations will be refused by the existing record validators (`replace_valid` and its siblings check
  magic, version, name shapes). I expect refusal, not silent OK, for single bit flips and truncations.
- The cases I think are most likely to find something: a record whose path field is changed to another valid
  in-workspace name (confinement holds, but the target differs), and a deleted commit marker with an intact
  intent (rollback versus roll-forward choice). I do not know that either fails; I name them because they are
  the places where a valid-looking record differs from what was journaled.
- Iteration cost is small (files of a few hundred bytes), so 400 iterations per journal kind per seed should
  fit well inside the current job times; not measured.

## Decisions for Antonio

1. Is journal fuzzing needed for the C5 declaration at all, or is "fixed crash points plus a foreign inode"
   enough? (This file only sizes it.)
2. If yes: the pass criterion, "fails closed or recovers to old or new" as above, or refusal only.
3. Seeds and iteration counts per journal kind (the path fuzz uses the default seed plus 1, 7, 12648430).
4. POSIX only first, or both platforms before C5 can be called shown.

## Not decided here

The M1 declaration and the C5 criterion text are Antonio's. Nothing here changes the current reading:
criterion 5 stays **partial**.
