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
