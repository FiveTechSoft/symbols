% experiment26.pl
% EXPERIMENT 26 - SKILL TRANSFER (persistencia + mundo nuevo)
% Fase A: 3 cadenas -> skills r3, r7, r9 (jerarquia L1-L3).
% WIPE: borra hechos AND reglas constrained; SOLO skills sobreviven.
% Fase B: entidades nuevas (r7/c1 dados, r9 jamas observado).
% La prediccion solo puede venir de EJECUTAR skills (sin constrained,
% sin induccion). Precondiciones: cadena incompleta no dispara.
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

experiment26 :-
    reset_experiment,
    phase_a_learn_skills,
    wipe_keep_skills_only,
    phase_b_new_world,
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
    retractall(check_results(_, _)).

% ---------- Fase A: 3 cadenas, r9 hidden en 1 ----------
chain(t1, u1, v1, w1, x1).
chain(t2, u2, v2, w2, x2).
chain(t3, u3, v3, w3, x3).
hidden(t3).

phase_a_learn_skills :-
    nl, writeln('===== PHASE A: learn skill hierarchy ====='),
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
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    learn_unit(r3, 2),
    learn_unit(r7, 2),
    learn_unit(r9, 2),
    show_skills,
    check(skill(r9, [r7, c1], [1, 2, 3]),
          'L3 skill r9:[r7,c1] registered').

% ---------- WIPE: solo skills ----------
wipe_keep_skills_only :-
    nl, writeln('===== WIPE (skills only survive) ====='),
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    memory_size(0),
    findall(S, skill(S, _, _), Ss),
    sort(Ss, [r3, r7, r9]),
    check(skill(r9, [r7, c1], [1, 2, 3]),
          'r9 skill intact after wipe'),
    check(\+ constrained_rule(_, _, _),
          'no constrained rules survive (skills are self-sufficient)').

% ---------- Fase B: mundo nuevo, solo ejecucion ----------
% xa1,xa2: cadenas completas, r9 jamas observado.
% xa3: cadena INCOMPLETA (sin c1) -> la skill no debe disparar.
new_chain(xa1, xz1, xw1).
new_chain(xa2, xz2, xw2).
new_incomplete(xa3, xz3).

phase_b_new_world :-
    nl, writeln('===== PHASE B: new entities, execution only ====='),
    forall(new_chain(X, Z, W),
           ( remember_relation(X, r7, Z, 1.0),
             remember_relation(Z, c1, W, 1.0)
           )),
    forall(new_incomplete(X, Z),
           remember_relation(X, r7, Z, 1.0)),
    check(\+ composed_rule(_, _, _),
          'no induction ran in phase B'),
    check(skill_predict(r9, xa1, xw1), 'transfer xa1->xw1'),
    check(skill_predict(r9, xa2, xw2), 'transfer xa2->xw2'),
    check(\+ skill_predict(r9, xa3, _),
          'incomplete chain: preconditions enforced'),
    check(\+ skill_predict(r9, xa1, xw2),
          'cross xa1->xw2 rejected'),
    check(\+ skill_predict(r9, xa2, xw1),
          'cross xa2->xw1 rejected'),
    check(\+ skill_predict(r9, t1, _),
          'old entities gone: t1 unknown').

% skill_predict: la skill es un programa autosuficiente (sin constrained).
skill_predict(T, S, O) :-
    skill(T, Path, Sig),
    full_bindings(S, O, Path, Full),
    eq_signature(Full, Sig).

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
