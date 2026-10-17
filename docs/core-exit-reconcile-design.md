# M0/M1 documentation reconciliation design

2026-10-17, base f14de345044faeaccfccc56db6d9e3bf138b015e.

Goal: stop historic gap text from generating duplicate production work. This is a bounded documentation-only correction, not a phase exit, production repair, or reinterpretation of acceptance criteria.

Frozen GOLD.json contains four claim rows and exact source SHA256 receipts before any docs patch: RP-LIVE, FUZZ-DEFAULT, PATCH-WAIT, M0-POINTER. Each separates source reading, historical measurement and current CTest status. The original raw629 results are already closed, actual checkout f14de34, both native jobs258PASS16SKIP0FAIL274. No suppressed stdout is invented. POSIX race_parent findings here are source only, not new Linux execution evidence.

Proposed files: docs/core-m0-m1-exit-audit.md receives a dated current-state pointer that leaves all old tables and reds unchanged; docs/core-m1-exit-reading.md receives a dated correction, C3 gap row reconciled with its already recorded waiting-process section, and explicit historical gate-note scope. ROADMAP.md receives a narrow link to this reconciliation only if needed to route its first pending item, not a declaration or removal of pending work. Add versioned design and claim-table JSON as support first.

Preserve historical paragraphs verbatim where feasible. Label old readings as historical via dated corrections rather than erasing predictions or failed runs. Add explicit caveat that regex-only CTest Passed can override exit status and cannot establish unprinted cell counts; six strict wrappers are separately verified. Do not widen that claim to all legacy tests. L6 claimed main-agent mandate remains non-authoritative and is not reused, accepted anew or changed by this patch.

No changes to src/, include/, tests/, CMakeLists.txt, workflows, allowlist, gates, gold values, 255/owner or build configuration. No stress rerun or compute-heavy step merely to update docs. No M0/M1 declaration; Antonio owns that decision.

Verification: local claim checker validates all four frozen rows against exact source receipts, new docs cover each with correct scope, original historical body/reds preserved except one top-row stale gap phrase with retained meaning pointer, and diff paths remain documentation only. Sensitivity checks remove a row, assert a phase declaration, claim default changed, claim regex proves exit status and modify a protected source; each must fail. These are document guard tests, not scientific production proof. Independent review checks exact design/gold and final patch SHAs before dispatch.

Publish support -> exact readback -> docs with fresh expected HEAD, once each after independent review. Record parent linkage and exact final doc/protected source SHA readback. Inherit complete standard CI matrix, both native Release/ASAN all six strict gates plus new i18n gate Passed without SKIP, preserve any actual failure and report before adjustment. No inherited fsrace repeat permission for this new slice.
