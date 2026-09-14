# 13 — STEM worlds: astro · algebra · calculus · nets · electro · physics · chem

**Scratch archive:** `motor/archive-stem/` (live mouth `motor/archive/` untouched — no reset).
**Doctrine:** NO textbooks, NO Wikipedia dumps, NO hardcoded essays, NO periodic-table facts, NO cosmology prose.
Only `obs → hypothesize → critic → verified/rejected`. Child brain = `theory.pl`.
UNKNOWN on names that are not atoms (mouth stays honest).

## Worlds registered in `build_worlds()`

| world | observation | productive families | dead-end |
|-------|-------------|---------------------|----------|
| astro | generated Keplerian (T,a) circular table | period / Kepler T²/a³ / ang-mom | T ∝ a |
| algebra | Z/nZ poly, 2×2 exact, mat assoc mod p | poly / linear / mat | (x+y)²=x²+y² |
| calculus | Δ + sum on fib/lucas + polynomials | fwd-diff / discrete FTC / power-diff / rec transfer | Δ≡0 |
| nets | AND/OR/XOR tables, 2-weight SE toy | perceptron / xor-depth / grad-step / chain | any-η loss↓ |
| electro | (V,I,R) triples; 3-edge KCL node | Ohm / Kirchhoff / switch transfer | I not conserved |
| physics | 1D elastic m,v; force pairs | collision / action-reaction | p not conserved |
| chem | generated count-vector reactions; K toy | atom-balance / equilibrium K | atoms not conserved |

Kepler lives only in **astro** (not duplicated in physics). Chem has **no** periodic table dump.

## Metrics (archive-stem)

- n_verified: **128**
- n_rejected: **42**
- n_types: **41**
- n_spawned (non-seed schemas): **22** → `['alg_poly_zn_n11_g4', 'alg_poly_zn_n9_g2', 'astro_kepler_scan_n3_g2', 'astro_kepler_scan_n4_g4', 'calc_fwd_diff_n7_g2', 'calc_fwd_diff_n9_g4', 'chance_entropy_scan_n3_g2', 'chance_entropy_scan_n6_g4', 'chem_atom_balance_n4_g2', 'chem_atom_balance_n5_g4', 'electro_ohm_n5_g2', 'electro_ohm_n8_g4', 'info_mi_scan_n4_g2', 'info_mi_scan_n4_g3', 'info_mi_scan_n4_g4', 'nets_perceptron_n4_g2', 'nets_perceptron_n4_g3', 'nets_perceptron_n4_g4', 'phys_collision_n4_g2', 'phys_collision_n6_g4', 'sym_invariant_scan_n4_g2', 'sym_invariant_scan_n6_g4']`

### Per-world skin (STEM)

- **astro**: gen=4 classes=6 spawned=['astro_kepler_scan_n3_g2', 'astro_kepler_scan_n4_g4']
- **algebra**: gen=4 classes=6 spawned=['alg_poly_zn_n11_g4', 'alg_poly_zn_n9_g2']
- **calculus**: gen=4 classes=7 spawned=['calc_fwd_diff_n7_g2', 'calc_fwd_diff_n9_g4']
- **nets**: gen=4 classes=8 spawned=['nets_perceptron_n4_g2', 'nets_perceptron_n4_g3', 'nets_perceptron_n4_g4']
- **electro**: gen=4 classes=6 spawned=['electro_ohm_n5_g2', 'electro_ohm_n8_g4']
- **physics**: gen=4 classes=5 spawned=['phys_collision_n4_g2', 'phys_collision_n6_g4']
- **chem**: gen=4 classes=5 spawned=['chem_atom_balance_n4_g2', 'chem_atom_balance_n5_g4']

## How this is not a textbook

- Tables are **generated or computed** (Keplerian circular with GM=1; random Ohm triples;
  elastic finals from exact 1D formulae; stoich count vectors) — not copied encyclopedia rows.
- Every clause is critic-gated on finite checks (eps or exhaustive mod n).
- Dead-end families exist so curiosity pays a tax when a false law is claimed.
- Saturation → `science_lang.molt` spawns **non-seed** schema ids (`*_n*_g*`); novelty floor stays alive.
- No PyTorch, no Swiss-ephemeris hard dependency, no backprop essay, no cosmology prose.

## Transfer of form across worlds

- **calculus ← sequences:** `rec/2` prior → Δ structure on fib/lucas (honest miss if no rec yet).
- **nets ← logic:** `bit_fn(and_all)` → AND linear-sep; XOR depth lesson (linear FAIL, 2-layer OK).
- **nets ← calculus:** discrete chain Δ(f∘g) for affine f.
- **electro ← logic:** AND/OR as series/parallel switches when bit_fn present.
- Form transfer ≠ dumping named facts. Mouth stays UNKNOWN on non-atom names.

## Example clauses (verified + rejected per STEM world)

Core six (2× astro / algebra / calculus) plus nets/electro/physics/chem as steered:

1. **verified** (`astro`) — Kepler T²/a³ constancy on generated circular table
   `verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n3', 'T^2/a^3 constant within eps on circular table')).`
2. **rejected** (`astro`) — broken Kepler exponent T²/a² — critic finite-fail
   `rejected('astro_kepler_wrong_exp_n3', 'T²/a² not const: [39.4784, 59.2176, 78.9568]').`
3. **verified** (`algebra`) — poly identity on Z/nZ — exhaustive critic
   `verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod6', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 6) exhaustive')).`
4. **rejected** (`algebra`) — dead-end (x+y)²=x²+y² rejected
   `rejected('NEG_alg_binom_no_cross', 'x=1,y=1: (x+y)^2=1 != x^2+y^2=2').`
5. **verified** (`calculus`) — discrete FTC / fwd-diff on prefix
   `verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N4', 'Delta(sum x^0) = x^0 on prefix')).`
6. **rejected** (`calculus`) — dead-end Δ≡0 rejected
   `rejected('NEG_calc_delta_always_zero', 'Delta[0]=1.0 != 0').`
7. **verified** (`nets`) — XOR via 2-layer/composition — lesson not a blog post
   `verified(fact(nets, nets_xor_depth, 'nets_XOR_two_layer_composition', 'XOR = (OR) AND NOT(AND) two-layer composition')).`
8. **rejected** (`nets`) — XOR single linear threshold MUST fail
   `rejected('NEG_nets_XOR_linear_threshold', 'no linear threshold for XOR').`
9. **verified** (`electro`) — Ohm V=IR on generated triples
   `verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n5_s4096', 'V=IR on generated triples (eps)')).`
10. **rejected** (`electro`) — current non-conservation / broken Ohm rejected
   `rejected('NEG_electro_current_not_conserved', 'sum=0.0≈0 so conservation holds (reject claim)').`
11. **verified** (`physics`) — 1D elastic momentum conservation
   `verified(fact(physics, phys_collision, 'phys_mom_conserve_s5157', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).`
12. **rejected** (`physics`) — dead-end momentum-not-conserved rejected
   `rejected('NEG_phys_momentum_not_conserved', '|Δp|=0.0≈0 (conservation holds)').`
13. **verified** (`chem`) — atom-count conservation on generated reaction
   `verified(fact(chem, chem_atom_balance, 'chem_atom_balance_2H2+O2→2H2O', 'atom counts conserved: 2H2+O2→2H2O')).`
14. **rejected** (`chem`) — atoms-not-conserved / unbalanced rejected
   `rejected('NEG_chem_atoms_not_conserved', 'counts match — conservation holds (reject claim)').`

## Success checks

- `build_worlds()` includes astro, algebra, calculus, nets, electro, physics, chem.
- Critic gated; zero cosmology / periodic-table prose facts.
- Spawned non-seed schemas present (molt ran).
- `python -m motor tick --steps 2` on DEFAULT `motor/archive/` still works (live mouth not reset).

## Honesty / limits

- Seed operators in `SCIENCE_SEED`, generators, epsilon, and UCB are human-authored.
- Finite run / force-ingest of dead-ends for coverage ≠ finished learning.
- Swiss ephemeris files may exist under `/workspace/ephe` but are unused — generated Kepler is more honest here.
- Gradient toy is one analytic SE step on 2 weights — not deep learning.
- Chemistry is count-algebra on tiny reactions — not spectroscopy or orbitals.

