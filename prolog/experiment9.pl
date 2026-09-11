% experiment9.pl
% EXPERIMENT 9 - MULTIVARIABLE COMPOSITION
% Cadenas: A --r1--> M --r2--> N --r3--> C, mas r4 directos observados.
% r4 oculto en 2 cadenas; 6 distractores cruzados.
% El sistema debe descubrir [r1,r2,r3] (longitud NO dada, busca 1..4)
% y mantener la identidad A=/=B de las dos variables internas.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment9 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 9 - MULTIVARIABLE COMPOSITION'),
    writeln('=============================================='),
    nl,
    memory_size(Size),
    format('Training facts: ~w~n~n', [Size]),
    discover_concepts,
    show_concepts,
    discover_concept_relations,
    show_concept_relations,
    discover_rules,
    show_rules,
    discover_composition(r4, 4),
    show_composed_rules,
    induce_distinct(r4, [r1, r2, r3]),
    show_distinct_rules,
    run_tests.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)).

% ground truth (cadena completa; r4 retenido en 2 casos)
chain(sura, pra, pla, suna).
chain(belo, tre, tlu, bena).
chain(kima, cri, klu, kina).
chain(doru, dro, dru, dona).
chain(fela, fru, flu, fena).
chain(grino, gru, glu, grina).

hidden(kima).
hidden(fela).

create_training_data :-
    forall(chain(A, M, N, C),
           ( remember_relation(A, r1, M, 1.0),
             remember_relation(M, r2, N, 1.0),
             remember_relation(N, r3, C, 1.0),
             ( hidden(A) -> true
             ; remember_relation(A, r4, C, 1.0)
             )
           )).

hidden_pair(A, C) :-
    hidden(A), chain(A, _, _, C).

distractor(A, C) :-
    member((A, C), [(sura, kina), (kima, suna), (fela, dona),
                    (belo, fena), (doru, bena), (grino, suna)]).

% --- conceptos (umbral 0.60) ---

entity(E) :- memory_relation(E, _, _, _, _).
entity(E) :- memory_relation(_, _, E, _, _).

discover_concepts :-
    findall(E, entity(E), E0),
    sort(E0, Es),
    forall(member(E, Es), assign_concept(E)).

assign_concept(E) :-
    entity_signature(E, Sig),
    findall(Sc-C,
            (concept(C, CSig, _), signature_similarity(Sig, CSig, Sc)),
            Ms),
    best_concept(Ms, Best, BC),
    ( Best >= 0.60 -> add_member(BC, E, Best)
    ; create_concept(E, Sig)
    ).

best_concept([], 0.0, none).
best_concept(Ms, Sc, C) :-
    keysort(Ms, S), reverse(S, [Sc-C|_]).

create_concept(E, Sig) :-
    findall(N, concept(concept(N), _, _), Ns),
    next_concept_number(Ns, N),
    C = concept(N),
    assertz(concept(C, Sig, 1)),
    assertz(concept_member(C, E, 1.0)).

add_member(C, E, _) :-
    concept_member(C, E, _), !.
add_member(C, E, Sc) :-
    assertz(concept_member(C, E, Sc)).

next_concept_number([], 1).
next_concept_number(Ns, N) :- max_list(Ns, M), N is M + 1.

entity_signature(E, signature(S, O)) :-
    findall(R, memory_relation(E, R, _, _, _), S0),
    findall(R, memory_relation(_, R, E, _, _), O0),
    sort(S0, S), sort(O0, O).

signature_similarity(signature(S1, O1), signature(S2, O2), Sc) :-
    jaccard(S1, S2, A), jaccard(O1, O2, B),
    Sc is (A + B) / 2.

jaccard([], [], 1.0) :- !.
jaccard(A, B, Sc) :-
    append(A, B, C), sort(C, U),
    intersection(A, B, I),
    length(U, LU), length(I, LI),
    ( LU =:= 0 -> Sc = 0.0 ; Sc is LI / LU ).

show_concepts :-
    nl, writeln('===== DISCOVERED CONCEPTS ====='),
    forall(concept(C, Sig, _),
           ( format('~w  signature=~w~n', [C, Sig]),
             findall(E, concept_member(C, E, _), Ms),
             length(Ms, N),
             format('   members=~w~n   ~w~n~n', [N, Ms])
           )).

discover_concept_relations :-
    forall(memory_relation(S, R, O, W, _),
           discover_relation(S, R, O, W)).

discover_relation(S, R, O, W) :-
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is W * SS * OS,
    add_concept_relation(SC, R, OC, Sc).

add_concept_relation(SC, R, OC, Sc) :-
    concept_relation(SC, R, OC, Old), !,
    New is max(Old, Sc),
    retract(concept_relation(SC, R, OC, Old)),
    assertz(concept_relation(SC, R, OC, New)).
add_concept_relation(SC, R, OC, Sc) :-
    assertz(concept_relation(SC, R, OC, Sc)).

show_concept_relations :-
    nl, writeln('===== CONCEPT RELATIONS ====='),
    forall(concept_relation(SC, R, OC, Sc),
           format('~w --~w--> ~w  score=~2f~n', [SC, R, OC, Sc])).

discover_rules :-
    forall(concept_relation(SC, R, OC, Sc),
           assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))).

show_rules :-
    nl, writeln('===== LEARNED RULES ====='),
    forall(learned_rule(Rule, _, _, _, Sc),
           format('~w confidence=~2f~n', [Rule, Sc])).

infer(S, R, O, Sc) :-
    learned_rule(_, SC, R, OC, RS),
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is RS * SS * OS.

% --- tests ---

run_tests :-
    nl, writeln('===== HIDDEN POSITIVE (never seen) ====='),
    forall(hidden_pair(A, C),
           ( (infer(A, r4, C, S1) ->
                 format('BASELINE PASS  ~w --r4--> ~w  ~2f~n', [A, C, S1])
             ; format('BASELINE FAIL  ~w --r4--> ~w~n', [A, C])
             ),
             ( mv_predict(A, r4, C) ->
                 format('MV       PASS  ~w --r4--> ~w~n', [A, C])
             ; format('MV       FAIL  ~w --r4--> ~w~n', [A, C])
             )
           )),
    nl, writeln('===== DISTRACTORS (must be rejected) ====='),
    forall(distractor(A, C),
           ( (infer(A, r4, C, S2) ->
                 format('BASELINE ERROR (overgeneralizes) ~w --r4--> ~w  ~2f~n',
                        [A, C, S2])
             ; format('BASELINE reject ~w --r4--> ~w~n', [A, C])
             ),
             ( mv_predict(A, r4, C) ->
                 format('MV       ERROR ~w --r4--> ~w~n', [A, C])
             ; format('MV       reject ~w --r4--> ~w~n', [A, C])
             )
           )),
    nl, writeln('===== METRICS: multivariable method ====='),
    findall(1, (hidden_pair(A, C), mv_predict(A, r4, C)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, C), \+ mv_predict(A, r4, C)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, C), mv_predict(A, r4, C)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, C), \+ mv_predict(A, r4, C)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 2,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 8,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).
