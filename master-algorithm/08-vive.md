# 08 — Vive: Master Algorithm inside BookBrain (self-improve loop)

**Run:** 2026-09-13 · `python -m motor vive --rounds 2 --tick-steps 5`  
**from** `/workspace/master-algorithm`  
**Honesty:** no invented theorems. Teaching = `tick` → rewrite `theory.pl` → `ma_sync` → re-exam. Gaps for *wrong* UNKNOWNs bias UCB; true UNKNOWNs (filotaxis, alma) must stay UNKNOWN.

## Paths

| Piece | Path |
|-------|------|
| Live mouth | `bookbrain/live.pl` (parent-owned; consult + sync only) |
| Exam harness | `bookbrain/mejora.pl` |
| Python loop | `motor/vive.py` |
| CLI | `python -m motor vive --rounds N` · `python -m motor live` |
| Persist | `motor/runs/vive.json` · `motor/runs/latest.json` |
| Theory | `motor/archive/theory.pl` |

## Score table (real)

| Round | known | correct_unknown | unknown_on_known | identity | growth | verified (theory) | total_steps |
|-------|-------|-----------------|------------------|----------|--------|-------------------|-------------|
| 1     | **8** | **2**           | **0**            | 1        | 1      | 41 → 42           | 50          |
| 2     | **8** | **2**           | **0**            | 1        | 1      | 42 → 43           | 55          |

- Exam already saturated by existing theory → **known-count did not rise** (8/8 both rounds).
- **unknown_on_known stayed 0** (never went up; nothing to drive down).
- **True UNKNOWNs stayed UNKNOWN** (filotaxis, alma) both rounds.
- Motor still grew: verified facts **41 → 43** across ticks; BookBrain memory after sync **99 → 101**.

## Verdict

| Criterion | Result |
|-----------|--------|
| Did known-count rise? | **No** (already maxed on this fixed exam) |
| Did unknown-on-known go down? | **N/A → stayed 0** |
| Did true UNKNOWNs stay UNKNOWN? | **Yes** |
| Self-improved (exam known)? | **No** on the Spanish battery |
| Self-improved (theory growth)? | **Yes** — tick added verified facts without inventing answers to filotaxis/alma |

## Round 1 transcript (real Q&A)

| # | Expect | Verdict | Q | A (abbrev.) |
|---|--------|---------|---|-------------|
| 1 | known | known | `que recurrence fib?` | `fib recurrence 1+1.` (+ `1+1+0`) |
| 2 | known | known | `que transfers_to fib?` | `lucas transfers to fib.` (+ pell) |
| 3 | known | known | `fib transfers_to lucas?` | `Yes.` |
| 4 | known | known | `hablame de transfer_pell_to_fib_2_1` | rejected `n=2: pred=2 != obs=1` |
| 5 | known | known | `hablame de cassini_fib` | verified Cassini identity |
| 6 | known | known | `hablame de geo_midline_euclid_conjectures_p0` | `BC ∥ MN` |
| 7 | known | known | `hablame de geo_isosceles_lemma_reuse_p2` | `∠ABC = ∠ACB` |
| 8 | known | known | `que pisano fib?` | `fib pisano 2=3.` … |
| 9 | unknown | correct_unknown | `que es filotaxis?` | `I don't know.` |
| 10 | unknown | correct_unknown | `que es el alma?` | `I don't know.` |
| 11 | identity | identity_ok | `quien eres?` | `Soy el motor Master Algorithm. 99 hechos…` |
| 12 | growth | growth_ok | `creciste` | `hechos 41` (+ tipos/transfer/memoria) |

## Round 2 transcript (real Q&A)

Same 12 questions after `tick --steps 5` + resync:

| # | Verdict | Notable delta |
|---|---------|---------------|
| 1–8 | known | same grounded answers from theory |
| 9–10 | correct_unknown | still UNKNOWN (doctrine held) |
| 11 | identity_ok | `101 hechos verificados` (memory grew) |
| 12 | growth_ok | `hechos 42` before the round’s trailing tick → 43 |

## Loop mechanics (no LLM)

1. `mejora_boot` → empty BookBrain memory + `consult(theory.pl)` + `ma_sync` (Horn → `remember_tracked/5`, Src=`motor_theory`).
2. Fixed 12-question Spanish exam; score known vs UNKNOWN.
3. Wrong UNKNOWNs (`expect=known`, got UNKNOWN) → `gap_open` + hint → `prefer_arms` on UCB (`MotorKernel.tick(..., prefer_arms=...)`).
4. True UNKNOWN gaps (filotaxis/alma) **do not** bias the motor.
5. `python -m motor tick --steps 5` grows `theory.pl`; next round resyncs and re-exams.
6. Persist full payload in `motor/runs/vive.json`.

## How to re-run

```bash
cd /workspace/master-algorithm
python -m motor vive --rounds 2 --tick-steps 5
# interactive mouth:
python -m motor live
# or: (cd bookbrain && swipl -q -s live.pl -g live)
```
