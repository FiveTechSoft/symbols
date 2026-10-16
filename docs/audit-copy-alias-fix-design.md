# Reserved source alias repair and two justified oracle corrections

Base f04616658a91d245658855a5f4179847b99b5845. This is a new bug slice following the preserved PASSsix 610 failures, not a green closure of that slice. No code changes yet. Parent accepted source diagnosis and ordered bug first, then two expectation corrections, combining if one verifiable change minimizes the red window.

## Evidence before code

610 native Release116989167658 and ASAN116989167942 both check out f046166 and print foreign_alias.copy_source=0e where the frozen contract requires3e. This actual native red is the pre-fix status reproduction. Its e observes the source, not the destination: destination absence is not proved by that log and will be asserted by the strengthened producer. No claim that a newly written test already ran before the repair. Proposed publication uses that existing native failure as the prior test evidence; no test-only re-dispatch is needed to reproduce a status already observed twice.

The intended restriction is documented in wc_final_reserved and the lock-alias fixture: public copy requests must not reach a reserved control file by its short alias. FsCopyFile checks textual wc_target_status but opens the source with generic win_open; wc_copy_source repeats that generic open. Neither passes wc_open's normalized final-name check. Generic read-only win_open must remain unchanged.

## Minimal implementation

At FsCopyFile's initial source-handle open and wc_copy_source's reopened current source handle, require wc_final_reserved(handle)==0. Reserved or unavailable normalized final name returns FS_READ_DENIED and goes through existing handle cleanup. Do not add a path reopen or broaden read APIs. Do not change source byte/identity checks, target gate, lock, journal, staging, recovery, or FS status enum. No exported API or new test-only hook. No claim of protection against all outside renames or all hard-link aliases: same-existing-capability limits remain.

Strengthen the existing native tests/test_fs_win_lock_alias.c instead of adding a Python wrapper or a new target. For its foreign_alias copy cell, assert DENIED, the source bytes remain empty, and inside/c does not exist. Also assert the existing foreign_long copy cell refuses without destination creation. Add fresh ordinary-file copy controls using real 8.3 aliases: empty and nonempty exact bytes both return OK and leave a byte-exact destination while source stays unchanged. Keep other 14 printed cells, names, working directory and fixture boundaries unchanged. Unique fixture, no parallel local Windows jobs. Reuse its existing 8.3 availability setup/restoration; missing support remains a failure, never skip. These assertions are normal exit status, no PASS regex.

## Two expectation corrections, independently derived

Only two semantic fields in windows_six_gate_gold.json change, including their frozen vector transcripts where appropriate:

* foreign_long.batch_replace3e ->1e. wb_path_ok explicitly returns INVALID for every batch path-validation refusal; wc_target_status rejects the literal reserved path before any lock/staging. INVALID=1, DENIED=3. Other 14 lock cells unchanged; foreign_alias.copy_source remains3e, never0e.
* batch sweep kills6/counts9 8 7 6 5 4 3 3 ->kills9/counts9 8 7 6 5 4 3 2 1 0 0. Writer crash23 has already made3oldpins+3newstages+3newpins. Each oldpin is still linked to its live old target, so has2links, not1. Sweep removes shared oldpins and all newstage/newpin names. All9 are eligible; each killed child removes1, the tenth finishes leaving0. CHAIN_MAX12 accommodates this. The old3remaining prediction confused these shared pins with unshared pins kept by design. Summarysuffix3/mode1,2,0=9/8 stays unchanged.

Do not broaden matching or accept alternatives, variable counts or either measured field. Keep strict unique complete final line+exit0, all six gates, both shell contracts and all other gold fields. Old pre-correction expected transcripts become explicit negative vectors; add foreign_alias.copy_source0e as an explicit regression negative. Correct frozen vector narratives only where a corrected complete-line literal is embedded. Runner and selftest implementation unchanged. The original published98-vector gold remains recoverable at f046166; record its SHA and the exact change, never imply it was always correct.

## Files and staged execution

Five changed paths total: docs/audit-copy-alias-fix-design.md (this design), tests/copy_alias_fix_gold.json (pre-code cases and source reasoning), src/fs_create_win.inc (two checks), tests/test_fs_win_lock_alias.c (assertions and ordinary controls), tests/windows_six_gate_gold.json (two corrected expectations plus vectors). CMake, build graph sidecar/generator/selector, generic win_open, other five native producers, FS core protocol, E/C/D/v2 and shell wrapping unchanged. New support JSON is a review artifact, not a runtime oracle dependency.

Parent design+gold review first; then exact support and code patches with independent SHA review. Support2paths -> raw readback -> code3paths expected support HEAD -> raw readback. Since status prior red is already native610, this proposal does not ask to introduce another test-only failing master. No dispatch until parent approves this prior-evidence interpretation and exact patches.

Validation: before implementation keep native610 logs and current source hashes. Post-code selftest must pass original98 vectors after exact semantic substitutions plus3new negatives,15 derived omissions and18 actual subprocess controls. Verify other four gates byte-identical and no removed vectors. Re-run10 private runner mutants, including dropped groups/OR/exit/substring/final/duplicate/snapshot arms; control green. Native regression check is the strengthened existing lock_alias test plus preserved foreign_alias3e oracle, ordinary copy controls both succeed. Source-local tests demonstrate removing the new guards restores the original bypass at native610; do not claim an unexecuted mutant was measured. If practical, run a dedicated native reversed-guard reproduction with a test-only compile target, but that is NOT included in this scope or required to weaken the oracle.

Full Linux build/test serial as feasible; no Charles compute. Final native MSVC Release+ASAN must each execute six wrapped gates and selftest PASS, no skip for them, full gate green, actual final checkout verified. Preserve every original red; fs_win_race only one unchanged repeat if it fails, reporting both. No repeat for these corrected gates. If any field disagrees after this source-derived repair, stop/report without tuning. No ownership or power-loss claims.

## Follow-up factual documentation, separate

After verified repair, explicitly correct docs/core-m1-exit-reading.md: its claims that the old regex pass proved all five refusals and6kills were unsupported because semicolon-separated alternatives accepted partial lines. Preserve historical predictions and label them refuted, cite610 actual raw logs and the source derivation, and report the new final verified evidence separately. This follows parent order and is not silently folded into the repair.

Sources pinned and byte-verified against current master:
https://raw.githubusercontent.com/FiveTechSoft/symbols/f04616658a91d245658855a5f4179847b99b5845/src/fs_create_win.inc
https://raw.githubusercontent.com/FiveTechSoft/symbols/f04616658a91d245658855a5f4179847b99b5845/src/fs_batch_win.inc
https://raw.githubusercontent.com/FiveTechSoft/symbols/f04616658a91d245658855a5f4179847b99b5845/src/fs_read.c
https://raw.githubusercontent.com/FiveTechSoft/symbols/f04616658a91d245658855a5f4179847b99b5845/tests/test_fs_win_lock_alias.c
https://raw.githubusercontent.com/FiveTechSoft/symbols/f04616658a91d245658855a5f4179847b99b5845/tests/test_fs_win_batch_replace.c
https://github.com/FiveTechSoft/symbols/commit/93c42bc128da1179c8a030922678f13dbba875b1/checks/116989167658/logs
https://github.com/FiveTechSoft/symbols/commit/93c42bc128da1179c8a030922678f13dbba875b1/checks/116989167942/logs
