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
