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
lemma('L2_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L2_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s3', 'BC ∥ MN')).
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
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1139', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1139', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1140', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1140', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1141', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1141', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s1139', 'P(H|E)=0.896539891664867 != P(H|¬E)=0.663110399724556').
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s2120', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s2120', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s2120', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s2120', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
bit_fn('bitfn_parity_is_parity', parity, parity).
holds_bit(1000, [0,0,0], 0).
holds_bit(1001, [0,0,1], 1).
holds_bit(1002, [0,1,0], 1).
holds_bit(1003, [0,1,1], 0).
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
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o3', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o3', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o3', 'Z2 addition associative on full table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_n_o3', 'parity conserved on 3-bit strings')).
% verified @ symmetry/sym_compare_transfer
verified(fact(symmetry, sym_compare_transfer, 'transfer_parity_to_z2_parity_parity', 'TRANSFER bit_fn(parity=parity) ⇒ Z2/parity invariant')).
% verified @ info/info_transfer_bitfn
verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).
% science skin symmetry gen=2
schema(sym_invariant_scan_n5_g2, unlocked(true)).
% science skin chance gen=2
schema(chance_entropy_scan_n3_g2, unlocked(true)).
% science skin info gen=2
schema(info_mi_scan_n4_g2, unlocked(true)).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s1207', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s1207', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s1208', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s1208', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s1209', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s1209', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k3_s1207', 'H(uniform)≥H(peaked) for k=3')).
% finite fail / dead-end
rejected('NEG_parity_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_and_all_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_xor2_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_z2_abelian_o2', 'Z2 table commutative + identity 0')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_klein_every_nonid_order2_o2', 'Klein: every non-identity element has order 2')).
% finite fail / dead-end
rejected('sym_z2_claim_mult_o2', '1+1=0 != 1*1=1').
% finite fail / dead-end
rejected('NEG_chance_H_negative', 'H=0.977854098489898 not < 0').
% finite fail / dead-end
rejected('NEG_chance_mix_decreases_H', 'H(mix)=0.9944848808171503 >= min=0.977854098489898').
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
% finite fail / dead-end
rejected('NEG_info_MI_always_0', 'MI=0.4990109628759932 ≠ 0').
% finite fail / dead-end
rejected('NEG_sym_product_mod2_conserved', 'flip0 [0, 1, 1]→[1, 1, 1] prod 0→1').
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_le_logk_k4', 'EXTRAPOLATE H_le_logk to k=4')).
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_nonneg_k4', 'EXTRAPOLATE H_nonneg to k=4')).
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_uni_ge_peaked_k4', 'EXTRAPOLATE H_uni_ge_peaked to k=4')).
% verified @ info/info_mi_scan_n4_g2
verified(fact(info, info_mi_scan_n4_g2, 'info_MI_indep_near0_s2290', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan_n4_g2
verified(fact(info, info_mi_scan_n4_g2, 'info_MI_dep_pos_s2290', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan_n4_g2
verified(fact(info, info_mi_scan_n4_g2, 'info_indep_factorization_s2290', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan_n4_g2
verified(fact(info, info_mi_scan_n4_g2, 'info_MI_and_related_s2290', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
