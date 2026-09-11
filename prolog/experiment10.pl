% experiment10.pl
% EXPERIMENT 10 - VARIABLE REUSE
% Familia D (distinta):  A-r1->M-r2->N-r3->C  => r4, sig [1,2,3,4]
% Familia S (reusada):   X-r1->A-r2->A-r3->Z  => r5, sig [1,2,2,3]
% Familia R (reflexiva): X-r1->A-r2->X        => r6, sig [1,2,1]
% Control mixto r7: un caso [A,B] + uno [A,A] => debe REHUSAR.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment10 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 10 - VARIABLE REUSE'),
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
    discover_composition(r4, 3),
    induce_constrained(r4, [r1, r2, r3]),
    discover_composition(r5, 3),
    induce_constrained(r5, [r1, r2, r3]),
    discover_composition(r6, 3),
    induce_constrained(r6, [r1, r2]),
    discover_composition(r7, 3),
    show_composed_rules,
    ( induce_constrained(r7, [r1, r2, r3]) ->
        format('CONTROL FAILED: r7 induced despite mixed signatures~n', [])
    ; format('CONTROL OK: r7 refused (mixed signatures)~n', [])
    ),
    show_constrained_rules,
    run_tests.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% --- familia D: variables distintas ---
chain_d(taro, pema, pemi, tako).
chain_d(nilo, rino, rini, nika).
chain_d(sare, salu, sali, sara).
chain_d(quvo, qeru, qeri, quma).
hidden_d(sare).

% --- familia S: variable interna reusada (self-loop r2) ---
chain_s(luma, huba, bela).
chain_s(beko, heki, beka).
chain_s(disa, hiso, diva).
chain_s(vono, hovu, voma).
hidden_s(disa).

% --- familia R: exterior reusado (reflexiva) ---
chain_r(zara, zaba).
chain_r(yeto, yeba).
chain_r(xilo, xiba).
chain_r(wemu, weba).
hidden_r(xilo).

% --- decoys sin loop: cadena [r1,r3] completa pero r5 FALSO.
% Sin ellos, [r1,r3] empata con [r1,r2,r3] y el loop seria decoracion.
decoy(d1x, d1a, d1z).
decoy(d2x, d2a, d2z).
decoy(d3x, d3a, d3z).
decoy(d4x, d4a, d4z).

% --- control mixto r7: un [A,B] + un [A,A] ---
control_mix(m1x, m1m, m1n, m1c).
control_self(m2x, m2m, m2c).

create_training_data :-
    forall(chain_d(A, M, N, C),
           ( remember_relation(A, r1, M, 1.0),
             remember_relation(M, r2, N, 1.0),
             remember_relation(N, r3, C, 1.0),
             ( hidden_d(A) -> true
             ; remember_relation(A, r4, C, 1.0)
             )
           )),
    forall(chain_s(X, A, Z),
           ( remember_relation(X, r1, A, 1.0),
             remember_relation(A, r2, A, 1.0),
             remember_relation(A, r3, Z, 1.0),
             ( hidden_s(X) -> true
             ; remember_relation(X, r5, Z, 1.0)
             )
           )),
    forall(chain_r(X, A),
           ( remember_relation(X, r1, A, 1.0),
             remember_relation(A, r2, X, 1.0),
             ( hidden_r(X) -> true
             ; remember_relation(X, r6, X, 1.0)
             )
           )),
    forall(decoy(X, A, Z),
           ( remember_relation(X, r1, A, 1.0),
             remember_relation(A, r3, Z, 1.0)
           )),
    control_mix(X, M, N, C),
    remember_relation(X, r1, M, 1.0),
    remember_relation(M, r2, N, 1.0),
    remember_relation(N, r3, C, 1.0),
    remember_relation(X, r7, C, 1.0),
    control_self(X2, A2, C2),
    remember_relation(X2, r1, A2, 1.0),
    remember_relation(A2, r2, A2, 1.0),
    remember_relation(A2, r3, C2, 1.0),
    remember_relation(X2, r7, C2, 1.0).

hidden_pair(A, C, r4) :- hidden_d(A), chain_d(A, _, _, C).
hidden_pair(X, Z, r5) :- hidden_s(X), chain_s(X, _, Z).
hidden_pair(X, X, r6) :- hidden_r(X).

distractor(A, C, r4) :-
    member((A, C), [(taro, sara), (taro, beka)]).
distractor(X, Z, r5) :-
    member((X, Z), [(luma, diva), (disa, bela), (d1x, d1z), (d2x, d2z)]).
distractor(X, Y, r6) :-
    member((X, Y), [(zara, yeto), (xilo, zara)]).

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
    ( Best >= 0.70 -> add_member(BC, E, Best)
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
    forall(concept_relation(SC, R, OC, SCc),
           format('~w --~w--> ~w  score=~2f~n', [SC, R, OC, SCc])).

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
    forall(hidden_pair(A, B, T),
           ( (infer(A, T, B, S1) ->
                 format('BASELINE PASS  ~w --~w--> ~w  ~2f~n', [A, T, B, S1])
             ; format('BASELINE FAIL  ~w --~w--> ~w~n', [A, T, B])
             ),
             ( reuse_predict(A, T, B) ->
                 format('REUSE    PASS  ~w --~w--> ~w~n', [A, T, B])
             ; format('REUSE    FAIL  ~w --~w--> ~w~n', [A, T, B])
             )
           )),
    nl, writeln('===== DISTRACTORS (must be rejected) ====='),
    forall(distractor(A, B, T),
           ( (infer(A, T, B, S2) ->
                 format('BASELINE ERROR (overgeneralizes) ~w --~w--> ~w  ~2f~n',
                        [A, T, B, S2])
             ; format('BASELINE reject ~w --~w--> ~w~n', [A, T, B])
             ),
             ( reuse_predict(A, T, B) ->
                 format('REUSE    ERROR ~w --~w--> ~w~n', [A, T, B])
             ; format('REUSE    reject ~w --~w--> ~w~n', [A, T, B])
             )
           )),
    nl, writeln('===== METRICS: reuse method ====='),
    findall(1, (hidden_pair(A, B, T), reuse_predict(A, T, B)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, B, T), \+ reuse_predict(A, T, B)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, B, T), reuse_predict(A, T, B)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, B, T), \+ reuse_predict(A, T, B)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 3,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 11,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).
