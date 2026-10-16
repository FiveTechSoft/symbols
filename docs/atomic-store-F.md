# F: atomic image replacement, explicit publication and durability

Review base: 8f808befd39c0bcad5f512537095148b55b51529. This is design and independent gold only. No source patch is authorized by this file. Review the exact implementation patch separately before publication.

## Established defect and scope

src/atomic_store.c currently shares path.tmp between writers, opens it with wb, and checks fwrite/fflush/fclose before rename. POSIX has no fsync(file) or fsync(parent). Windows requests MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH. Atomic replacement, durable publication and read-modify-write isolation are different properties.

F replaces the shared temporary pathname with an exclusively created, per-operation sibling temporary file, adds POSIX file and directory synchronization, and makes publication status explicit. It keeps the Windows WRITE_THROUGH route. It does not change stored TSV/checksum bytes, solver decisions, engineering episode locking, model structures, or unrelated filesystem journals. General hostile-path traversal/symlink defense, transactions across files, and serializing whole episodic read-modify-write operations remain outside F.

## Result contract

Add ATOMIC_STORE_RESULT and AtomicStoreReplaceDetailed(path,data,len). Exactly three states:

- UNCHANGED: this invocation did not publish a replacement. It does not promise that another writer did not change the target, or that parent directories/temporary artifacts were not created. Pre-publication failures preserve the old target bytes, or leave an absent target absent, in the absence of another writer.
- COMMITTED_DURABILITY_UNKNOWN: rename/publication succeeded, but the required subsequent directory synchronization failed. The new complete image is visible now. Never pretend to restore the old image or report that nothing changed. A subsequent power loss has an unconfirmed outcome.
- DURABLE: the prescribed platform synchronization and replacement calls succeeded. This is a filesystem/OS API-level durability result, not a physical power-cut proof for every device, filesystem or network mount.

Keep AtomicStoreReplace's existing signature and binary interface. It returns 1 only for DURABLE; 0 includes both UNCHANGED and COMMITTED_DURABILITY_UNKNOWN. Correct include/atomic_store.h and README. Do not rely on enum truthiness. No struct layout changes. Zero-length images accept NULL data; NULL with nonzero length, invalid path and overlength input fail before publication. Retain existing accepted path bound, rather than expanding it silently.

The detailed operation retains the relevant OS failure information for debugging; do not manufacture a rollback. Cleanup is attempted for this invocation's unpublished temporary file only. A cleanup failure may leave its orphan, documented separately from target state. A crash can also leave an orphan. Do not sweep matching names belonging to another operation.

## POSIX sequence

1. Validate input and prepare missing parent directories. For every traversed directory, synchronize the directory containing its entry before proceeding (including existing entries, so a retry after interrupted/failed creation does not assume an unsynchronized ancestor is durable); failure remains UNCHANGED for the target. Handle relative leaf-only paths through parent '.', absolute leaf paths through '/', and nested paths. Failure to open or synchronize a required directory is not a success. Directory-entry synchronization is stated in the header/README; do not claim fsync(leaf directory) alone persists an unsynchronized newly created ancestor.
2. Open the actual destination parent directory before publication, with close-on-exec where available. Keep its descriptor through the final synchronization. Use an exclusive O_CREAT|O_EXCL sibling path with mode0666 under the active umask, never path.tmp or an unchecked fopen. For an existing destination, open/fstat it before replacement and attempt fchmod of the temp to the prior mode before rename. fchmod is best effort: failure is not a persistence failure. Ownership/ACL preservation is not promised. This follows the explicit permission decision; no silent0600 change.
3. Write all bytes with checked short-write/error handling. If a FILE stream is used, check fflush, fsync(fileno(stream)), and fclose in that order, before rename. For a descriptor implementation, use a checked write loop, then fsync(file), then checked close; no fflush is needed without buffering. Retry interrupted writes/fsyncs as appropriate, never close the same descriptor twice after a close error.
4. Rename this unique sibling over the target. Any write, file-sync, close, or rename failure is UNCHANGED. Remove only this operation's unpublished temporary file. A directory target or unavailable destination fails without truncating the prior target.
5. After successful rename, fsync the retained parent descriptor. If it fails, return COMMITTED_DURABILITY_UNKNOWN and leave the new target visible. Do not remove the destination or attempt another rename as rollback. If it succeeds, return DURABLE. Close parent descriptors on all paths. A close-only error after successful directory fsync does not reverse publication or invalidate an already successful fsync.

Calls require a stable/trusted parent namespace during the operation; this change is not a defense against a malicious actor renaming ancestor directories. Filesystems that reject required directory fsync fail honestly rather than falling back to 'durable'.

## Windows sequence

Create a unique sibling with CreateFileA(CREATE_NEW), using a bounded retry loop for name collisions. A PID/tick/counter candidate can be predictable; correctness comes from CREATE_NEW, not name secrecy. Never open a preexisting candidate. Keep the returned handle, perform checked writes and FlushFileBuffers, and check CloseHandle before publication. Retain MoveFileExA(REPLACE_EXISTING | WRITE_THROUGH). Failures before successful MoveFileEx are UNCHANGED; success is DURABLE under this Windows API contract. No unsupported POSIX-equivalent directory durability is claimed. Do not regress overwrite-existing behavior, paths with spaces, zero-length images, or native MSVC/ASAN cleanup.

## Callers and outward reporting

WriteStoreAtomic becomes a detailed private writer. Add episodic Ex variants for append/forget/save/clear with an optional out ATOMIC_STORE_RESULT; legacy public signatures delegate and preserve their existing 0/1/2 result conventions. Invalid/no-op/auto_save=0 paths are specified explicitly: the out status is UNCHANGED when no disk publication was attempted, while the legacy return continues to describe the operation (for example duplicate=2, append with auto_save=0 succeeds in RAM). Never infer disk publication from the legacy return alone.

Append and forget restore prior RAM only for UNCHANGED persistence failure. Clear keeps prior RAM only for UNCHANGED; if the empty image was published, clear RAM even when durability is unknown. A post-publication unknown still returns legacy failure, not a false durability success.

ChatLearnTriple, ChatForgetTriple and ChatEpisodicClear use the detailed status. When a new image was published, synchronize/scrub the live session to match the current episodic state even if durability is unknown, then preserve their legacy failure result. For a pre-publication failure leave the session unchanged. Use no new persistent field or graph algorithm. Update include/episodic_memory.h, include/chat.h and comments that currently promise 'nothing changed' for every failure.

Server and chat CLI failure strings must not claim the fact/store is intact when publication may have occurred. Conservative wording: 'No se confirmo la persistencia; el estado puede haber cambiado.' Apply it to learn/forget/clear failure branches without changing success or absent/duplicate meanings. Engineering EpisodeAppend keeps its whole read/validate/rewrite lock and legacy return; its header and agent_runner/task_ops warning say durability was not confirmed, rather than implying the audit record is definitely absent. The boolean return cannot prove absence after publication.

Other callers in server_proto use current in-memory records; no new all-or-nothing transaction across multiple store operations is promised. Existing ignored-return/multi-step semantics remain a documented limitation, not something this file certifies as durable.

## Tests and independent gold

GOLD.json is authored from the contract before implementation. Tests must verify bytes and state, not only printed PASS text. Test-only fault injection is compiled into dedicated private test targets, never the production library and never an environment-controlled production failpoint. Link the real atomic/episodic implementations in the private targets so the negative proves the actual state transitions and caller paths. A separate chat integration target can exercise the live-session paths without production failpoints.

Inject failure at temp create, write, flush (if stream), file fsync/FlushFileBuffers, file close, rename, and parent fsync after rename. Track the actual OS-operation order in the test adapter; do not derive expected state from the implementation's returned enum. Include a failure before and after publication with an absent target as well as an existing image. Verify failure propagation and RAM/disk/session agreement, particularly the required fsync(parent)-after-rename negative.

The existing test_atomic_store and test_engineering_episode tests create path.tmp to force failure; those mechanics become obsolete. Preserve their no-truncation oracles through explicit failure injection, and add an inverse test proving a preexisting path.tmp directory/symlink/file remains untouched while replacement succeeds through its own unique temp. Update test_episodic_memory's leftover assertion to inspect operation-owned artifacts, not one historical filename.

Two concurrent writers need separate temporary files and complete final bytes equal to one writer's full image, never a mixture. Force both to pause after exclusive temp creation, release in controlled order, and prove one operation's cleanup cannot remove the other's temp. This is last-writer-wins replacement, not lost-update protection. Keep engineering whole-cycle lock tests.

Mutation negatives must reject: skipped file fsync, skipped parent fsync, parent failure collapsed to DURABLE/UNCHANGED, rollback of RAM after committed unknown, use of path.tmp, nonexclusive candidate open, cleaning another operation's temp, loss of WRITE_THROUGH, and a wrapper treating enum truthiness as success. Normal target tests, private fault tests, build-graph inputs and native CI must pass. Assert-based tests keep assertions enabled in Release; CTest must enforce exit status without a PASS regex that can mask nonzero.

## Gates and review boundary

Required: full Linux build/tests, targeted atomic/episodic/engineering/session tests, independent private fault matrix, concurrency checks, native MSVC Release + MSVC ASAN replacement/caller tests, and exact readback on publication. POSIX fault injection and operation trace prove that the API sequence and error paths are exercised; they are not a real power-cut experiment. Native Windows is mandatory, not simulated by POSIX tests.

Next approval is this design+gold, including the explicit approved0666/umask and best-effort mode-preservation behavior and bounded downstream caller changes. Then prepare a separate exact patch and evidence for review. No source mutation, commit, CI dispatch, or new publication is part of this design handoff.
