% experiment24.pl
% EXPERIMENT 24 - HIERARCHICAL RULE COMPOSITION
% Nivel 1: r3:-[a1,a2], r6:-[b1,b2] entran a la biblioteca (skills).
% Nivel 2: r7 se descubre SOLO sobre unidades {r3,r6} (6 patrones),
%   sin expandir jamas sus internos.
% Contraste: FLAT sobre atomos {a1,a2,b1,b2} (340 patrones).
% Reclamo: mismos conjuntos de prediccion, ordenes menos de evaluacion.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('guided_search.pl').
:- consult('hierarchical.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.
:- dynamic flat_predictions/1.
:- dynamic hier_predictions/1.

experiment24 :-
    reset_experiment,
    build_world,
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    branch_flat,
    branch_hierarchical,
    compare_branches,
    run_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(skill(_, _, _)),
    retractall(check_results(_, _)),
    retractall(flat_predictions(_)),
    retractall(hier_predictions(_)).

% 6 cadenas A-a1->M1-a2->B-b1->N1-b2->C; r3,r6 siempre; r7 hidden en 2
chain(u1, v1, w1, x1, y1). chain(u2, v2, w2, x2, y2).
chain(u3, v3, w3, x3, y3). chain(u4, v4, w4, x4, y4).
chain(u5, v5, w5, x5, y5). chain(u6, v6, w6, x6, y6).
hidden(u5). hidden(u6).

build_world :-
    forall(chain(A, M1, B, N1, C),
           ( remember_relation(A, a1, M1, 1.0),
             remember_relation(M1, a2, B, 1.0),
             remember_relation(B, b1, N1, 1.0),
             remember_relation(N1, b2, C, 1.0),
             remember_relation(A, r3, B, 1.0),
             remember_relation(B, r6, C, 1.0),
             ( hidden(A) -> true
             ; remember_relation(A, r7, C, 1.0)
             )
           )),
    memory_size(N),
    format('world facts: ~w~n', [N]).

hidden_pair(A, C) :-
    hidden(A), chain(A, _, _, _, C).

distractor(A, C) :-
    member((A, C), [(u1, y2), (u2, y1), (u3, y4), (u4, y3)]).

% ---------- rama FLAT: solo atomos, longitud<=4 ----------
branch_flat :-
    nl, writeln('===== FLAT (atoms only) ====='),
    discover_over_units(r7, [a1, a2, b1, b2], 4, StatsF),
    StatsF = stats(_, _, _, _, EvF, _, _),
    format('flat evals: ~w~n', [EvF]),
    induce_constrained(r7, [a1, a2, b1, b2]),
    grid_predictions(Set),
    retractall(flat_predictions(_)),
    assertz(flat_predictions(Set)),
    length(Set, N),
    format('flat predictions stored: ~w pairs~n', [N]).

% ---------- rama HIER: unidades nivel 1 + composicion nivel 2 ----------
branch_hierarchical :-
    nl, writeln('===== HIERARCHICAL (skill library) ====='),
    learn_unit(r3, 2),
    learn_unit(r6, 2),
    show_skills,
    discover_over_units(r7, [r3, r6], 2, StatsH),
    StatsH = stats(_, _, _, _, EvH, _, _),
    format('hier L2 evals: ~w~n', [EvH]),
    induce_constrained(r7, [r3, r6]),
    grid_predictions(Set),
    retractall(hier_predictions(_)),
    assertz(hier_predictions(Set)),
    length(Set, N),
    format('hier predictions stored: ~w pairs~n', [N]).

% predicciones sobre la rejilla SC x OC del target r7
grid_predictions(Set) :-
    concept_relation(SC, r7, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(S-O, ( member(S, SS), member(O, OS),
                   reuse_predict(S, r7, O)
                 ),
            P0),
    sort(P0, Set).

compare_branches :-
    nl, writeln('===== FLAT vs HIERARCHICAL ====='),
    flat_predictions(F),
    hier_predictions(H),
    ( F == H ->
        format('prediction sets IDENTICAL (~w pairs)~n', [F]),
        assertz(check_results('prediction sets identical', pass))
    ; format('MISMATCH flat=~w hier=~w~n', [F, H]),
      assertz(check_results('prediction sets identical', fail))
    ).

run_tests :-
    nl, writeln('===== TESTS (hierarchical rule) ====='),
    findall(1, (hidden_pair(A, C), reuse_predict(A, r7, C)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, C), \+ reuse_predict(A, r7, C)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, C), reuse_predict(A, r7, C)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, C), \+ reuse_predict(A, r7, C)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    check(TP =:= 2, 'hidden 2/2 via units'),
    check(FP =:= 0, 'distractors 0 FP').

% --- checks & discovery (threshold 0.70, idempotent) ---

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

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

discover_rules :-
    forall(concept_relation(SC, R, OC, Sc),
           assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
