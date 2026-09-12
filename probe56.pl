% probe56.pl — reproduccion minima del setof con conjuncion.
:- dynamic m/3.

p56 :-
    retractall(m(_, _, _)),
    assertz(m(god, create, heaven)),
    assertz(m(god, divide, light)),
    assertz(m(moses, lead, people)),
    assertz(m(people, see, land)),
    assertz(m(david, rule, israel)),
    assertz(m(israel, fear, god)),
    setof(B, A^R^(m(A, R, B), m(B, _, _)), Ms),
    format('conj: ~w~n', [Ms]),
    setof(S, R^O^m(S, R, O), SS),
    setof(O, S^R^m(S, R, O), OO),
    ord_intersection(SS, OO, Both),
    format('separate: ~w~n', [Both]).
