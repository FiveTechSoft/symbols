% composition.pl
% Composicion simbolica emergente: descubre patrones de camino
%   r1(X,Y) /\ r2(Y,Z)  =>  r3(X,Z)
% sin que nadie le diga ni las relaciones ni la longitud del camino.
% Busca secuencias de relaciones de longitud 1..MaxLen sobre todo el
% vocabulario (menos el Target) y elige la de mejor F1 con margen claro.
:- use_module(library(lists)).

:- dynamic composed_rule/3.  % composed_rule(Target, Path, F1)

% discover_composition(+Target, +MaxLen)
discover_composition(Target, MaxLen) :-
    retractall(composed_rule(_, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(R, (memory_relation(_, R, _, _, _), R \== Target), Rs0),
    sort(Rs0, Vocab),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    format('Path candidates for ~w (SC=~w OC=~w, maxlen=~w):~n',
           [Target, SC, OC, MaxLen]),
    forall(member(F1-P-S, Ranked),
           format('  Path=~w F1=~4f support=~w~n', [P, F1, S])),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(composed_rule(Target, BestPath, BestF1)),
    format('Composed: ~w(S,O) :- ~w  (F1=~4f support=~w)~n',
           [Target, BestPath, BestF1, BestSup]).

% pattern(+Len, +Vocab, -Path): secuencia ordenada con repeticion
pattern(1, Vocab, [R]) :- member(R, Vocab).
pattern(Len, Vocab, [R|Rs]) :-
    Len > 1, member(R, Vocab),
    Len1 is Len - 1, pattern(Len1, Vocab, Rs).

% path_holds(+S, +O, +Path): camino dirigido S -Path-> O
path_holds(S, O, [R]) :- memory_relation(S, R, O, _, _).
path_holds(S, O, [R1|Rs]) :-
    Rs \= [],
    memory_relation(S, R1, M, _, _),
    path_holds(M, O, Rs).

score_path(Target, SS, OS, Path, F1, Support) :-
    findall(S-O,
            ( member(S, SS), member(O, OS),
              memory_relation(S, Target, O, _, _)
            ),
            TPairs0),
    sort(TPairs0, TPairs),
    length(TPairs, Support),
    ( Support =:= 0 -> F1 = 0.0
    ; findall(S-O,
              ( member(S, SS), member(O, OS),
                path_holds(S, O, Path)
              ),
              Match0),
      sort(Match0, Match),
      intersection(TPairs, Match, TP),
      length(TP, NTP),
      length(Match, NM),
      ( NM =:= 0 -> P = 0.0 ; P is NTP / NM ),
      Rcall is NTP / Support,
      ( P + Rcall =:= 0 -> F1 = 0.0
      ; F1 is 2 * P * Rcall / (P + Rcall)
      )
    ).

% composed_predict(+S, +Target, +O)
composed_predict(S, Target, O) :-
    composed_rule(Target, Path, _),
    path_holds(S, O, Path).

show_composed_rules :-
    nl, writeln('===== COMPOSED RULES ====='),
    forall(composed_rule(T, P, F1),
           format('~w(S,O) :- ~w  F1=~4f~n', [T, P, F1])).
