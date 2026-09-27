# Attempt-level capture (slice 6)

`SYMBOLS_ENGINEERING_EPISODES=1` together with `SYMBOLS_ATTEMPT_CAPTURE=/absolute/private/empty-directory` captures the `task_ops` source tree at the boundary of each attempt. The directory must be private (owner, mode 0700 on POSIX), outside the workspace, and created by the caller. It must not be published: captured source can be private. The source tree must fit 64 regular text files, 256 KiB per file and 4 MiB in total. Symlinks, hard links, binary files, `.git`, oversized paths, and unsupported tree shapes fail closed for *capture*, without changing the solver result. `.symbols` audit metadata is excluded from source comparison. The audit destination is not read by the solver to choose an operator.

Each attempt writes `run/NNN.pending/{before,after,manifest.v2}`, then publishes the complete pair as `run/NNN` with one directory rename. A crash before publication leaves `.pending`, which the validator refuses. A crash after publication leaves a complete pair, which must still validate. The manifest names the run, attempt, parent ordinal, outcome, SHA-256 of before and after. SHA-256 is over sorted relative file names, each prefixed with a 32-bit big-endian path length, followed by path bytes, a 64-bit big-endian file length, and the ASCII hex SHA-256 of file bytes. The validator recomputes hashes from the actual tree, requires a complete exact manifest and ordinal sequence, rejects extra run or pair entries, and requires previous after to equal next before. A missing, altered or interrupted pair is unavailable, not replayable. A capture failure emits a warning and does not change the solver report or source mutation.

The v1 engineering episode's FNV64 fingerprints remain audit hints, **not** source images. The private v2 snapshots neither reinterpret v1 nor make old rows replayable; they are also not a live decision memory. No replay performance or resolution gain is claimed. This slice captures `task_ops` only, not `agent_runner`, and does not lift the independent seal needed for any future measurement. Set O is spent and cannot be used as a fresh evaluation.

This is a process-crash publication boundary, not a power-loss guarantee: there is no cross-platform directory durability claim. A concurrent external writer may change workspace bytes during capture; this is not an atomic workspace transaction. The verifier checks the copied bytes and chain, not author intent. The caller must isolate workspaces for replay evidence. Abandoned private `.pending` directories need manual inspection/removal; they cannot be promoted by the validator.

On Windows, the tool uses the caller's existing access controls and does not
assert Unix-style 0700 ACL equivalence. An evaluator must provide a private
per-run directory with suitable Windows ACLs before enabling capture. This
module does not create that ACL or authenticate a path supplied in the
process environment.

An offline evaluator can call `attempt_capture_report ROOT RUN COUNT` to run
the same read-only v2 validation as the C tests. It does not reinterpret v1
fingerprints or promote a pending pair. Captures are private and never passed
to the solver for candidate selection.
