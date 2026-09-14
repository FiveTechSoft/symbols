# 14 — Transfer of form: not everything transfers

**Question (Anto):** ¿Todo lo que aprende aplica a otras áreas? ¿Puede extrapolar su razonamiento?

**Answer:** No. Intelligence here is **reuse of form under critic** — not dumping names.
A prior transfers only when the destination world accepts the same shape; otherwise the critic rejects.

- Intelligence metric = `0.75` = 12 hit / (12+4) attempts (0 skips excluded)
- Required negatives still failing (Pell / XOR-linear / bad Kepler / cassini→lucas): **True**
- New positives besides Fib→Lucas: **['rec fib→Δ(fib)', 'bit_fn AND→nets perceptron', 'bit_fn AND→electro switch', 'chem atom-balance → physics momentum', 'chem atom-balance → electro KCL', 'physics momentum → chem atoms', 'physics momentum → electro KCL', 'electro KCL → chem atoms', 'electro KCL → physics momentum', 'Kepler T²∝a³ → same table', 'bilinear cassini → fib']**

## Doctrine

- Not everything transfers.
- Intelligence = reuse of form under critic.
- Skip = shape does not apply (not a fail).
- Honest miss on Pell, XOR-as-linear, bogus Kepler exponent, cassini-on-lucas.

## Transfer table

| pair | expected | result | why |
|------|----------|--------|-----|
| rec fib→lucas | hit | hit | shared law [1, 1] |
| rec fib→pell | miss | miss | n=2: pred=1 != obs=2 |
| rec fib→Δ(fib) | hit | hit | TRANSFER rec(fib,[1,1])→Delta=prev |
| bit_fn AND→nets perceptron | hit | hit | AND linear-sep under bit_fn(and_all) prior |
| bit_fn AND→electro switch | hit | hit | series≡AND |
| bit_fn XOR→linear perceptron | miss | miss | XOR not linearly separable (honest negative) |
| chem atom-balance → physics momentum | hit | hit | momentum Δ=0 |
| chem atom-balance → electro KCL | hit | hit | KCL ΣI=0 |
| physics momentum → chem atoms | hit | hit | atom Δ=0 |
| physics momentum → electro KCL | hit | hit | KCL ΣI=0 |
| electro KCL → chem atoms | hit | hit | atom Δ=0 |
| electro KCL → physics momentum | hit | hit | momentum Δ=0 |
| Kepler T²∝a³ → same table | hit | hit | ratios≈39.478418 |
| Kepler → bogus T²∝a² | miss | miss | T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696, 118.4353] |
| bilinear cassini → fib | hit | hit | 14 checks offset_pm1 |
| bilinear cassini → lucas | miss | miss | fail n=1 lhs≠(-1)^n |

## Forms collected from live archive

- recs: `{'fib': [1, 1], 'lucas': [1, 1], 'pell': [2, 1]}`
- bit_fns: `[('bitfn_parity_is_parity', 'parity', 'parity'), ('bitfn_and_all_is_and_all', 'and_all', 'and_all'), ('bitfn_xor2_is_xor2', 'xor2', 'xor2')]`
- bilin fib present: `True`

## New verified / rejected transfer clauses (live archive)

- `verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_0_3_2', 'TRANSFER rec(fib,[0, 0, 3, 2]) ⇒ try on lucas: (0)*lucas(n-1) + (0)*lucas(n-2) + (3)*lucas(n-3) + (2)*lucas(n-4)')).`
- `rejected('transfer_pell_to_lucas_1_4_-1_-1', 'n=4: pred=13 != obs=7').`
- `rejected('transfer_pell_to_lucas_2_2_-2_-1', 'n=4: pred=10 != obs=7').`
- `rejected('transfer_fib_to_pell_4_-2_-3', 'n=3: pred=6 != obs=5').`
- `rejected('transfer_fib_to_pell_-3_5_4', 'n=3: pred=-1 != obs=5').`
- `rejected('transfer_fib_to_pell_2_-1_0_1', 'n=4: pred=8 != obs=12').`
- `rejected('transfer_fib_to_pell_0_0_3_2', 'n=4: pred=3 != obs=12').`
- `rejected('transfer_lucas_to_pell_4_-2_-3', 'n=3: pred=6 != obs=5').`
- `rejected('transfer_lucas_to_pell_-3_5_4', 'n=3: pred=-1 != obs=5').`
- `rejected('transfer_lucas_to_pell_2_-1_0_1', 'n=4: pred=8 != obs=12').`
- `rejected('transfer_lucas_to_pell_0_0_3_2', 'n=4: pred=3 != obs=12').`
- `verified(fact(nets, nets_perceptron, 'transfer_bitfn_and_to_perceptron_and_all', 'TRANSFER bit_fn(and_all=and_all) ⇒ AND linear-sep')).`
- `verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).`
- `verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_and_all_and_all_to_MI', 'TRANSFER bit_fn(and_all=and_all) ⇒ MI>0 on (X,X∧Y)')).`
- `verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_xor2_xor2_to_MI', 'TRANSFER bit_fn(xor2=xor2) ⇒ MI>0 on xor-coupled joint')).`
- `verified(fact(calculus, calc_transfer_form, 'transfer_rec_fib_to_delta_N12', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).`
- `verified(fact(nets, nets_transfer_form, 'transfer_bitfn_and_to_perceptron_form', 'TRANSFER bit_fn(and_all) ⇒ AND linear-sep (perceptron)')).`
- `verified(fact(electro, electro_switch_transfer, 'transfer_and_to_series_switch_and_all', 'TRANSFER bit_fn(and_all) ⇒ series switches')).`
- `rejected('transfer_bitfn_xor_to_linear_perceptron', 'XOR not linearly separable (honest negative)').`
- `verified(fact(physics, phys_transfer_form, 'transfer_conserv_chem_atoms_to_physics_conserv_mom', 'TRANSFER linear-Δ=0 (chem:atoms) ⇒ conserv_mom on physics')).`
- `verified(fact(electro, electro_transfer_form, 'transfer_conserv_chem_atoms_to_electro_conserv_kcl', 'TRANSFER linear-Δ=0 (chem:atoms) ⇒ conserv_kcl on electro')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_conserv_physics_momentum_to_chem_conserv_chem', 'TRANSFER linear-Δ=0 (physics:momentum) ⇒ conserv_chem on chem')).`
- `verified(fact(electro, electro_transfer_form, 'transfer_conserv_physics_momentum_to_electro_conserv_kcl', 'TRANSFER linear-Δ=0 (physics:momentum) ⇒ conserv_kcl on electro')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_conserv_electro_kcl_to_chem_conserv_chem', 'TRANSFER linear-Δ=0 (electro:kcl) ⇒ conserv_chem on chem')).`
- `verified(fact(physics, phys_transfer_form, 'transfer_conserv_electro_kcl_to_physics_conserv_mom', 'TRANSFER linear-Δ=0 (electro:kcl) ⇒ conserv_mom on physics')).`
- `verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n5', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).`
- `rejected('transfer_kepler_bogus_power_n5', 'T²/a² not const: [39.47841760435743, 59.21762640653614, 78.95683520871486, 98.69604401089356, 118.43525281307228]').`
- `verified(fact(sequences, bilinear_schema, 'transfer_bilin_cassini_shape_on_fib', 'TRANSFER cassini-shape bilin ⇒ fib(n+1)fib(n-1)-fib(n)^2=(-1)^n')).`
- `rejected('transfer_bilin_cassini_shape_on_lucas', 'fail n=1 lhs≠(-1)^n').`
- `verified(fact(calculus, calc_transfer_rec, 'transfer_rec_fib_to_delta_N4', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).`
- `verified(fact(calculus, calc_transfer_rec, 'transfer_rec_lucas_to_delta_N4', 'TRANSFER rec(lucas,[1, 1]) ⇒ Delta structure matches recurrence')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_conserv_mom_shape_to_chem_atoms', 'TRANSFER linear-Δ=0 (momentum shape) ⇒ atom counts balanced')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_conserv_kcl_shape_to_chem_atoms', 'TRANSFER linear-Δ=0 (KCL shape) ⇒ atom counts balanced')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_chem_conserv_form_on_physics_mom', 'TRANSFER chem atom-Δ=0 form ⇒ try on physics momentum')).`
- `verified(fact(chem, chem_transfer_form, 'transfer_chem_conserv_form_on_electro_kcl', 'TRANSFER chem atom-Δ=0 form ⇒ try on electro KCL')).`

## Before / after metric

| metric | value |
|--------|-------|
| before (Fib→Lucas/Pell only) | **0.5** |
| after (xfer battery) | **0.75** (12 hit / 16 attempts) |
| new positives besides Fib→Lucas | 11 |
| required negatives still fail | True |

Files: `motor/xfer_battery.py`, `motor/worlds/conserv_form.py`, `motor/runs/xfer-battery.json`, `14-transfer.md`; schemas `*_transfer_form` in SCIENCE_SEED (UCB-pickable).
