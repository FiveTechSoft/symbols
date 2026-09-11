% experiment7.pl
% EXPERIMENT 7 - ABDUCTION (variable latente, sin inventar nodos)
% 5 cadenas completas entrenan r3(S,O) :- r1(S,M), r2(M,O).
% 3 casos test con UN eslabon retenido (ground truth withheld/3):
%   T1,T2 prefijo-presente (falta r2(M,H)), T3 sufijo-presente (falta r1(G,M)).
% Distractores: 3 sin stubs (coste 2 -> reject) + 3 cruzados con stub.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('abduction.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment7 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 7 - ABDUCTION (LEVEL 1)'),
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

% --- ground truth completa (el modulo NO la ve: retenemos eslabones) ---
full_chain(a, m1, c).
full_chain(b, m2, d).
full_chain(e, m3, f).
full_chain(i, m6, j).
full_chain(k, m7, l).
% casos test (cadena completa real, pero entrenamos parcial)
full_chain(g1, m4, h1).
full_chain(g2, m5, h2).
full_chain(g3, m8, h3).

% eslabones retenidos = hipotesis correctas que el sistema debe abducir
withheld(m4, r2, h1).
withheld(m5, r2, h2).
withheld(g3, r1, m8).

create_training_data :-
    forall(full_chain(A, M, C), create_chain(A, M, C)),
    forall(bg_entity(A, C), anchor(A, C)),
    anchor_stubs,
    anchor_nostub.

% cadenas completas de entrenamiento (las test se crean rotas en anchor_stubs)
create_chain(A, _, _) :-
    test_subject(A), !.
create_chain(A, M, C) :-
    full_chain(A, M, C),
    \+ test_subject(A),
    remember_relation(A, r1, M, 1.0),
    remember_relation(M, r2, C, 1.0),
    remember_relation(A, r3, C, 1.0).

test_subject(g1).
test_subject(g2).
test_subject(g3).

% cadenas test ROTAS: todo menos el eslabon retenido
anchor_stubs :-
    % T1: falta r2(m4,h1)
    remember_relation(g1, r1, m4, 1.0),
    % T2: falta r2(m5,h2)
    remember_relation(g2, r1, m5, 1.0),
    % T3: falta r1(g3,m8)
    remember_relation(m8, r2, h3, 1.0).

% fondo para estabilizar conceptos (todas las A y C, test incluidas)
bg_entity(A, C) :-
    full_chain(A, _, C).
anchor(A, C) :-
    remember_relation(A, r83, w0, 1.0),
    remember_relation(w3, r42, A, 1.0),
    remember_relation(w2, r84, C, 1.0),
    remember_relation(C, r87, w5, 1.0),
    forall(bg_mediator(M), remember_relation(M, r85, w4, 1.0)).

bg_mediator(M) :- full_chain(_, M, _).

% entidades aisladas (distractores sin stubs)
nostub(u1, v1).
nostub(u2, v2).
nostub(u3, v3).

anchor_nostub :-
    forall(nostub(U, V),
           ( remember_relation(U, r83, w0, 1.0),
             remember_relation(w3, r42, U, 1.0),
             remember_relation(w2, r84, V, 1.0),
             remember_relation(V, r87, w5, 1.0)
           )).

hidden_pair(G, H) :-
    member((G, H), [(g1, h1), (g2, h2), (g3, h3)]).

distractor_nostub(U, V) :- nostub(U, V).
distractor_cross(G, H) :-
    member((G, H), [(g1, h2), (g2, h1), (g1, h3)]).

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
    nl, writeln('===== ABDUCTION: hidden (withheld link recovery) ====='),
    forall(hidden_pair(G, H),
           ( show_abduction(r3, G, H),
             ( abduce(r3, G, H, accepted(1, [need(S, R, O)])),
               withheld(S, R, O) ->
                 format('EXACT withheld link recovered: ~w --~w--> ~w~n~n',
                        [S, R, O])
             ; format('NOT exact recovery~n~n', [])
             )
           )),
    nl, writeln('===== ABDUCTION: no-stub distractors (must reject) ====='),
    forall(distractor_nostub(U, V),
           ( show_abduction(r3, U, V),
             ( abductive_predict(U, r3, V) ->
                 format('ABDUCE ERROR ~w --r3--> ~w~n~n', [U, V])
             ; format('ABDUCE reject ~w --r3--> ~w~n~n', [U, V])
             )
           )),
    nl, writeln('===== ABDUCTION: stub-cross distractors ====='),
    forall(distractor_cross(G, H),
           ( show_abduction(r3, G, H),
             ( abductive_predict(G, r3, H) ->
                 format('ABDUCE hypothesizes (over-generation, documented) ~w --r3--> ~w~n~n',
                        [G, H])
             ; format('ABDUCE reject/ambiguous ~w --r3--> ~w~n~n', [G, H])
             )
           )),
    nl, writeln('===== BASELINE (membership) on same pairs ====='),
    forall(( hidden_pair(G, H) ; distractor_nostub(G, H) ; distractor_cross(G, H) ),
           ( ( infer(G, r3, H, Sc) ->
                 format('BASELINE accepts ~w --r3--> ~w ~2f~n', [G, H, Sc])
             ; format('BASELINE rejects ~w --r3--> ~w~n', [G, H])
             )
           )).
