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
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n3_s4542', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s4542', 'V≠I+R (as required)').
% verified @ nets/nets_perceptron
verified(fact(nets, nets_perceptron, 'nets_AND_linear_threshold', 'AND separable by linear threshold (exists w,b)')).
% verified @ nets/nets_perceptron
verified(fact(nets, nets_perceptron, 'nets_OR_linear_threshold', 'OR separable by linear threshold (exists w,b)')).
% verified @ nets/nets_perceptron
verified(fact(nets, nets_perceptron, 'transfer_bitfn_and_to_perceptron_and_all', 'TRANSFER bit_fn(and_all=and_all) ⇒ AND linear-sep')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5713', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5713', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s5714', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s5714', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s3883', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s3884', '2x2 Cramer exact on generated integer system')).
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
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k2_s2413', 'H(p)≥0 on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k2_s2413', 'H(p)≤log2(k) on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k2_s2414', 'H(p)≥0 on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k2_s2414', 'H(p)≤log2(k) on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k2_s2415', 'H(p)≥0 on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k2_s2415', 'H(p)≤log2(k) on generated 2-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k2_s2413', 'H(uniform)≥H(peaked) for k=2')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6422', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6423', 'K = [B]/[A] on 2-species toy')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4598', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4599', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s4600', 'sum I_k = 0 at 3-edge node')).
% verified @ info/info_transfer_bitfn
verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_parity_parity_to_MI', 'TRANSFER bit_fn(parity=parity) ⇒ MI>0 on xor-coupled joint')).
% verified @ info/info_transfer_bitfn
verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_and_all_and_all_to_MI', 'TRANSFER bit_fn(and_all=and_all) ⇒ MI>0 on (X,X∧Y)')).
% verified @ info/info_transfer_bitfn
verified(fact(info, info_transfer_bitfn, 'transfer_bitfn_xor2_xor2_to_MI', 'TRANSFER bit_fn(xor2=xor2) ⇒ MI>0 on xor-coupled joint')).
% finite fail / dead-end
rejected('NEG_nets_XOR_linear_threshold', 'no linear threshold for XOR').
% verified @ nets/nets_xor_depth
verified(fact(nets, nets_xor_depth, 'nets_XOR_two_layer_composition', 'XOR = (OR) AND NOT(AND) two-layer composition')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5794', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5795', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s5796', 'F_12 + F_21 = 0')).
% finite fail / dead-end
rejected('phys_action_same_sign_broken_s5794', 'F12=4.68605264438847 F21=4.68605264438847 not opposite').
% hypothesis-language skin gen=7
schema(bilinear_schema_r4_g7, unlocked(true)).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_z2_abelian_o2', 'Z2 table commutative + identity 0')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_klein_every_nonid_order2_o2', 'Klein: every non-identity element has order 2')).
% finite fail / dead-end
rejected('sym_z2_claim_mult_o2', '1+1=0 != 1*1=1').
% verified @ algebra/alg_mat_assoc
verified(fact(algebra, alg_mat_assoc, 'alg_mat2_assoc_mod3', '(AB)C = A(BC) for all 2x2 matrices mod 3')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h_eq_sqrt_a_n3', 'circular specific ang-mom h = sqrt(a) (GM=1) within eps')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h2_over_a_const_n3', 'h^2/a constant (=1) on circular 2-body table')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x1_vs_n_x0_N4', 'Delta(x^1) / (power * x^0) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x2_vs_n_x1_N4', 'Delta(x^2) / (power * x^1) → 1 (eps on lattice)')).
% finite fail / dead-end
rejected('NEG_chance_H_negative', 'H=0.9442524549486193 not < 0').
% finite fail / dead-end
rejected('NEG_chance_mix_decreases_H', 'H(mix)=0.9861995129233668 >= min=0.9442524549486193').
% finite fail / dead-end
rejected('NEG_chem_atoms_not_conserved', 'counts match — conservation holds (reject claim)').
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_fib_to_delta_N12', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ nets/nets_transfer_form
verified(fact(nets, nets_transfer_form, 'transfer_bitfn_and_to_perceptron_form', 'TRANSFER bit_fn(and_all) ⇒ AND linear-sep (perceptron)')).
% verified @ electro/electro_switch_transfer
verified(fact(electro, electro_switch_transfer, 'transfer_and_to_series_switch_and_all', 'TRANSFER bit_fn(and_all) ⇒ series switches')).
% finite fail / dead-end
rejected('transfer_bitfn_xor_to_linear_perceptron', 'XOR not linearly separable (honest negative)').
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_conserv_chem_atoms_to_physics_conserv_mom', 'TRANSFER linear-Δ=0 (chem:atoms) ⇒ conserv_mom on physics')).
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_conserv_chem_atoms_to_electro_conserv_kcl', 'TRANSFER linear-Δ=0 (chem:atoms) ⇒ conserv_kcl on electro')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_conserv_physics_momentum_to_chem_conserv_chem', 'TRANSFER linear-Δ=0 (physics:momentum) ⇒ conserv_chem on chem')).
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_conserv_physics_momentum_to_electro_conserv_kcl', 'TRANSFER linear-Δ=0 (physics:momentum) ⇒ conserv_kcl on electro')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_conserv_electro_kcl_to_chem_conserv_chem', 'TRANSFER linear-Δ=0 (electro:kcl) ⇒ conserv_chem on chem')).
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_conserv_electro_kcl_to_physics_conserv_mom', 'TRANSFER linear-Δ=0 (electro:kcl) ⇒ conserv_mom on physics')).
% verified @ astro/astro_transfer_form
verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n5', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).
% finite fail / dead-end
rejected('transfer_kepler_bogus_power_n5', 'T²/a² not const: [39.47841760435743, 59.21762640653614, 78.95683520871486, 98.69604401089356, 118.43525281307228]').
% verified @ sequences/bilinear_schema
verified(fact(sequences, bilinear_schema, 'transfer_bilin_cassini_shape_on_fib', 'TRANSFER cassini-shape bilin ⇒ fib(n+1)fib(n-1)-fib(n)^2=(-1)^n')).
% finite fail / dead-end
rejected('transfer_bilin_cassini_shape_on_lucas', 'fail n=1 lhs≠(-1)^n').
% finite fail / dead-end
rejected('NEG_info_MI_always_0', 'MI=0.7421139952238672 ≠ 0').
% verified @ nets/nets_grad_step
verified(fact(nets, nets_grad_step, 'nets_grad_step_se_s0', 'Δw = -η x (yhat-y) on squared-error toy (note sign)')).
% verified @ nets/nets_grad_step
verified(fact(nets, nets_grad_step, 'nets_grad_step_se_s1', 'Δw = -η x (yhat-y) on squared-error toy (note sign)')).
% finite fail / dead-end
rejected('NEG_phys_momentum_not_conserved', '|Δp|=8.881784197001252e-16≈0 (conservation holds)').
% finite fail / dead-end
rejected('NEG_sym_product_mod2_conserved', 'flip0 [0, 1, 1]→[1, 1, 1] prod 0→1').
% finite fail / dead-end
rejected('NEG_alg_binom_no_cross', 'x=1,y=1: (x+y)^2=1 != x^2+y^2=2').
% finite fail / dead-end
rejected('NEG_astro_T_prop_a', 'T/a not const: [6.2832, 7.6953, 8.8858]').
% finite fail / dead-end
rejected('NEG_electro_current_not_conserved', 'sum=0.0≈0 so conservation holds (reject claim)').
% verified @ nets/nets_chain_transfer
verified(fact(nets, nets_chain_transfer, 'nets_discrete_chain_vs_product', 'Delta(f∘g) vs Delta(f)·Delta(g) — composition identity check')).
% finite fail / dead-end
rejected('NEG_calc_delta_always_zero', 'Delta[0]=1.0 != 0').
% finite fail / dead-end
rejected('NEG_nets_loss_decreases_any_eta', 'eta=10.0 overshoot L 1.0→361.0').
% verified @ sequences/bilinear_schema_r4_g7
verified(fact(sequences, bilinear_schema_r4_g7, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema_r4_g7
verified(fact(sequences, bilinear_schema_r4_g7, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema_r4_g7
verified(fact(sequences, bilinear_schema_r4_g7, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% verified @ sequences/bilinear_schema_r4_g7
verified(fact(sequences, bilinear_schema_r4_g7, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% verified @ sequences/bilinear_schema_r4_g7
verified(fact(sequences, bilinear_schema_r4_g7, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% verified @ electro/electro_switch_transfer
verified(fact(electro, electro_switch_transfer, 'electro_parallel_OR_table', 'parallel switches ≡ OR on {0,1}^2')).
% verified @ calculus/calc_transfer_rec
verified(fact(calculus, calc_transfer_rec, 'transfer_rec_fib_to_delta_N4', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_rec
verified(fact(calculus, calc_transfer_rec, 'transfer_rec_lucas_to_delta_N4', 'TRANSFER rec(lucas,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_conserv_mom_shape_to_chem_atoms', 'TRANSFER linear-Δ=0 (momentum shape) ⇒ atom counts balanced')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_conserv_kcl_shape_to_chem_atoms', 'TRANSFER linear-Δ=0 (KCL shape) ⇒ atom counts balanced')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_chem_conserv_form_on_physics_mom', 'TRANSFER chem atom-Δ=0 form ⇒ try on physics momentum')).
% verified @ chem/chem_transfer_form
verified(fact(chem, chem_transfer_form, 'transfer_chem_conserv_form_on_electro_kcl', 'TRANSFER chem atom-Δ=0 form ⇒ try on electro KCL')).
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_conserv_atom_shape_to_physics_mom', 'TRANSFER linear-Δ=0 (atom-balance shape) ⇒ momentum conserved')).
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_conserv_kcl_shape_to_physics_mom', 'TRANSFER linear-Δ=0 (KCL shape) ⇒ momentum conserved')).
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_phys_conserv_form_on_chem_atoms', 'TRANSFER physics mom-Δ=0 form ⇒ try on chem atom balance')).
% verified @ physics/phys_transfer_form
verified(fact(physics, phys_transfer_form, 'transfer_phys_conserv_form_on_electro_kcl', 'TRANSFER physics mom-Δ=0 form ⇒ try on electro KCL')).
% verified @ symmetry/sym_compare_transfer
verified(fact(symmetry, sym_compare_transfer, 'transfer_parity_to_z2_parity_parity', 'TRANSFER bit_fn(parity=parity) ⇒ Z2/parity invariant')).
% verified @ symmetry/sym_compare_transfer
verified(fact(symmetry, sym_compare_transfer, 'transfer_xor2_to_z2_parity_xor2', 'TRANSFER bit_fn(xor2=xor2) ⇒ Z2/parity invariant')).
% verified @ astro/astro_transfer_form
verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n3', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).
% finite fail / dead-end
rejected('transfer_kepler_bogus_power_n3', 'T²/a² not const: [39.4784, 59.2176, 78.9568]').
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_conserv_atom_shape_to_electro_kcl', 'TRANSFER linear-Δ=0 (atom-balance shape) ⇒ KCL ΣI=0')).
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_conserv_mom_shape_to_electro_kcl', 'TRANSFER linear-Δ=0 (momentum shape) ⇒ KCL ΣI=0')).
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_electro_conserv_form_on_chem_atoms', 'TRANSFER electro KCL-Σ=0 form ⇒ try on chem atom balance')).
% verified @ electro/electro_transfer_form
verified(fact(electro, electro_transfer_form, 'transfer_electro_conserv_form_on_physics_mom', 'TRANSFER electro KCL-Σ=0 form ⇒ try on physics momentum')).
% verified @ nets/nets_transfer_form
verified(fact(nets, nets_transfer_form, 'transfer_bitfn_and_to_switch_shape', 'TRANSFER bit_fn(and_all) ⇒ series-switch ≡ AND table')).
% learned recurrence on fib
rec(fib, [5,-3,-4]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_5_-3_-4', 'fib(n) = (5)*fib(n-1) + (-3)*fib(n-2) + (-4)*fib(n-3)')).
% learned recurrence on fib
rec(fib, [1,2,-1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_2_-1_-1', 'fib(n) = (1)*fib(n-1) + (2)*fib(n-2) + (-1)*fib(n-3) + (-1)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [-1,3,2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_3_2_0', 'fib(n) = (-1)*fib(n-1) + (3)*fib(n-2) + (2)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [5,-3,-4]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_5_-3_-4', 'lucas(n) = (5)*lucas(n-1) + (-3)*lucas(n-2) + (-4)*lucas(n-3)')).
% learned recurrence on lucas
rec(lucas, [1,2,-1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_2_-1_-1', 'lucas(n) = (1)*lucas(n-1) + (2)*lucas(n-2) + (-1)*lucas(n-3) + (-1)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,3,2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_3_2_0', 'lucas(n) = (-1)*lucas(n-1) + (3)*lucas(n-2) + (2)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [3,-2,1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_-2_1_1', 'pell(n) = (3)*pell(n-1) + (-2)*pell(n-2) + (1)*pell(n-3) + (1)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [3,0,-3,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_0_-3_-1', 'pell(n) = (3)*pell(n-1) + (0)*pell(n-2) + (-3)*pell(n-3) + (-1)*pell(n-4)')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s3026', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s3026', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s3027', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s3027', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k3_s3028', 'H(p)≥0 on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k3_s3028', 'H(p)≤log2(k) on generated 3-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k3_s3026', 'H(uniform)≥H(peaked) for k=3')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3043', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3043', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3044', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3044', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3045', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3045', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s3043', 'P(H|E)=0.6740533229104554 != P(H|¬E)=0.2043078977310901').
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s3061', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s3061', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s3062', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s3062', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k4_s3063', 'H(p)≥0 on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k4_s3063', 'H(p)≤log2(k) on generated 4-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k4_s3061', 'H(uniform)≥H(peaked) for k=4')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k5_s3079', 'H(p)≥0 on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k5_s3079', 'H(p)≤log2(k) on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k5_s3080', 'H(p)≥0 on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k5_s3080', 'H(p)≤log2(k) on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k5_s3081', 'H(p)≥0 on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k5_s3081', 'H(p)≤log2(k) on generated 5-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k5_s3079', 'H(uniform)≥H(peaked) for k=5')).
% hypothesis-language skin gen=8
schema(bilinear_schema_r4_g8, unlocked(true)).
% verified @ sequences/bilinear_schema_r4_g8
verified(fact(sequences, bilinear_schema_r4_g8, 'bilin_fib_offset_pm1', 'fib(n+1)fib(n-1)-fib(n)^2 = (-1)^n')).
% verified @ sequences/bilinear_schema_r4_g8
verified(fact(sequences, bilinear_schema_r4_g8, 'bilin_fib_r1', 'fib(n)^2 - fib(n+1)fib(n-1) = (-1)^(n-1) fib(1)^2')).
% verified @ sequences/bilinear_schema_r4_g8
verified(fact(sequences, bilinear_schema_r4_g8, 'bilin_fib_r2', 'fib(n)^2 - fib(n+2)fib(n-2) = (-1)^(n-2) fib(2)^2')).
% verified @ sequences/bilinear_schema_r4_g8
verified(fact(sequences, bilinear_schema_r4_g8, 'bilin_fib_r3', 'fib(n)^2 - fib(n+3)fib(n-3) = (-1)^(n-3) fib(3)^2')).
% verified @ sequences/bilinear_schema_r4_g8
verified(fact(sequences, bilinear_schema_r4_g8, 'bilin_fib_r4', 'fib(n)^2 - fib(n+4)fib(n-4) = (-1)^(n-4) fib(4)^2')).
% learned recurrence on fib
rec(fib, [1,-1,2,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_-1_2_2', 'fib(n) = (1)*fib(n-1) + (-1)*fib(n-2) + (2)*fib(n-3) + (2)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [2,1,-2,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_2_1_-2_-1', 'fib(n) = (2)*fib(n-1) + (1)*fib(n-2) + (-2)*fib(n-3) + (-1)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [1,-1,2,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_-1_2_2', 'lucas(n) = (1)*lucas(n-1) + (-1)*lucas(n-2) + (2)*lucas(n-3) + (2)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [2,1,-2,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_2_1_-2_-1', 'lucas(n) = (2)*lucas(n-1) + (1)*lucas(n-2) + (-2)*lucas(n-3) + (-1)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [0,4,4,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_0_4_4_1', 'pell(n) = (0)*pell(n-1) + (4)*pell(n-2) + (4)*pell(n-3) + (1)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [1,1,5,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_1_1_5_2', 'pell(n) = (1)*pell(n-1) + (1)*pell(n-2) + (5)*pell(n-3) + (2)*pell(n-4)')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3129', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3129', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3130', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3130', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s3131', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s3131', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s3129', 'P(H|E)=0.17743158399745776 != P(H|¬E)=0.18086853681061626').
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s3165', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s3165', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s3166', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s3166', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s3167', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s3167', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k6_s3165', 'H(uniform)≥H(peaked) for k=6')).
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_le_logk_k7', 'EXTRAPOLATE H_le_logk to k=7')).
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_nonneg_k7', 'EXTRAPOLATE H_nonneg to k=7')).
% verified @ chance/chance_extrapolate
verified(fact(chance, chance_extrapolate, 'extrap_H_uni_ge_peaked_k7', 'EXTRAPOLATE H_uni_ge_peaked to k=7')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s3693', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s3693', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s3693', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s3693', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6191', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6191', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6192', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6192', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6193', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6193', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6201', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6201', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6202', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6202', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6203', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6203', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6204', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6204', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6211', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6211', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6212', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6212', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6213', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6213', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6214', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6214', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6215', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6215', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% finite fail / dead-end
rejected('false_mom_as_energy_on_collision', 'form mismatch: linear-Δp=0 ≠ quadratic ½mv² (even if both hold on elastic)').
% finite fail / dead-end
rejected('false_series_and_as_parallel_or', 'series∧ vs parallel∨ mismatch at (0,1): series=0 parallel=1').
% finite fail / dead-end
rejected('false_fib_rec_on_trib', 'n=2: pred=0 != obs=1').
% finite fail / dead-end
rejected('false_fib_padded_on_trib', 'n=5: pred=3 != obs=4').
% finite fail / dead-end
rejected('false_kepler_as_ohm_on_resistor', 'T²/a³-shape on Ohm not const: [0.8267, 5.6582, 0.9737, 0.0142, 0.009]; Ohm itself holds=True').
% finite fail / dead-end
rejected('false_xor_as_and', 'XOR≠AND on (1,1): xor=0 and=1').
% finite fail / dead-end
rejected('false_and_as_xor', 'AND≠XOR').
% finite fail / dead-end
rejected('false_cassini_as_pell_law', 'form-family mismatch: bilin_cassini(bilinear_identity) ≠ pell_rec(linear_recurrence); identity_on_pell=True').
% verified @ electro/electro_switch_transfer
verified(fact(electro, electro_switch_transfer, 'transfer_parallel_switches_equiv_OR', 'TRANSFER OR-form ⇒ parallel switches ≡ OR on {0,1}^2')).
% verified @ calculus/calc_transfer_rec
verified(fact(calculus, calc_transfer_rec, 'transfer_rec_11_to_lucas_delta_N12', 'TRANSFER rec([1,1]) ⇒ Delta structure on lucas')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'transfer_alg_distrib_mod5_to_mod7', 'TRANSFER poly distrib form (mod5 prior) ⇒ exhaustive distrib mod 7')).
% verified @ algebra/alg_mat_assoc
verified(fact(algebra, alg_mat_assoc, 'transfer_alg_mat_assoc_mod3_to_mod5', 'TRANSFER mat assoc form (mod3 prior) ⇒ sampled 2x2 assoc mod 5')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_5_-3_-4', 'TRANSFER rec(lucas,[5, -3, -4]) ⇒ try on fib: (5)*fib(n-1) + (-3)*fib(n-2) + (-4)*fib(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_2_-1_-1', 'TRANSFER rec(lucas,[1, 2, -1, -1]) ⇒ try on fib: (1)*fib(n-1) + (2)*fib(n-2) + (-1)*fib(n-3) + (-1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_3_2_0', 'TRANSFER rec(lucas,[-1, 3, 2, 0]) ⇒ try on fib: (-1)*fib(n-1) + (3)*fib(n-2) + (2)*fib(n-3) + (0)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_-2_1_1', 'n=4: pred=5 != obs=3').
% learned recurrence on fib
rec(fib, [3,0,-3,-1]).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_pell_to_fib_3_0_-3_-1', 'TRANSFER rec(pell,[3, 0, -3, -1]) ⇒ try on fib: (3)*fib(n-1) + (0)*fib(n-2) + (-3)*fib(n-3) + (-1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_-1_2_2', 'TRANSFER rec(lucas,[1, -1, 2, 2]) ⇒ try on fib: (1)*fib(n-1) + (-1)*fib(n-2) + (2)*fib(n-3) + (2)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_2_1_-2_-1', 'TRANSFER rec(lucas,[2, 1, -2, -1]) ⇒ try on fib: (2)*fib(n-1) + (1)*fib(n-2) + (-2)*fib(n-3) + (-1)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_0_4_4_1', 'n=4: pred=8 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_1_5_2', 'n=4: pred=8 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_5_-3_-4', 'TRANSFER rec(fib,[5, -3, -4]) ⇒ try on lucas: (5)*lucas(n-1) + (-3)*lucas(n-2) + (-4)*lucas(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_2_-1_-1', 'TRANSFER rec(fib,[1, 2, -1, -1]) ⇒ try on lucas: (1)*lucas(n-1) + (2)*lucas(n-2) + (-1)*lucas(n-3) + (-1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_3_2_0', 'TRANSFER rec(fib,[-1, 3, 2, 0]) ⇒ try on lucas: (-1)*lucas(n-1) + (3)*lucas(n-2) + (2)*lucas(n-3) + (0)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_-2_1_1', 'n=4: pred=9 != obs=7').
% learned recurrence on lucas
rec(lucas, [3,0,-3,-1]).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_pell_to_lucas_3_0_-3_-1', 'TRANSFER rec(pell,[3, 0, -3, -1]) ⇒ try on lucas: (3)*lucas(n-1) + (0)*lucas(n-2) + (-3)*lucas(n-3) + (-1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_-1_2_2', 'TRANSFER rec(fib,[1, -1, 2, 2]) ⇒ try on lucas: (1)*lucas(n-1) + (-1)*lucas(n-2) + (2)*lucas(n-3) + (2)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_2_1_-2_-1', 'TRANSFER rec(fib,[2, 1, -2, -1]) ⇒ try on lucas: (2)*lucas(n-1) + (1)*lucas(n-2) + (-2)*lucas(n-3) + (-1)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_0_4_4_1', 'n=4: pred=18 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_1_5_2', 'n=4: pred=16 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_5_-3_-4', 'n=3: pred=7 != obs=5').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_2_-1_-1', 'n=4: pred=8 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_3_2_0', 'n=4: pred=3 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_5_-3_-4', 'n=3: pred=7 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_2_-1_-1', 'n=4: pred=8 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_3_2_0', 'n=4: pred=3 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_-1_2_2', 'n=4: pred=5 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_2_1_-2_-1', 'n=4: pred=10 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_-1_2_2', 'n=4: pred=5 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_2_1_-2_-1', 'n=4: pred=10 != obs=12').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_3_0_-3_-1', 'TRANSFER rec(lucas,[3, 0, -3, -1]) ⇒ try on fib: (3)*fib(n-1) + (0)*fib(n-2) + (-3)*fib(n-3) + (-1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_3_0_-3_-1', 'TRANSFER rec(fib,[3, 0, -3, -1]) ⇒ try on lucas: (3)*lucas(n-1) + (0)*lucas(n-2) + (-3)*lucas(n-3) + (-1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_pell_3_0_-3_-1', 'TRANSFER rec(fib,[3, 0, -3, -1]) ⇒ try on pell: (3)*pell(n-1) + (0)*pell(n-2) + (-3)*pell(n-3) + (-1)*pell(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_pell_3_0_-3_-1', 'TRANSFER rec(lucas,[3, 0, -3, -1]) ⇒ try on pell: (3)*pell(n-1) + (0)*pell(n-2) + (-3)*pell(n-3) + (-1)*pell(n-4)')).
% verified @ astro/astro_transfer_form
verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n4', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).
% finite fail / dead-end
rejected('transfer_kepler_bogus_power_n4', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6329', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6329', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6330', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6330', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6331', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6331', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6332', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6332', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6333', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6333', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s6334', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s6334', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s3928', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s3928', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s3928', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s3928', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_bangbang_s7069', 'bang-bang toward 0 reduces |error| over T on held-out x0')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_linear_gain_s7069', 'a=-k*x (k=0.5) reduces |error| over T on held-out x0')).
% finite fail / dead-end
rejected('loop_taxis_always_right_s7069', 'increases error: x0=2.0 2→7; x0=4.0 4→9; x0=6.0 6→11').
% finite fail / dead-end
rejected('loop_taxis_always_left_s7069', 'increases error: x0=-2.0 2→7; x0=-4.0 4→9; x0=-5.0 5→10').
% verified @ loop/loop_pred_scan
verified(fact(loop, loop_pred_scan, 'loop_pred_dx_eq_action_s7069', 'next_x = x + a (Δ(position)=action) on held-out starts')).
% finite fail / dead-end
rejected('loop_pred_ignore_action_s7069', 'finite fail: x0=-6.0 a=1.0 pred=-6.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_double_action_s7069', 'finite fail: x0=-6.0 pred=-4.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_overfit_one_x0_s7069', 'fails new x0: new_x0=-6.0 pred=3.0 obs=-5.0').
% verified @ loop/loop_transfer_form
verified(fact(loop, loop_transfer_form, 'transfer_conserv_delta0_to_loop_dx_eq_a', 'TRANSFER linear-Δ=0 ⇒ Δ(position)-action=0 (next_x=x+a)')).
% finite fail / dead-end
rejected('transfer_rec11_to_loop_position', 'n=2: pred=9.0 != obs=3.0').
% finite fail / dead-end
rejected('transfer_conserv_to_loop_position_frozen', 'finite fail: x0=-6.0 moved under a=1').
% finite fail / dead-end
rejected('NEG_loop_policy_increases_error', 'finite fail (dead-end): deltas=[-4.0, -4.0]').
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_bangbang_s8642', 'bang-bang toward 0 reduces |error| over T on held-out x0')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_linear_gain_s8642', 'a=-k*x (k=0.5) reduces |error| over T on held-out x0')).
% finite fail / dead-end
rejected('loop_taxis_always_right_s8642', 'increases error: x0=2.0 2→7; x0=4.0 4→9; x0=6.0 6→11').
% finite fail / dead-end
rejected('loop_taxis_always_left_s8642', 'increases error: x0=-2.0 2→7; x0=-4.0 4→9; x0=-5.0 5→10').
% verified @ loop/loop_pred_scan
verified(fact(loop, loop_pred_scan, 'loop_pred_dx_eq_action_s8653', 'next_x = x + a (Δ(position)=action) on held-out starts')).
% finite fail / dead-end
rejected('loop_pred_ignore_action_s8653', 'finite fail: x0=-6.0 a=1.0 pred=-6.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_double_action_s8653', 'finite fail: x0=-6.0 pred=-4.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_overfit_one_x0_s8653', 'fails new x0: new_x0=-6.0 pred=3.0 obs=-5.0').
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o3', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o3', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o3', 'Z2 addition associative on full table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_n_o3', 'parity conserved on 3-bit strings')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5082', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5083', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5084', 'sum I_k = 0 at 3-edge node')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N5', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N5', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N5', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N5', 'Delta(sum x^0) = x^0 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x1_N5', 'Delta(sum x^1) = x^1 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x2_N5', 'Delta(sum x^2) = x^2 on prefix')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6788', 'atom counts conserved: synth_s6788')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6789', 'atom counts conserved: synth_s6789')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6790', 'atom counts conserved: synth_s6790')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6788', 'mismatch {\'H\': 4, \'O\': 2} vs {\'H\': 5, \'O\': 2}').
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6425', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6426', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6427', 'F_12 + F_21 = 0')).
% finite fail / dead-end
rejected('phys_action_same_sign_broken_s6425', 'F12=2.8878525425541985 F21=2.8878525425541985 not opposite').
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s4752', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s4753', '2x2 Cramer exact on generated integer system')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6803', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6804', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6805', 'K = [B]/[A] on 2-species toy')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x3_vs_n_x2_N4', 'Delta(x^3) / (power * x^2) → 1 (eps on lattice)')).
% hypothesis-language skin gen=9
schema(delta_conserv, unlocked(true)).
% learned recurrence on fib
rec(fib, [3,-1,-2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_3_-1_-2_0', 'fib(n) = (3)*fib(n-1) + (-1)*fib(n-2) + (-2)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [-1,2,3,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_2_3_1', 'fib(n) = (-1)*fib(n-1) + (2)*fib(n-2) + (3)*fib(n-3) + (1)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [3,-1,-2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_3_-1_-2_0', 'lucas(n) = (3)*lucas(n-1) + (-1)*lucas(n-2) + (-2)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,2,3,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_2_3_1', 'lucas(n) = (-1)*lucas(n-1) + (2)*lucas(n-2) + (3)*lucas(n-3) + (1)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [2,-1,4,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_2_-1_4_2', 'pell(n) = (2)*pell(n-1) + (-1)*pell(n-2) + (4)*pell(n-3) + (2)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [4,-4,0,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_4_-4_0_1', 'pell(n) = (4)*pell(n-1) + (-4)*pell(n-2) + (0)*pell(n-3) + (1)*pell(n-4)')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n4', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n4', 'T>0 and a>0 for all generated bodies')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h_eq_sqrt_a_n4', 'circular specific ang-mom h = sqrt(a) (GM=1) within eps')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h2_over_a_const_n4', 'h^2/a constant (=1) on circular 2-body table')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_z2_abelian_o3', 'Z2 table commutative + identity 0')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_klein_every_nonid_order2_o3', 'Klein: every non-identity element has order 2')).
% finite fail / dead-end
rejected('sym_z2_claim_mult_o3', '1+1=0 != 1*1=1').
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod7', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 7) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod7', 'x(y+z) ≡ xy+xz (mod 7)')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_bangbang_s8863', 'bang-bang toward 0 reduces |error| over T on held-out x0')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_linear_gain_s8863', 'a=-k*x (k=0.5) reduces |error| over T on held-out x0')).
% finite fail / dead-end
rejected('loop_taxis_always_right_s8863', 'increases error: x0=2.0 2→7; x0=4.0 4→9; x0=6.0 6→11').
% finite fail / dead-end
rejected('loop_taxis_always_left_s8863', 'increases error: x0=-2.0 2→7; x0=-4.0 4→9; x0=-5.0 5→10').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_3_-1_-2_0', 'TRANSFER rec(lucas,[3, -1, -2, 0]) ⇒ try on fib: (3)*fib(n-1) + (-1)*fib(n-2) + (-2)*fib(n-3) + (0)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_2_3_1', 'TRANSFER rec(lucas,[-1, 2, 3, 1]) ⇒ try on fib: (-1)*fib(n-1) + (2)*fib(n-2) + (3)*fib(n-3) + (1)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_-1_4_2', 'n=4: pred=7 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_4_-4_0_1', 'n=4: pred=4 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_3_-1_-2_0', 'TRANSFER rec(fib,[3, -1, -2, 0]) ⇒ try on lucas: (3)*lucas(n-1) + (-1)*lucas(n-2) + (-2)*lucas(n-3) + (0)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_2_3_1', 'TRANSFER rec(fib,[-1, 2, 3, 1]) ⇒ try on lucas: (-1)*lucas(n-1) + (2)*lucas(n-2) + (3)*lucas(n-3) + (1)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_-1_4_2', 'n=4: pred=13 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_4_-4_0_1', 'n=4: pred=6 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_3_-1_-2_0', 'n=4: pred=11 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_2_3_1', 'n=4: pred=2 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_3_-1_-2_0', 'n=4: pred=11 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_2_3_1', 'n=4: pred=2 != obs=12').
% verified @ algebra/alg_mat_assoc
verified(fact(algebra, alg_mat_assoc, 'alg_mat2_assoc_mod5', '(AB)C = A(BC) for all 2x2 matrices mod 5')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6884', 'atom counts conserved: synth_s6884')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6885', 'atom counts conserved: synth_s6885')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6886', 'atom counts conserved: synth_s6886')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6887', 'atom counts conserved: synth_s6887')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6884', 'mismatch {\'H\': 4, \'O\': 3} vs {\'H\': 5, \'O\': 3}').
% verified @ protocell/unit_joint
verified(fact(protocell, unit_joint, 'unit_protocell_levers', 'UNIT{bit_circuit,conserv_delta0,form_gate,loop_taxis,rec_companion,rec_to_delta} jointly closes on held-out x0 (conserv residual ∝ loop error drop)')).
% finite fail / dead-end
rejected('NEG_unit_missing_taxis', 'bundle missing taxis (notebook of laws, no loop) — joint fail').
% finite fail / dead-end
rejected('NEG_unit_offpath_bilinear', 'off-path individual (bilinear/energy/matmul) — form_gate reject').
% finite fail / dead-end
rejected('NEG_unit_offpath_energy', 'off-path individual (bilinear/energy/matmul) — form_gate reject').
% finite fail / dead-end
rejected('NEG_unit_clone_rec_pad', 'clone pad (rec_order) — not a new individual (brute)').
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n4_s5243', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s5243', 'V≠I+R (as required)').
% verified @ astro/astro_kepler_scan
verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n4', 'T^2/a^3 constant within eps on circular table')).
% finite fail / dead-end
rejected('astro_kepler_wrong_exp_n4', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ loop/loop_pred_scan
verified(fact(loop, loop_pred_scan, 'loop_pred_dx_eq_action_s8984', 'next_x = x + a (Δ(position)=action) on held-out starts')).
% finite fail / dead-end
rejected('loop_pred_ignore_action_s8984', 'finite fail: x0=-6.0 a=1.0 pred=-6.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_double_action_s8984', 'finite fail: x0=-6.0 pred=-4.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_overfit_one_x0_s8984', 'fails new x0: new_x0=-6.0 pred=3.0 obs=-5.0').
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5307', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5308', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5309', 'sum I_k = 0 at 3-edge node')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o4', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o4', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o4', 'Z2 addition associative on full table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_n_o4', 'parity conserved on 4-bit strings')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N6', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N6', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N6', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N6', 'Delta(sum x^0) = x^0 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x1_N6', 'Delta(sum x^1) = x^1 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x2_N6', 'Delta(sum x^2) = x^2 on prefix')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6714', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6715', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s6716', 'F_12 + F_21 = 0')).
% finite fail / dead-end
rejected('phys_action_same_sign_broken_s6714', 'F12=1.4231307597366212 F21=1.4231307597366212 not opposite').
% hypothesis-language skin gen=10
schema(taxis_conserv, unlocked(true)).
% learned recurrence on fib
rec(fib, [-1,4,1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_4_1_-1', 'fib(n) = (-1)*fib(n-1) + (4)*fib(n-2) + (1)*fib(n-3) + (-1)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [0,4,-1,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_4_-1_-2', 'fib(n) = (0)*fib(n-1) + (4)*fib(n-2) + (-1)*fib(n-3) + (-2)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,4,1,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_4_1_-1', 'lucas(n) = (-1)*lucas(n-1) + (4)*lucas(n-2) + (1)*lucas(n-3) + (-1)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [0,4,-1,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_4_-1_-2', 'lucas(n) = (0)*lucas(n-1) + (4)*lucas(n-2) + (-1)*lucas(n-3) + (-2)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [4,-3,-2,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_4_-3_-2_0', 'pell(n) = (4)*pell(n-1) + (-3)*pell(n-2) + (-2)*pell(n-3) + (0)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [1,5,-3,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_1_5_-3_-2', 'pell(n) = (1)*pell(n-1) + (5)*pell(n-2) + (-3)*pell(n-3) + (-2)*pell(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_4_1_-1', 'TRANSFER rec(lucas,[-1, 4, 1, -1]) ⇒ try on fib: (-1)*fib(n-1) + (4)*fib(n-2) + (1)*fib(n-3) + (-1)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_4_-1_-2', 'TRANSFER rec(lucas,[0, 4, -1, -2]) ⇒ try on fib: (0)*fib(n-1) + (4)*fib(n-2) + (-1)*fib(n-3) + (-2)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_4_-3_-2_0', 'n=5: pred=4 != obs=5').
% finite fail / dead-end
rejected('transfer_pell_to_fib_1_5_-3_-2', 'n=4: pred=4 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_4_1_-1', 'TRANSFER rec(fib,[-1, 4, 1, -1]) ⇒ try on lucas: (-1)*lucas(n-1) + (4)*lucas(n-2) + (1)*lucas(n-3) + (-1)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_4_-1_-2', 'TRANSFER rec(fib,[0, 4, -1, -2]) ⇒ try on lucas: (0)*lucas(n-1) + (4)*lucas(n-2) + (-1)*lucas(n-3) + (-2)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_4_-3_-2_0', 'n=4: pred=5 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_1_5_-3_-2', 'n=4: pred=12 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_4_1_-1', 'n=4: pred=4 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_4_-1_-2', 'n=4: pred=7 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_4_1_-1', 'n=4: pred=4 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_4_-1_-2', 'n=4: pred=7 != obs=12').
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6980', 'atom counts conserved: synth_s6980')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6981', 'atom counts conserved: synth_s6981')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6982', 'atom counts conserved: synth_s6982')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6983', 'atom counts conserved: synth_s6983')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6984', 'atom counts conserved: synth_s6984')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6980', 'mismatch {\'H\': 3, \'O\': 1} vs {\'H\': 4, \'O\': 1}').
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s5182', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s5183', '2x2 Cramer exact on generated integer system')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s6999', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7000', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7001', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7002', 'K = [B]/[A] on 2-species toy')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n5', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n5', 'T>0 and a>0 for all generated bodies')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h_eq_sqrt_a_n5', 'circular specific ang-mom h = sqrt(a) (GM=1) within eps')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h2_over_a_const_n5', 'h^2/a constant (=1) on circular 2-body table')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x4_vs_n_x3_N4', 'Delta(x^4) / (power * x^3) → 1 (eps on lattice)')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_z2_abelian_o4', 'Z2 table commutative + identity 0')).
% verified @ symmetry/sym_group_table
verified(fact(symmetry, sym_group_table, 'sym_klein_every_nonid_order2_o4', 'Klein: every non-identity element has order 2')).
% finite fail / dead-end
rejected('sym_z2_claim_mult_o4', '1+1=0 != 1*1=1').
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod9', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 9) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod9', 'x(y+z) ≡ xy+xz (mod 9)')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_bangbang_s9282', 'bang-bang toward 0 reduces |error| over T on held-out x0')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_linear_gain_s9282', 'a=-k*x (k=0.5) reduces |error| over T on held-out x0')).
% finite fail / dead-end
rejected('loop_taxis_always_right_s9282', 'increases error: x0=2.0 2→7; x0=4.0 4→9; x0=6.0 6→11').
% finite fail / dead-end
rejected('loop_taxis_always_left_s9282', 'increases error: x0=-2.0 2→7; x0=-4.0 4→9; x0=-5.0 5→10').
% verified @ astro/astro_transfer_form
verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n6', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).
% finite fail / dead-end
rejected('transfer_kepler_bogus_power_n6', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% hypothesis-language skin gen=11
schema(rec_to_delta, unlocked(true)).
% learned recurrence on fib
rec(fib, [2,-2,1,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_2_-2_1_2', 'fib(n) = (2)*fib(n-1) + (-2)*fib(n-2) + (1)*fib(n-3) + (2)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [3,-2,-1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_3_-2_-1_1', 'fib(n) = (3)*fib(n-1) + (-2)*fib(n-2) + (-1)*fib(n-3) + (1)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [2,-2,1,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_2_-2_1_2', 'lucas(n) = (2)*lucas(n-1) + (-2)*lucas(n-2) + (1)*lucas(n-3) + (2)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [3,-2,-1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_3_-2_-1_1', 'lucas(n) = (3)*lucas(n-1) + (-2)*lucas(n-2) + (-1)*lucas(n-3) + (1)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [2,3,-4,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_2_3_-4_-2', 'pell(n) = (2)*pell(n-1) + (3)*pell(n-2) + (-4)*pell(n-3) + (-2)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [3,-3,3,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_-3_3_2', 'pell(n) = (3)*pell(n-1) + (-3)*pell(n-2) + (3)*pell(n-3) + (2)*pell(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_2_-2_1_2', 'TRANSFER rec(lucas,[2, -2, 1, 2]) ⇒ try on fib: (2)*fib(n-1) + (-2)*fib(n-2) + (1)*fib(n-3) + (2)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_3_-2_-1_1', 'TRANSFER rec(lucas,[3, -2, -1, 1]) ⇒ try on fib: (3)*fib(n-1) + (-2)*fib(n-2) + (-1)*fib(n-3) + (1)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_3_-4_-2', 'n=5: pred=6 != obs=5').
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_-3_3_2', 'n=4: pred=6 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_2_-2_1_2', 'TRANSFER rec(fib,[2, -2, 1, 2]) ⇒ try on lucas: (2)*lucas(n-1) + (-2)*lucas(n-2) + (1)*lucas(n-3) + (2)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_3_-2_-1_1', 'TRANSFER rec(fib,[3, -2, -1, 1]) ⇒ try on lucas: (3)*lucas(n-1) + (-2)*lucas(n-2) + (-1)*lucas(n-3) + (1)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_3_-4_-2', 'n=4: pred=9 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_-3_3_2', 'n=4: pred=10 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_2_-2_1_2', 'n=4: pred=7 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_3_-2_-1_1', 'n=4: pred=10 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_2_-2_1_2', 'n=4: pred=7 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_3_-2_-1_1', 'n=4: pred=10 != obs=12').
% verified @ algebra/alg_mat_assoc
verified(fact(algebra, alg_mat_assoc, 'alg_mat2_assoc_mod7', '(AB)C = A(BC) for all 2x2 matrices mod 7')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5553', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5554', 'sum I_k = 0 at 3-edge node')).
% verified @ electro/electro_kirchhoff
verified(fact(electro, electro_kirchhoff, 'electro_KCL_node_s5555', 'sum I_k = 0 at 3-edge node')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_N7', 'Delta(fib)[n] = fib[n+1]-fib[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_lucas_N7', 'Delta(lucas)[n] = lucas[n+1]-lucas[n] well-defined')).
% verified @ calculus/calc_fwd_diff
verified(fact(calculus, calc_fwd_diff, 'calc_delta_fib_eq_prev_N7', 'Delta(fib)[n] = fib[n-1] for n>=1')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_o5', 'sum(bits) mod 2 invariant under permute')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_involution_double_o5', 'f(f(x))=x for swap/negate')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_z2_assoc_o5', 'Z2 addition associative on full table')).
% verified @ symmetry/sym_invariant_scan
verified(fact(symmetry, sym_invariant_scan, 'sym_parity_mod2_sum_n_o5', 'parity conserved on 5-bit strings')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x0_N7', 'Delta(sum x^0) = x^0 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x1_N7', 'Delta(sum x^1) = x^1 on prefix')).
% verified @ calculus/calc_ft_discrete
verified(fact(calculus, calc_ft_discrete, 'calc_ft_x2_N7', 'Delta(sum x^2) = x^2 on prefix')).
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n5_s5580', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s5580', 'V≠I+R (as required)').
% verified @ astro/astro_kepler_scan
verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n5', 'T^2/a^3 constant within eps on circular table')).
% finite fail / dead-end
rejected('astro_kepler_wrong_exp_n5', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s7048', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s7049', 'F_12 + F_21 = 0')).
% verified @ physics/phys_action_reaction
verified(fact(physics, phys_action_reaction, 'phys_action_reaction_s7050', 'F_12 + F_21 = 0')).
% finite fail / dead-end
rejected('phys_action_same_sign_broken_s7048', 'F12=4.875611685567913 F21=4.875611685567913 not opposite').
% verified @ loop/loop_pred_scan
verified(fact(loop, loop_pred_scan, 'loop_pred_dx_eq_action_s9513', 'next_x = x + a (Δ(position)=action) on held-out starts')).
% finite fail / dead-end
rejected('loop_pred_ignore_action_s9513', 'finite fail: x0=-6.0 a=1.0 pred=-6.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_double_action_s9513', 'finite fail: x0=-6.0 pred=-4.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_overfit_one_x0_s9513', 'fails new x0: new_x0=-6.0 pred=3.0 obs=-5.0').
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s5568', '2x2 Cramer exact on generated integer system')).
% verified @ algebra/alg_linear_2x2
verified(fact(algebra, alg_linear_2x2, 'alg_solve_2x2_s5569', '2x2 Cramer exact on generated integer system')).
% verified @ astro/astro_transfer_form
verified(fact(astro, astro_transfer_form, 'transfer_kepler3_form_n7', 'TRANSFER Kepler form T^2/a^3 constancy on circular table')).
% finite fail / dead-end
rejected('transfer_kepler_bogus_power_n7', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n6', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n6', 'T>0 and a>0 for all generated bodies')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h_eq_sqrt_a_n6', 'circular specific ang-mom h = sqrt(a) (GM=1) within eps')).
% verified @ astro/astro_angmom_scan
verified(fact(astro, astro_angmom_scan, 'astro_h2_over_a_const_n6', 'h^2/a constant (=1) on circular 2-body table')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7190', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7191', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7192', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7193', 'K = [B]/[A] on 2-species toy')).
% verified @ chem/chem_equilibrium_K
verified(fact(chem, chem_equilibrium_K, 'chem_K_def_s7194', 'K = [B]/[A] on 2-species toy')).
% hypothesis-language skin gen=12
schema(bit_circuit_compose, unlocked(true)).
% learned recurrence on fib
rec(fib, [-1,1,4,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_1_4_2', 'fib(n) = (-1)*fib(n-1) + (1)*fib(n-2) + (4)*fib(n-3) + (2)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [-1,5,0,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_5_0_-2', 'fib(n) = (-1)*fib(n-1) + (5)*fib(n-2) + (0)*fib(n-3) + (-2)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,1,4,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_1_4_2', 'lucas(n) = (-1)*lucas(n-1) + (1)*lucas(n-2) + (4)*lucas(n-3) + (2)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,5,0,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_5_0_-2', 'lucas(n) = (-1)*lucas(n-1) + (5)*lucas(n-2) + (0)*lucas(n-3) + (-2)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [3,1,-5,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_1_-5_-2', 'pell(n) = (3)*pell(n-1) + (1)*pell(n-2) + (-5)*pell(n-3) + (-2)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [4,-2,-4,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_4_-2_-4_-1', 'pell(n) = (4)*pell(n-1) + (-2)*pell(n-2) + (-4)*pell(n-3) + (-1)*pell(n-4)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x1_vs_n_x0_N5', 'Delta(x^1) / (power * x^0) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x2_vs_n_x1_N5', 'Delta(x^2) / (power * x^1) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x3_vs_n_x2_N5', 'Delta(x^3) / (power * x^2) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x4_vs_n_x3_N5', 'Delta(x^4) / (power * x^3) → 1 (eps on lattice)')).
% verified @ calculus/calc_power_diff
verified(fact(calculus, calc_power_diff, 'calc_delta_x5_vs_n_x4_N5', 'Delta(x^5) / (power * x^4) → 1 (eps on lattice)')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod11', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 11) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod11', 'x(y+z) ≡ xy+xz (mod 11)')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_bangbang_s9690', 'bang-bang toward 0 reduces |error| over T on held-out x0')).
% verified @ loop/loop_taxis_scan
verified(fact(loop, loop_taxis_scan, 'loop_taxis_linear_gain_s9690', 'a=-k*x (k=0.5) reduces |error| over T on held-out x0')).
% finite fail / dead-end
rejected('loop_taxis_always_right_s9690', 'increases error: x0=2.0 2→7; x0=4.0 4→9; x0=6.0 6→11').
% finite fail / dead-end
rejected('loop_taxis_always_left_s9690', 'increases error: x0=-2.0 2→7; x0=-4.0 4→9; x0=-5.0 5→10').
lemma('L5_midline_parallel', Parallel, 'AB ∥ NX2').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L5_midline_parallel', 'AB ∥ NX2')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s246', 'AB ∥ NX2')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_1_4_2', 'TRANSFER rec(lucas,[-1, 1, 4, 2]) ⇒ try on fib: (-1)*fib(n-1) + (1)*fib(n-2) + (4)*fib(n-3) + (2)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-1_5_0_-2', 'TRANSFER rec(lucas,[-1, 5, 0, -2]) ⇒ try on fib: (-1)*fib(n-1) + (5)*fib(n-2) + (0)*fib(n-3) + (-2)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_1_-5_-2', 'n=4: pred=2 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_4_-2_-4_-1', 'n=4: pred=2 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_1_4_2', 'TRANSFER rec(fib,[-1, 1, 4, 2]) ⇒ try on lucas: (-1)*lucas(n-1) + (1)*lucas(n-2) + (4)*lucas(n-3) + (2)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-1_5_0_-2', 'TRANSFER rec(fib,[-1, 5, 0, -2]) ⇒ try on lucas: (-1)*lucas(n-1) + (5)*lucas(n-2) + (0)*lucas(n-3) + (-2)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_1_-5_-2', 'n=4: pred=6 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_4_-2_-4_-1', 'n=4: pred=4 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_1_4_2', 'n=4: pred=1 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-1_5_0_-2', 'n=4: pred=5 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_1_4_2', 'n=4: pred=1 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-1_5_0_-2', 'n=4: pred=5 != obs=12').
lemma('L5_midline_parallel', Parallel, 'BC ∥ MN').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L5_midline_parallel', 'BC ∥ MN')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d2_s251', 'BC ∥ MN')).
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n6_s5770', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s5770', 'V≠I+R (as required)').
% verified @ astro/astro_kepler_scan
verified(fact(astro, astro_kepler_scan, 'astro_kepler3_const_n6', 'T^2/a^3 constant within eps on circular table')).
% finite fail / dead-end
rejected('astro_kepler_wrong_exp_n6', 'T²/a² not const: [39.4784, 59.2176, 78.9568, 98.696]').
% verified @ loop/loop_pred_scan
verified(fact(loop, loop_pred_scan, 'loop_pred_dx_eq_action_s9811', 'next_x = x + a (Δ(position)=action) on held-out starts')).
% finite fail / dead-end
rejected('loop_pred_ignore_action_s9811', 'finite fail: x0=-6.0 a=1.0 pred=-6.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_double_action_s9811', 'finite fail: x0=-6.0 pred=-4.0 obs=-5.0').
% finite fail / dead-end
rejected('loop_pred_overfit_one_x0_s9811', 'fails new x0: new_x0=-6.0 pred=3.0 obs=-5.0').
lemma('L6_midline_parallel', Parallel, 'BM ∥ MX4').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L6_midline_parallel', 'BM ∥ MX4')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d3_s257', 'BM ∥ MX4')).
lemma('L7_EqSeg', EqSeg, 'CN = AN').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L7_EqSeg', 'CN = AN')).
% hypothesis-language skin gen=13
schema(form_gate, unlocked(true)).
% learned recurrence on fib
rec(fib, [0,-1,4,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_0_-1_4_3', 'fib(n) = (0)*fib(n-1) + (-1)*fib(n-2) + (4)*fib(n-3) + (3)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [1,3,-2,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_3_-2_-2', 'fib(n) = (1)*fib(n-1) + (3)*fib(n-2) + (-2)*fib(n-3) + (-2)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [0,-1,4,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_0_-1_4_3', 'lucas(n) = (0)*lucas(n-1) + (-1)*lucas(n-2) + (4)*lucas(n-3) + (3)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [1,3,-2,-2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_3_-2_-2', 'lucas(n) = (1)*lucas(n-1) + (3)*lucas(n-2) + (-2)*lucas(n-3) + (-2)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [4,-5,2,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_4_-5_2_2', 'pell(n) = (4)*pell(n-1) + (-5)*pell(n-2) + (2)*pell(n-3) + (2)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [5,-5,-3,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_5_-5_-3_0', 'pell(n) = (5)*pell(n-1) + (-5)*pell(n-2) + (-3)*pell(n-3) + (0)*pell(n-4)')).
lemma('L8_midline_parallel', Parallel, 'AB ∥ NP').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L8_midline_parallel', 'AB ∥ NP')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s265', 'AB ∥ NP')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_0_-1_4_3', 'TRANSFER rec(lucas,[0, -1, 4, 3]) ⇒ try on fib: (0)*fib(n-1) + (-1)*fib(n-2) + (4)*fib(n-3) + (3)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_3_-2_-2', 'TRANSFER rec(lucas,[1, 3, -2, -2]) ⇒ try on fib: (1)*fib(n-1) + (3)*fib(n-2) + (-2)*fib(n-3) + (-2)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_4_-5_2_2', 'n=4: pred=5 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_5_-5_-3_0', 'n=4: pred=2 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_0_-1_4_3', 'TRANSFER rec(fib,[0, -1, 4, 3]) ⇒ try on lucas: (0)*lucas(n-1) + (-1)*lucas(n-2) + (4)*lucas(n-3) + (3)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_3_-2_-2', 'TRANSFER rec(fib,[1, 3, -2, -2]) ⇒ try on lucas: (1)*lucas(n-1) + (3)*lucas(n-2) + (-2)*lucas(n-3) + (-2)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_4_-5_2_2', 'n=5: pred=16 != obs=11').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_5_-5_-3_0', 'n=4: pred=2 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_0_-1_4_3', 'n=4: pred=2 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_3_-2_-2', 'n=4: pred=9 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_0_-1_4_3', 'n=4: pred=2 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_3_-2_-2', 'n=4: pred=9 != obs=12').
lemma('L5_EqSeg', EqSeg, 'AM = BM').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_euclid_conjectures_p0::L5_EqSeg', 'AM = BM')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_midline_full_lemma_reuse_p1', 'AB ∥ NP')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_fib_to_delta_N5', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_lucas_to_delta_N5', 'TRANSFER rec(lucas,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_fib_to_delta_N6', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_lucas_to_delta_N6', 'TRANSFER rec(lucas,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_fib_to_delta_N7', 'TRANSFER rec(fib,[1, 1]) ⇒ Delta structure matches recurrence')).
% verified @ calculus/calc_transfer_form
verified(fact(calculus, calc_transfer_form, 'transfer_rec_lucas_to_delta_N7', 'TRANSFER rec(lucas,[1, 1]) ⇒ Delta structure matches recurrence')).
% hypothesis-language skin gen=14
schema(companion_rec, unlocked(true)).
% learned recurrence on fib
rec(fib, [3,-3,0,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_3_-3_0_2', 'fib(n) = (3)*fib(n-1) + (-3)*fib(n-2) + (0)*fib(n-3) + (2)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [-2,4,3,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-2_4_3_0', 'fib(n) = (-2)*fib(n-1) + (4)*fib(n-2) + (3)*fib(n-3) + (0)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [3,-3,0,2]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_3_-3_0_2', 'lucas(n) = (3)*lucas(n-1) + (-3)*lucas(n-2) + (0)*lucas(n-3) + (2)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [-2,4,3,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-2_4_3_0', 'lucas(n) = (-2)*lucas(n-1) + (4)*lucas(n-2) + (3)*lucas(n-3) + (0)*lucas(n-4)')).
% learned recurrence on pell
rec(pell, [3,-4,5,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_3_-4_5_3', 'pell(n) = (3)*pell(n-1) + (-4)*pell(n-2) + (5)*pell(n-3) + (3)*pell(n-4)')).
% learned recurrence on pell
rec(pell, [5,-4,-5,-1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o4_5_-4_-5_-1', 'pell(n) = (5)*pell(n-1) + (-4)*pell(n-2) + (-5)*pell(n-3) + (-1)*pell(n-4)')).
lemma('L6_midline_parallel', Parallel, 'AB ∥ NX2').
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent::L6_midline_parallel', 'AB ∥ NX2')).
% verified @ geometry/geo_invent
verified(fact(geometry, geo_invent, 'geo_invent_d1_s290', 'AB ∥ NX2')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_3_-3_0_2', 'TRANSFER rec(lucas,[3, -3, 0, 2]) ⇒ try on fib: (3)*fib(n-1) + (-3)*fib(n-2) + (0)*fib(n-3) + (2)*fib(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_-2_4_3_0', 'TRANSFER rec(lucas,[-2, 4, 3, 0]) ⇒ try on fib: (-2)*fib(n-1) + (4)*fib(n-2) + (3)*fib(n-3) + (0)*fib(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_3_-4_5_3', 'n=4: pred=7 != obs=3').
% finite fail / dead-end
rejected('transfer_pell_to_fib_5_-4_-5_-1', 'n=4: pred=1 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_3_-3_0_2', 'TRANSFER rec(fib,[3, -3, 0, 2]) ⇒ try on lucas: (3)*lucas(n-1) + (-3)*lucas(n-2) + (0)*lucas(n-3) + (2)*lucas(n-4)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_-2_4_3_0', 'TRANSFER rec(fib,[-2, 4, 3, 0]) ⇒ try on lucas: (-2)*lucas(n-1) + (4)*lucas(n-2) + (3)*lucas(n-3) + (0)*lucas(n-4)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_3_-4_5_3', 'n=4: pred=11 != obs=7').
% finite fail / dead-end
rejected('transfer_pell_to_lucas_5_-4_-5_-1', 'n=4: pred=1 != obs=7').
% finite fail / dead-end
rejected('transfer_fib_to_pell_3_-3_0_2', 'n=4: pred=9 != obs=12').
% finite fail / dead-end
rejected('transfer_fib_to_pell_-2_4_3_0', 'n=4: pred=1 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_3_-3_0_2', 'n=4: pred=9 != obs=12').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_-2_4_3_0', 'n=4: pred=1 != obs=12').
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7778', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7778', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7779', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7779', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7780', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7780', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7781', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7781', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7782', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7782', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_mom_conserve_s7783', 'm1*u1+m2*u2 = m1*v1+m2*v2 (1D elastic)')).
% verified @ physics/phys_collision
verified(fact(physics, phys_collision, 'phys_energy_conserve_s7783', '½m1 u1²+½m2 u2² = ½m1 v1²+½m2 v2²')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s6259', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s6259', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s6260', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s6260', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_nonneg_k6_s6261', 'H(p)≥0 on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_le_logk_k6_s6261', 'H(p)≤log2(k) on generated 6-outcome dist')).
% verified @ chance/chance_entropy_scan
verified(fact(chance, chance_entropy_scan, 'chance_H_uni_max_k6_s6259', 'H(uniform)≥H(peaked) for k=6')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s6274', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s6274', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s6275', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s6275', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s6276', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s6276', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s6274', 'P(H|E)=0.23215878401412207 != P(H|¬E)=0.42703956975055113').
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_indep_near0_s6047', 'MI(X;Y)≈0 on generated independent joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_dep_pos_s6047', 'MI(X;Y)>0 on xor-coupled joint')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_indep_factorization_s6047', 'P(x,y)=P(x)P(y) on independent joint (eps)')).
% verified @ info/info_mi_scan
verified(fact(info, info_mi_scan, 'info_MI_and_related_s6047', 'MI>0 on (X, X∧Y) joint from full 2-bit table')).
% learned recurrence on fib
rec(fib, [-1,0,5,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_-1_0_5_3', 'fib(n) = (-1)*fib(n-1) + (0)*fib(n-2) + (5)*fib(n-3) + (3)*fib(n-4)')).
% learned recurrence on fib
rec(fib, [1,-2,3,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_-2_3_3', 'fib(n) = (1)*fib(n-1) + (-2)*fib(n-2) + (3)*fib(n-3) + (3)*fib(n-4)')).
% learned recurrence on lucas
rec(lucas, [-1,0,5,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_-1_0_5_3', 'lucas(n) = (-1)*lucas(n-1) + (0)*lucas(n-2) + (5)*lucas(n-3) + (3)*lucas(n-4)')).
% learned recurrence on lucas
rec(lucas, [1,-2,3,3]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o4_1_-2_3_3', 'lucas(n) = (1)*lucas(n-1) + (-2)*lucas(n-2) + (3)*lucas(n-3) + (3)*lucas(n-4)')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s7570', 'atom counts conserved: synth_s7570')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s7571', 'atom counts conserved: synth_s7571')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s7572', 'atom counts conserved: synth_s7572')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s7573', 'atom counts conserved: synth_s7573')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s7574', 'atom counts conserved: synth_s7574')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s7570', 'mismatch {\'H\': 5, \'O\': 1} vs {\'H\': 6, \'O\': 1}').
holds_bit(2, [0,1,0], 1).
holds_bit(3, [0,1,1], 1).
