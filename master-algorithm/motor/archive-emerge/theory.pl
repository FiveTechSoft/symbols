% scratch emerge
:- dynamic obs/3, rec/2, rejected/2, verified/1, lemma/3.
verified(fact(sequences, bilinear_schema, 'bilin_fib_r1', 'bilinear product identity')).
verified(fact(sequences, bilinear_schema_r4_g4, 'bilin_fib_r2', 'bilinear product identity')).
verified(fact(sequences, bilinear_schema_r4_g5, 'bilin_fib_r3', 'bilinear product identity')).
verified(fact(physics, phys_collision, 'phys_mom_conserve_s1', 'm1u1+m2u2=m1v1+m2v2')).
verified(fact(physics, phys_collision, 'phys_mom_conserve_s2', 'm1u1+m2u2=m1v1+m2v2')).
verified(fact(sequences, linear_recurrences, 'rec_fib_o2_1_1', 'fib=[1,1]')).
verified(fact(sequences, linear_recurrences, 'rec_fib_o4_1_1_0_0', 'fib pad o4')).
rejected('false_cassini_as_pell_law', 'form-family mismatch: bilin_cassini ≠ pell_rec').
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
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_binom_expand_mod5', '(x+y)^2 ≡ x^2+2xy+y^2 (mod 5) exhaustive')).
% verified @ algebra/alg_poly_zn
verified(fact(algebra, alg_poly_zn, 'alg_distrib_mod5', 'x(y+z) ≡ xy+xz (mod 5)')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_increases_with_a_n3', 'T increases with a on generated circular table')).
% verified @ astro/astro_period_scan
verified(fact(astro, astro_period_scan, 'astro_T_pos_n3', 'T>0 and a>0 for all generated bodies')).
