# 17 — Emerge (necessary form, not a fatter bin)

**One line:** intelligence = evolutionary leap under critic on the exact evidence levers — not more ticks, not bilinear_gN, not a fatter bin.

Emergence = **new necessary form under critic**, not more verified clones of the same shape.

## Six levers (only these pay)

| # | form_family | lever |
|---|-------------|--------|
| 1 | `rec_companion` | additive rec as form → companion world (Fib→Lucas; Fib→Pell = carving reject) |
| 2 | `conserv_delta0` | linear Δ=0 → another additive world (chem/phys/electro **and** loop Δx=action) |
| 3 | `rec_to_delta` | rec → discrete Δ (calculus), not more rec clones |
| 4 | `bit_circuit` | bit compose as circuit (AND=series, OR=parallel), not perceptron dots |
| 5 | `loop_taxis` | closed-loop sign(error)→action that generalizes to a new x0 |
| 6 | `form_gate` | identity ≠ defining law (cassini holds on Pell, is not pell rec) |

Tags: **lever** | **clone** | **off-path**. Random min/max/xor-of-sums are **not** levers unless they compose one of the six.

## Live census (`motor/understand.py`)

| metric | value |
|--------|-------|
| n_facts | **420** (doctrine snapshot was 375; same collapse) |
| n_form_families | **32** |
| n_necessary | **6** (= the six levers; N ≪ facts) |
| noise facts (not on a lever) | **268** |
| unique-form understanding score | **0.1875** = 6 / (6 + 26 non-lever families) |

### Necessary form families (raise understanding)

| family | n_facts | xfer | carve | tag |
|--------|---------|------|-------|-----|
| rec_companion | 54 | ✓ | ✓ | lever |
| conserv_delta0 | 59 | ✓ | ✓ | lever |
| rec_to_delta | 19 | ✓ | ✓ | lever |
| bit_circuit | 14 | ✓ | ✓ | lever |
| loop_taxis | 6 | | ✓ | lever |
| form_gate | 0 verified | | ✓ (reject-only) | lever |

### Clone / off-path mills (history kept; UCB prize ≈ 0)

| family | n_facts | tag | note |
|--------|---------|-----|------|
| rec_order_clone | 69 | clone | linrec order/coeff pads — brute |
| bilinear_schema | 31 | off-path | **g4..g8 collapsed to 1 family**; products |
| chance_entropy | 35 | off-path | H on k-sweep |
| phys_energy | 20 | off-path | ½mv² product |
| geo_invent_depth | 25 | clone | depth sweep |
| nets_dot / alg_mat_assoc / kepler_power | … | off-path | matmul / dots / powers |

`bilinear_schema_r4_g4..g8` → **1** family. `phys_mom_conserve_*` → `conserv_delta0`. `rec_fib_o4_*` → `rec_order_clone` (not necessary).

## Score formula (`kernel._score`)

```
reward =
  +0.02  each new_true on off-path/clone          # multiply/matrix/coeff mills die
  +0.05  each new_true on ALREADY-known family    # farming clones dies
  +3.0   each new_true that opens a NEW form_family (lever)
  +2.0   extra if that fact is a transfer into a new world   # largest
  +1.5   honest reject that carves a lever form-family
  +0.4   other carving reject
  +0.25  dead-end NEG confirm
score = reward - (0.05*|facts| + 0.02*param)
```

Saturate: if an arm yields **0 new form_families for K=3 ticks**, mark saturated even when `new_true>0` (clone mill).

## Molt change

Spawn must **change the operator set** on a lever (compose/transfer): e.g. `delta_conserv`, `taxis_conserv`, `rec_to_delta`, `bit_circuit_compose`, `form_gate`, `companion_rec`.

**Forbidden:** `bilinear_schema_r*_g*`, `linrec_scan_o*_v*`, coeff widen as “leap”, matmul, entropy clones, random min/max.

If no new lever composition can be named in one word → `spawn_skip` (do not suffix `_gN`).

## Scratch proof (`motor/archive-emerge/`, `motor/runs/emerge-proof.json`)

| case | score | expected |
|------|------:|----------|
| bilinear clone ×2 (known family) | **-0.14** | ≈0 |
| new lever `bit_circuit` | **2.87** | large |
| transfer new lever world | **4.87** | largest |
| carve reject `false_xor_as_and` | **1.37** | understanding |

Assertions all true. **Merged nothing that is a clone** into live; merge only if a genuinely new lever family verified.

## Files

- `motor/understand.py` — form_family map, lever\|clone\|off-path, necessary census
- `motor/kernel.py` — `_score` rewrite + K=3 form-family saturation
- `motor/worlds/base.py` — `ticks_no_new_form_family`
- `motor/worlds/schema_lang.py` — lever-only molt spawn
- `motor/worlds/science_lang.py` — lever-only molt spawn (no `_n*_g*` clones)
- `motor/archive-emerge/` — scratch proof archive
- `motor/runs/emerge-proof.json` — before/after scores
- `17-emerge.md` — this file

Path: unification / addition / comparison / conserv-as-sum-0 / discrete Δ / bit logic / closed-loop — **not** matmul, bilinear products, perceptron dots.
