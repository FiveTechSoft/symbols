# Engineering episode audit, v1

Set `SYMBOLS_ENGINEERING_EPISODES=1` to write task-ops shadow records to
`<workspace>/.symbols/engineering_episodes.v1`. The default is off. The
standalone `engineering_episode_report` reads that store; the solver never
reads it. Neither candidate order nor task-ops memory, reflections, or trace
semantics change. A failed append warns on stderr and does not change the
solve result. A missing record means incomplete audit coverage, not zero
attempts. The store and source edits are not one transaction.

A row is emitted after each outer task-ops attempt. Its goal provenance is
`task_text_unverified`: a mechanically verified edit is not proof that the
user's intended goal was correct. Workspace and task fields are labelled FNV64
fingerprints, not cryptographic hashes. Repository SHA and patch hash are
unavailable in this slice; the oracle is unversioned. Candidate-build, probe,
and tool-call counters are serialized as zero because the current API does not
count them: do not interpret zero as observed absence. No raw task or output
text is stored. The typed stdout continuation is audit-off, including its
internal scratch preflight.

For a negative attempt, the audit rereads source bytes after the solver's
rollback. Matching loaded bytes support `confirmed`; mismatch or failed load
receives `verification_incomplete` and `unknown`, never a confident refutation.
The existing solver does not check each rollback write/remove return value,
so a source readback cannot distinguish a failed rollback call that happened
to leave matching bytes from a successful one. Slice-2 tests exercise an
actual refuted/restored attempt, opt-in equivalence, corrupt-store refusal,
and a deterministic audit-lock refusal without changing solve results. They do
**not** induce disk-full or rollback-write failure; those two injection gates
remain unexercised. They must be added before claiming coverage of those
failure modes. Similarly, the store is historical; a verified row does not
establish that the workspace is unchanged now.

## Runner shadow emission (slice 3)

The same opt-in switch writes `agent_runner` rows at the outer applied-attempt
boundary. A build/test pass is recorded after the target file can be reread;
its provenance remains `task_text_unverified`. A failed build/test is recorded
as `refuted` only after `PatchRollback` succeeds and a readback matches the
pre-attempt target bytes. A failed rollback call is labelled `rollback_failed`
with rollback `failed`; successful rollback with unreadable/mismatched bytes is
`verification_incomplete` with rollback `unknown`. A failed `PatchApplyAtomic`
is not counted as an applied attempt by the
existing runner, so no shadow row is emitted there; if it writes partial bytes
without setting `is_applied`, this slice cannot assert restored source.
No raw diagnostic, command, task, or source bytes are written to the store.
The target-file FNV64 fingerprints are **not** complete workspace snapshots;
other files mutated by commands are outside this audit. This slice does not
inject patch-apply, verification, rollback-write or audit-write faults at every
boundary; a deterministic audit-lock refusal is tested, but rollback failure
labels are not exercised with a forced failed `PatchRollback`. Those cases
remain a known untested gate, not evidence of recovery. The old runner result
and its planner's decision inputs are unchanged. `test_agent_runner` had a
flaky Windows MSVC attempt-count assertion in slice 2: the first run failed
39/40, and an unchanged rerun passed. It is not modified here.

## Why the legacy trace remains

The episode store is not a replacement for `SYMBOLS_TRACE`. The trace records
pre-edit compile/run observations, a masked first compiler diagnostic, and an
operator-specific detail field that the v1 episode schema does not carry.
`tools/mutation_corpus.py` still reads trace rows for `relop_search` operator
labels used by the induction evaluation. Suppressing trace emission would lose
those features and change that consumer's labeled data. Both opt-in streams
remain available; neither is a solver decision input by virtue of this audit.
