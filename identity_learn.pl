% identity_learn.pl — IDENTIDAD DEDUCIDA, no programada.
% Dos simbolos son el mismo si comparten contextos relacionales:
%   X ~ Y  ssi  #{(R,Z) : (X,R,Z) y (Y,R,Z)} >= MinShared
%   (mas la direccion objeto: {(S,R) : (S,R,X) y (S,R,Y)})
% Usa SOLO memory_relation (hechos observados). Sin listas de alias,
% sin casefolding semantico, sin reglas de equivalencia. Emite
% candidate_identity/4 con soporte y evidencia (los contextos compartidos).
% La fusion NO es automatica (requiere politica de confianza): v1 reporta.
% EXP18 ya hacia abduccion de identidad; esto lo lleva a escala libro.
:- consult('corpus.pl').

:- use_module(library(lists)).

:- dynamic candidate_identity/4.

learn_identities(File, MinShared) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(candidate_identity(_, _, _, _)),
    consult(File),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    memory_size(NF),
    format('IDENT load facts=~w min_shared=~w~n', [NF, MinShared]),
    findall((R, Z), memory_relation(_, R, Z, _, _), RZ0),
    sort(RZ0, Contexts),
    length(Contexts, NC),
    format('IDENT contexts=~w~n', [NC]),
    findall(X-Y-N-Ev, (member((R, Z), Contexts),
                       subjects_of(R, Z, Ss),
                       pair_support(Ss, R, Z, X, Y, N, Ev),
                       N >= MinShared),
            Raw),
    msort(Raw, SR),
    clump_pairs(SR, Grouped),
    msort(Grouped, SG),
    print_identities(SG, 20).

% Sujetos con (S,R,Z), excluyendo nodos evento/mencion/narrador.
subjects_of(R, Z, Ss) :-
    findall(S, (memory_relation(S, R, Z, _, _),
                \+ sub_atom(S, _, _, _, '_ch'),
                S \== narrator,
                \+ mention_node(S)), Ss0),
    sort(Ss0, Ss).

mention_node(S) :-
    sub_atom(S, _, 1, 0, '_'), !, fail.
mention_node(S) :-
    atom_chars(S, Cs),
    append(_, ['_'|Ds], Cs),
    Ds \== [],
    forall(member(D, Ds), char_type(D, digit)).

% Pares dentro de un grupo con su contexto como evidencia inicial.
pair_support(Ss, R, Z, X, Y, 1, [(R, Z)]) :-
    select(X, Ss, Rest),
    member(Y, Rest),
    X @< Y.

% Agrupa por par: suma soportes y evidencias (findall+sort, sin ^).
clump_pairs([], []).
clump_pairs([X-Y-N-Ev|T], Out) :-
    clump_same(T, X, Y, N, Ev, NSup, NEv, Rest),
    Out = [(NSup, X, Y, NEv)|More],
    clump_pairs(Rest, More).

clump_same([], _, _, N, Ev, N, Ev, []).
clump_same([X-Y-N-Ev|T], X, Y, Acc, EvAcc, NSup, NEv, Rest) :- !,
    N1 is Acc + N,
    append(EvAcc, Ev, Ev1),
    clump_same(T, X, Y, N1, Ev1, NSup, NEv, Rest).
clump_same([H|T], X, Y, Acc, EvAcc, Acc, EvAcc, [H|T]).

print_identities(_, 0) :- !.
print_identities([], _) :-
    writeln('IDENT: no candidates.'), !.
print_identities([(N, X, Y, Ev)|T], K) :-
    take_ev(Ev, 5, Show),
    format('IDENTITY ~w ~~ ~w support=~w e.g.~w~n', [X, Y, N, Show]),
    K1 is K - 1,
    print_identities(T, K1).

take_ev(_, 0, []) :- !.
take_ev([], _, []) :- !.
take_ev([H|T], K, [H|R]) :-
    K1 is K - 1,
    take_ev(T, K1, R).
