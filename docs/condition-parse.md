# Opt-in controlled prose extraction

`ConditionParseQuestion` and `ConditionExtractClaims` accept explicit ASCII controlled grammar, separate from CHAT and server routing. They do not promise arbitrary natural-language understanding, scientific truth or general entailment. Runtime inputs are source/question bytes, not the expected annotations in the offline gold. Extraction origin remains `extractor_asserted`; `ConditionMatch` still matches assertions, not their truth.

Supported forms, exactly:

```text
What does nacre records during phase=dawn?
What does nacre records during phase=dawn and zone=north?
nacre records amber during phase=dawn.
nacre does not records during phase=night.
```

The claim forms also allow two qualifiers joined with ` and `. This is deliberately structured prose; the uninflected relation in `does ... records` is an opaque ID, not English morphology understanding. No free-text paraphrase, natural multilingual question, multiword names, aliases, tense, ranges, quantifiers, coreference or context inference. No default activation or new semantic citation flag.

IDs begin with a lower-case ASCII letter then lower-case letters/digits/underscore, at most 63 bytes. `what`, `does`, `not`, `during`, `and` are reserved. Exactly one space separates grammar components; no leading/trailing whitespace. Questions end with `?`, claims with `.`, with no extra suffix. Claim lines may end LF or CRLF, including blank lines. Bare final CR is rejected; a final nonblank line need not have a newline. Every nonblank line must parse: malformed later lines reject all extraction, no partial output. The key/value marker makes boundaries explicit, not guessed from arbitrary natural prepositions.

The parser supports one or two unique qualifier keys; duplicate keys reject even identical values. Question polarity is positive only. Negative claims have empty objects. Contradictory claims are retained for the separate matcher to report CONFLICT. Absent claims do not imply negation. Parsing success certifies this syntax only; broader qualifier semantics are not inferred.

## Byte evidence, limits and outputs

Claims refer to caller source index 0, SHA256 of its complete original image, and half-open byte spans of each nonblank original line excluding LF/CRLF. No normalization or sentence-boundary claim. Source integrity authenticates bytes, not the semantic correctness of an assertion. No file loading, paths, tools or execution from source content. Inputs are untrusted data.

Caps: 512-byte question, 1024-byte nonblank line, 64 KiB source, 64 claims, 63-byte IDs. With this grammar and two qualifiers, the syntactically valid maximum line/question is smaller than its byte cap. The 512/1024 boundary fixtures are in-cap but grammar-invalid, not falsely called successful boundary parsing. 63-byte IDs, 64 claims and 64 KiB source have successful boundary fixtures. 64 KiB is exercised with blank lines, not near-limit meaningful prose.

Caller supplies valid disjoint memory and an output array of at most 64 claims. Rejection sets count to zero; outputs within an allowed supplied capacity are cleared. A capacity above 64 is refused before writing to that array (its count is zero, but the array is untouched). A null count also refuses. Publication is all-or-nothing after validation. No allocation or persistent state; a bounded temporary claim array is used. An empty source returns zero claims; null source is permitted only with length zero. No OOM injection claim.

## Original oracle and gates

`tools/condition_parse/gold-v2.json` preserves the 32 pre-code cases and source hashes, SHA256 `8abb210c3790b3cc0b45cbe28d5b51ab9346f195217528951d932b71f575d43e`. Its original stage labels remain unchanged. The earlier 22-case design gold existed before the ten boundary cases; neither was tuned from engine outputs. Source fixture bytes are embedded/transcribed in `tests/test_condition_parse.c`; large boundary images are generated there by the fixed pre-code recipes, not stored as standalone corpus files. Gold file names identify oracle sources, not files the repository claims to ship at those names.

The CTest checks parser-only fields, extractor-only fields/polarity/origin/SHA/spans (including CRLF), end-to-end structured matching for positive/negative/absent/conflicting assertions, ordered/permuted qualifiers and claims, malformed suffixes/second lines/NUL/duplicates, boundaries/capacity/null outputs and empty-call isolation. It does not treat rejected free grammar as a semantic safety success. External mutation builds remove polarity, qualifier values, full-line anchoring or duplicate-key checks; they are not CTest targets. Tests of pre-supplied annotations and tests of extraction are separate.

One strict local compilation initially refused a misleading-indentation line (`if ...; memset ...`). The two statements were separated with no logical or gold change before test execution. That development error is recorded, not a production defect claim.

A next free-natural-language design needs new independent data and its own ambiguity/meaning contract. This pilot must not be relabeled as that capability. Existing CHAT behavior/citation policy is unchanged.
