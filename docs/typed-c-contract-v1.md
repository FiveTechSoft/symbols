# Typed C contract v1: frozen pilot format

Status: proposed frozen format for the static, read-only validator and candidate-preview pilot, pending the final patch gate. This is a data format, not mutation permission. No repository file, JSON field, or CLI typist authenticates the user's approval. The assistant mutation path must not invoke this pilot as an authority source.

Invocation: `python3 tools/typed_c_contract/preview.py --preview-c-contract FILE -w WORKSPACE` with no task description, legacy continuation flags, or other modes. `FILE` must be outside `WORKSPACE`, a regular non-symlink file. The command does not write the workspace. Its result is either a validated static preview, or a refusal reason. A preview is not a verified repair. The static generator must be a fixed, installed tool binary selected by the release, never a contract field, workspace file, environment variable or CLI argument. This prototype checks a closed, deterministic list relative to its own trusted script location: POSIX `build/c_contract_static`; Windows `build/c_contract_static.exe`, then `build/Release/c_contract_static.exe`, `build/Debug/c_contract_static.exe`, `build-asan/Debug/c_contract_static.exe`. It selects the first regular, non-symlink binary with no symlinked intervening directory, or refuses `generator_unavailable`; release packaging must bind the script and binary together and reject untrusted workspace copies. Do not invoke the preview script from a repository supplied by another party. A missing or unverifiable installation refuses. The reference command expects a release-built binary at one of these fixed locations; the CLI does not accept a generator path. The exact promise label is "contract validated + static candidate preview; stdout NOT verified, nothing executed". Result status is `static_candidate_unverified` when nonempty, or `no_static_candidate` otherwise. Both carry `not_executed: true`. No target C code is compiled or run in v1. A trusted static generator executable inspects source bytes but never builds or runs the target.

The UTF-8 JSON document is at most 8192 bytes, has no duplicate or unknown keys (including in nested objects), and uses exact case-sensitive field names. JSON booleans are not integers. Strings have no Unicode normalization or path expansion. The canonical identity of a contract is SHA-256 of its source file bytes after strict JSON validation; whitespace differences therefore make a different contract. Never use it as an authentication token.

```
{
  "schema": "symbols.c-repair-contract.v1",
  "workspace_digest": "<64 lowercase hex digits>",
  "stdout": {"bytes_b64": "T0sK", "length": 3, "termination": "exact"},
  "exit_code": 0,
  "probe": {"kind": "single-c-main", "timeout_ms": 5000},
  "edit_scope": {"allow": ["main.c"], "deny": []},
  "source_predicates": []
}
```

`workspace_digest` uses the exact v2 snapshot construction: sort all included regular text files by relative UTF-8 path bytes; for each append 32-bit big-endian path length, path bytes, 64-bit big-endian byte length, and the 64 ASCII lowercase hex digits of that file's SHA-256; SHA-256 the concatenation. The workspace scanner refuses symlinks, hard links, unreadable/oversized files, binary files, unsupported entries and collisions instead of silently omitting them; on Windows it obtains the hard-link count and file identity (volume and file index), size, and last-write time from `GetFileInformationByHandle` rather than trusting Python `DirEntry.stat`; it repeats that check after reading bytes and refuses a changed identity/metadata or a failed handle query (`file_identity`); on POSIX, the read-race identity is `(st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_size, st_mtime_ns, st_ctime_ns)`, deliberately excluding access time because reading a stable file may advance `st_atime`; `.git` and the top-level `.symbols` audit directory are excluded only under the scanner's documented safe-directory rules. Up to 64 files, 256 KiB each, 4 MiB total, path under 512 bytes. A changed digest refuses.

`stdout.bytes_b64` is standard padded RFC 4648 base64 with no whitespace, alternate alphabet, or noncanonical pad bits. Decode to 0..127 bytes and check `length` equals decoded bytes. `termination` is exactly `exact`; zero bytes means empty stdout. Newline, CR and NUL bytes are literal. This static pilot validates and retains any such byte vector, but its reused C-string candidate generator refuses an asserted value containing NUL or non-ASCII bytes (`literal_guard_unrepresentable`) rather than silently disabling the answer-literal guard. No candidate preview is promised for those contracts until a binary-safe generator is separately reviewed. The encoded bytes are retained as an asserted goal, not compared with program output in v1. `probe.timeout_ms=5000` is a future executable-probe parameter, inert in this static release. No target code is compiled or run. `probe.kind` identifies the intended future `single-c-main` execution shape; v1 does not assert the workspace fits an executable probe because it never builds or runs it. The static generator inspects up to four allowed C files, each at most 64 KiB, and enumerates single-file source edits. Unlike the legacy executable repair, this enumeration does not certify `main` uniqueness, buildability, probe suitability or runtime success; the `probe` object is inert in v1. For a nonempty result each entry reports the original/edited source SHA-256 and a bounded unified diff; these are candidate data, not a patch applied to the workspace. An unsupported source shape may produce no candidates.

`edit_scope.allow` contains 1..4 exact source paths; `deny` contains 0..4. Paths must exist as unique regular files in the snapshot, be normalized relative slash-separated names, never absolute or `.`/`..`, and have no glob characters. A path may not occur twice or in both lists. Exactly one allowed file may change; denied and all other files remain byte-identical.

`source_predicates` contains 0..4 objects, each one of:

- `{"kind":"file_bytes_equal","path":"helper.c","sha256":"<64 lowercase hex>"}`. The digest must equal the original file and each proposed candidate's corresponding file.
- `{"kind":"span_bytes_equal","path":"main.c","start":0,"length":8,"sha256":"<64 lowercase hex>"}`. The nonzero byte span must fit the original file. Its digest must equal the original bytes at that exact offset and each proposed candidate's bytes at that same offset. A shift or overlap with the edit refuses. No searching for a moved span.

The `start` and `length` values are JSON integers, not booleans or decimal strings. Repeated, contradictory or uncheckable predicates refuse. `file_bytes_equal` for an allowed file is valid but makes any edit to that file impossible; the preview should report no reachable candidate rather than dropping the predicate. Other predicate kinds, including semantic call order, are unsupported.

The pilot reports schema validity, snapshot match, source-derived static candidate count and scope/predicate-filtered edit summaries. It cannot report an exact-byte baseline match, a passing tier, runtime uniqueness, or a verified repair. Candidate coverage is static only; each record includes its source path, operator rule, tier, before/after source SHA-256, and a bounded edit hunk suitable for matching an expected edit. The candidate list is not ranked by runtime success. All positive results say `not_executed: true` and "contract validated + static candidate preview; stdout NOT verified, nothing executed". There is no target-code compilation, run, hidden flag or dead execution path in v1. The trusted static generator is a source-text transformation tool, not a target-code executor. Successful preview JSON deliberately contains bounded unified diffs, which can include private source lines. Treat the preview output as private; do not log, publish or send it to an unapproved audience. Refusal messages and routine logs contain only reason codes and digests. `operator-supplied/identity-unverified` is the only pilot provenance label. A later authenticated adapter needs a separately approved and verified receipt bound to this exact file digest, workspace and action before any mutation. Existing `--continue-stdout-goal` remains the separate normalized one-line path.


## Exact output contract for Set S v2

For a syntactically valid invocation, success exits 0, writes exactly one compact JSON object followed by `\n` to **stdout**, and leaves stderr empty. JSON string escapes follow the standard encoder; the literal terminal newline is separate from the escaped `\n` inside a `unified_diff` string. JSON key order is not significant. It has exactly these top-level keys and types:

| Key | Type and meaning |
| --- | --- |
| `status` | String: `static_candidate_unverified` when `candidate_count > 0`; otherwise `no_static_candidate`. Neither means a program ran or passed. |
| `promise` | Literal string `contract validated + static candidate preview; stdout NOT verified, nothing executed`. |
| `not_executed` | JSON boolean `true`, not text `not_executed=true`. |
| `provenance` | Literal string `operator-supplied/identity-unverified`. |
| `contract_sha256` | 64 lowercase hex digits, SHA-256 of the validated contract file's exact bytes. |
| `workspace_digest` | 64 lowercase hex digits, the checked snapshot digest. |
| `candidate_count` | Integer from 0 through 384; equals `len(candidates)`. There is no text field `candidates=N`. |
| `candidates` | Array of zero or more static candidate records, in generator enumeration order. |

Each candidate record has exactly `tier` (integer 1..4), `rule` (string naming a source-derived transformation), `path` (exact relative allowed C source path), `before_sha256` and `after_sha256` (64 lowercase hex-digit source-byte digests), and `unified_diff` (string, bounded to 4096 UTF-8 bytes). Diff headers are `before/<path>` and `after/<path>`; this may reveal private source lines. No candidate is called passing, verified, safe or uniquely successful at runtime. The exact operator rule set and enumeration order are implementation-specific, so match an expected edit by path and diff/source-byte identity, not by array index.

Example success object (illustrative digest values, not a runnable fixture):

```json
{"status":"static_candidate_unverified","promise":"contract validated + static candidate preview; stdout NOT verified, nothing executed","not_executed":true,"provenance":"operator-supplied/identity-unverified","contract_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","workspace_digest":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","candidate_count":1,"candidates":[{"tier":2,"rule":"init_mul","path":"main.c","before_sha256":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","after_sha256":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","unified_diff":"--- before/main.c\n+++ after/main.c\n@@ -1 +1 @@\n-int x=0;\n+int x=1;\n"}]}
```

A valid contract with no surviving static candidates exits 0 with `status: "no_static_candidate"`, `candidate_count: 0`, `candidates: []`, and the same `promise`, `not_executed`, provenance and digest fields. It is not a runtime failure.

A **contract/workspace/preview refusal** exits 2, writes no bytes to stdout, and writes `refused: <reason>\n` to **stderr**. `<reason>` belongs to this closed list (future changes require a new surface review):

- Schema: `limit`, `utf8_bom`, `duplicate_key`, `fields`, `version`, `digest`, `stdout`, `base64`, `exit`, `probe`, `scope`, `predicates`, `path`, `span`, `predicate`.
- Contract location and snapshot: `contract_file`, `contract_link`, `contract_limit`, `contract_in_workspace`, `workspace_root`, `file_identity`, `depth`, `audit_dir`, `git_dir`, `path_limit`, `path_encoding`, `symlink`, `file_limit`, `binary_or_race`, `total_limit`, `unsupported_entry`, `workspace_digest`, `workspace_changed_during_preview`.
- Scope, predicate and generator: `allow_path`, `deny_path`, `predicate_path`, `span_range`, `predicate_mismatch`, `span_changed`, `no_c_source`, `generator_unavailable`, `generator_source_limit`, `literal_guard_unrepresentable`, `scope_conflict`, `generator_failed`, `candidate_limit`, `generator_output`, `candidate_bytes`, `candidate_no_change`, `candidate_patch_limit`.
- Unexpected validation/I/O errors are redacted to `invalid_or_unavailable`, also in the closed list; no exception text, task/source bytes or untrusted path is printed.

The v1 reference implementation maps invalid inputs to exact refusal codes in validation order (the first reached check wins). The closed list above is not an interchangeable set of aliases:

| Stage and defect | Code |
|---|---|
| Raw contract empty or over 8192 bytes; UTF-8 BOM; duplicate JSON key at any depth | `limit`; `utf8_bom`; `duplicate_key` |
| Missing/extra keys or wrong object shape at a checked object; wrong schema version; malformed 64-digit lowercase SHA-256 field | `fields`; `version`; `digest` |
| Stdout field type, termination or length outside 0..127; malformed/noncanonical base64 OR decoded length not equal declared length | `stdout`; `base64` |
| Exit code not integer zero; probe object not exactly `single-c-main`/5000 | `exit`; `probe` |
| Allow/deny not lists or list lengths outside limits, OR duplicate/overlapping paths; malformed path shape/encoding/glob/traversal | `scope`; `path` |
| Predicate collection not a list or over four; predicate item wrong type, unknown kind or duplicate predicate; predicate item with missing/extra keys | `predicates`; `predicate`; `fields` |
| Span start/length invalid, noninteger, nonpositive or beyond 262144; predicate path invalid; predicate digest malformed | `span`; `path`; `digest` |
| Contract file missing/symlink/nonregular; hardlinked; over 8192 bytes; located inside workspace | `contract_file`; `contract_link`; `contract_limit`; `contract_in_workspace` |
| Workspace root invalid; Windows handle identity query failed; depth over eight; unsafe top-level `.symbols`; any `.git` entry | `workspace_root`; `file_identity`; `depth`; `audit_dir`; `git_dir` |
| Snapshot relative path at least 512 bytes; non-ASCII/control name; symlink; hardlink/file too large/too many; changed metadata, length or NUL; total bytes over 4 MiB; other entry type | `path_limit`; `path_encoding`; `symlink`; `file_limit`; `binary_or_race`; `total_limit`; `unsupported_entry` |
| Snapshot hash differs from declared digest; workspace bytes/digest changed during preview | `workspace_digest`; `workspace_changed_during_preview` |
| Allowed path absent/not C; denied path absent; predicate path absent, span exceeds source, digest mismatch, guarded span changed | `allow_path`; `deny_path`; `predicate_path`; `span_range`; `predicate_mismatch`; `span_changed` |
| No C files in workspace; installed generator absent/unverifiable; allowed source exceeds 64 KiB; asserted answer has NUL/non-ASCII; deny/allow conflict when enumerating | `no_c_source`; `generator_unavailable`; `generator_source_limit`; `literal_guard_unrepresentable`; `scope_conflict` |
| Generator nonzero/timed out; too many candidates; invalid generator line/record; bad candidate bytes; no source change; diff over 4096 bytes | `generator_failed` (nonzero), `invalid_or_unavailable` (timeout); `candidate_limit`; `generator_output`; `candidate_bytes`; `candidate_no_change`; `candidate_patch_limit` |
| Other validation, decode or I/O failure, including malformed JSON and invalid UTF-8 | `invalid_or_unavailable` |

In particular, syntactically invalid paths return `path` in the schema stage, whereas valid but absent/non-C allowed paths return `allow_path` only after the snapshot. A malformed/duplicate allow/deny list returns `scope` before the later `scope_conflict` generator check. An unsupported predicate kind returns `predicate`; only collection shape/size returns `predicates`. Base64 byte/length mismatch returns `base64`, not `stdout`.

Malformed CLI arguments are handled by the argument parser, not the contract-refusal path: they exit 2, leave stdout empty and print usage/error text to stderr. A harness must capture exit code, stdout **and** stderr; it must not treat `refused:` as stdout or parse a refusal as successful JSON. The target C program is never executed, including on a schema or workspace refusal.

A workspace with multiple `.c` files is **valid in v1** when it satisfies the snapshot and named-path rules. `probe.kind` is inert, so this pilot cannot reject multi-C solely because a future executable `single-c-main` probe would be ineligible. Static candidate enumeration visits only the explicit allowed C paths, and does not assert a buildable single-main program. This matters for Set S v2: a multi-C case may return `static_candidate_unverified` or `no_static_candidate`; it is not categorically a refusal.

For a sealed static Set S, a positive means an expected source edit is in the reported candidate set, not that the code builds or matches stdout. A runtime wrong-edit count is not a meaningful metric because no edits are applied; report incorrect static suggestions and scope/predicate leakage separately. The executable probe, sandbox evidence and Windows isolation are separate future work.
