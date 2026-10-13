# Structured condition matching, not entailment

`ConditionMatch` is an opt-in, stateless C library call over caller-supplied requirements and claim assertions. It is not wired into CHAT, the server, retrieval acceptance or question parsing. `MATCH` means an assertion matches the supplied exact key and polarity, not that the text entails it or that the assertion is true. The pilot adds no semantic flag to citation receipts.

A requirement has version, subject, relation, polarity and a set of qualifier key/value pairs. Claims add an object, annotation origin (caller/extractor asserted), source index, SHA256 and half-open byte span. Source integrity checks validate bytes, not the meaning of annotations. A well-formed wrong annotation can MATCH. Spans need not be sentence boundaries; no source-to-claim extractor is included.

## Outcomes

- MATCH: an exact subject/relation/qualifier-set assertion of the requested polarity.
- REFUTED: an exact assertion of the opposite polarity. The returned object, when present, belongs to that opposite assertion.
- CONFLICT: both polarities or different objects on the same positive exact key. No object or claim index is returned.
- UNDETERMINED: no exact key. Absence is not negation.
- INVALID: malformed or oversized input, unsupported version, invalid UTF-8 identifier, repeated qualifier key, bad hash or span. All claims are validated before any match; a malformed unrelated claim invalidates the call.

Identifiers are exact UTF-8 bytes with no normalization, case folding, stems, aliasing or translation. Qualifier order is irrelevant but sets must match exactly: an unqualified claim cannot narrow, and a qualified claim cannot broaden. No temporal/range/unit/quantifier logic, dialogue inheritance, compound logical expressions or inference. Negative claim objects must be empty; positive ones must be nonempty. The caller owns valid memory and disjoint input/output arrays.

Representative evidence is selected by source hash, span and object ordering, not input order. Duplicate identical evidence can return a different index under a permutation; the same evidence content is the invariant, not array index identity. No allocation or persistent state in the call. Hashes are computed once per supplied source per call. Cost is bounded by the caps, not constant time: O(total source bytes + claims × qualifier pairs); validation includes pairwise duplicate-key checks.

## Resource contract

Maximums: 64 claims, 8 sources, 8 qualifiers per key, 63 bytes per nonempty identifier, 1 MiB per source and 4 MiB total source bytes. UTF-8 must fit within the byte cap. Non-NUL-terminated fixed arrays and ASCII control bytes are refused. Empty arrays are allowed and return UNDETERMINED for a valid requirement. Sources in a nonempty supplied list must be nonempty with valid memory. Over-limit calls refuse before dereferencing the over-limit list or hashing an over-limit source. These are explicit pilot bounds, not learned semantic rules.

## Fixed fixtures and gates

`tools/condition_match/gold.json` preserves the pre-implementation design gold SHA256 `9447e6a0052d3880f3f4a06391a25cac9bf8546b9aa959ffc5077c187c5cc804`: 23 cases, 15 manually annotated claims across four fictional corpora plus a conflict corpus. Reader questions are examples only; the API consumes structured requirements, not those questions. Truly absent conditions have no "not described" sentence. `tests/test_condition_match.c` transcribes these cases and source hashes/spans, then checks permutations, polarity/object conflicts, malformed UTF-8, bounds, stale hashes, invalid spans and fresh-call isolation. Qualifier, polarity and conflict-removal mutants are separate external measurement builds, not CTest targets.

The gold's `design_only` label records its creation stage and remains unchanged after implementation. It is not a claim that executing a matching test validates extraction. Caller-provided annotations are the inputs being matched, not proof of corpus understanding. A later parser/extractor needs its own design, unseen gold and validation. Existing CHAT output, status, state and citation policy are out of scope for changes here.
