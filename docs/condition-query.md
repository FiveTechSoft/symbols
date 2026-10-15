# Controlled assertion query composition

Base: master 01a45609fa6e7b7538219e782f2a755612c8f2a8. Implementation: include/condition_query.h and src/condition_query.c. Technical fixtures: tests/test_condition_query.c and tests/condition_query_gold.json.

## Purpose and API

New opt-in read-only C API in separate condition_query.h/.c. No CHAT, routing,
retrieval, fallback, corpus ingestion, persistence or episodic changes.
Function ConditionQuery(question bytes/length, source bytes/length,
expected_source_sha256[32], output). Inputs and output are caller-owned valid,
disjoint memory. Source must remain immutable during the call. The expected
hash is a mandatory byte-image pin supplied by the caller, not a truth claim.
The API recomputes the hash and rejects a mismatch before matching.

Use the existing exact ASCII question and claim grammar without extensions.
No caller-supplied CM_CLAIM annotations accepted. Parse the whole source into
private temporary claims before matching. One source only, no allocation and
no persistent state. Reuse current parser/matcher semantics and limits.

Output version 1 and status: INVALID, ASSERTION_MATCH, ASSERTION_REFUTED,
CONFLICT, UNKNOWN. Status names explicitly describe assertions, not entailment.
For MATCH only: object, origin=extractor_asserted, source SHA, first/end span.
For REFUTED: negative assertion evidence (same provenance fields), empty object.
CONFLICT/UNKNOWN/INVALID: empty object and no evidence (hash/span/origin zero).
No claim index or borrowed pointer escapes. No textual answer rendering.
A receipt establishes source bytes for a parsed assertion, not source truth.

Output is fully cleared before validation, with only version/status assigned.
A NULL output yields INVALID. All later failures leave no partial success.
Callers must not alias input/output or concurrently mutate source memory.

## Exact decisions

- Question size 1..512 bytes excluding any final NUL; entire question consumed.
- Source size 1..65536 bytes; NULL source, empty source or missing pin INVALID.
- At most 64 nonblank claim lines, each at most 1024 bytes excluding LF/CRLF.
- Blank lines are allowed; a nonempty all-blank source yields UNKNOWN.
- Parser accepts one or two distinct qualifiers; their order is irrelevant.
- Every nonblank line must parse, including unrelated late lines. Otherwise
  INVALID even if a matching assertion was seen earlier.
- Same exact subject/relation/complete qualifier set: one positive object gives
  ASSERTION_MATCH; negative-only gives ASSERTION_REFUTED; distinct positive
  objects or positive plus negative gives CONFLICT; none gives UNKNOWN.
- Extra/missing qualifier or different value is not a partial match.
- Repeated identical positives do not conflict. Evidence is the matcher's
  deterministic minimum SHA/span/object. For a single source this picks the
  earliest qualifying line. Permuting source lines changes SHA/spans, not the
  semantic assertion result. Permuting qualifier pairs changes neither key.
- Terminal LF, CRLF and no terminal newline retain current parser semantics.
  Span excludes LF and any paired CR; source SHA covers all original bytes.
- Unsupported syntax, non-ASCII tokens, NUL, duplicate qualifier key, third
  qualifier, trailing question text or standalone terminal CR are INVALID.

## Gold and technical gate

tests/condition_query_gold.json freezes four invented corpora with opaque identifiers, explicit
question/source bytes, required pin and expected statuses/objects/spans/SHA.
These labels are grammar-contract expectations, not natural-language gold.
No implementation output was used to derive them. Bad-input scenarios remain
red if the existing dependencies behave differently; do not edit gold to green.

Boundary scenarios additionally specify exact construction formulas in gold:
source 65536/65537, identifier 63/64, 64/65 claims, 512/513 question bytes,
1024/1025 line bytes, zero/NULL arguments and stale pin. At the question/line
maxima padded input is deliberately syntactically invalid; those tests do not
claim a valid 512-byte question or 1024-byte grammar sentence exists.

Repeat calls in one process: MATCH -> INVALID -> CONFLICT -> UNKNOWN -> MATCH,
with sentinel-filled output each time. Compare every serialized defined field
with its fresh-call result; no partial object/evidence from earlier calls.
Verify source/question buffers unchanged. Technical tests only, not efficacy.

Legacy gate: no CHAT source/header/callsite edits. Rebuild and run existing
technical suite; compare existing fixed CHAT probe outputs and states before/
after with the same inputs/build configuration. Byte identity is scoped to
those probes, not a universal proof over all possible CHAT inputs. Publication
requires parent approval of this design/gold, expected master HEAD, reviewed
patch hash and final CI 5/5 verified programmatically. No efficacy run here.

## Excluded

General NL, aliases, multilingual, multiword identifiers, ranges, units,
quantifiers, coreference, domain reasoning, semantic truth, causal extraction,
automatic veto, evidence reuse across changed bytes, and hidden target oracle.
