% experiment8.pl
% EXPERIMENT 8 - CONTINUAL LEARNING (sin reconstruir)
% Fase 1: r1,r2 -> r3   (familia N: nora/fel/udi/oxa)
% Fase 2: r4,r5 -> r6   (familia K: kalo/mire/tusa/poda)
% Fase 3: r7,r8 -> r9   (familia R: ram/seq/til/vor) + ruido
% Fase 4: interferencia: nora/fel (fase 1) cruzan a r4,r5 -> r6.
%   Debe mantener r3(nora,abla) Y r6(nora,ek) sin cruzar reglas.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('continuous.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment8 :-
    reset_all,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 8 - CONTINUAL LEARNING'),
    writeln('=============================================='),
    phase(1, r3),
    phase(2, r6),
    phase(3, r9),
    phase(4, none),
    final_report.

reset_all :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)).

% learn_phase incremental: SIN borrar nada (conceptos pegajosos,
% relaciones por max(), reglas con dedup, composicion con alcance).
learn_phase(Target, MaxLen) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup,
    ( Target == none -> true
    ; discover_composition_scoped(Target, MaxLen)
    ).

% --- datos por fase (familias disjuntas salvo fase 4) ---
phase_data(1) :-
    forall(member((A, M, C),
                  [(nora, eb, abla), (fel, id, ebre),
                   (udi, og, ibro), (oxa, uf, ogro)]),
           ( remember_relation(A, r1, M, 1.0),
             remember_relation(M, r2, C, 1.0),
             remember_relation(A, r3, C, 1.0)
           )).
phase_data(2) :-
    forall(member((A, M, C),
                  [(kalo, vul, ak), (mire, xeq, ek),
                   (tusa, yir, ik), (poda, zof, ok)]),
           ( remember_relation(A, r4, M, 1.0),
             remember_relation(M, r5, C, 1.0),
             remember_relation(A, r6, C, 1.0)
           )).
phase_data(3) :-
    forall(member((A, M, C),
                  [(ram, wad, um), (seq, xeb, am),
                   (til, yic, em), (vor, zod, im)]),
           ( remember_relation(A, r7, M, 1.0),
             remember_relation(M, r8, C, 1.0),
             remember_relation(A, r9, C, 1.0)
           )),
    % ruido: hechos sueltos sin estructura componible
    remember_relation(nza, rz, nzb, 1.0),
    remember_relation(nzb, rz, nzc, 1.0),
    remember_relation(nora, rz, kalo, 0.3).
phase_data(4) :-
    % solapamiento: sujetos de fase 1 usan relaciones de fase 2
    remember_relation(nora, r4, qvn, 1.0),
    remember_relation(qvn, r5, ek, 1.0),
    remember_relation(fel, r4, qvf, 1.0),
    remember_relation(qvf, r5, ok, 1.0).

phase_target(1, r3).
phase_target(2, r6).
phase_target(3, r9).
phase_target(4, none).

phase(N, _) :-
    format('~n===== PHASE ~w =====~n', [N]),
    kb_counts(Before),
    print_counts('before', Before),
    phase_data(N),
    phase_target(N, T),
    learn_phase(T, 3),
    kb_counts(After),
    print_counts('after ', After),
    delta_counts(Before, After, D),
    format('delta : mem=~w concepts=~w crel=~w rules=~w composed=~w~n', D),
    run_phase_suites(N).

% --- suites acumulativas ---
suite(1, [expect_true(nora, r3, abla), expect_true(fel, r3, ebre),
          expect_true(udi, r3, ibro),
          expect_false(nora, r3, ebre), expect_false(fel, r3, abla),
          expect_false(udi, r3, ogro)]).
suite(2, [expect_true(kalo, r6, ak), expect_true(mire, r6, ek),
          expect_true(tusa, r6, ik),
          expect_false(kalo, r6, ek), expect_false(mire, r6, ak),
          expect_false(tusa, r6, ok)]).
suite(3, [expect_true(ram, r9, um), expect_true(seq, r9, am),
          expect_false(ram, r9, am), expect_false(seq, r9, um)]).
% fase 4: doble modelo superpuesto + anti-cruce entre reglas
suite(4, [expect_true(nora, r3, abla), expect_true(fel, r3, ebre),
          expect_true(nora, r6, ek), expect_true(fel, r6, ok),
          expect_false(nora, r3, ek), expect_false(nora, r6, abla),
          expect_false(kalo, r3, ak), expect_false(nora, r6, ik)]).

run_phase_suites(N) :-
    forall(between(1, N, K),
           ( suite(K, S),
             run_suite(S, P, Tt),
             format('suite~w (RET/ACQ): ~w/~w~n', [K, P, Tt])
           )),
    check_persistence.

check_persistence :-
    forall(member(T-P, [r3-[r1, r2], r6-[r4, r5], r9-[r7, r8]]),
           ( ( T == r3 ; composed_rule(T, _, _) ) ->
               ( rule_persisted(T, P) ->
                   format('persist ~w :- ~w  intact~n', [T, P])
               ; ( composed_rule(T, P2, _) ->
                     format('persist ~w CHANGED ~w (interference!)~n', [T, P2])
                 ; format('persist ~w not yet learned~n', [T])
                 )
               )
           ; true
           )).

final_report :-
    nl, writeln('===== FINAL: full suites ====='),
    findall(K, between(1, 4, K), Ks),
    findall(P-Tt, (member(K, Ks), suite(K, S), run_suite(S, P, Tt)), Rs),
    sum_pairs(Rs, PAll, TAll),
    format('TOTAL retention+acquisition: ~w/~w~n', [PAll, TAll]),
    kb_counts(Final),
    print_counts('final ', Final).

sum_pairs([], 0, 0).
sum_pairs([P-T|Rs], PA, TA) :-
    sum_pairs(Rs, P0, T0),
    PA is P0 + P, TA is T0 + T.

% --- descubrimiento (igual que exps previos; reglas con dedup) ---

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

discover_rules_dedup :-
    forall(concept_relation(SC, R, OC, Sc),
           assert_learned_rule_dedup(rule(SC, R, OC), SC, R, OC, Sc)).
