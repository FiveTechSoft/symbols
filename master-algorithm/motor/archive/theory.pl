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
rejected('rec_fib_order_1_none', 'no fit').
% finite fail / dead-end
rejected('rec_lucas_order_1_none', 'no fit').
% finite fail / dead-end
rejected('rec_pell_order_1_none', 'no fit').
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_midline_lemma_reuse_p0', 'BC ∥ MN')).
% finite fail / dead-end
rejected('NEG_parity_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_and_all_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_xor2_const1', 'bits=[0, 0, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('NEG_scalene_ab_eq_ac', 'finite fail (expected)').
% verified @ sequences/ratio_limits
verified(fact(sequences, ratio_limits, 'ratio_fib_to_phi', 'fib(n+1)/fib(n) → φ')).
% finite fail / dead-end
rejected('NEG_ratio_fib_to_e', '|ratio-e|=1.100e+00 (e=2.718282, φ=1.618034)').
% finite fail / dead-end
rejected('NEG_fib_always_prime', 'F(0)=0 not prime').
lemma('L2_midline_half', HalfSeg, '2·NP = AB').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_full_euclid_conjectures_p1::L2_midline_half', '2·NP = AB')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_midline_full_euclid_conjectures_p1', '2·NP = AB')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_midline_full_lemma_reuse_p1', 'AC ∥ MP')).
% finite fail / dead-end
rejected('bitfn_and_all_is_parity', 'bits=[0, 0, 1]: pred=1 != 0').
bit_fn('bitfn_and_all_is_and_all', and_all, and_all).
holds_bit(2000, [0,0,0], 0).
holds_bit(2001, [0,0,1], 0).
holds_bit(2002, [0,1,0], 0).
holds_bit(2003, [0,1,1], 0).
% verified @ logic/boolean_from_examples
verified(fact(logic, boolean_from_examples, 'bitfn_and_all_is_and_all', 'examples[and_all] ⊨ and_all')).
% finite fail / dead-end
rejected('bitfn_and_all_is_xor2', 'bits=[0, 1, 0]: pred=1 != 0').
% finite fail / dead-end
rejected('bitfn_and_all_is_const_0', 'bits=[1, 1, 1]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_and_all_is_const_1', 'bits=[0, 0, 0]: pred=1 != 0').
lemma('L3_isos_base', EqAng, '∠ABC = ∠ACB').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_isosceles_euclid_conjectures_p2::L3_isos_base', '∠ABC = ∠ACB')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_isosceles_euclid_conjectures_p2', '∠ABC = ∠ACB')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_isosceles_lemma_reuse_p2', '∠ABC = ∠ACB')).
% finite fail / dead-end
rejected('bitfn_xor2_is_parity', 'bits=[0, 0, 1]: pred=1 != 0').
% finite fail / dead-end
rejected('bitfn_xor2_is_and_all', 'bits=[0, 1, 0]: pred=0 != 1').
bit_fn('bitfn_xor2_is_xor2', xor2, xor2).
holds_bit(8000, [0,0,0], 0).
holds_bit(8001, [0,0,1], 0).
holds_bit(8002, [0,1,0], 1).
holds_bit(8003, [0,1,1], 1).
% verified @ logic/boolean_from_examples
verified(fact(logic, boolean_from_examples, 'bitfn_xor2_is_xor2', 'examples[xor2] ⊨ xor2')).
% finite fail / dead-end
rejected('bitfn_xor2_is_const_0', 'bits=[0, 1, 0]: pred=0 != 1').
% finite fail / dead-end
rejected('bitfn_xor2_is_const_1', 'bits=[0, 0, 0]: pred=1 != 0').
lemma('L4_EqSeg', EqSeg, 'AC = BC').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_equilateral_euclid_conjectures_p3::L4_EqSeg', 'AC = BC')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_equilateral_euclid_conjectures_p3', 'AC = BC')).
lemma('L5_EqSeg', EqSeg, 'AC = BC').
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_equilateral_lemma_reuse_p3::L5_EqSeg', 'AC = BC')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_equilateral_lemma_reuse_p3', 'AC = BC')).
lemma('L6_midline_parallel', Parallel, 'PQ ∥ RS').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_varignon_euclid_conjectures_p4::L6_midline_parallel', 'PQ ∥ RS')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_varignon_euclid_conjectures_p4', 'PQ ∥ RS')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_varignon_lemma_reuse_p4', 'PQ ∥ RS')).
lemma('L6_para_opp', Parallel, 'AD ∥ BC').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_para_euclid_conjectures_p5::L6_para_opp', 'AD ∥ BC')).
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_para_euclid_conjectures_p5', 'AD ∥ BC')).
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_para_lemma_reuse_p5', 'AB ∥ CD')).
% learned recurrence on fib
rec(fib, [1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o2_1_1', 'fib(n) = (1)*fib(n-1) + (1)*fib(n-2)')).
% learned recurrence on lucas
rec(lucas, [1,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o2_1_1', 'lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2)')).
% learned recurrence on pell
rec(pell, [2,1]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o2_2_1', 'pell(n) = (2)*pell(n-1) + (1)*pell(n-2)')).
% learned recurrence on fib
rec(fib, [1,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_fib_o3_1_1_0', 'fib(n) = (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3)')).
% learned recurrence on lucas
rec(lucas, [1,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_lucas_o3_1_1_0', 'lucas(n) = (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3)')).
% learned recurrence on pell
rec(pell, [2,1,0]).
% verified @ sequences/linear_recurrences
verified(fact(sequences, linear_recurrences, 'rec_pell_o3_2_1_0', 'pell(n) = (2)*pell(n-1) + (1)*pell(n-2) + (0)*pell(n-3)')).
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_1', 'TRANSFER rec(lucas,[1, 1]) ⇒ try on fib: (1)*fib(n-1) + (1)*fib(n-2)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_1', 'n=2: pred=2 != obs=1').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_lucas_to_fib_1_1_0', 'TRANSFER rec(lucas,[1, 1, 0]) ⇒ try on fib: (1)*fib(n-1) + (1)*fib(n-2) + (0)*fib(n-3)')).
% finite fail / dead-end
rejected('transfer_pell_to_fib_2_1_0', 'n=3: pred=3 != obs=2').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_1', 'TRANSFER rec(fib,[1, 1]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_1', 'n=2: pred=4 != obs=3').
% verified @ sequences/transfer_recurrence
verified(fact(sequences, transfer_recurrence, 'transfer_fib_to_lucas_1_1_0', 'TRANSFER rec(fib,[1, 1, 0]) ⇒ try on lucas: (1)*lucas(n-1) + (1)*lucas(n-2) + (0)*lucas(n-3)')).
% finite fail / dead-end
rejected('transfer_pell_to_lucas_2_1_0', 'n=3: pred=7 != obs=4').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_1', 'n=2: pred=1 != obs=2').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_1', 'n=2: pred=1 != obs=2').
% finite fail / dead-end
rejected('transfer_fib_to_pell_1_1_0', 'n=3: pred=3 != obs=5').
% finite fail / dead-end
rejected('transfer_lucas_to_pell_1_1_0', 'n=3: pred=3 != obs=5').
true_mod(fib, 2, 3).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m2', 'π_fib(2)=3')).
true_mod(fib, 3, 8).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m3', 'π_fib(3)=8')).
true_mod(fib, 4, 6).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m4', 'π_fib(4)=6')).
true_mod(fib, 5, 20).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m5', 'π_fib(5)=20')).
% finite fail / dead-end
rejected('pisano_fib_m6', 'insufficient prefix').
true_mod(fib, 7, 16).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m7', 'π_fib(7)=16')).
true_mod(fib, 8, 12).
% verified @ sequences/modular_periods
verified(fact(sequences, modular_periods, 'pisano_fib_m8', 'π_fib(8)=12')).
% finite fail / dead-end
rejected('pisano_fib_m9', 'insufficient prefix').
% finite fail / dead-end
rejected('pisano_fib_m10', 'insufficient prefix').
% verified @ sequences/bilinear_fib
verified(fact(sequences, bilinear_fib, 'cassini_fib', 'F(n+1)F(n-1)-F(n)^2 = (-1)^n')).
lemma('L1_para_opp', Parallel, 'AB ∥ CD').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_para_euclid_conjectures_p5::L1_para_opp', 'AB ∥ CD')).
lemma('L1_para_opp', Parallel, 'AD ∥ BC').
% verified @ geometry/lemma_reuse
verified(fact(geometry, lemma_reuse, 'geo_para_lemma_reuse_p5::L1_para_opp', 'AD ∥ BC')).
holds_bit(9000, [0,0,0], 0).
holds_bit(9001, [0,0,1], 0).
holds_bit(9002, [0,1,0], 1).
holds_bit(9003, [0,1,1], 1).
lemma('L2_EqAng', EqAng, '∠ABC = ∠ADC').
% verified @ geometry/euclid_conjectures
verified(fact(geometry, euclid_conjectures, 'geo_para_euclid_conjectures_p5::L2_EqAng', '∠ABC = ∠ADC')).
