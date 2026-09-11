% experiment23.pl
% EXPERIMENT 23 - GUIDED SEARCH vs EXHAUSTIVE (misma memoria)
% Mundo con ruido disjunto + 2 puentes. Reclamo: mismo ganador, mismo F1,
% candidatos evaluados <<. El conocimiento enfoca la busqueda futura.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('guided_search.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.

experiment23 :-
    reset_experiment,
    build_world,
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    run_comparison,
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
    retractall(check_results(_, _)).

% --- mundo: familia target (6 cadenas, 2 hidden) + ruido + puentes ---
chain(qa1, qm1, qc1). chain(qa2, qm2, qc2). chain(qa3, qm3, qc3).
chain(qa4, qm4, qc4). chain(qa5, qm5, qc5). chain(qa6, qm6, qc6).
hidden(qa5). hidden(qa6).

noise_n(na1, nb1, nc1). noise_n(na2, nb2, nc2).
noise_n(na3, nb3, nc3). noise_n(na4, nb4, nc4).
noise_m(ma1, mb1, mc1). noise_m(ma2, mb2, mc2).
noise_m(ma3, mb3, mc3). noise_m(ma4, mb4, mc4).
noise_k(kx1, ky1). noise_k(kx2, ky2). noise_k(kx3, ky3).

build_world :-
    forall(chain(A, M, C),
           ( remember_relation(A, r3, M, 1.0),
             remember_relation(M, r5, C, 1.0),
             ( hidden(A) -> true
             ; remember_relation(A, t9, C, 1.0)
             )
           )),
    forall(noise_n(A, B, C),
           ( remember_relation(A, n1, B, 1.0),
             remember_relation(B, n2, C, 1.0)
           )),
    forall(noise_m(A, B, C),
           ( remember_relation(A, m1, B, 1.0),
             remember_relation(B, m2, C, 1.0)
           )),
    forall(noise_k(A, C),
           remember_relation(A, k1, C, 1.0)),
    % puentes: tocan a la familia target pero no explican nada
    remember_relation(qa1, n1, zn, 1.0),
    remember_relation(zn, n2, qc1, 1.0),
    memory_size(N),
    format('world facts: ~w~n', [N]).

hidden_pair(A, C) :-
    hidden(A), chain(A, _, C).

distractor(A, C) :-
    member((A, C), [(qa1, qc3), (qa2, qc1), (qa3, qc4), (qa4, qc3)]).

run_comparison :-
    nl, writeln('===== EXHAUSTIVE ====='),
    run_discovery(exhaustive, t9, 3, SE),
    SE = stats(PE, FE, SupE, GenE, EvE, PrE, MsE),
    format('winner=~w F1=~4f support=~w gen=~w eval=~w pruned=~w ~wms~n',
           [PE, FE, SupE, GenE, EvE, PrE, MsE]),
    nl, writeln('===== GUIDED ====='),
    run_discovery(guided, t9, 3, SG),
    SG = stats(PG, FG, SupG, GenG, EvG, PrG, MsG),
    format('winner=~w F1=~4f support=~w gen=~w eval=~w pruned=~w ~wms~n',
           [PG, FG, SupG, GenG, EvG, PrG, MsG]),
    nl, writeln('===== COMPARISON ====='),
    check(PE == PG, 'same winner path'),
    check(abs(FE - FG) < 0.0001, 'same F1'),
    check(EvG < EvE, 'guided evaluates fewer'),
    format('eval ratio exhaustive/guided = ~2f~n', [EvE / EvG]),
    assertz(composed_rule(t9, PG, FG)),
    induce_constrained(t9, [r3, r5]).

run_tests :-
    nl, writeln('===== TESTS (shared winner) ====='),
    findall(1, (hidden_pair(A, C), reuse_predict(A, t9, C)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, C), \+ reuse_predict(A, t9, C)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, C), reuse_predict(A, t9, C)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, C), \+ reuse_predict(A, t9, C)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 2,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    format('Precision=~4f Recall=~4f F1=~4f~n', [P, R, F1]),
    check(TP =:= 2, 'hidden 2/2'),
    check(FP =:= 0, 'distractors 0 FP').

% --- checks & discovery (threshold 0.60, idempotent) ---

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
