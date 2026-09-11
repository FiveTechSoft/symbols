% correspondence.pl
% Descubre correspondencias estructurales emergentes (variables simbolicas)
% sin que nadie le diga al sistema que significa "N".
% Idea: observar regularidades de sufijo en pares (S,O) bajo una relacion R,
% inducir regla suffix_match y usarla para predecir combinaciones nunca vistas
% y rechazar distractores.
:- use_module(library(lists)).

:- dynamic correspondence/3.
:- dynamic correspondence_rule/3.

% entity_suffix(+Entity, -Num)
% Extrae el sufijo numerico final de un atomo (q13 -> 13, x7 -> 7).
% Falla si la entidad no tiene sufijo numerico.
entity_suffix(Entity, Num) :-
    atom(Entity),
    atom_chars(Entity, Chars),
    reverse(Chars, Rev),
    take_trailing_digits(Rev, RevDigits, _Rest),
    RevDigits \= [],
    reverse(RevDigits, Digits),
    number_chars(Num, Digits).

take_trailing_digits([], [], []).
take_trailing_digits([H|T], [H|Ds], Rest) :-
    char_type(H, digit), !,
    take_trailing_digits(T, Ds, Rest).
take_trailing_digits(L, [], L).

both_have_suffix(S-O) :-
    entity_suffix(S, _),
    entity_suffix(O, _).

same_suffix(S-O) :-
    entity_suffix(S, N),
    entity_suffix(O, N).

% discover_suffix_correspondence(+Relation)
% Observa todos los pares S --Relation--> O en memoria y mide
% que fraccion comparte sufijo. Si Rate >= 0.90 induce regla.
discover_suffix_correspondence(Rel) :-
    retractall(correspondence(_, _, _)),
    retractall(correspondence_rule(_, _, _)),
    findall(S-O, memory_relation(S, Rel, O, _, _), Pairs),
    include(both_have_suffix, Pairs, Valid),
    length(Valid, Total),
    ( Total =:= 0 ->
        format('Correspondence ~w: no suffix pairs found~n', [Rel]),
        fail
    ; true
    ),
    include(same_suffix, Valid, Same),
    length(Same, SameCount),
    Rate is SameCount / Total,
    format('Correspondence ~w: ~w/~w same suffix (rate=~2f)~n',
           [Rel, SameCount, Total, Rate]),
    forall(member(S-O, Same),
           assertz(correspondence(S, O, 1.0))),
    ( Rate >= 0.90, Total >= 5 ->
        assertz(correspondence_rule(Rel, suffix_match, Rate)),
        format('Rule induced: ~w --suffix_match--> (rate=~2f)~n',
               [Rel, Rate])
    ; format('No rule induced (need rate>=0.90, total>=5)~n', []),
      fail
    ).

% correspondence_predict(+S, +Rel, +O)
% Regla abstracta con variable: Q(N) --Rel--> X(N).
% Funciona incluso para combinaciones NUNCA vistas en entrenamiento,
% porque solo exige la regla inducida + igualdad de sufijos.
correspondence_predict(S, Rel, O) :-
    correspondence_rule(Rel, suffix_match, _),
    entity_suffix(S, N),
    entity_suffix(O, N).

show_correspondence_rules :-
    nl, writeln('===== CORRESPONDENCE RULES ====='),
    forall(correspondence_rule(Rel, Kind, Rate),
           format('~w --~w--> (rate=~2f)~n', [Rel, Kind, Rate])).
