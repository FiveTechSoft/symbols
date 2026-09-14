% Autodidactic motor — growing Horn theory
% Appended by Python kernel; queried by critic (SWI or Horn fallback).
:- dynamic obs/3, rec/2, rejected/2, verified/1, lemma/3.
:- dynamic holds_bit/3, bit_fn/3, companion/2, true_mod/3.

% Companion graph for sequence transfer
companion(fib, lucas).
companion(fib, pell).
companion(lucas, fib).
companion(lucas, pell).
companion(pell, fib).
companion(pell, lucas).

obs(fib, 0, 0).
obs(fib, 1, 1).
obs(fib, 2, 1).
obs(fib, 3, 2).
obs(fib, 4, 3).
obs(fib, 5, 5).
obs(fib, 6, 8).
obs(fib, 7, 13).
obs(fib, 8, 21).
obs(fib, 9, 34).
obs(fib, 10, 55).
obs(fib, 11, 89).
obs(fib, 12, 144).
obs(fib, 13, 233).
obs(fib, 14, 377).
obs(fib, 15, 610).
obs(fib, 16, 987).
obs(fib, 17, 1597).
obs(fib, 18, 2584).
obs(fib, 19, 4181).
obs(fib, 20, 6765).
obs(fib, 21, 10946).
obs(fib, 22, 17711).
obs(fib, 23, 28657).
obs(fib, 24, 46368).
obs(lucas, 0, 2).
obs(lucas, 1, 1).
obs(lucas, 2, 3).
obs(lucas, 3, 4).
obs(lucas, 4, 7).
obs(lucas, 5, 11).
obs(lucas, 6, 18).
obs(lucas, 7, 29).
obs(lucas, 8, 47).
obs(lucas, 9, 76).
obs(lucas, 10, 123).
obs(lucas, 11, 199).
obs(lucas, 12, 322).
obs(lucas, 13, 521).
obs(lucas, 14, 843).
obs(lucas, 15, 1364).
obs(lucas, 16, 2207).
obs(lucas, 17, 3571).
obs(lucas, 18, 5778).
obs(lucas, 19, 9349).
obs(lucas, 20, 15127).
obs(lucas, 21, 24476).
obs(lucas, 22, 39603).
obs(lucas, 23, 64079).
obs(lucas, 24, 103682).
obs(pell, 0, 0).
obs(pell, 1, 1).
obs(pell, 2, 2).
obs(pell, 3, 5).
obs(pell, 4, 12).
obs(pell, 5, 29).
obs(pell, 6, 70).
obs(pell, 7, 169).
obs(pell, 8, 408).
obs(pell, 9, 985).
obs(pell, 10, 2378).
obs(pell, 11, 5741).
obs(pell, 12, 13860).
obs(pell, 13, 33461).
obs(pell, 14, 80782).
obs(pell, 15, 195025).
obs(pell, 16, 470832).
obs(pell, 17, 1136689).
obs(pell, 18, 2744210).
obs(pell, 19, 6625109).
obs(pell, 20, 15994428).
obs(pell, 21, 38613965).
obs(pell, 22, 93222358).
obs(pell, 23, 225058681).
obs(pell, 24, 543339720).
lemma('L1_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L1_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0', 'BC ∥ MN')).
% finite fail / dead-end
rejected('rec_fib_o1_none_linrec_scan', 'no fit').
% finite fail / dead-end
rejected('rec_lucas_o1_none_linrec_scan', 'no fit').
% finite fail / dead-end
rejected('rec_pell_o1_none_linrec_scan', 'no fit').
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_midline_lemma_reuse_p0', 'BC ∥ MN')).
lemma('L2_midline_parallel', Parallel, 'AC ∥ MX2').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L2_midline_parallel', 'AC ∥ MX2')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s3', 'AC ∥ MX2')).
% verified @ sequences/ratio_limits
verified(fact(sequences, ratio_limits, 'ratio_fib_to_phi', 'fib(n+1)/fib(n) → φ')).
% finite fail / dead-end
rejected('NEG_ratio_fib_to_e', '|ratio-e|=1.100e+00 (e=2.718282, φ=1.618034)').
% finite fail / dead-end
rejected('NEG_scalene_ab_eq_ac', 'finite fail (expected)').
% finite fail / dead-end
rejected('sym_compare_wait_no_bitfn', 'no bit_fn in archive yet').
% finite fail / dead-end
rejected('info_transfer_wait_no_bitfn', 'no bit_fn in archive').
% finite fail / dead-end
rejected('calc_transfer_wait_no_rec', 'no rec/2 in archive').
% verified @ nets/nets_perceptron
verified(fact(nets, nets_perceptron, 'nets_AND_linear_threshold', 'AND separable by linear threshold (exists w,b)')).
% verified @ nets/nets_perceptron
verified(fact(nets, nets_perceptron, 'nets_OR_linear_threshold', 'OR separable by linear threshold (exists w,b)')).
% verified @ nets/nets_chain_transfer
verified(fact(nets, nets_chain_transfer, 'nets_discrete_chain_vs_product', 'Delta(f∘g) vs Delta(f)·Delta(g) — composition identity check')).
% finite fail / dead-end
rejected('electro_switch_wait_no_bitfn', 'no bit_fn').
% verified @ electro/electro_switch_transfer
verified(fact(electro, electro_switch_transfer, 'electro_parallel_OR_table', 'parallel switches ≡ OR on {0,1}^2')).
% science skin symmetry gen=1
schema(sym_invariant_scan, unlocked(true)).
% science skin symmetry gen=1
schema(sym_group_table, unlocked(true)).
% science skin symmetry gen=1
schema(sym_dead_product, unlocked(true)).
% science skin symmetry gen=1
schema(sym_compare_transfer, unlocked(true)).
% science skin chance gen=1
schema(chance_bayes_scan, unlocked(true)).
% science skin chance gen=1
schema(chance_entropy_scan, unlocked(true)).
% science skin chance gen=1
schema(chance_dead_negH, unlocked(true)).
% science skin chance gen=1
schema(chance_extrapolate, unlocked(true)).
% science skin info gen=1
schema(info_mi_scan, unlocked(true)).
% science skin info gen=1
schema(info_transfer_bitfn, unlocked(true)).
% science skin info gen=1
schema(info_dead_mi0, unlocked(true)).
% science skin astro gen=1
schema(astro_period_scan, unlocked(true)).
% science skin astro gen=1
schema(astro_kepler_scan, unlocked(true)).
% science skin astro gen=1
schema(astro_angmom_scan, unlocked(true)).
% science skin astro gen=1
schema(astro_dead_linear, unlocked(true)).
% science skin algebra gen=1
schema(alg_poly_zn, unlocked(true)).
% science skin algebra gen=1
schema(alg_linear_2x2, unlocked(true)).
% science skin algebra gen=1
schema(alg_mat_assoc, unlocked(true)).
% science skin algebra gen=1
schema(alg_dead_binom, unlocked(true)).
% science skin calculus gen=1
schema(calc_fwd_diff, unlocked(true)).
% science skin calculus gen=1
schema(calc_ft_discrete, unlocked(true)).
% science skin calculus gen=1
schema(calc_power_diff, unlocked(true)).
% science skin calculus gen=1
schema(calc_transfer_rec, unlocked(true)).
% science skin calculus gen=1
schema(calc_dead_zero, unlocked(true)).
% science skin nets gen=1
schema(nets_perceptron, unlocked(true)).
% science skin nets gen=1
schema(nets_xor_depth, unlocked(true)).
% science skin nets gen=1
schema(nets_grad_step, unlocked(true)).
% science skin nets gen=1
schema(nets_chain_transfer, unlocked(true)).
% science skin nets gen=1
schema(nets_dead_any_eta, unlocked(true)).
% science skin electro gen=1
schema(electro_ohm, unlocked(true)).
% science skin electro gen=1
schema(electro_kirchhoff, unlocked(true)).
% science skin electro gen=1
schema(electro_switch_transfer, unlocked(true)).
% science skin electro gen=1
schema(electro_dead_noconserve, unlocked(true)).
% science skin physics gen=1
schema(phys_collision, unlocked(true)).
% science skin physics gen=1
schema(phys_action_reaction, unlocked(true)).
% science skin physics gen=1
schema(phys_dead_nomom, unlocked(true)).
% science skin chem gen=1
schema(chem_atom_balance, unlocked(true)).
% science skin chem gen=1
schema(chem_equilibrium_K, unlocked(true)).
% science skin chem gen=1
schema(chem_dead_noatoms, unlocked(true)).
% hypothesis-language skin gen=1
schema(linrec_scan, unlocked(true)).
% hypothesis-language skin gen=1
schema(transfer_horn, unlocked(true)).
% hypothesis-language skin gen=1
schema(ratio_scan, unlocked(true)).
% hypothesis-language skin gen=1
schema(dead_prime, unlocked(true)).
% hypothesis-language skin gen=1
schema(modperiod_schema, unlocked(false)).
% hypothesis-language skin gen=1
schema(bilinear_schema, unlocked(true)).
% hypothesis-language skin gen=1
schema(geo_invent, unlocked(true)).
% finite fail / dead-end
rejected('NEG_fib_always_prime', 'F(0)=0 not prime').
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% finite fail / dead-end
rejected('bilin_fib_bogus_const2', 'n=1: lhs=-1≠2').
% finite fail / dead-end
rejected('bilin_lucas_offset_pm1', 'fail n=1 lhs≠(-1)^n').
% finite fail / dead-end
rejected('bilin_lucas_r1', 'fail n=1').
% finite fail / dead-end
rejected('bilin_lucas_r2', 'fail n=2').
% finite fail / dead-end
rejected('bilin_lucas_bogus_const2', 'n=1: lhs=5≠2').
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod6', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 6) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod6', 'x(y+z) ≡ xy+xz (mod 6)')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n4', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n4', 'T>0 and a>0 for all generated bodies')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N5', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N5', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N5', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1190', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1190', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1191', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1191', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1192', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1192', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s1190', 'P(H|E)=0.39325435065996983 != P(H|¬E)=0.632326787609022').
% science skin symmetry gen=2
schema(sym_invariant_scan_n4_g2, unlocked(true)).
% science skin chance gen=2
schema(chance_entropy_scan_n3_g2, unlocked(true)).
% science skin info gen=2
schema(info_mi_scan_n4_g2, unlocked(true)).
% science skin astro gen=2
schema(astro_kepler_scan_n3_g2, unlocked(true)).
% science skin algebra gen=2
schema(alg_poly_zn_n9_g2, unlocked(true)).
% science skin calculus gen=2
schema(calc_fwd_diff_n7_g2, unlocked(true)).
% science skin nets gen=2
schema(nets_perceptron_n4_g2, unlocked(true)).
% science skin electro gen=2
schema(electro_ohm_n5_g2, unlocked(true)).
% science skin physics gen=2
schema(phys_collision_n4_g2, unlocked(true)).
% science skin chem gen=2
schema(chem_atom_balance_n4_g2, unlocked(true)).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_2H2+O2→2H2O', 'atom counts conserved: 2H2+O2→2H2O')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6064', 'atom counts conserved: synth_s6064')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6065', 'atom counts conserved: synth_s6065')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6066', 'atom counts conserved: synth_s6066')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6067', 'atom counts conserved: synth_s6067')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6064', 'mismatch {\'H\': 3, \'O\': 5} vs {\'H\': 4, \'O\': 5}').
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n5_s4096', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s4096', 'V≠I+R (as required)').
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s2186', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s2186', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s2186', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s2186', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
bit_fn('bitfn_parity_is_parity', parity, parity).
holds_bit(5000, [0,0,0], 0).
holds_bit(5001, [0,0,1], 1).
holds_bit(5002, [0,1,0], 1).
holds_bit(5003, [0,1,1], 0).
% verified @ logic/boolean_from_examples
verified(fact(logic, boolean_from_examples, 'bitfn_parity_is_parity', 'examples[parity] ⊨ parity')).
% finite fail / dead-end
rejected('bitfn_parity_is_and_all', 'bits=[0, 0, 1]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_parity_is_xor2', 'bits=[0, 0, 1]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_parity_is_const_0', 'bits=[0, 0, 1]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_parity_is_const_1', 'bits=[0, 0, 0]: pred=1 != 0').
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5157', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5157', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5158', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5158', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5159', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5159', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5160', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5160', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ symmetry/sym_compare_transfer
verified(fact(symmetry, sym_compare_transfer, 'transfer_parity_to_z2_parity_parity', 'TRANSFER bit_fn(parity=parity) ⇒ Z2/parity invariant')).
% verified @ info/info_transfer_bitfn
verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).
% science skin info gen=3
schema(info_mi_scan_n4_g3, unlocked(true)).
% science skin nets gen=3
schema(nets_perceptron_n4_g3, unlocked(true)).
% hypothesis-language skin gen=2
schema(modperiod_schema, unlocked(true)).
true_mod(fib, 2, 3).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m2', 'π_fib(2)=3')).
true_mod(fib, 3, 8).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m3', 'π_fib(3)=8')).
true_mod(fib, 4, 6).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m4', 'π_fib(4)=6')).
true_mod(fib, 5, 20).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m5', 'π_fib(5)=20')).
% finite fail / dead-end
rejected('period_fib_m6', 'insufficient prefix').
true_mod(lucas, 2, 3).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_lucas_m2', 'π_lucas(2)=3')).
% finite fail / dead-end
rejected('period_lucas_m3', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m4', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m5', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m6', 'insufficient prefix').
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o5', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o5', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o5', 'Z2 addition associative on full table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_n_o5', 'parity conserved on 5-bit strings')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s3223', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s3224', '2x2 Cramer exact on generated integer system')).
% verified @ astro/astro_kepler_scan
verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n3', 'T^2/a^3 constant within eps on circular table')).
% finite fail / dead-end
rejected('astro_kepler_wrong_exp_n3', 'T²/a² not const: [39.4784, 59.2176, 78.9568]').
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N4', 'Delta(sum x^0) = x^0 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x1_N4', 'Delta(sum x^1) = x^1 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x2_N4', 'Delta(sum x^2) = x^2 on prefix')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s1395', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s1395', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s1396', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s1396', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s1397', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s1397', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k4_s1395', 'H(uniform)≥H(peaked) for k=4')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n7', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n7', 'T>0 and a>0 for all generated bodies')).
% verified @ astro/astro_kepler_scan
verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n4', 'T^2/a^3 constant within eps on circular table')).
% finite fail / dead-end
rejected('astro_kepler_wrong_exp_n4', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h_eq_sqrt_a_n3', 'circular specific ang-mom h = sqrt(a) (GM=1) within eps')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h2_over_a_const_n3', 'h^2/a constant (=1) on circular 2-body table')).
% finite fail / dead-end
rejected('NEG_astro_T_prop_a', 'T/a not const: [6.2832, 7.6953, 8.8858]').
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod10', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 10) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod10', 'x(y+z) ≡ xy+xz (mod 10)')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s4092', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s4093', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_mat_assoc
verified(fact(algebra, alg_mat_assoc, 'alg_mat2_assoc_mod3', '(AB)C = A(BC) for all 2x2 matrices mod 3')).
% finite fail / dead-end
rejected('NEG_alg_binom_no_cross', 'x=1,y=1: (x+y)^2=1 != x^2+y^2=2').
% verified @ algebra/alg_poly_zn_n9_g2
verified(fact(algebra, alg_poly_zn_n9_g2, 'alg_binom_expand_mod9', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 9) exhaustive')).
% verified @ algebra/alg_poly_zn_n9_g2
verified(fact(algebra, alg_poly_zn_n9_g2, 'alg_distrib_mod9', 'x(y+z) ≡ xy+xz (mod 9)')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N8', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N8', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N8', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N5', 'Delta(sum x^0) = x^0 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x1_N5', 'Delta(sum x^1) = x^1 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x2_N5', 'Delta(sum x^2) = x^2 on prefix')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x1_vs_n_x0_N4', 'Delta(x^1) / (power * x^0) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x2_vs_n_x1_N4', 'Delta(x^2) / (power * x^1) → 1 (eps on lattice)')).
% finite fail / dead-end
rejected('NEG_calc_delta_always_zero', 'Delta[0]=1.0 != 0').
% verified @ calculus/calc_fwd_diff_n7_g2
verified(fact(calculus, calc_fwd_diff_n7_g2, 'calc_delta_fib_N7', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff_n7_g2
verified(fact(calculus, calc_fwd_diff_n7_g2, 'calc_delta_lucas_N7', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff_n7_g2
verified(fact(calculus, calc_fwd_diff_n7_g2, 'calc_delta_fib_eq_prev_N7', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% finite fail / dead-end
rejected('NEG_nets_XOR_linear_threshold', 'no linear threshold for XOR').
% verified @ nets/nets_xor_depth
verified(fact(nets, nets_xor_depth, 'nets_XOR_two_layer_composition', 'XOR = (OR) AND NOT(AND) two-layer composition')).
% verified @ nets/nets_grad_step
verified(fact(nets, nets_grad_step, 'nets_grad_step_se_s0', 'Δw = -η x (yhat-y) on squared-error toy (note sign)')).
% verified @ nets/nets_grad_step
verified(fact(nets, nets_grad_step, 'nets_grad_step_se_s1', 'Δw = -η x (yhat-y) on squared-error toy (note sign)')).
% finite fail / dead-end
rejected('NEG_nets_loss_decreases_any_eta', 'eta=10.0 overshoot L 1.0→361.0').
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n7_s4700', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s4700', 'V≠I+R (as required)').
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4696', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4697', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4698', 'sum I_k = 0 at 3-edge node')).
% finite fail / dead-end
rejected('NEG_electro_current_not_conserved', 'sum=0.0≈0 so conservation holds (reject claim)').
% verified @ electro/electro_ohm_n5_g2
verified(fact(electro, electro_ohm_n5_g2, 'electro_ohm_V_eq_IR_n5_s4698', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s4698', 'V≠I+R (as required)').
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5897', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5897', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5898', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5898', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5899', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5899', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5900', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5900', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5901', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5901', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5902', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5902', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5893', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5894', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5895', 'F_12 + F_21 = 0')).
% finite fail / dead-end
rejected('phys_action_same_sign_broken_s5893', 'F12=1.1853407156236426 F21=1.1853407156236426 not opposite').
% finite fail / dead-end
rejected('NEG_phys_momentum_not_conserved', '|Δp|=0.0≈0 (conservation holds)').
% verified @ physics/phys_collision_n4_g2
verified(fact(physics, phys_collision_n4_g2, 'phys_mom_conserve_s5895', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision_n4_g2
verified(fact(physics, phys_collision_n4_g2, 'phys_energy_conserve_s5895', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision_n4_g2
verified(fact(physics, phys_collision_n4_g2, 'phys_mom_conserve_s5896', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision_n4_g2
verified(fact(physics, phys_collision_n4_g2, 'phys_energy_conserve_s5896', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6500', 'atom counts conserved: synth_s6500')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6501', 'atom counts conserved: synth_s6501')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6502', 'atom counts conserved: synth_s6502')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6503', 'atom counts conserved: synth_s6503')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6504', 'atom counts conserved: synth_s6504')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6500', 'mismatch {\'H\': 3, \'O\': 3} vs {\'H\': 4, \'O\': 3}').
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6498', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6499', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6500', 'K = [B]/[A] on 2-species toy')).
% finite fail / dead-end
rejected('NEG_chem_atoms_not_conserved', 'counts match — conservation holds (reject claim)').
% verified @ chem/chem_atom_balance_n4_g2
verified(fact(chem, chem_atom_balance_n4_g2, 'chem_atom_balance_synth_s6499', 'atom counts conserved: synth_s6499')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6499', 'mismatch {\'H\': 2, \'O\': 3} vs {\'H\': 3, \'O\': 3}').
% science skin symmetry gen=4
schema(sym_invariant_scan_n6_g4, unlocked(true)).
% science skin chance gen=4
schema(chance_entropy_scan_n6_g4, unlocked(true)).
% science skin info gen=4
schema(info_mi_scan_n4_g4, unlocked(true)).
% science skin astro gen=4
schema(astro_kepler_scan_n4_g4, unlocked(true)).
% science skin algebra gen=4
schema(alg_poly_zn_n11_g4, unlocked(true)).
% science skin calculus gen=4
schema(calc_fwd_diff_n9_g4, unlocked(true)).
% science skin nets gen=4
schema(nets_perceptron_n4_g4, unlocked(true)).
% science skin electro gen=4
schema(electro_ohm_n8_g4, unlocked(true)).
% science skin physics gen=4
schema(phys_collision_n6_g4, unlocked(true)).
% science skin chem gen=4
schema(chem_atom_balance_n5_g4, unlocked(true)).
