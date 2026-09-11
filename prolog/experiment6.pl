% experiment6.pl
% EXPERIMENT 6 - LATENT BRIDGE COMPOSITION
% Cadenas: A --r1--> M --r2--> C, mas r3 directos observados.
% Simbolos arbitrarios sin significado compartido en los nombres.
% Se ocultan 3 hechos r3; el sistema debe inducir
%   r3(S,O) :- r1(S,M), r2(M,O)
% sin saber ni las relaciones ni la longitud del camino (busca 1..3).
:- consult('memory.pl').
:- consult('composition.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment6 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 6 - LATENT BRIDGE COMPOSITION'),
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
    discover_composition(r3, 3),
    show_composed_rules,
    run_tests.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)).

% ground truth (NO visible para el modulo de composicion)
chain(aqua, vex, uno).
chain(nero, zor, dos).
chain(luma, pel, tres).
chain(taro, qux, cuatro).
chain(sora, juno, cinco).
chain(mila, fed, seis).
chain(iva, rul, siete).
chain(oda, tas, ocho).

hidden(luma).
hidden(mila).
hidden(iva).

create_training_data :-
    forall(chain(A, M, C), create_chain(A, M, C)).

create_chain(A, M, C) :-
    remember_relation(A, r1, M, 1.0),
    remember_relation(M, r2, C, 1.0),
    ( hidden(A) -> true
    ; remember_relation(A, r3, C, 1.0)
    ).

hidden_pair(A, C) :-
    hidden(A), chain(A, _, C).

distractor(A, C) :-
    member((A, C), [(luma, dos), (mila, siete), (aqua, tres),
                    (nero, uno), (taro, cinco), (sora, ocho)]).

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

add_member(C, E, Sc) :-
    \+ concept_member(C, E, _),
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
           ( (infer(A, r3, C, S1) ->
                 format('BASELINE PASS  ~w --r3--> ~w  ~2f~n', [A, C, S1])
             ; format('BASELINE FAIL  ~w --r3--> ~w~n', [A, C])
             ),
             ( composed_predict(A, r3, C) ->
                 format('COMPOSED PASS  ~w --r3--> ~w~n', [A, C])
             ; format('COMPOSED FAIL  ~w --r3--> ~w~n', [A, C])
             )
           )),
    nl, writeln('===== DISTRACTORS (must be rejected) ====='),
    forall(distractor(A, C),
           ( (infer(A, r3, C, S2) ->
                 format('BASELINE ERROR (overgeneralizes) ~w --r3--> ~w  ~2f~n',
                        [A, C, S2])
             ; format('BASELINE reject ~w --r3--> ~w~n', [A, C])
             ),
             ( composed_predict(A, r3, C) ->
                 format('COMPOSED ERROR ~w --r3--> ~w~n', [A, C])
             ; format('COMPOSED reject ~w --r3--> ~w~n', [A, C])
             )
           )),
    nl, writeln('===== METRICS: composed method ====='),
    findall(1, (hidden_pair(A, C), composed_predict(A, r3, C)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, C), \+ composed_predict(A, r3, C)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, C), composed_predict(A, r3, C)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, C), \+ composed_predict(A, r3, C)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 3,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 9,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).
