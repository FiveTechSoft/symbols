% probe54.pl — diagnostico: conjuntos S/O, interseccion, atomos sospechosos.
:- consult('corpus.pl').

p54 :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    consult('kjv_memory.pl'),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    setof(S, R^O^memory_relation(S, R, O, _, _), SS),
    length(SS, NS),
    setof(O, S^R^memory_relation(S, R, O, _, _), OO),
    length(OO, NO),
    ord_intersection(SS, OO, Both),
    length(Both, NB),
    format('subjects=~w objects=~w both=~w~n', [NS, NO, NB]),
    ( NB < 20 -> format('both_list=~w~n', [Both]) ; pr_first(Both, 20) ),
    setof(R, S^O^memory_relation(S, R, O, _, _), Rs),
    length(Rs, NR),
    format('relations=~w~n', [NR]),
    % relaciones cuyo nombre impreso colisiona con otro atomo distinto
    findall(R, (member(R, Rs), atom(R),
                atom_chars(R, Cs), member(C, Cs), \+ char_ok(C)), Weird),
    format('weird_atoms=~w~n', [Weird]).

char_ok(C) :- char_code(C, N), N >= 32, N =< 126.

pr_first(_, 0) :- !.
pr_first([], _) :- !.
pr_first([H|T], K) :- format('both: ~q~n', [H]), K1 is K - 1, pr_first(T, K1).
