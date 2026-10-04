# Journal record integrity field: sizing (not built)

Status: a sizing note only. Nothing in this file is implemented. Whether to build it is Antonio's decision
(it changes the on-disk record format and touches M1 criterion 5), taken after reading this.

## Why it is being sized

The journal fuzz ([core-m1-c5-journal-fuzz-proposal.md](core-m1-c5-journal-fuzz-proposal.md), tests in
`tests/test_fs_journal_fuzz.c`) found two shapes that the current records cannot detect, because no record
carries anything that proves its bytes are the bytes that were written:

- **F2 (remove):** a damaged remove record whose target field becomes another valid in-workspace name is
  restored under that name. Recovery returns OK, nothing is lost, nothing outside the workspace changes.
- **F4 (move):** a damaged move record whose source field becomes another valid name leaves that name and
  the real source as two links of one inode, with the destination removed. Recovery returns OK.

Both are pinned as asserted cells. The claim "fails closed or recovers to the old or new state" is NOT met
for these two shapes. A checksum would turn them into refusals.

## What exists today (read from `src/fs_read.c`)

Every POSIX journal record is a fixed-size struct that starts with `uint32_t magic, version` and has no
reserved bytes and no checksum: create intent (`FS_CREATE_INTENT`), remove, move, replace record
(`FS_REPLACE_RECORD`, 1160 bytes of fields) and its marker (`FS_REPLACE_MARK`, which embeds a copy of the
record), the batch and batch-replace records. Version is checked as exactly 1 in 8 places. Validators check
magic, version and name shapes only.

## What would change

1. Add a 32-bit checksum (CRC-32 is enough: the threat is accidental damage, not an attacker, and the
   records already say "not protection from hostile writes") over all preceding bytes of the record, stored
   as a last field. Record size grows by 8 bytes (4 for the value, 4 to keep the 8-byte alignment).
2. Writers compute it before the record is written and synced. No new fsync, no new file.
3. Validators recompute it. A mismatch is DENIED (journal left untouched), the same as a bad magic today.
4. For the replace marker, which embeds the record: checksum the whole marker, and keep the existing
   equality check of the embedded record against the intent.
5. Version becomes 2 for records that carry the field.

## Old-format journals

A journal written by a build that predates the field has version 1 and no checksum. Recovery must keep
accepting version 1 (an interrupted operation from the previous build must still roll back or forward),
without the check, and write only version 2. That means the first recovery after an upgrade has the old
guarantee; only journals created by the new build are protected. The existing accepted-in-flight-journal
precedent is the `.fsrp-` stage name sentence in `docs/core-m1-exit-reading.md`. A test needs a fixture
that writes a version 1 record by hand for each kind.

## Cost

- Source: one helper (about 20 lines), plus a field, a write-side call and a validator call per record
  kind. The POSIX kinds are replace, marker, create, remove, move, batch and batch-replace: seven places
  each side. Estimate S to M for POSIX.
- Tests: the fuzz oracle changes from "OK or refuse, and no extra name" to "refuse" for any damaged
  record that carries a checksum. The classes that stay valid (stage_decoy, temp_decoy, and the delete
  class) need their expected outcome re-read one by one. Estimate M with the version 1 compatibility cells.
- Windows: the Windows journals are separate code (`src/fs_batch_win.inc` and the Windows replace path).
  I have not read their record layouts for this note, so the Windows cost is not sized. Same field, same
  compatibility rule, plus one CI round at about 10 minutes each, 2 to 3 rounds as before.
- Risk: moderate. It changes the format that recovery reads, so a bug here can make a good journal
  unreadable. The compatibility cells and the existing crash-point tests are the guard.

## Pinned cells it would flip

- F2 (remove, crash 12, target field "renamed"): from "OK, file restored under renamed" to DENIED, journal
  and file left in place.
- F4 (move, crash 22, source field "rrc"): from "OK, rrc and src are one inode, dst gone" to DENIED.

Both pinned cells would have to be changed in the same patch, deliberately. The `known-limit-hits` counter
in the fuzz output would go to 0 and could then be made a failure.

## What it would not fix

- F3 (delete class, move crash 22: src and dst are the same inode after the journal is deleted). A deleted
  journal has no checksum to fail. Stays a limit.
- A damaged journal whose damage is the whole file replaced by another valid record. The checksum proves
  integrity, not freshness.
- Power-loss durability, other runners and non-cooperating writers: unchanged.

## Decision for Antonio

Build it (POSIX first, Windows after), or keep F2 and F4 as documented limits. The sizing does not decide.
