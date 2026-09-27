# C stdout contract: free-form parsing boundary

Status: negative design finding, September 27, 2026. Base: `2b55fa88cf3622d3bfde427969d553b7374a2800`. This note contains no product code. The experimental implementation was discarded; do not treat any of its local measurements as a promotion gate or a result on a sealed set.

## Intended slice

The proposed `c_contract` slice would bind an asserted stdout value to the original task clause, preserve exact bytes where specified (including empty stdout and a final newline), and enforce checkable edit restrictions before candidate execution and after the final write. It was meant to replace the old last-token inference, not to widen edits on the strength of a successful stdout run alone. An earlier design was reviewed separately; no code from this attempt is recommended for dispatch.

The safety condition is conjunctive: a candidate must meet the output claim *and* all operative source restrictions. A stdout-only oracle cannot establish that a source-preservation or ordering clause was satisfied. Unknown clauses cannot quietly become permission to edit.

## Three parsing routes tested

| Route | Observation | Why it cannot support promotion |
| --- | --- | --- |
| Ignore unknown clauses once a desired stdout clause is found | The permissive prototype accepted a task that required `main.c` to stay as it was while extracting the separate `OK` stdout goal. A later lexical patch recognized that exact wording, but the problem remained open-ended. | A correct run can coexist with an edit that violates an unrepresented instruction. The parser would have to prove the skipped clause was non-prescriptive, not merely fail to spot a familiar restriction word. |
| Strict structural whitelist for *every* clause | The local rewrite admitted bounded standalone goal forms and explicit basename restrictions, and refused everything else. Its development-bank result was 0/20 resolved and 0 wrong edits; no files were changed. It also rejected descriptive task context and the corpus's bracketed development banner ahead of an explicit stdout claim. | This is safe as an abstaining parser but does not deliver the intended free-form capability. Making exceptions for a development banner or specific known task phrasing would contaminate the measurement and leave ordinary context unresolved. |
| Lexical markers to distinguish context from restrictions | The intermediate parser used words such as `keep`, `leave`, `change`, and `only`, plus targeted negative and newline checks. Adversarial neighbors exposed missed preservation wording, contradictory termination, and conditional output clauses; each new check only moved the boundary. | A marker's absence is not evidence that the clause is non-prescriptive. This is an unbounded blacklist, not a finite structural account of what may safely be ignored. |

The unresolved issue is not a missing synonym. Before accepting a partial parse, the system needs a defensible way to classify each remaining clause as non-prescriptive context or a supported, verified constraint. These experiments did not establish one. No independent sealed Set R was run; Sets P and Q were not used for this iteration.

## Local evidence and limits

The historical development baseline and the earlier permissive prototype each resolved `dv_001`, `dv_004`, and `dv_017` (3/20) with zero recorded wrong edits. The strict whitelist probe, `/home/sandbox/gate/dv-narrow-probe2.json` (SHA-256 `d157964ffe38b7cfa0971ed907730f7d74f4e04cfe1ec4daca2cce497a3fb4e9`), resolved 0/20, recorded zero wrong edits, and left all 20 workspaces untouched. It is a local diagnostic, not a release result. The earlier permissive local file is `/home/sandbox/gate/dv-contract-prelim4.json`; its three positives do not prove the new contract semantics.

Near-miss abstention is not claimed as a fully verified regression gate. A subsequent spot-check of the development manifest's `dimension` field identifies `dv_014`, `dv_015`, and `dv_016` as `near_miss`; all three had no changed files in the strict-whitelist result. That checks the manifest label and local no-edit observation, but no independent complete checker review, final full test matrix, or staged-diff validation was performed. An earlier informal list of four different IDs was mistaken and must not be used as a near-miss count.

`dv_004` is a separate contract conflict. The task explicitly requires wrapping before clamping, then states stdout `5`. The old positive came from matching the output; it did not separately verify the call-order requirement. Treating that output as sufficient would preserve a lucky result while claiming a capability that was not proved. A fail-closed parser should abstain until call order is mechanically checkable. That predicate is deferred to a separate future slice, not a synonym to add here.

The legacy `test_task_ops` reflection-memory fixture also depended on a wrong-file trial being applied, rejected, and recorded. The prototype's pretrial file filter suppressed that trial, so the fixture failed. This is the intended direction for safety, but the fixture was not rewritten and the suite did not pass. The reflection path writes only after a candidate is applied and refuted; suppressing the trial means no reflection to recall. Per-attempt audit/capture can still record an abstention when enabled, but no downstream equivalence was proved for the experimental implementation. Do not change the fixture just to make the test green.

## Recommendation

Close this free-form stdout extraction route for now. Keep the conservative existing guard rather than promote a parser that either drops unknown clauses or cannot distinguish context from obligations. A separate typed contract could make the stdout bytes, termination rule, edit allow/deny scope, and any independently checkable source predicates explicit. That would need its own user-facing authority boundary, compatibility tests for the existing typed one-line continuation, full regression matrix, and a new sealed evaluation. The direction choice belongs to the owner; this note only establishes why this attempted slice did not pass.
