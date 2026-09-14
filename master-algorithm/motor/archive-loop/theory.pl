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
% learned recurrence on fib
rec(fib, [1,1]).
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
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1053', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1053', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1054', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1054', 'logit(post)=logit(prior)+log(LR) on generated table')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_bayes_id_s1055', 'P(H|E)=P(E|H)P(H)/P(E) on generated 2x2')).
% verified @ chance/chance_bayes_scan
verified(fact(chance, chance_bayes_scan, 'chance_logodds_add_s1055', 'logit(post)=logit(prior)+log(LR) on generated table')).
% finite fail / dead-end
rejected('chance_bayes_swap_s1053', 'P(H|E)=0.6387213381318649 != P(H|¬E)=0.7142428226980971').
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_2H2+O2→2H2O', 'atom counts conserved: 2H2+O2→2H2O')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6022', 'atom counts conserved: synth_s6022')).
% verified @ chem/chem_atom_balance
verified(fact(chem, chem_atom_balance, 'chem_atom_balance_synth_s6023', 'atom counts conserved: synth_s6023')).
% finite fail / dead-end
rejected('chem_unbalanced_broken_s6022', 'mismatch {\'H\': 4, \'O\': 2} vs {\'H\': 5, \'O\': 2}').
% verified @ electro/electro_ohm
verified(fact(electro, electro_ohm, 'electro_ohm_V_eq_IR_n3_s4038', 'V=IR on generated triples (eps)')).
% finite fail / dead-end
rejected('electro_ohm_broken_add_s4038', 'V≠I+R (as required)').
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
