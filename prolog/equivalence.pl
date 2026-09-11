% equivalence.pl
% Identidad (clase de equivalencia: evidencia EXACTA) vs pertenencia
% (concepto: similitud de perfiles >= umbral). Perfiles = pares
% (Rel,Obj)/(Subj,Rel): la granularidad que las firmas planas pierden.
% revision/0: re-deriva todo, registra SPLIT/MERGE/JOIN/LEAVE y compara
% reglas por extension (miembros ordenados), no por IDs de concepto.
:- use_module(library(lists)).

:- dynamic prev_class/1.      % prev_class(sorted_members)
:- dynamic prev_concept/2.    % prev_concept(C, sorted_members)
:- dynamic prev_rulekey/1.    % prev_rulekey((SS, R, OS))

% entity_profile(+E, -Profile): pares que mencionan a E.
entity_profile(E, Profile) :-
    findall(out(R, O), memory_relation(E, R, O, _, _), Outs),
    findall(in(S, R), memory_relation(S, R, E, _, _), Ins),
    append(Outs, Ins, All),
    sort(All, Profile).

% equiv_class(+E, -Members): misma evidencia exacta.
equiv_class(E, Members) :-
    entity_profile(E, P),
    findall(X, ( entity(X), entity_profile(X, P) ), M0),
    sort(M0, Members).

entity(X) :- memory_relation(X, _, _, _, _).
entity(X) :- memory_relation(_, _, X, _, _).

% all_classes(-Classes): particion en clases exactas.
all_classes(Classes) :-
    findall(E, entity(E), E0),
    sort(E0, Es),
    all_classes_acc(Es, [], Classes).

all_classes_acc([], Acc, Acc).
all_classes_acc([E|Es], Acc, Classes) :-
    ( member(C, Acc), member(E, C) ->
        all_classes_acc(Es, Acc, Classes)
    ; equiv_class(E, M),
      all_classes_acc(Es, [M|Acc], Classes)
    ).

% discriminant(+A, +B, -Diff): evidencia que los separa.
discriminant(A, B, OnlyA, OnlyB) :-
    entity_profile(A, PA),
    entity_profile(B, PB),
    subtract(PA, PB, OnlyA),
    subtract(PB, PA, OnlyB).

profile_similarity(A, B, Score) :-
    entity_profile(A, PA),
    entity_profile(B, PB),
    intersection(PA, PB, Common),
    append(PA, PB, Both),
    sort(Both, Union),
    length(Union, LU),
    length(Common, LC),
    ( LU =:= 0 -> Score = 1.0
    ; Score is LC / LU
    ).

% snapshot_verdicts: congela clases, conceptos (por miembros) y reglas.
snapshot_verdicts :-
    retractall(prev_class(_)),
    retractall(prev_concept(_, _)),
    retractall(prev_rulekey(_)),
    all_classes(Cs),
    forall(member(C, Cs), assertz(prev_class(C))),
    forall(concept(C, _, _),
           ( findall(M, concept_member(C, M, _), Ms0),
             sort(Ms0, Ms),
             assertz(prev_concept(C, Ms))
           )),
    forall(learned_rule(_, SC, R, OC, _),
           ( concept_members_sorted(SC, SS),
             concept_members_sorted(OC, OS),
             assertz(prev_rulekey((SS, R, OS)))
           )).

concept_members_sorted(C, Ms) :-
    findall(M, concept_member(C, M, _), M0),
    sort(M0, Ms).

% revision_report: compara antes/después.
revision_report :-
    nl, writeln('===== REVISION REPORT ====='),
    all_classes(CsNow),
    forall(member(C, CsNow),
           ( prev_class(C) -> true
           ; format('NEW/CHANGED class: ~w~n', [C])
           )),
    forall(prev_class(C),
           ( member(C, CsNow) -> true
           ; format('LOST class: ~w (split or merged away)~n', [C])
           )),
    findall((SS, R, OS),
            ( learned_rule(_, SC, R, OC, _),
              concept_members_sorted(SC, SS),
              concept_members_sorted(OC, OS)
            ),
            Now0),
    sort(Now0, Now),
    findall(K, prev_rulekey(K), Prev0),
    sort(Prev0, Prev),
    subtract(Now, Prev, Added),
    subtract(Prev, Now, Dropped),
    length(Added, NA),
    length(Dropped, ND),
    forall(member(K, Added), format('RULE ADDED:   ~w~n', [K])),
    forall(member(K, Dropped), format('RULE DROPPED: ~w~n', [K])),
    subtract(Now, Added, Kept),
    length(Kept, NK),
    format('rules kept=~w added=~w dropped=~w~n', [NK, NA, ND]).
