% hierarchical.pl
% Biblioteca de habilidades: reglas aprendidas como unidades de busqueda.
% learn_unit/3 registra skill(Unit, Path, Sig); discover_over_units/4
% busca SOLO sobre unidades (jamas expande sus internos).
:- consult('guided_search.pl').
:- use_module(library(lists)).

:- dynamic skill/3.
% skill(Unit, Path, Sig)

% learn_unit(+Target, +MaxLen): incident discovery + constrained + registro.
learn_unit(Target, MaxLen) :-
    run_discovery(guided, Target, MaxLen, Stats),
    Stats = stats(Path, F1, Sup, Gen, Ev, Pr, Ms),
    retractall(composed_rule(Target, _, _)),
    assertz(composed_rule(Target, Path, F1)),
    induce_constrained(Target, Path),
    constrained_rule(Target, Path, Sig),
    retractall(skill(Target, _, _)),
    assertz(skill(Target, Path, Sig)),
    format('SKILL ~w :- ~w + ~w (F1=~4f sup=~w eval=~w)~n',
           [Target, Path, Sig, F1, Sup, Ev]).

% discover_over_units(+Target, +Units, +MaxLen, -Stats): vocabulario dado.
discover_over_units(Target, Units, MaxLen, Stats) :-
    retractall(composed_rule(Target, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    sort(Units, Vocab),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(composed_rule(Target, BestPath, BestF1)),
    length(Scored, Ev),
    format('UNITS composed: ~w :- ~w (F1=~4f sup=~w eval=~w)~n',
           [Target, BestPath, BestF1, BestSup, Ev]),
    Stats = stats(BestPath, BestF1, BestSup, Ev, Ev, 0, 0).

show_skills :-
    nl, writeln('===== SKILL LIBRARY ====='),
    forall(skill(U, P, S),
           format('SKILL ~w :- ~w + ~w~n', [U, P, S])).
