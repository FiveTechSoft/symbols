# 11 — Deduce: the brain forms like a child

The brain **is** `theory.pl`. Vocabulary is a byproduct of growth — stage 1 `obs/3` (see), stage 2 `rec/2` and verified formulas (form a regularity), stage 3 a name exists only if it already appears as an atom inside a clause the child grew. Nobody whispers “Cassini” or “Fibonacci” into Python. A new word attaches only by stem/overlap with atoms already present, or by unifying a formula already proved; otherwise the mouth says UNKNOWN — “I don’t have that yet.” Learning a name later means a new verified Horn clause from an experiment (or an explicit grounded teach that writes theory), never an adult alias dict.

## What was deleted

From `motor/talk.py`:

- `SEQ_ALIASES` (fibonacci→fib, lukas→lucas, …)
- `TRAP_RE` (filotaxis, alma, universo, …)
- `_cassini_hits` / `_answer_cassini` (special-case synonym for `bilin_fib_offset_pm1`)
- Topic-enum keyword routes: cassini / pisano / geometry / ratio as adult word lists
- Letter remaps `F(n)→fib`, `π→pisano` inside fold

## How binding works

1. Load lexicon from the kb: `rec` heads, verified/lemma/rejected/schema names and `_`-parts, predicate heads (`true_mod`, `lemma`, `companion`, …), tokens already inside formulas/lemma texts.
2. Fold + tokenize generically (accents off; no domain word list).
3. `bind_tokens`: exact match; prefix stem (short len-3 atoms only if they are rec heads, so `fib`→`fibonacci` but not `rec`→`receta`); shared prefix ≥4; one-indel near-stem (`lema`↔`lemma`); one-edit equal-length for rec heads (`lukas`↔`lucas`).
4. `retrieve` ranks clause hits by bound atoms; `prove_formula` unifies equation-shaped questions with verified formulas by char-bigram overlap above a real threshold. Empty bind + no formula → UNKNOWN.

## 8 Q / A

| Q | A (summary) |
|---|---|
| `quien eres` | Identity speech act; cites **140** verified facts from kb. |
| `fibonacci` | Binds atom `fib` by stem → `rec(fib,[1, 1])`. |
| `cassini` | No atom → **UNKNOWN**. |
| `fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n` | Formula unify → `verified bilin_fib_offset_pm1`. |
| `se transfiere a pell` | Transfer speech + atom `pell` → **No**; cites `rejected('transfer_fib_to_pell_1_1')`. |
| `por que` (after pell refusal) | Dialogue why → same counterexample clause. |
| `el alma` | No atoms → **UNKNOWN**. |
| `L1_midline_parallel` / `BC ∥ MN` | Lemma atom / text unify → cites real `lemma/3`. |

## Remaining hardcoded speech acts (honest list)

These are dialogue / self-knowledge, **not** domain math synonym tables:

- **identity** — quien eres / who are you / tu tribu
- **growth** — creciste / growth / como vas / estado / progreso (reads `latest.json`)
- **summary** — que sabes / resumen / inventario
- **why / more / follow** — por que, más, «y X» on last dialogue state
- **transfer speech** — transfiere / transfer / misma / aplica a / → (then seq atoms from kb)
- **prove speech** — demuestra / prove (content still from deduce)
- **false-law arithmetic speech** — el doble / siempre 2 / order 1 (checked against `rec/2`)
- **prime claim speech** — siempre primo → cites existing `rejected` if any
- **invent speech** — inventa / teorema nuevo → UNKNOWN

Exam (`deduce-round`): known 153/153, unknown 60/60, reject 39/39, **inventions 0**, follow-ups 7/7.
