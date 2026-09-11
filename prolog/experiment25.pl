% experiment25.pl
% EXPERIMENT 25 - DEEP HIERARCHICAL COMPOSITION (3 niveles)
% L1: r3 :-[a1,a2]            (atomos)
% L2: r7 :-[r3,b1]            (unidad + atomo; flat-equiv len 3)
% L3: r9 :-[r7,c1]            (unidad + atomo; flat-equiv len 4)
% Solo r9 final oculto (2 casos). FLAT atomico {a1,a2,b1,c1} len<=4
% contra HIER (~14+6+6 evals). Reclamo: mismas predicciones.
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

experiment25 :-
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

% 6 cadenas A-a1->M-a2->B-b1->C-c1->D; r3,r7 siempre; r9 hidden en 2
chain(p1, q1, r1, s1, t1). chain(p2, q2, r2, s2, t2).
chain(p3, q3, r3, s3, t3). chain(p4, q4, r4, s4, t4).
chain(p5, q5, r5, s5, t5). chain(p6, q6, r6, s6, t6).
hidden(p5). hidden(p6).

build_world :-
    forall(chain(A, M, B, C, D),
           ( remember_relation(A, a1, M, 1.0),
             remember_relation(M, a2, B, 1.0),
             remember_relation(B, b1, C, 1.0),
             remember_relation(C, c1, D, 1.0),
             remember_relation(A, r3, B, 1.0),
             remember_relation(A, r7, C, 1.0),
             ( hidden(A) -> true
             ; remember_relation(A, r9, D, 1.0)
             )
           )),
    memory_size(N),
    format('world facts: ~w~n', [N]).

hidden_pair(A, D) :-
    hidden(A), chain(A, _, _, _, D).

distractor(A, D) :-
    member((A, D), [(p1, t2), (p2, t1), (p3, t4), (p4, t3)]).

% ---------- rama FLAT: atomos {a1,a2,b1,c1}, len<=4 ----------
branch_flat :-
    nl, writeln('===== FLAT (atoms only, len<=4) ====='),
    discover_over_units(r9, [a1, a2, b1, c1], 4, StatsF),
    StatsF = stats(_, _, _, _, EvF, _, _),
    format('flat evals: ~w~n', [EvF]),
    induce_constrained(r9, [a1, a2, b1, c1]),
    grid_predictions(Set),
    retractall(flat_predictions(_)),
    assertz(flat_predictions(Set)),
    length(Set, N),
    format('flat predictions stored: ~w pairs~n', [N]).

% ---------- rama HIER: L1 -> L2 -> L3 ----------
branch_hierarchical :-
    nl, writeln('===== HIERARCHICAL (3 levels) ====='),
    learn_unit(r3, 2),
    learn_unit(r7, 2),
    show_skills,
    discover_over_units(r9, [r7, c1], 2, StatsH),
    StatsH = stats(_, _, _, _, EvH, _, _),
    format('hier L3 evals: ~w~n', [EvH]),
    induce_constrained(r9, [r7, c1]),
    grid_predictions(Set),
    retractall(hier_predictions(_)),
    assertz(hier_predictions(Set)),
    length(Set, N),
    format('hier predictions stored: ~w pairs~n', [N]).

grid_predictions(Set) :-
    concept_relation(SC, r9, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(S-O, ( member(S, SS), member(O, OS),
                   reuse_predict(S, r9, O)
                 ),
            P0),
    sort(P0, Set).

compare_branches :-
    nl, writeln('===== FLAT vs HIER-3 ====='),
    flat_predictions(F),
    hier_predictions(H),
    ( F == H ->
        format('prediction sets IDENTICAL (~w pairs)~n', [F]),
        assertz(check_results('prediction sets identical', pass))
    ; format('MISMATCH flat=~w hier=~w~n', [F, H]),
      assertz(check_results('prediction sets identical', fail))
    ).

run_tests :-
    nl, writeln('===== TESTS (hierarchical L3 rule) ====='),
    findall(1, (hidden_pair(A, D), reuse_predict(A, r9, D)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, D), \+ reuse_predict(A, r9, D)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, D), reuse_predict(A, r9, D)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, D), \+ reuse_predict(A, r9, D)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    check(TP =:= 2, 'hidden 2/2 via L3 units'),
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
