# doc_sync automatic edit boundary, pre-code design

Base: aa99ecd2f7df9267dc9678e86a65052981451d96, inspected 15 October 2026.

Problem: TaskOps currently treats lexical A-in-docs/A-absent-in-C plus B-in-C
as sufficient to rewrite every occurrence of A in documentation. Compilation
cannot certify those replacements. Original old-code history is neither present
in TASK_OPS_WORKSPACE nor sufficient evidence of a specific A-to-B relation.
A historical API section can correctly retain an obsolete name.

Proposed safety behavior: doc_sync is discovery-only until a separately defined
verifiable documentation relation exists. When it would otherwise choose its
unique A/B plan, refuse that operator before writing any file, expose a stable
reason "doc_sync requires verified documentation relation", and allow the
existing solve lifecycle to finish without calling it verified. Other operators
are unchanged. Do not claim a global no-edit guarantee if a different operator
has valid independent preconditions; fixtures isolate the documentation path.

No history subprocess, network, git trust, caller hash/boolean, prose inference,
semantic receipt, target checker, automatic veto, or global operator disablelist.
No new production switch to override this boundary. A test-only fault hook may
re-enable the old behavior solely in the existing fault-enabled library target.

Coverage tradeoff: two current positive developer fixtures (real flag update
and real call signature update) cease resolving. Preserve their inputs and old
expected results as archived pre-guard evidence; document the loss explicitly.
Their active checks may assert the new refused contract, but cannot be counted
as unchanged positive efficacy. The ambiguous old fixture stays ambiguous.
This is a safety restriction, not a measured net efficacy improvement.

Gate before publication: frozen nine isolated fixtures in gold.json; exact
source and documentation bytes before/after, no published edit, explicit reason
on unique candidates, no guard-specific reason required for ambiguous/no-plan
cases. Add a test-only mutant reopening doc_sync and demonstrate at least the
misleading identifier fixture is edited as the original mechanism predicts.
Keep every original red and dependency failure. Full serial technical suite,
master-only apply-patch with expected HEAD and reviewed hash, final exact
readback and five successful jobs. No independent efficacy run authorized.

Relationship to the corpus goal: unsupported text updates must not be labeled
verified just because an unrelated domain evaluator (C compiler) succeeds.
This slice restores the abstention boundary; it does not supply a prose truth
checker or generalized document understanding.

## Implementation and technical evidence

The guard runs before doc_sync allocates replacement buffers or writes files.
Discovery is not promoted to edit authority. If another operator independently
resolves the task, that result is not suppressed; otherwise a discovered unique
doc_sync plan yields the stable refusal reason. There is no production override.

Nine frozen developer fixtures, not a blind efficacy bank. The fault-only
variant deliberately reopens doc_sync and fails exactly the first six unique
plan cases against the guard oracle, including both useful updates and the
misleading identifier. The last three no-plan cases stay unchanged. This proves
sensitivity to the restricted mechanism, not that all other operators are safe.
The two formerly successful fixtures' exact original inputs and expectations
are retained in doc-sync-prior-positive-tests.txt. Their active tests now check
the new refusal contract. They are explicitly lost positive coverage, not
positive safety gains or a net efficacy result. Gold is not relabeled after
execution. No history-enabled recovery of those useful cases is implemented.
