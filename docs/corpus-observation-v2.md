# Corpus-answer observations v2

Observation-only contract, not semantic support or a delivery guarantee. Only knowledge-question, no-tools coding corpus fallback (not file-creation guidance), and ordinary-chat branches are covered. Other routes, including title transform and tool dispatch/continuation, remain unchanged and observation-uncovered.

Existing observation keys and the ServerBuildObservation API are preserved. Ordinary-chat `status` remains its legacy output-string classification, not a core GOAL result. Newly covered early returns use `UNKNOWN` with `status_kind=unavailable`, empty intent/slots/focus/entities/prov and unmeasured latency0. They never reuse a prior request's parser state.

The pure ServerBuildCorpusObservation formatter adds:
- `observation_version`:2.
- `route`: observed branch identity, not semantic handled.
- `session_id`: existing session key. No new request-ID system.
- `response_seq`: existing sequence only if the payload builder succeeded, otherwise null. `response_sequence_attempt` always records the attempted existing sequence. No successful response identity is claimed on builder failure.
- `response_exists`: boolean returned by ServerAnswerQuery, or null when not attempted. This proves response production, not correct semantic handling or support.
- `core_status`: always `unknown`; the API exposes no GOAL status.
- `raw_result`, `raw_result_available`: returned core text before mapping, or unavailable. `result` stays the mapped output. Wording is not an oracle.
- `status_kind`: `legacy_text_classification` for ordinary-chat, `unavailable` for early returns.
- `latency_measured`: ordinary-chat true; early returns false.
- `source_receipt`: `unavailable`; no text-match reconstruction.
- `payload_built`: actual builder return value for both JSON/SSE and all three branches, not socket write or receipt by client.
- `delivery`: always `unknown`, even after a send attempt.

One observer call per covered return, including ordinary payload-build failure. The old ordinary-tail logger is replaced, not duplicated. A failed builder leaves its output empty under the existing wire-builder contract; the observer binds only an attempted sequence, never an old response identity. Newly covered early returns retain existing send/error behavior; this patch does not fix unrelated transport policy.

## Failure and resource limits

Logging is best effort, using existing cwd/temporary fallback. Failed open/write, allocation/capacity limits and process termination can prevent persistence. Exactly-one is a call-count invariant, not durable exactly-once delivery. Formatting failure clears output and emits an observer-format diagnostic, never a successful record or a changed answer. The existing legacy formatter's bounded query/result/focus fields still limit accepted observations.

Helper stack contains32KiB observation buffer plus small metadata. The pure formatter has small metadata and nested legacy formatter arrays of roughly12KiB. Escaped raw text uses heap storage sized6n+1 for the JSON worst case, at most49147bytes for an8191byte raw result. The caller's existing stack is additional. One allocation/free and synchronous file-open/append/close occur per covered request, not duplicate logging. Best effort is functional isolation, NOT nonblocking I/O or bounded latency. A slow filesystem/FIFO may block as with the old logger; no queue/retry/service is added.

## Validation scope

Durable tests extend existing test_server_tool_calling, without CTest/workflow registration changes. They check legacy-prefix compatibility, early sentinel/empty-state semantics, response-vs-core distinction, raw8191control-byte expansion beyond16KiB, escaped session values, small-buffer failure with empty output, nullable response identity and actual JSON/SSE builders clearing an old identity on escape failure. These pure-unit checks do not prove HTTP call-site reachability, session behavior or no duplication.

Separate directed HTTP baseline/candidate evidence covers the three branches in JSON/SSE, logging-open failure, exactly one record, session IDs, title/tool exclusions, complete response identity and unchanged source/sentinel. Created timestamp alone is normalized for byte comparison. Original title fixture lacked the existing transform prerequisites; versioned fixture corrections added the output bound and instruction/material turns, not serving-policy changes.

Release candidate and unchanged baseline each completed all252 native CTests serially across bounded batches, without test-timeout changes or skips. Invalid parallel attempts collided in shared fs/git/doc scratch paths and are retained as rejected validation attempts. Directed Release and ASan/UBSan HTTP matrices are compatibility evidence, not exposed80 replay or independent evaluation. ASan leak detection was disabled; Windows/MSVC runtime remains unverified. Historical48 routeunknown calibration rows remain unknown and are not retroactively repaired.
