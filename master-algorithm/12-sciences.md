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

- n_verified: **47**
- n_rejected: **31**
- n_types: **19**
- n_spawned (non-seed schemas): **3** → `['chance_entropy_scan_n3_g2', 'info_mi_scan_n4_g2', 'sym_invariant_scan_n5_g2']`
- compare totals (last pass): hits=2 misses=0
- elapsed_s: 0.358

### Per-world skin

- **symmetry**: gen=3 classes=5 spawned=['sym_invariant_scan_n5_g2']
- **chance**: gen=3 classes=5 spawned=['chance_entropy_scan_n3_g2']
- **info**: gen=3 classes=4 spawned=['info_mi_scan_n4_g2']
- **sequences**: gen=2 classes=7 spawned=[]

## Beyond limits (emergent schema)

Verified/associated clause under **non-seed** family `info_mi_scan_n4_g2` (world `info`):

```
verified(fact(info, info_mi_scan_n4_g2, 'info_MI_indep_near0_s2290', 'MI(X;Y)≈0 on generated independent joint')).
```

family/schema id not in original seed catalogs — emerged via molt/spawn

## 8 example clauses

1. **verified_emergent** — verified under NON-SEED spawned schema; critic accepted
   `verified(fact(info, info_mi_scan_n4_g2, 'info_MI_indep_near0_s2290', 'MI(X;Y)≈0 on generated independent joint')).`
2. **verified** — science world; critic numeric/table gate accepted
   `verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1139', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).`
3. **verified** — science world; critic numeric/table gate accepted
   `verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1139', 'logit(post)=logit(prior)+log(LR) on generated table')).`
4. **verified** — COMPARE/EXTRAPOLATE form; critic accepted
   `verified(fact(symmetry, sym_compare_transfer, 'transfer_parity_to_z2_parity_parity', 'TRANSFER bit_fn(parity=parity) ⇒ Z2/parity invariant')).`
5. **verified** — COMPARE/EXTRAPOLATE form; critic accepted
   `verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).`
6. **verified_bit_fn** — logic bit_fn prior available for cross-world transfer
   `bit_fn('bitfn_parity_is_parity', parity, parity).`
7. **rejected** — science dead-end / broken hypothesis; critic finite-fail
   `rejected('chance_bayes_swap_s1139', 'P(H|E)=0.896539891664867 != P(H|¬E)=0.663110399724556').`
8. **rejected** — science dead-end / broken hypothesis; critic finite-fail
   `rejected('sym_z2_claim_mult_o2', '1+1=0 != 1*1=1').`

## COMPARE / EXTRAPOLATE (hits and honest misses)

- HIT `transfer_parity_to_z2_parity_parity` ← logic:parity (COMPARE bit_fn:parity → Z2 xor table)
- HIT `transfer_bitfn_parity_parity_to_MI` ← logic:parity (MI=0.665189>0)

## What is still human seed

Operator catalogs (SCIENCE_SEED / SEED_SCHEMAS), UCB constant, epsilon, finite-table generators, and the standing loop itself are human-authored. Every verified/1 still requires critic acceptance on obs.

## Why this is the path to “multiverse”

Here, understanding a multiverse means **the same form transfers across worlds** (Fib→Lucas already; now invariant/bit_fn → symmetry, identity → larger chance tables, bit_fn → MI structure). Mouth correctly stays UNKNOWN on the word “multiverso” until atoms exist — we do not dump cosmology into `theory.pl`.

## Honesty / limits

- Generators, epsilon, and operator seeds are human-authored.
- Dead-end families must fail (curiosity tax).
- Finite run cap ≠ finished learning.
- Zero invented cosmology facts.

