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
% hypothesis-language skin gen=0
schema(linrec_scan, unlocked(true)).
% hypothesis-language skin gen=0
schema(transfer_horn, unlocked(true)).
% hypothesis-language skin gen=0
schema(ratio_scan, unlocked(true)).
% hypothesis-language skin gen=0
schema(dead_prime, unlocked(true)).
% hypothesis-language skin gen=0
schema(modperiod_schema, unlocked(false)).
% hypothesis-language skin gen=0
schema(bilinear_schema, unlocked(false)).
% hypothesis-language skin gen=0
schema(geo_invent, unlocked(true)).
lemma('L1_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L1_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0', 'BC ∥ MN')).
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
% finite fail / dead-end
rejected('rec_fib_o1_none_linrec_scan', 'no fit').
% finite fail / dead-end
rejected('rec_lucas_o1_none_linrec_scan', 'no fit').
% finite fail / dead-end
rejected('rec_pell_o1_none_linrec_scan', 'no fit').
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_midline_lemma_reuse_p0', 'BC ∥ MN')).
% finite fail / dead-end
rejected('NEG_parity_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_and_all_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_xor2_const1', 'bits=[0, 0, 0]: pred=1 != 0').
lemma('L2_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L2_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s6', 'BC ∥ MN')).
% verified @ sequences/ratio_limits
verified(fact(sequences, ratio_limits, 'ratio_fib_to_phi', 'fib(n+1)/fib(n) → φ')).
% finite fail / dead-end
rejected('NEG_ratio_fib_to_e', '|ratio-e|=1.100e+00 (e=2.718282, φ=1.618034)').
% finite fail / dead-end
rejected('NEG_scalene_ab_eq_ac', 'finite fail (expected)').
% finite fail / dead-end
rejected('NEG_fib_always_prime', 'F(0)=0 not prime').
% hypothesis-language skin gen=1
schema(bilinear_schema, unlocked(true)).
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
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% finite fail / dead-end
rejected('bilin_lucas_r3', 'fail n=3').
lemma('L3_midline_half', HalfSeg, '2·MN = BC').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L3_midline_half', '2·MN = BC')).
lemma('L4_midline_parallel', Parallel, 'AC ∥ MP').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L4_midline_parallel', 'AC ∥ MP')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d2_s15', 'AC ∥ MP')).
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% finite fail / dead-end
rejected('bilin_lucas_r4', 'fail n=4').
% finite fail / dead-end
rejected('bitfn_and_all_is_parity', 'bits=[0, 0, 1]: pred=1 != 0').
bit_fn('bitfn_and_all_is_and_all', and_all, and_all).
holds_bit(0, [0,0,0], 0).
holds_bit(1, [0,0,1], 0).
holds_bit(2, [0,1,0], 0).
holds_bit(3, [0,1,1], 0).
% verified @ logic/boolean_from_examples
verified(fact(logic, boolean_from_examples, 'bitfn_and_all_is_and_all', 'examples[and_all] ⊨ and_all')).
% finite fail / dead-end
rejected('bitfn_and_all_is_xor2', 'bits=[0, 1, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('bitfn_and_all_is_const_0', 'bits=[1, 1, 1]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_and_all_is_const_1', 'bits=[0, 0, 0]: pred=1 != 0').
lemma('L5_midline_parallel', Parallel, 'MN ∥ NX2').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L5_midline_parallel', 'MN ∥ NX2')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d3_s18', 'MN ∥ NX2')).
% finite fail / dead-end
rejected('bitfn_xor2_is_parity', 'bits=[0, 0, 1]: pred=1 != 0').
% finite fail / dead-end
rejected('bitfn_xor2_is_and_all', 'bits=[0, 1, 0]: pred=0 != 1').
bit_fn('bitfn_xor2_is_xor2', xor2, xor2).
holds_bit(9000, [0,0,0], 0).
holds_bit(9001, [0,0,1], 0).
holds_bit(9002, [0,1,0], 1).
holds_bit(9003, [0,1,1], 1).
% verified @ logic/boolean_from_examples
verified(fact(logic, boolean_from_examples, 'bitfn_xor2_is_xor2', 'examples[xor2] ⊨ xor2')).
% finite fail / dead-end
rejected('bitfn_xor2_is_const_0', 'bits=[0, 1, 0]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_xor2_is_const_1', 'bits=[0, 0, 0]: pred=1 != 0').
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
true_mod(fib, 7, 16).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m7', 'π_fib(7)=16')).
true_mod(fib, 8, 12).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m8', 'π_fib(8)=12')).
% finite fail / dead-end
rejected('period_lucas_m7', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m8', 'insufficient prefix').
% finite fail / dead-end
rejected('period_fib_m9', 'insufficient prefix').
% finite fail / dead-end
rejected('period_fib_m10', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m9', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m10', 'insufficient prefix').
true_mod(fib, 11, 10).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'period_fib_m11', 'π_fib(11)=10')).
% finite fail / dead-end
rejected('period_fib_m12', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m11', 'insufficient prefix').
% finite fail / dead-end
rejected('period_lucas_m12', 'insufficient prefix').
lemma('L6_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L6_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d3_s25', 'BC ∥ MN')).
lemma('L7_midline_parallel', Parallel, 'NP ∥ NX3').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L7_midline_parallel', 'NP ∥ NX3')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d4_s29', 'NP ∥ NX3')).
lemma('L8_isos_base', EqAng, '∠X0CX1 = ∠CX0X1').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L8_isos_base', '∠X0CX1 = ∠CX0X1')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d4_s32', '∠X0CX1 = ∠CX0X1')).
lemma('L9_EqSeg', EqSeg, 'BM = AM').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L9_EqSeg', 'BM = AM')).
% learned recurrence on fib
rec(fib, [1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o2_1_1', 'fib(n) = (1)*fib(n-1) + (1)*fib(n-2)')).
% learned recurrence on fib
rec(fib, [1,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_1_1_0', 'fib(n) = (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [0,2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_0_2_1', 'fib(n) = (0)*fib(n-1) + (2)*fib(n-2) + (1)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [1,1,0,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_1_0_0', 'fib(n) = (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [0,2,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_2_1_0', 'fib(n) = (0)*fib(n-1) + (2)*fib(n-2) + (1)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o2_1_1', 'lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2)')).
% learned recurrence on lucas
rec(lucas, [1,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_1_1_0', 'lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [0,2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_0_2_1', 'lucas(n) = (0)*lucas(n-1) + (2)*lucas(n-2) + (1)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [1,1,0,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_1_0_0', 'lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [0,2,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_2_1_0', 'lucas(n) = (0)*lucas(n-1) + (2)*lucas(n-2) + (1)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o2_2_1', 'pell(n) = (2)*pell(n-1) + (1)*pell(n-2)')).
% learned recurrence on pell
rec(pell, [2,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_2_1_0', 'pell(n) = (2)*pell(n-1) + (1)*pell(n-2) + (0)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [1,3,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_1_3_1', 'pell(n) = (1)*pell(n-1) + (3)*pell(n-2) + (1)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [2,1,0,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_2_1_0_0', 'pell(n) = (2)*pell(n-1) + (1)*pell(n-2) + (0)*pell(n-3) + (0)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [1,3,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_1_3_1_0', 'pell(n) = (1)*pell(n-1) + (3)*pell(n-2) + (1)*pell(n-3) + (0)*pell(n-4)')).
% hypothesis-language skin gen=4
schema(bilinear_schema_r4_g4, unlocked(true)).
% verified @ sequences/bilinear_schema_r4_g4
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema_r4_g4
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema_r4_g4
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% verified @ sequences/bilinear_schema_r4_g4
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% verified @ sequences/bilinear_schema_r4_g4
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% learned recurrence on fib
rec(fib, [2,0,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_2_0_-1', 'fib(n) = (2)*fib(n-1) + (0)*fib(n-2) + (-1)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [-1,3,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_-1_3_2', 'fib(n) = (-1)*fib(n-1) + (3)*fib(n-2) + (2)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [1,0,1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_0_1_1', 'fib(n) = (1)*fib(n-1) + (0)*fib(n-2) + (1)*fib(n-3) + (1)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [2,0,-1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_2_0_-1_0', 'fib(n) = (2)*fib(n-1) + (0)*fib(n-2) + (-1)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [2,0,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_2_0_-1', 'lucas(n) = (2)*lucas(n-1) + (0)*lucas(n-2) + (-1)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [-1,3,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_-1_3_2', 'lucas(n) = (-1)*lucas(n-1) + (3)*lucas(n-2) + (2)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [1,0,1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_0_1_1', 'lucas(n) = (1)*lucas(n-1) + (0)*lucas(n-2) + (1)*lucas(n-3) + (1)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [2,0,-1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_2_0_-1_0', 'lucas(n) = (2)*lucas(n-1) + (0)*lucas(n-2) + (-1)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [3,-1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_3_-1_-1', 'pell(n) = (3)*pell(n-1) + (-1)*pell(n-2) + (-1)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [0,5,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_0_5_2', 'pell(n) = (0)*pell(n-1) + (5)*pell(n-2) + (2)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [2,0,2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_2_0_2_1', 'pell(n) = (2)*pell(n-1) + (0)*pell(n-2) + (2)*pell(n-3) + (1)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [3,-1,-1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_-1_-1_0', 'pell(n) = (3)*pell(n-1) + (-1)*pell(n-2) + (-1)*pell(n-3) + (0)*pell(n-4)')).
lemma('L10_para_opp', Parallel, 'AD ∥ BC').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L10_para_opp', 'AD ∥ BC')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d4_s42', 'AD ∥ BC')).
lemma('L11_EqSeg', EqSeg, 'BM = AM').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L11_EqSeg', 'BM = AM')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_1', 'TRANSFER rec(lucas,[1, 1]) ⇒ try on fib: (1)*fib(n-1) + (1)*fib(n-2)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_1_0', 'TRANSFER rec(lucas,[1, 1, 0]) ⇒ try on fib: (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_2_1', 'TRANSFER rec(lucas,[0, 2, 1]) ⇒ try on fib: (0)*fib(n-1) + (2)*fib(n-2) + (1)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_1_0_0', 'TRANSFER rec(lucas,[1, 1, 0, 0]) ⇒ try on fib: (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3) + (0)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_2_1_0', 'TRANSFER rec(lucas,[0, 2, 1, 0]) ⇒ try on fib: (0)*fib(n-1) + (2)*fib(n-2) + (1)*fib(n-3) + (0)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_1', 'n=2: pred=2 != obs=1').
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_1_0', 'n=3: pred=3 != obs=2').
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_3_1', 'n=3: pred=4 != obs=2').
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_1_0_0', 'n=4: pred=5 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_3_1_0', 'n=4: pred=6 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_2_0_-1', 'TRANSFER rec(lucas,[2, 0, -1]) ⇒ try on fib: (2)*fib(n-1) + (0)*fib(n-2) + (-1)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_3_2', 'TRANSFER rec(lucas,[-1, 3, 2]) ⇒ try on fib: (-1)*fib(n-1) + (3)*fib(n-2) + (2)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_0_1_1', 'TRANSFER rec(lucas,[1, 0, 1, 1]) ⇒ try on fib: (1)*fib(n-1) + (0)*fib(n-2) + (1)*fib(n-3) + (1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_2_0_-1_0', 'TRANSFER rec(lucas,[2, 0, -1, 0]) ⇒ try on fib: (2)*fib(n-1) + (0)*fib(n-2) + (-1)*fib(n-3) + (0)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_-1_-1', 'n=4: pred=4 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_0_5_2', 'n=3: pred=5 != obs=2').
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_0_2_1', 'n=4: pred=6 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_-1_-1_0', 'n=4: pred=4 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_1', 'TRANSFER rec(fib,[1, 1]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_1_0', 'TRANSFER rec(fib,[1, 1, 0]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_2_1', 'TRANSFER rec(fib,[0, 2, 1]) ⇒ try on lucas: (0)*lucas(n-1) + (2)*lucas(n-2) + (1)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_1_0_0', 'TRANSFER rec(fib,[1, 1, 0, 0]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3) + (0)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_2_1_0', 'TRANSFER rec(fib,[0, 2, 1, 0]) ⇒ try on lucas: (0)*lucas(n-1) + (2)*lucas(n-2) + (1)*lucas(n-3) + (0)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_1', 'n=2: pred=4 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_1_0', 'n=3: pred=7 != obs=4').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_3_1', 'n=3: pred=8 != obs=4').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_1_0_0', 'n=4: pred=11 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_3_1_0', 'n=4: pred=14 != obs=7').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_2_0_-1', 'TRANSFER rec(fib,[2, 0, -1]) ⇒ try on lucas: (2)*lucas(n-1) + (0)*lucas(n-2) + (-1)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_3_2', 'TRANSFER rec(fib,[-1, 3, 2]) ⇒ try on lucas: (-1)*lucas(n-1) + (3)*lucas(n-2) + (2)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_0_1_1', 'TRANSFER rec(fib,[1, 0, 1, 1]) ⇒ try on lucas: (1)*lucas(n-1) + (0)*lucas(n-2) + (1)*lucas(n-3) + (1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_2_0_-1_0', 'TRANSFER rec(fib,[2, 0, -1, 0]) ⇒ try on lucas: (2)*lucas(n-1) + (0)*lucas(n-2) + (-1)*lucas(n-3) + (0)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_-1_-1', 'n=3: pred=6 != obs=4').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_0_5_2', 'n=3: pred=9 != obs=4').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_0_2_1', 'n=4: pred=12 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_-1_-1_0', 'n=4: pred=8 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_1', 'n=2: pred=1 != obs=2').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_1_0', 'n=3: pred=3 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_2_1', 'n=3: pred=2 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_1_0_0', 'n=4: pred=7 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_2_1_0', 'n=4: pred=5 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_1', 'n=2: pred=1 != obs=2').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_1_0', 'n=3: pred=3 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_2_1', 'n=3: pred=2 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_1_0_0', 'n=4: pred=7 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_2_1_0', 'n=4: pred=5 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_2_0_-1', 'n=3: pred=4 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_3_2', 'n=3: pred=1 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_0_1_1', 'n=4: pred=6 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_2_0_-1_0', 'n=4: pred=9 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_2_0_-1', 'n=3: pred=4 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_3_2', 'n=3: pred=1 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_0_1_1', 'n=4: pred=6 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_2_0_-1_0', 'n=4: pred=9 != obs=12').
% hypothesis-language skin gen=5
schema(bilinear_schema_r4_g5, unlocked(true)).
% verified @ sequences/bilinear_schema_r4_g5
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema_r4_g5
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema_r4_g5
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% verified @ sequences/bilinear_schema_r4_g5
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% verified @ sequences/bilinear_schema_r4_g5
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% learned recurrence on fib
rec(fib, [3,-1,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_3_-1_-2', 'fib(n) = (3)*fib(n-1) + (-1)*fib(n-2) + (-2)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [-2,4,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_-2_4_3', 'fib(n) = (-2)*fib(n-1) + (4)*fib(n-2) + (3)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [0,1,2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_1_2_1', 'fib(n) = (0)*fib(n-1) + (1)*fib(n-2) + (2)*fib(n-3) + (1)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [0,3,0,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_3_0_-1', 'fib(n) = (0)*fib(n-1) + (3)*fib(n-2) + (0)*fib(n-3) + (-1)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [3,-1,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_3_-1_-2', 'lucas(n) = (3)*lucas(n-1) + (-1)*lucas(n-2) + (-2)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [-2,4,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_-2_4_3', 'lucas(n) = (-2)*lucas(n-1) + (4)*lucas(n-2) + (3)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [0,1,2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_1_2_1', 'lucas(n) = (0)*lucas(n-1) + (1)*lucas(n-2) + (2)*lucas(n-3) + (1)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [0,3,0,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_3_0_-1', 'lucas(n) = (0)*lucas(n-1) + (3)*lucas(n-2) + (0)*lucas(n-3) + (-1)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [4,-3,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_4_-3_-2', 'pell(n) = (4)*pell(n-1) + (-3)*pell(n-2) + (-2)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [5,-5,-3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_5_-5_-3', 'pell(n) = (5)*pell(n-1) + (-5)*pell(n-2) + (-3)*pell(n-3)')).
% learned recurrence on pell
rec(pell, [0,5,2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_0_5_2_0', 'pell(n) = (0)*pell(n-1) + (5)*pell(n-2) + (2)*pell(n-3) + (0)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [1,2,3,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_1_2_3_1', 'pell(n) = (1)*pell(n-1) + (2)*pell(n-2) + (3)*pell(n-3) + (1)*pell(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_3_-1_-2', 'TRANSFER rec(lucas,[3, -1, -2]) ⇒ try on fib: (3)*fib(n-1) + (-1)*fib(n-2) + (-2)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-2_4_3', 'TRANSFER rec(lucas,[-2, 4, 3]) ⇒ try on fib: (-2)*fib(n-1) + (4)*fib(n-2) + (3)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_1_2_1', 'TRANSFER rec(lucas,[0, 1, 2, 1]) ⇒ try on fib: (0)*fib(n-1) + (1)*fib(n-2) + (2)*fib(n-3) + (1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_3_0_-1', 'TRANSFER rec(lucas,[0, 3, 0, -1]) ⇒ try on fib: (0)*fib(n-1) + (3)*fib(n-2) + (0)*fib(n-3) + (-1)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_4_-3_-2', 'n=3: pred=1 != obs=2').
% finite fail / dead-end
rejected('transfer_pell_to_fib_5_-5_-3', 'n=3: pred=0 != obs=2').
% finite fail / dead-end
rejected('transfer_pell_to_fib_0_5_2_0', 'n=4: pred=7 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_2_3_1', 'n=4: pred=7 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_3_-1_-2', 'TRANSFER rec(fib,[3, -1, -2]) ⇒ try on lucas: (3)*lucas(n-1) + (-1)*lucas(n-2) + (-2)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-2_4_3', 'TRANSFER rec(fib,[-2, 4, 3]) ⇒ try on lucas: (-2)*lucas(n-1) + (4)*lucas(n-2) + (3)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_1_2_1', 'TRANSFER rec(fib,[0, 1, 2, 1]) ⇒ try on lucas: (0)*lucas(n-1) + (1)*lucas(n-2) + (2)*lucas(n-3) + (1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_3_0_-1', 'TRANSFER rec(fib,[0, 3, 0, -1]) ⇒ try on lucas: (0)*lucas(n-1) + (3)*lucas(n-2) + (0)*lucas(n-3) + (-1)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_4_-3_-2', 'n=3: pred=5 != obs=4').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_5_-5_-3', 'n=4: pred=2 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_0_5_2_0', 'n=4: pred=17 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_2_3_1', 'n=4: pred=15 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_3_-1_-2', 'n=4: pred=11 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-2_4_3', 'n=3: pred=0 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_1_2_1', 'n=4: pred=4 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_3_0_-1', 'n=4: pred=6 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_3_-1_-2', 'n=4: pred=11 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-2_4_3', 'n=3: pred=0 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_1_2_1', 'n=4: pred=4 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_3_0_-1', 'n=4: pred=6 != obs=12').
lemma('L12_midline_parallel', Parallel, 'CX0 ∥ X0X1').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L12_midline_parallel', 'CX0 ∥ X0X1')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d4_s54', 'CX0 ∥ X0X1')).
lemma('L13_EqSeg', EqSeg, 'BM = AM').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L13_EqSeg', 'BM = AM')).
% hypothesis-language skin gen=6
schema(bilinear_schema_r4_g6, unlocked(true)).
% verified @ sequences/bilinear_schema_r4_g6
verified(fact(sequences, bilinear_schema_r4_g6, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema_r4_g6
verified(fact(sequences, bilinear_schema_r4_g6, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema_r4_g6
verified(fact(sequences, bilinear_schema_r4_g6, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% verified @ sequences/bilinear_schema_r4_g6
verified(fact(sequences, bilinear_schema_r4_g6, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% verified @ sequences/bilinear_schema_r4_g6
verified(fact(sequences, bilinear_schema_r4_g6, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% learned recurrence on fib
rec(fib, [4,-2,-3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_4_-2_-3', 'fib(n) = (4)*fib(n-1) + (-2)*fib(n-2) + (-3)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [-3,5,4]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_-3_5_4', 'fib(n) = (-3)*fib(n-1) + (5)*fib(n-2) + (4)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [2,-1,0,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_2_-1_0_1', 'fib(n) = (2)*fib(n-1) + (-1)*fib(n-2) + (0)*fib(n-3) + (1)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [0,0,3,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_0_3_2', 'fib(n) = (0)*fib(n-1) + (0)*fib(n-2) + (3)*fib(n-3) + (2)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [4,-2,-3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_4_-2_-3', 'lucas(n) = (4)*lucas(n-1) + (-2)*lucas(n-2) + (-3)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [-3,5,4]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_-3_5_4', 'lucas(n) = (-3)*lucas(n-1) + (5)*lucas(n-2) + (4)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [2,-1,0,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_2_-1_0_1', 'lucas(n) = (2)*lucas(n-1) + (-1)*lucas(n-2) + (0)*lucas(n-3) + (1)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [0,0,3,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_0_3_2', 'lucas(n) = (0)*lucas(n-1) + (0)*lucas(n-2) + (3)*lucas(n-3) + (2)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [1,4,-1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_1_4_-1_-1', 'pell(n) = (1)*pell(n-1) + (4)*pell(n-2) + (-1)*pell(n-3) + (-1)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [2,2,-2,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_2_2_-2_-1', 'pell(n) = (2)*pell(n-1) + (2)*pell(n-2) + (-2)*pell(n-3) + (-1)*pell(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_4_-2_-3', 'TRANSFER rec(lucas,[4, -2, -3]) ⇒ try on fib: (4)*fib(n-1) + (-2)*fib(n-2) + (-3)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-3_5_4', 'TRANSFER rec(lucas,[-3, 5, 4]) ⇒ try on fib: (-3)*fib(n-1) + (5)*fib(n-2) + (4)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_2_-1_0_1', 'TRANSFER rec(lucas,[2, -1, 0, 1]) ⇒ try on fib: (2)*fib(n-1) + (-1)*fib(n-2) + (0)*fib(n-3) + (1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_0_3_2', 'TRANSFER rec(lucas,[0, 0, 3, 2]) ⇒ try on fib: (0)*fib(n-1) + (0)*fib(n-2) + (3)*fib(n-3) + (2)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_4_-1_-1', 'n=4: pred=5 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_2_-2_-1', 'n=4: pred=4 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_4_-2_-3', 'TRANSFER rec(fib,[4, -2, -3]) ⇒ try on lucas: (4)*lucas(n-1) + (-2)*lucas(n-2) + (-3)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-3_5_4', 'TRANSFER rec(fib,[-3, 5, 4]) ⇒ try on lucas: (-3)*lucas(n-1) + (5)*lucas(n-2) + (4)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_2_-1_0_1', 'TRANSFER rec(fib,[2, -1, 0, 1]) ⇒ try on lucas: (2)*lucas(n-1) + (-1)*lucas(n-2) + (0)*lucas(n-3) + (1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_0_3_2', 'TRANSFER rec(fib,[0, 0, 3, 2]) ⇒ try on lucas: (0)*lucas(n-1) + (0)*lucas(n-2) + (3)*lucas(n-3) + (2)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_4_-1_-1', 'n=4: pred=13 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_2_-2_-1', 'n=4: pred=10 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_4_-2_-3', 'n=3: pred=6 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-3_5_4', 'n=3: pred=-1 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_2_-1_0_1', 'n=4: pred=8 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_0_3_2', 'n=4: pred=3 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_4_-2_-3', 'n=3: pred=6 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-3_5_4', 'n=3: pred=-1 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_2_-1_0_1', 'n=4: pred=8 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_0_3_2', 'n=4: pred=3 != obs=12').
lemma('L14_midline_parallel', Parallel, 'AB ∥ CD').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L14_midline_parallel', 'AB ∥ CD')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d4_s64', 'AB ∥ CD')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s2192', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s2192', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s2193', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s2193', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s2194', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s2194', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s2192', 'P(H|E)=0.9572290174677728 != P(H|¬E)=0.6357986130011273').
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s2925', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s2925', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s2925', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s2925', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o2', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o2', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o2', 'Z2 addition associative on full table')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod5', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 5) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod5', 'x(y+z) ≡ xy+xz (mod 5)')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n3', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n3', 'T>0 and a>0 for all generated bodies')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N4', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N4', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N4', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_2H2+O2→2H2O', 'atom counts conserved: 2H2+O2→2H2O')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6382', 'atom counts conserved: synth_s6382')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6383', 'atom counts conserved: synth_s6383')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6382', 'mismatch {\'H\': 5, \'O\': 5} vs {\'H\': 6, \'O\': 5}').
