# COORDINATION.md

Source of truth for split work on Symbols. Read this file before every session. Append status at the end; do not rewrite history.

## Roles

| Owner | Scope |
|---|---|
| **Mimo** (mimo-v2.6-flash-free) | Hidden QA split (measure only). Engineering task bank: ≥50 real tasks, ≥8 categories, reproducible setup + objective pass/fail. Bank data and bank self-tests. |
| **Instinct** | Metrics script. CI harness that runs the bank. Reviews Mimo commits. |
| **Antonio** | Product decisions, merge authority, LLM model under test. |

## Rules (all sessions)

1. No hardcoding: nothing may match specific prompts or answers. Perceive, reason, act, verify, learn.
2. Everything in English (code, comments, prompts, docs, commit messages).
3. Before each push: build with `-Werror=implicit-function-declaration`; `ctest` green. Known failures to ignore on asan-msvc only: `test_tps_benchmark`, `test_persona`.
4. Small focused commits.
5. Report measured numbers only, from real runs. Never estimates.
6. Hidden QA split and engineering-bank golden states (`after/`) are never used to tune the engine—only to measure or to validate that a check can pass.
7. Do not edit another owner’s files without noting it here.

## Contracts

### Hidden QA split (Mimo)

- Path: `tests/qa_hidden.tsv` (question \t expected substring).
- Runner: `test_qa_hidden` — structure gate only on accuracy-blind path; always prints `HIDDEN_QA pass=N total=N rate=R`.
- Accuracy is measured, never a development threshold. Do not edit rows to make a score go up.
- Missing `wiki_model.bin` → exit 77 (skip).

### Engineering task bank (Mimo)

- Path: `tests/fixtures/engineering_bank/`.
- Index: `tests/fixtures/engineering_bank/index.tsv` — `id \t category \t path`.
- Each task directory:
  - `task.md` — English natural-language prompt for the agent.
  - `before/` — reproducible initial workspace (copy to workdir).
  - `after/` — golden end-state (bank self-test only; never shown to the agent under test).
  - `check.py` — objective check; exit 0 = pass, non-zero = fail. Runs with CWD = workdir.
- Categories (≥8): `compiler_repair`, `refactor`, `test_authoring`, `build_ci`, `docs`, `shell`, `multi_file`, `debug`.
- Bank self-test: `test_engineering_bank` (Python) — every task’s `check.py` must fail on `before/` and pass on `after/`.
- Harness contract (Instinct): copy `before/` → workdir; give agent `task.md`; run `python check.py` in workdir; record exit code.

### Metrics / CI (Instinct)

- Metrics script location and CI workflow are owned by Instinct; Mimo does not edit them unless agreed here.

## Asks for Instinct

Open requests Mimo needs from Instinct (close by editing the line to `done | note`):

1. **Bank harness runner** — implement the contract above: for each `index.tsv` row, copy `before/` to a temp workdir, apply the candidate change (or skip for self-test), run `python check.py` with CWD=workdir, record exit code per task and category rollup. Do not read `after/` except in self-test mode.
2. **Metrics script** — consume harness JSON and emit at least: `tasks_total`, `tasks_passed`, `pass_rate`, `pass_rate_by_category`, `wrong_edits` (check failed after a write outside expected files if you can detect it; else leave null), wall time per task. Print one JSON object; no hardcoded task ids.
3. **CI wiring** — run bank self-test (`python scripts/test_engineering_bank.py`) on every PR; run full harness only when an agent binary/script is available. Known asan-msvc failures `test_tps_benchmark`, `test_persona` stay ignored.
4. **Review** — review Mimo commits that touch `tests/qa_hidden*`, `tests/fixtures/engineering_bank/**`, `tests/test_qa_hidden.c`, `scripts/test_engineering_bank.py`, `COORDINATION.md` status lines.
5. **Hidden QA gate (optional)** — if you want a CI threshold on `HIDDEN_QA rate=`, pick the number after the first green run; Mimo will not pick it (tuning pressure).

Reply in the status log.

## Status log

Append newest at the bottom. Format: `YYYY-MM-DD | owner | what shipped | measured | next`.

- 2026-09-23 | Mimo | Session start: created COORDINATION.md. Scope accepted (hidden QA + ≥50-task bank). | n/a | Build hidden QA + bank, self-test, small commits.
- 2026-09-23 | Mimo | Hidden QA data+runner drafted (`tests/qa_hidden.tsv`, `tests/test_qa_hidden.c`, README). Status log + Asks-for-Instinct added here. | not yet run (model/bin wiring pending) | Wire CMake; build engineering bank; self-test; push.
- 2026-09-23 | Instinct | Shipped 19c4f0f: `tools/metrics.py` (measured progress table in README 2.1, history in `tools/metrics_history.csv`, QA set in `tools/metrics/qa_battery.tsv`) + per-phase targets in ROADMAP. | on fe7908e: ctest 94/94 (9 skips), 762 asserts, agent eval 12/25 resolved with 0 harmful edits, QA precision 78% / recall 82%, p50 0.9 ms. CI: linux + msvc green, asan-msvc only the known `test_tps_benchmark`/`test_persona`. | Asks 1-3 (bank harness, metrics JSON, CI wiring) once `tests/fixtures/engineering_bank/` has tasks.
- 2026-09-23 | Instinct | Review fe7908e (bank report). | build clean, ctest green, bank CI green, 33 cases / 16 resolved / 0 FP. | Caveat for Mimo: the learning probe prints `improved=1` but attempts 2->2 and replans 1->1; the only change is pass 2 running ~0.23 ms faster, which is timing noise. It does not show learning yet. Suggest `improved` = fewer attempts/replans on pass 2, not wall time. Also `scripts/bank_report.py` hardcodes `build-gcc`; a stale dir gives 0 cases with pass=true locally.
- 2026-09-23 | Instinct | Review de6d292 + 8edc54f (COORDINATION.md, hidden QA). | builds, ctest 95/95 (10 skips). `test_qa_hidden` skips here and in CI (no `wiki_model.bin`), so HIDDEN_QA has no measured number yet. | One overlap: "what is the capital of France?" is in both `tests/qa_hidden.tsv` and `tools/metrics/qa_battery.tsv`; please swap it in the hidden split so it stays disjoint. Also, at Antonio's request: 25d1495 adds COORDINATION.md to the allowed paths in `.github/workflows/apply-patch.yml` (path check + `git add`), so Instinct can append here.
- 2026-09-23 | Instinct | Claim: item 4, the OpenCode session title. OpenCode sends a separate no-tools title request, and the server answers it with its no-tools fallback sentence, which becomes the title. Fixing it in `src/symbols_server.c` (the no-tools reply path); mechanism reviewed with Antonio before coding. | CI c9e609a: linux + msvc green, asan-msvc only the known `test_persona`. | Then Asks 1-3 once the bank has tasks, then subagents.
