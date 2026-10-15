# Evidence metrics snapshot, pre-code contract

Base: 1ae65742e420e1c464452f6bc7429059b8529e37. Design only.

Deliverables: tools/metrics_snapshot.py pure renderer, versioned input
 tools/metrics/current.json, tests/test_metrics_snapshot.py and frozen fixtures.
No solver, build, subprocess, network, server or measurement execution in renderer.
The renderer validates supplied evidence structure; it cannot authenticate a URL
or prove a claimed measurement occurred. Curating the production snapshot needs
independent source checks and parent review. Hashes bind bytes, not truth.

Schema v1, exact keys:
root {version:1, subject_sha:40 lowercase hex, entries:[entry...]}
entry {id, title, claim_type, measured_sha, date, environment, source_url, value}
claim_type in technical_result, developer_contract, historical_efficacy, unknown.
IDs [a-z][a-z0-9_]{0,63}, unique. title/environment bounded printable ASCII with
no pipes/backticks/HTML/newlines, to avoid Markdown injection. ISO date parsed
as a real date. URL absolute HTTPS, host github.com, repository FiveTechSoft/
symbols and path actions/runs/<digits> or commit/<40hex>; no credentials, port,
query, fragment or arbitrary suffix. URL form validity is not content validity.
measured_sha40hex mandatory except unknown(null). value is bounded printable
ASCII text, or null only for unknown. Finite scalar numerical values are NOT
accepted: metrics are explicit source-described statements, never arithmetic.
Reject duplicate JSON keys, unknown keys/types, bool as integer, oversized input,
missing values, duplicate IDs, unknown claim types, NaN/Infinity and invalid UTF8.
At least1entry,at most32. Input max64KiB.

Freshness: label every row with measured_sha and claim type. technical_result
and developer_contract may reference older SHA but render as historical technical
or historical developer when unequal to subject_sha. historical_efficacy ALWAYS
renders Historical efficacy, even when equal. unknown renders Unmeasured on
subject SHA. Never infer current results from a previous commit. No net-gain
math, threshold claim or phase declaration.

CLI: --snapshot FILE [--readme FILE --write]. Default print table only. --write
requires --readme, validates all input and exactly one ordered standalone pair
of METRICS markers before any write. Preserve all bytes outside interior marker
region, including BOM/newlines. Render interior with detected marker-line LF or
CRLF; reject mixed marker-line styles, missing/duplicate/reversed markers and
NUL README. Atomic temporary-file replace, permissions preserved. On any failure
original README bytes unchanged, no history CSV appended. Reject symlink target
and nonregular file; caller provides a local trusted workspace without hostile
concurrent writers. No broad filesystem safety or crash-durability claim.

metrics.py --write must exit nonzero before its measurement functions run,
with clear migration message. metrics.py without --write stays legacy measurement
on explicit request, not invoked by this work. Its --ci semantics unchanged.
Remove README wording that promises all rows came from metrics.py --write.

Production snapshot first draft evidence:
- subject1ae6574;technical CI#583fivejobs green,rerunMSVConceaftertimeout.
- developer9doc_syncfixtures,mutant6,228localtechnicaltests from saved logs,
  all on exact source image corresponding to1ae6574;source commit URL, context
  explicitly localLinux,notCItestcount. 2developerpositiveslost explicitly stated.
- unknown current solver/QA/bank effectiveness;no new run.
- old d6a0aad README table remains reachable through history, not imported as
  fresh verified evidence. If historical row included, full measurement SHA and
  actual run source must first be recovered. No guessed hash or result link.

Gate: frozen fixtures below;tests check deterministic exact bytes;failure
no-write;outside-block preservation;render-only no-effect;legacy--write stops
before measurement (stub functions fail loudly if called). No benchmark run.
Publication requires reviewed patchSHA,expectedHEAD,apply-patchmaster,exact
readback and5successfuljobs. Metrics update itself adds no efficacy evidence.

## Implemented boundary

The production snapshot's subject is explicitly the previously validated commit,
not the later commit that publishes the snapshot. There is no implied live-master
freshness check. New evidence requires a separately reviewed snapshot update.
The source URL is a citation, not authentication. A well-formed false assertion
can render, so this is a reporting contract, not an evidence-truth verifier.

Tests cover all 35 frozen scenario constructions plus replacement-failure cleanup
and idempotence. Native symlink refusal is tested where symlink creation is
allowed; when Windows denies creation the test checks the nonregular-mode gate
with a mocked lstat and makes no native-link claim. Atomic replace is not a
power-loss durability promise. The local workspace must have no hostile writer.
