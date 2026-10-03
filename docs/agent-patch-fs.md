# AgentPatch on the Fs* API (M1-3, patch 1 of 2)

Scope: the write paths of `src/agent_patch.c` that mutate the user's working
tree. Patch 2 covers `src/swe_bench_harness.c`. Runner reads, internal stores
(`atomic_store.c`, `reflect.c`) and data/model/log files are out of scope by the
write-path criterion.

Behavior
- Reads use `FsReadFile`; apply and rollback use `FsReplaceFile` with the exact
  expected bytes. There is no `fopen` fallback.
- Root is the plan workspace (`PatchPlanSetWorkspace`, default `.`; the runner
  passes `workspace_dir`). A target keeps its historical meaning (relative to the
  current directory, or absolute) and must lie beneath the workspace; anything
  else, including `..` escapes, is rejected.
- Drift: if the file changed between the read and the replace, apply is DENIED
  and the file is not touched. Rollback restores only if the file still holds
  exactly what the plan wrote; otherwise it refuses and keeps the user's bytes.
- `FS_READ_PENDING` is never success: recovery runs and the on-disk bytes decide.
  `PatchRecover` clears a pending transaction left by a crash.
- Limits: 1 MiB per file (`FS_READ_MAX`), refused explicitly. Symlink and
  hard-linked targets are DENIED. Windows requires NTFS; other volumes fail
  closed. The inode changes on replace; the permission bits are kept.
- Control files `.fstxn.lock`, `.fstxn-*`, `.fsrp-*` (and `.fsrb-*`, `.fsmv-*`,
  `.fsrm-*`, `.fs*-stage`) appear in the workspace root. They are internal.
  `AgentGitInspect` excludes them from untracked/ignored counts. The user's
  `.gitignore` and `.git/info/exclude` are never edited.

Incident and rule (6a5ad6c)
- The apply workflow ran the tests and committed the 33 `.fstxn.lock` files they
  created in fixture workspaces. A checkout gives them mode 0644, and Fs* refuses
  a lock with group/other bits (fail closed), so every patch in those workspaces
  was DENIED and `test_agent_runner_heldout` / `_external` failed on Linux CI.
- Control files are never committed. `.fstxn.lock` is in `.gitignore`.
  The `AgentGitInspect` exclusion only keeps them out of the clean-tree check;
  it is not permission to track them.
- A lock with an unsafe mode makes apply fail closed (`test_agent_patch_fs` T9).
- Known limitation: `FS_READ_DENIED` covers drift, links, a pending transaction
  and an unsafe lock; `io_diag` cannot tell them apart. Fixing that needs Fs* to
  expose the reason and is out of scope here.
- Correction (follow-up): `.gitignore` does not protect files that are already
  tracked, and the apply workflow runs ctest before committing, so a test that
  recreates a tracked lock cancels its deletion. The held-out and external
  runner tests now remove `.fstxn.lock` from each versioned fixture directory
  before and after every case, and the 33 tracked locks are deleted. Tests
  must leave no control file in versioned fixtures.

SWE-bench harness (M1-3, patch 2 of 2)
- `SweBenchHarnessRun` seeds its benchmark file with `FsCreateFile` (create-only)
  and removes it with `FsRemoveFile` (compare-and-remove with the bytes read just
  before). Root is the harness workspace; the target must lie beneath it
  (`PatchResolveTarget`), otherwise the task is skipped and counted as failed.
- Behavior change: an existing file at the target path is never overwritten or
  removed (before, `fopen "wb"` truncated it and `remove` deleted it, which
  could destroy user code). Such a task is counted as failed and a line is
  written to stderr. Only a file this run created is removed.
- Parent directories are still created with `mkdir` (Fs* has no directory
  creation); directories are left in place, as before.
- Symlink targets are refused; Windows needs NTFS and otherwise fails closed.

Multi-file replace (M1-2, `FsBatchReplace`)
- Scope of the M1 criterion "interrupted multi-file mutations either commit fully
  or restore the original byte-for-byte state", as delivered: multi-file create
  (`FsBatchCreate`, earlier) and multi-file replace (`FsBatchReplace`). Mixed
  operations (move, remove, copy together with create or replace) are NOT covered
  and stay open as M1-2b. `AgentPatch` does not call `FsBatchReplace` yet; one
  plan is still one file.
- API: 2..8 distinct existing regular files, each with its byte-exact expected
  image and its replacement. Each target must have exactly one hard link.
- Protocol under the workspace lock: verify every expected image, pin every old
  inode with a hard link (`.fsrp-<nonce>`), stage every new image durably, write a
  durable journal (`.fstxn.batch`, record version 2), publish one rename per
  target, sync, write the commit marker (`.fstxn.commit`), then clean up.
- Crash or in-process failure before the marker: every target is restored from its
  pinned old inode (original bytes, original inode). After the marker: every
  target keeps the new bytes. In-process failures roll back immediately and report
  the original error; a crash needs `FsBatchRecover`.
- It reuses the create batch journal name, so a pending batch of either kind
  blocks every other writer until `FsBatchRecover`.
- In-process rollback for every writer (patches P1 and P2): create, copy, remove,
  move, replace and batch create also undo their own journal when a step fails
  before the commit, using the same code as crash replay (create removes only its
  own byte-identical intent and stage). The original error is returned and no
  recovery call is needed. If the rollback itself fails the journal stays, the
  call returns `FS_READ_IO` and later writers are refused until recovery. A
  failure after a name became visible and the source was removed (move, remove)
  or after the new target was published (replace) still reports IO or PENDING and
  needs the explicit recovery call.
- Recovery validates every target, stage and rollback name before changing any;
  a target holding neither the old nor the new inode (foreign bytes) makes
  recovery fail closed with nothing changed.
- Limits and non-claims: the protocol above is the POSIX one. Windows (NTFS only)
  has its own implementation of batch create, replace and recover in
  `src/fs_batch_win.inc` (hard-link pins, rename-over, marker, recovery); a
  foreign open handle on a target makes the rename fail and the call returns
  `FS_READ_PENDING` after rolling back what it published. Its evidence and
  non-claims are in `docs/core-m0-m1-exit-audit.md` (2026-10-03 update); other
  volumes fail closed. Cooperating writers only. Not atomic
  visibility to readers and not a power-loss guarantee. A crash between journal
  retirement and marker retirement leaves a marker-only state that fails closed
  for manual inspection (same as create). 1 MiB per file. Inodes change.
- Test: `tests/test_fs_batch_replace.c` (fork+kill at steps 60 journal, 61
  partial publish, 62 all published without marker, 63 committed with cleanup
  pending; recovery interrupted at 65 and 66; foreign target; malformed journal;
  hard-linked and drifted targets; in-process publish failure) with byte and mode
  comparison after every case. The re-check of the target identity just before
  each rename is defensive and not exercised by a test.
