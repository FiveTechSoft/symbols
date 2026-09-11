% abduction.pl
% ABDUCCION NIVEL 1: variable intermedia latente SIN inventar el nodo.
% Dado composed_rule(Target, Path), si el camino S -Path-> O esta roto,
% devuelve la hipotesis minima (eslabones need/3 que faltan) sin assertarla.
% - accepted(1, Hyps): un eslabon missing, hipotesis unica -> abducible.
% - ambiguous(H1, H2): dos stubs compiten (forward vs backward) -> nivel 2.
% - rejected: 2+ eslabones (Occam) -> se rechaza en nivel 1.
% - deduced: camino completo (no es abduccion, ya estaba observado).
:- use_module(library(lists)).

% abduce(+Target, +S, +O, -Result)
abduce(Target, S, O, deduced) :-
    composed_rule(Target, Path, _),
    forward(S, O, Path, 0, _), !.
abduce(Target, S, O, Result) :-
    composed_rule(Target, Path, _),
    forward(S, O, Path, FC, FH),
    backward(S, O, Path, BC, BH),
    ( FC =:= 1, BC =:= 1, FH \== BH ->
        Result = ambiguous(FH, BH)
    ; FC =:= 1 ->
        Result = accepted(1, FH)
    ; BC =:= 1 ->
        Result = accepted(1, BH)
    ; Result = rejected(FC, BC)
    ).

% forward: alinea desde S; prefiere stubs existentes (Occam).
forward(S, O, [R], 0, []) :-
    memory_relation(S, R, O, _, _), !.
forward(S, O, [R], 1, [need(S, R, O)]).
forward(S, O, [R|Rs], C, H) :-
    Rs \== [],
    findall(M, memory_relation(S, R, M, _, _), Ms),
    Ms \== [], !,
    findall(C1-H1,
            ( member(M, Ms), forward(M, O, Rs, C1, H1) ),
            Pairs),
    keysort(Pairs, [C-H|_]).
forward(S, O, [R|Rs], C, [need(S, R, M)|H2]) :-
    Rs \== [],
    forward(M, O, Rs, C2, H2),
    C is C2 + 1.

% backward: alinea desde O (detecta caso sufijo-presente y ambiguedad).
backward(S, O, [R], 0, []) :-
    memory_relation(S, R, O, _, _), !.
backward(S, O, [R], 1, [need(S, R, O)]).
backward(S, O, Path, C, H) :-
    append(Init, [R], Path),
    Init \== [],
    findall(M, memory_relation(M, R, O, _, _), Ms),
    Ms \== [], !,
    findall(C1-H1,
            ( member(M, Ms), backward(S, M, Init, C1, H1) ),
            Pairs),
    keysort(Pairs, [C-H|_]).
backward(S, O, Path, C, H2) :-
    append(Init, [R], Path),
    Init \== [],
    backward(S, M, Init, C2, H1),
    C is C2 + 1,
    append(H1, [need(M, R, O)], H2).

% abductive_predict: solo hipotesis unicas de coste <= 1.
abductive_predict(S, Target, O) :-
    abduce(Target, S, O, accepted(C, _)),
    C =< 1.

show_abduction(Target, S, O) :-
    abduce(Target, S, O, R),
    format('~w --~w--> ~w : ~w~n', [S, Target, O, R]).
