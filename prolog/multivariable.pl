% multivariable.pl
% Composicion multivariable: no basta recorrer N aristas, hay que mantener
% la IDENTIDAD de cada variable interna (A =/= B).
% - analyze_variables/2: tabula los ligados internos por instancia y
%   comprueba distincion (A\==B) y variacion (no son hubs constantes).
% - induce_distinct/2: si la distincion se sostiene en todo el soporte
%   observado, afirma distinct_rule con restriccion all_different.
% - mv_predict/3: camino + restriccion de identidad.
:- use_module(library(lists)).

:- dynamic distinct_rule/3.  % distinct_rule(Target, Path, all_different)

% path_bindings(+S, +O, +Path, -Internals): nodos internos ligados.
path_bindings(S, O, [R], []) :-
    memory_relation(S, R, O, _, _).
path_bindings(S, O, [R|Rs], [M|Bs]) :-
    Rs \== [],
    memory_relation(S, R, M, _, _),
    path_bindings(M, O, Rs, Bs).

% analyze_variables(+Target, +Path): evidencia de identidad.
analyze_variables(Target, Path) :-
    findall(S-O,
            ( memory_relation(S, Target, O, _, _),
              path_bindings(S, O, Path, _)
            ),
            Pairs0),
    sort(Pairs0, Pairs),
    length(Pairs, NP),
    findall(Bs, ( member(S-O, Pairs),
                  path_bindings(S, O, Path, Bs)
                ),
            AllBs),
    length(AllBs, NB),
    include(alldiff, AllBs, Dists),
    length(Dists, ND),
    format('Variable analysis ~w :- ~w:~n', [Target, Path]),
    format('  supported pairs=~w bindings=~w all_different=~w/~w~n',
           [NP, NB, ND, NB]),
    forall(member(S-O, Pairs),
           ( findall(Bs, path_bindings(S, O, Path, Bs), BsList),
             format('  ~w -> ~w : ~w~n', [S, O, BsList])
           )),
    varying(AllBs).

% varying: cada posicion interna toma >1 valor (no es hub constante).
varying(AllBs) :-
    ( AllBs = [] -> format('  (no bindings)~n', [])
    ; AllBs = [First|_],
      length(First, K),
      forall(between(1, K, I),
             ( findall(V, (member(Bs, AllBs), nth1(I, Bs, V)), Vs),
               sort(Vs, UV), length(UV, U),
               format('  internal[~w] distinct values=~w ~w~n',
                      [I, U, UV])
             ))
    ).

alldiff(Bs) :-
    sort(Bs, S), length(Bs, N), length(S, N).

% induce_distinct(+Target, +Path)
induce_distinct(Target, Path) :-
    retractall(distinct_rule(_, _, _)),
    composed_rule(Target, Path, _),
    analyze_variables(Target, Path),
    findall(Bs, ( memory_relation(S, Target, O, _, _),
                  path_bindings(S, O, Path, Bs)
                ),
            AllBs),
    AllBs \== [],
    forall(member(Bs, AllBs), alldiff(Bs)),
    assertz(distinct_rule(Target, Path, all_different)),
    format('Distinct rule: ~w(X,Z) :- ~w + all_different~n',
           [Target, Path]).

% mv_predict(+S, +Target, +O): camino con identidad preservada.
mv_predict(S, Target, O) :-
    distinct_rule(Target, Path, all_different),
    path_bindings(S, O, Path, Bs),
    alldiff(Bs).

show_distinct_rules :-
    nl, writeln('===== DISTINCT RULES ====='),
    forall(distinct_rule(T, P, C),
           format('~w(X,Z) :- ~w + ~w~n', [T, P, C])).
