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
- 2026-09-23 | Instinct | (fill in when you pick up Asks 1–5) | | |
