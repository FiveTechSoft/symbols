# 10 — Sublime kernel: generative language + molt loop

**Scratch archive:** `motor/archive-sublime/` (live `motor/archive/` untouched).
**Critic:** Prolog `holds_rec` / `holds_period` / finite bilinear checks — no Python-fiat `verified/1`.

## What changed vs the toy catalog

Before: UCB scheduled a **human-named** family menu (`bilinear_fib` → canned Cassini string).
After: a **HypothesisLanguage** (skin) proposes candidates from **operators**:

- `linrec_scan` — scan coeff space; order grows; novelty bonus escapes `[1,1]`-only exploit
- `modperiod_schema` — search modulus `m`, discover period (not a fixed Pisano table)
- `bilinear_schema` — search offset/`r` forms (Cassini-shaped / Catalan-like) + bogus reject
- `geo_invent` — `mutate_construction`; lemmas archived only if the engine proves them
- `transfer_horn` — Fib `rec/2` → Lucas/Pell (intelligence metric)

## Before / after metrics

| stage | n_verified | n_types | n_schema_classes | transfer_acc | lucas | pell | reject_rate |
|-------|------------|---------|------------------|--------------|-------|------|-------------|
| empty | 0 | 0 | 7 | 0.0 | 0.0 | 0.0 | 0.0 |
| after warmup | 7 | 7 | 7 | 0.0 | 0.0 | 0.0 | 0.65 |
| final (post-molt) | 140 | 19 | 10 | 0.5 | 1.0 | 0.0 | 0.4309 |

Molts: **6 kept**, **0 reverted** (attempted ≤ 6, min 3).

### Transfer honesty

- Fib→Lucas transfer_accuracy = **1.0** (must succeed / ~1.0)
- Fib→Pell transfer_accuracy = **0.0** (must fail honestly / ~0; Pell law is `[2,1]`)
- summary: `{'lucas_transfer_beats_baseline': True, 'pell_transfer_beats_baseline': False, 'absorbed_intelligence': True, 'negative_transfer_pell_expected': True}`

## Molt history

- **molt 0** gen=1 → `keep_skin` | n_verified 7→19; n_types 7→9; unlocked ['bilinear_schema']
  - saturation: schema ratio_scan saturated; 0 new verified and 0 new types this batch; reject_rate stuck + type plateau; transfer plateau
  - actions: ['raise_linrec_order 2→3', 'widen_coeff_range [-4,4]', 'unlock_schema bilinear_schema', 'raise_r_max 1→2', 'raise_mut_depth 1→2', 'novelty_bonus→0.5']
  - Δ verified 7→19, types 7→9, schemas unlocked ['dead_prime', 'geo_invent', 'linrec_scan', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'ratio_scan', 'transfer_horn']
- **molt 1** gen=2 → `keep_skin` | n_verified 19→31; n_types 9→11; unlocked ['modperiod_schema']
  - saturation: schema ratio_scan saturated; schema bilinear_schema saturated; 0 new verified and 0 new types this batch; reject_rate stuck + type plateau
  - actions: ['raise_linrec_order 3→4', 'widen_coeff_range [-5,5]', 'unlock_schema modperiod_schema', 'raise_m_max 4→6', 'raise_mut_depth 2→3', 'novelty_bonus→1.0']
  - Δ verified 19→31, types 9→11, schemas unlocked ['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']
- **molt 2** gen=3 → `keep_skin` | n_verified 31→49; n_types 11→15; lucas_transfer 0.0→1.0
  - saturation: schema ratio_scan saturated; schema modperiod_schema saturated; schema bilinear_schema saturated; 0 new verified and 0 new types this batch
  - actions: ['raise_mut_depth 3→4', 'novelty_bonus→1.5']
  - Δ verified 31→49, types 11→15, schemas unlocked ['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']
- **molt 3** gen=4 → `keep_skin` | n_verified 49→87; n_types 15→17; n_schema_classes 7→8; unlocked ['bilinear_schema_r4_g4']
  - saturation: schema linrec_scan saturated; schema ratio_scan saturated; schema modperiod_schema saturated; schema bilinear_schema saturated
  - actions: ['novelty_bonus→2.0', 'spawn_schema bilinear_schema_r4_g4']
  - Δ verified 49→87, types 15→17, schemas unlocked ['bilinear_schema', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'bilinear_schema_r4_g4', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']
- **molt 4** gen=5 → `keep_skin` | n_verified 87→115; n_types 17→18; n_schema_classes 8→9; unlocked ['bilinear_schema_r4_g5']
  - saturation: schema linrec_scan saturated; schema transfer_horn saturated; schema ratio_scan saturated; schema modperiod_schema saturated
  - actions: ['novelty_bonus→2.0', 'spawn_schema bilinear_schema_r4_g5']
  - Δ verified 87→115, types 17→18, schemas unlocked ['bilinear_schema', 'bilinear_schema_r4_g4', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'bilinear_schema_r4_g4', 'bilinear_schema_r4_g5', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']
- **molt 5** gen=6 → `keep_skin` | n_verified 115→140; n_types 18→19; n_schema_classes 9→10; unlocked ['bilinear_schema_r4_g6']
  - saturation: schema linrec_scan saturated; schema transfer_horn saturated; schema bilinear_schema_r4_g4 saturated; schema bilinear_schema_r4_g5 saturated
  - actions: ['novelty_bonus→2.0', 'spawn_schema bilinear_schema_r4_g6']
  - Δ verified 115→140, types 18→19, schemas unlocked ['bilinear_schema', 'bilinear_schema_r4_g4', 'bilinear_schema_r4_g5', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']→['bilinear_schema', 'bilinear_schema_r4_g4', 'bilinear_schema_r4_g5', 'bilinear_schema_r4_g6', 'dead_prime', 'geo_invent', 'linrec_scan', 'modperiod_schema', 'ratio_scan', 'transfer_horn']

## 5 example clauses (verified or rejected)

1. **verified_rec**: `rec(fib, [1,1]).`
   - reason: critic holds_rec accepted; archived rec/2
2. **verified**: `verified(fact(sequences, bilinear_schema, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).`
   - reason: bilinear_schema operator (NOT old canned bilinear_fib); critic accepted
3. **verified**: `verified(fact(geometry, geo_invent, 'geo_invent::L2_midline_parallel', 'BC ∥ MN')).`
   - reason: geo_invent mutate_construction; engine proved then archived
4. **verified_period**: `true_mod(fib, 2, 3).`
   - reason: modperiod_schema search; holds_period accepted
5. **rejected**: `rejected('bilin_fib_bogus_const2', 'n=1: lhs=-1≠2').`
   - reason: bilinear_schema bogus RHS; critic finite-fail

## Remaining honest limits (still human-designed)

- Seed operator set (`SEED_SCHEMAS`) is authored — molt unlocks/grows params, does not invent analysis.
- Geometry axiom set in `geometry/engine.py` is fixed; invent mutates constructions inside that closure.
- Companion graph `fib/lucas/pell` is given; transfer tests reuse, not discovery of which sequences exist.
- UCB + molt heuristics are bandit rules, not a full DreamCoder/NEAT.
- Scratch archive only; mouth still reads live `motor/archive/`.

## How to run

```bash
cd /workspace/master-algorithm
python -m motor.run_sublime --ticks 12 --min-molts 3 --max-molts 8
# default live archive still works:
python -m motor tick --steps 5
# artifacts: motor/runs/sublime.json, motor/runs/molt.json, motor/archive-sublime/
```

Elapsed: 20.328s. Molt mutates hypothesis language (skin) only. Every verified/1 and rec/2 clause still requires critic acceptance. Reverted skins discard language mutations but keep critic-gated archive facts already written.
