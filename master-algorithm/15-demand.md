# 15 — Demand round (false analogies + unique-form)

Anto: *sigue probando, sigue exigiendo* after 0.75.

## Verdict this round

**We found the ceiling of the inflated metric, not a smarter motor.**
Raw attempt ratio fell because we added honest rejects; unique-form score is the honest intelligence number.

| metric | value |
|--------|-------|
| raw attempt ratio | **0.5714** = 16 hit / (16+12) |
| UNIQUE form-family score | **0.6923** = 9 / (9+4) |
| false analogies all reject | **True** (8 honest misses) |
| mouth grill | **43/43** inventions=0 |
| tick --steps 2 | works (total_steps advanced) |

### Unique form-family hits
['bilin_cassini', 'bit_and_linear', 'bit_and_series', 'bit_or_parallel', 'conserv_delta0', 'mat_assoc_reuse', 'poly_distrib_reuse', 'rec_11', 'rec_to_delta']

Conservation-Δ=0 counts **once**. Kepler-on-own-table excluded as re-verify.

## False-analogy table (critic MUST miss)

| pair | expected | result | why |
|------|----------|--------|-----|
| FALSE mom-Δ=0 as energy ½mv² | miss | miss | form-family mismatch: conserv_mom(linear_delta_zero) ≠ energy_half_mv2(quadratic_energy) |
| FALSE series-AND as parallel-OR | miss | miss | form-family mismatch: series_and(bit_and) ≠ parallel_or(bit_or) |
| FALSE fib[1,1] on trib | miss | miss | n=2: pred=0 != obs=1 |
| FALSE fib[1,1,0] padded on trib | miss | miss | n=5: pred=3 != obs=4 |
| FALSE Kepler as Ohm on resistor | miss | miss | form-family mismatch: kepler_t2_a3(power_ratio_const) ≠ ohm_vir(linear_proportional) |
| FALSE cassini bilin as Pell law | miss | miss | form-family mismatch: bilin_cassini(bilinear_identity) ≠ pell_rec(linear_recurrence); identity_on_pe |
| FALSE XOR as AND | miss | miss | form-family mismatch: bit_xor(bit_xor) ≠ bit_and(bit_and) |
| FALSE AND as XOR | miss | miss | form-family mismatch: bit_and(bit_and) ≠ bit_xor(bit_xor) |

## New distinct true transfers (not conserv×6, not Fib→Lucas)

| pair | result | why |
|------|--------|-----|
| parallel switches ≡ OR | hit | parallel≡OR |
| rec[1,1]→Lucas Δ | hit | TRANSFER rec([1,1])→Delta=prev on lucas |
| alg distrib mod5→mod7 | hit | exhaustive distrib mod 7 |
| alg mat_assoc mod3→mod5 | hit | sampled 2x2 assoc mod 5 |

New distinct form-families this round: `bit_or_parallel`, `poly_distrib_reuse`, `mat_assoc_reuse` (plus `rec_to_delta` on Lucas).

## Mouth grill fail cases

zero inventions. Fail list: []
Invention cases: []

Cassini-as-word → UNKNOWN. `cassini es la ley de pell` → cites rejected `false_cassini_as_pell_law`.
Nameless atoms (multiverso, alma) → UNKNOWN.

## Critic bugs fixed

1. **Cassini bilin on Pell identity holds** (same Q=-1 Lucas sequence) — raw identity is NOT a false analogy.
   False claim reframed as **cassini bilin = Pell defining rec [2,1]** → form-family gate rejects.
2. **Mouth**: bare `cassini` hitchhiked onto `transfer_bilin_cassini_shape_on_fib` as verified transfer — patched in `talk.py` (cassini word guard + retrieve hitchhike block).

## Files

- `motor/worlds/conserv_form.py` — form-family critic gate
- `motor/xfer_battery.py` — demand-15 false + true transfers + unique-form metric
- `motor/talk.py` — minimal cassini hitchhike fix
- `motor/runs/xfer-battery.json`, `motor/runs/demand-grill.json`
- `15-demand.md` (this file)

## Ceiling note

0.75 was inflated by counting 6 conservation directions as separate intelligence.
Unique-form ≈ **0.6923** with 9 real form families.
Adding more chem↔phys↔electro pairs would juice raw ratio without new forms — we did not.
