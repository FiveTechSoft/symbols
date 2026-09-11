% experiment4.pl
% EXPERIMENT 4 - STRUCTURAL CORRESPONDENCE
% Test: Q(N) --r17--> X(N) como regla abstracta con variable.
% Entrenamiento: qN --r17--> xN para todo N EXCEPTO {7,13,21} (ocultos).
% Se mantienen xN --r42--> qN + fondo (r83,r61,r84) para que q7/x7
% sigan perteneciendo a sus conceptos (sin bug de object_index de exp3).
% Tests: predecir ocultos nunca vistos + rechazar distractores cruzados.
:- consult('memory.pl').
:- consult('correspondence.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment4 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 4 - STRUCTURAL CORRESPONDENCE'),
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
    discover_suffix_correspondence(r17),
    show_correspondence_rules,
    run_tests.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(correspondence(_, _, _)),
    retractall(correspondence_rule(_, _, _)).

% --- training ---

hidden(7).
hidden(13).
hidden(21).

q_symbol(N, S) :-
    atom_number(NAtom, N),
    atom_concat(q, NAtom, S).

x_symbol(N, S) :-
    atom_number(NAtom, N),
    atom_concat(x, NAtom, S).

create_training_data :-
    forall(between(1, 30, N), create_entity(N)).

create_entity(N) :-
    q_symbol(N, Q),
    x_symbol(N, X),
    % hecho objetivo (OCULTO para N en {7,13,21})
    ( hidden(N) -> true
    ; remember_relation(Q, r17, X, 1.0)
    ),
    % reverso SIEMPRE presente (ancla q7/x7 en sus conceptos)
    remember_relation(X, r42, Q, 1.0),
    % fondo comun: estabiliza firmas para que los ocultos
    % sigan en el mismo concepto (Jaccard >= 0.60)
    remember_relation(Q, r83, w0, 1.0),
    remember_relation(Q, r61, w1, 1.0),
    remember_relation(w2, r84, X, 1.0).

% --- conceptos (igual que experiment3, umbral 0.60) ---

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

% inferencia vieja por pertenencia: Q--r-->X sin variable
infer(S, R, O, Sc) :-
    learned_rule(_, SC, R, OC, RS),
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is RS * SS * OS.

% --- tests ---

hidden_pair(Q, X) :-
    hidden(N), q_symbol(N, Q), x_symbol(N, X).

% distractores: combinaciones cruzadas que la regla Q-->X aceptaria
% pero la regla con variable Q(N)-->X(N) debe rechazar
distractor(Q, X) :-
    member((A, B), [(7, 13), (13, 21), (21, 7), (1, 2), (5, 6), (10, 11)]),
    q_symbol(A, Q), x_symbol(B, X).

run_tests :-
    nl, writeln('===== HIDDEN POSITIVE (never seen in training) ====='),
    forall(hidden_pair(Q, X),
           ( (infer(Q, r17, X, S1) ->
                 format('BASELINE PASS  ~w --r17--> ~w  ~2f~n', [Q, X, S1])
             ; format('BASELINE FAIL  ~w --r17--> ~w~n', [Q, X])
             ),
             ( correspondence_predict(Q, r17, X) ->
                 format('CORR     PASS  ~w --r17--> ~w~n', [Q, X])
             ; format('CORR     FAIL  ~w --r17--> ~w~n', [Q, X])
             )
           )),
    nl, writeln('===== DISTRACTORS (must be rejected) ====='),
    forall(distractor(Q, X),
           ( (infer(Q, r17, X, S2) ->
                 format('BASELINE ERROR (overgeneralizes) ~w --r17--> ~w  ~2f~n',
                        [Q, X, S2])
             ; format('BASELINE reject ~w --r17--> ~w~n', [Q, X])
             ),
             ( correspondence_predict(Q, r17, X) ->
                 format('CORR     ERROR ~w --r17--> ~w~n', [Q, X])
             ; format('CORR     reject ~w --r17--> ~w~n', [Q, X])
             )
           )),
    nl, writeln('===== METRICS: correspondence method ====='),
    findall(1, (hidden_pair(Q, X), correspondence_predict(Q, r17, X)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(Q, X), \+ correspondence_predict(Q, r17, X)), FNL),
    length(FNL, FN),
    findall(1, (distractor(Q, X), correspondence_predict(Q, r17, X)), FPL),
    length(FPL, FP),
    findall(1, (distractor(Q, X), \+ correspondence_predict(Q, r17, X)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 3,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 9,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).
