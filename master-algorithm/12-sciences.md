# 12 — Sciences: transfer of form toward a multiverse

**Scratch archive:** `motor/archive-sciences/` (live mouth archive untouched).
**Doctrine:** no cosmology essays, no hardcoded “the multiverse is…”.
A science exists here only as `obs → conjecture → critic → verified/rejected`.

## Standing loop (no done)

```
tick → compare/extrapolate → if plateau then molt/spawn → forever
```

This run used `--max-molts 3` as a **single-run cap only**, not the philosophy. UCB never disables all productive arms (saturated → reopen); science skins keep `novelty_bonus ≥ novelty_floor`; a saturated family **molts/spawns** instead of dying.

## Worlds

| world | role |
|-------|------|
| sequences | Fib/Lucas/Pell — form transfer already (rec priors) |
| logic | bit_fn from examples (parity / and_all / xor2) — priors for COMPARE |
| geometry | kept; lemma reuse path |
| symmetry | discrete invariants, Z2/Klein tables, involution |
| chance | generated Bayes/log-odds/entropy identities (eps critic) |
| info | MI/independence on tiny joints; transfers bit_fn — does not re-learn XOR/AND |

## Metrics (this run)

- n_verified: **69**
- n_rejected: **28**
- n_types: **28**
- n_spawned (non-seed schemas): **12** → `['alg_poly_zn_n9_g2', 'astro_kepler_scan_n3_g2', 'calc_fwd_diff_n7_g2', 'chance_entropy_scan_n3_g2', 'chem_atom_balance_n4_g2', 'electro_ohm_n5_g2', 'info_mi_scan_n4_g2', 'info_mi_scan_n4_g3', 'nets_perceptron_n4_g2', 'nets_perceptron_n4_g3', 'phys_collision_n4_g2', 'sym_invariant_scan_n4_g2']`
- compare totals (last pass): hits=10 misses=1
- elapsed_s: 0.423

### Per-world skin

- **symmetry**: gen=3 classes=5 spawned=['sym_invariant_scan_n4_g2']
- **chance**: gen=3 classes=5 spawned=['chance_entropy_scan_n3_g2']
- **info**: gen=3 classes=5 spawned=['info_mi_scan_n4_g2', 'info_mi_scan_n4_g3']
- **astro**: gen=3 classes=5 spawned=['astro_kepler_scan_n3_g2']
- **algebra**: gen=3 classes=5 spawned=['alg_poly_zn_n9_g2']
- **calculus**: gen=3 classes=6 spawned=['calc_fwd_diff_n7_g2']
- **nets**: gen=3 classes=7 spawned=['nets_perceptron_n4_g2', 'nets_perceptron_n4_g3']
- **electro**: gen=3 classes=5 spawned=['electro_ohm_n5_g2']
- **physics**: gen=3 classes=4 spawned=['phys_collision_n4_g2']
- **chem**: gen=3 classes=4 spawned=['chem_atom_balance_n4_g2']
- **sequences**: gen=2 classes=7 spawned=[]

## Beyond limits (emergent schema)

Honest limit: this run may have spawned schemas without yet parking a `verified/1` under the new id (UCB still exploring). Spawned ids are listed above; critic still gates any future facts under them.

## 8 example clauses

1. **verified** — science world; critic numeric/table gate accepted
   `verified(fact(nets, nets_perceptron, 'nets_AND_linear_threshold', 'AND separable by linear threshold (exists w,b)')).`
2. **verified** — science world; critic numeric/table gate accepted
   `verified(fact(nets, nets_perceptron, 'nets_OR_linear_threshold', 'OR separable by linear threshold (exists w,b)')).`
3. **verified** — COMPARE/EXTRAPOLATE form; critic accepted
   `verified(fact(symmetry, sym_compare_transfer, 'transfer_parity_to_z2_parity_parity', 'TRANSFER bit_fn(parity=parity) ⇒ Z2/parity invariant')).`
4. **verified** — COMPARE/EXTRAPOLATE form; critic accepted
   `verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).`
5. **verified_bit_fn** — logic bit_fn prior available for cross-world transfer
   `bit_fn('bitfn_parity_is_parity', parity, parity).`
6. **rejected** — science dead-end / broken hypothesis; critic finite-fail
   `rejected('chance_bayes_swap_s1190', 'P(H|E)=0.39325435065996983 != P(H|¬E)=0.632326787609022').`
7. **rejected** — science dead-end / broken hypothesis; critic finite-fail
   `rejected('astro_kepler_wrong_exp_n3', 'T²/a² not const: [39.4784, 59.2176, 78.9568]').`
8. **skin_meta_emergent** — schema/2 for NON-SEED spawned class (not a verified theorem)
   `schema(sym_group_table, unlocked(true)).`

## COMPARE / EXTRAPOLATE (hits and honest misses)

- HIT `transfer_parity_to_z2_parity_parity` ← logic:parity (COMPARE bit_fn:parity → Z2 xor table)
- HIT `transfer_bitfn_parity_parity_to_MI` ← logic:parity (MI=0.571428>0)
- MISS `calc_transfer_wait_no_rec` ← sequences (honest miss)
- HIT `nets_AND_linear_threshold` ← None (linear-sep search AND)
- HIT `nets_OR_linear_threshold` ← None (linear-sep search OR)
- HIT `nets_discrete_chain_vs_product` ← calculus:fwd_diff (discrete chain for f(u)=2u)
- HIT `nets_AND_linear_threshold` ← None (linear-sep search AND)
- HIT `nets_OR_linear_threshold` ← None (linear-sep search OR)

## What is still human seed

Operator catalogs (SCIENCE_SEED / SEED_SCHEMAS), UCB constant, epsilon, finite-table generators, and the standing loop itself are human-authored. Every verified/1 still requires critic acceptance on obs.

## Why this is the path to “multiverse”

Here, understanding a multiverse means **the same form transfers across worlds** (Fib→Lucas already; now invariant/bit_fn → symmetry, identity → larger chance tables, bit_fn → MI structure). Mouth correctly stays UNKNOWN on the word “multiverso” until atoms exist — we do not dump cosmology into `theory.pl`.

## Honesty / limits

- Generators, epsilon, and operator seeds are human-authored.
- Dead-end families must fail (curiosity tax).
- Finite run cap ≠ finished learning.
- Zero invented cosmology facts.

