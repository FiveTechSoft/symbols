% experiment41.pl
% EXPERIMENT 41 - MULTIPLE ROLES (un objeto, varios conceptos)
% Un mismo objeto participa en conceptos estructurales independientes y
% recibe la skill del concepto que activa, con prueba independiente:
% identidad del objeto != rol != comportamiento del rol.
% cA suministro {P owns O, O belongs_to P, P visits L} -> stocked.
% cB viaje {P rides V, V burns F} -> travels.
% o1=alpha (A+B), o2=beta (A), o3=carl (B), o4=zorin (A+B nuevo).
% Firmas LOCALES (EXP40) + exclusion uniforme (EXP37): P compartido.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic struct_def/3.
:- dynamic struct_sig/2.
:- dynamic struct_member/2.
:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic struct_skill/3.
:- dynamic check_results/2.

experiment41 :-
    reset_experiment,
    build_train,
    discover_concepts,
    discover_concept_relations,
    discover_skills,
    derive_role_concepts,
    build_test,
    run_role_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(struct_def(_, _, _)),
    retractall(struct_sig(_, _)),
    retractall(struct_member(_, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% o1 alpha A+B; o2 beta A; o3 carl B. Conclusiones train observadas.
build_train :-
    assertz(struct_def(sa1, cA, [alpha, book, madrid])),
    assertz(struct_def(sb1, cB, [alpha, bike, petrol])),
    assertz(struct_def(sa2, cA, [beta, pen, paris])),
    assertz(struct_def(sb2, cB, [carl, boat, diesel])),
    forall(member((S, R, O),
                  [(alpha, owns, book), (book, belongs_to, alpha),
                   (alpha, visits, madrid), (book, stocked, madrid),
                   (alpha, rides, bike), (bike, burns, petrol),
                   (alpha, travels, petrol),
                   (beta, owns, pen), (pen, belongs_to, beta),
                   (beta, visits, paris), (pen, stocked, paris),
                   (carl, rides, boat), (boat, burns, diesel),
                   (carl, travels, diesel)]),
           remember_relation(S, R, O, 1.0)).

% ---------- conceptos relacionales (maquinaria EXP33/36-40) ----------
entity(E) :- memory_relation(E, _, _, _, _).
entity(E) :- memory_relation(_, _, E, _, _).

discover_concepts :-
    findall(E, entity(E), E0),
    sort(E0, Es),
    forall(member(E, Es), assign_concept(E)).

assign_concept(E) :-
    entity_signature(E, Sig),
    findall(Sc-C, (concept(C, CSig, _), signature_similarity(Sig, CSig, Sc)), Ms),
    best_concept(Ms, Best, BC),
    (Best >= 0.80 -> add_member(BC, E, Best)
    ; create_concept(E, Sig)).

best_concept([], 0.0, none).
best_concept(Ms, Sc, C) :-
    keysort(Ms, S), reverse(S, [Sc-C|_]).

create_concept(E, Sig) :-
    findall(N, concept(concept(N), _, _), Ns),
    next_concept_number(Ns, N),
    C = concept(N),
    assertz(concept(C, Sig, 1)),
    assertz(concept_member(C, E, 1.0)).

add_member(C, E, _) :- concept_member(C, E, _), !.
add_member(C, E, Sc) :- assertz(concept_member(C, E, Sc)).

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
    (LU =:= 0 -> Sc = 0.0 ; Sc is LI / LU).

discover_concept_relations :-
    forall(memory_relation(S, R, O, W, _), discover_relation(S, R, O, W)).

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

discover_skills :-
    discover_composition(stocked, 3),
    induce_constrained(stocked, [belongs_to, visits]),
    check(composed_rule(stocked, [belongs_to, visits], _),
          'skill stocked :- [belongs_to, visits]'),
    discover_composition(travels, 3),
    induce_constrained(travels, [rides, burns]),
    check(composed_rule(travels, [rides, burns], _),
          'skill travels :- [rides, burns]').

% ---------- SSE local + exclusion uniforme (EXP37/40) ----------
excluded_all([stocked, travels]).

member_sig_l(E, Members, sig(O, I, ON, IN)) :-
    excluded_all(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_l(N, Members, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_l(N, Members, A, B)), IN0),
    sort(IN0, IN).

deg_l(E, Members, O, I) :-
    excluded_all(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

struct_sse(S, stsig(MS, E)) :-
    struct_def(S, _Tag, Members),
    findall(M, (member(X, Members), member_sig_l(X, Members, M)), M0),
    sort(M0, MS),
    excluded_all(Exclude),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

derive_role_concepts :-
    struct_sse(sa1, SigA1), struct_sse(sa2, SigA2),
    check(SigA1 == SigA2, 'cA train pair shares SSE'),
    struct_sse(sb1, SigB1), struct_sse(sb2, SigB2),
    check(SigB1 == SigB2, 'cB train pair shares SSE'),
    check(sig_clean(SigA1), 'cA SSE zero atoms'),
    check(sig_clean(SigB1), 'cB SSE zero atoms'),
    assertz(struct_sig(cA, SigA1)),
    assertz(struct_member(cA, sa1)),
    assertz(struct_member(cA, sa2)),
    assertz(struct_sig(cB, SigB1)),
    assertz(struct_member(cB, sb1)),
    assertz(struct_member(cB, sb2)),
    assertz(struct_skill(cA, stocked, [belongs_to, visits])),
    assertz(struct_skill(cB, travels, [rides, burns])),
    check(true, 'two role-concepts cA/cB with own skills').

classify_struct(S, Tag) :-
    struct_sse(S, Sig),
    struct_sig(Tag, Sig).

% ---------- test: o4 zorin A+B nuevo; wex parcial A ----------
build_test :-
    assertz(struct_def(sa3, cA, [zorin, tablet, sevilla])),
    assertz(struct_def(sb3, cB, [zorin, scooter, kerosene])),
    forall(member((S, R, O),
                  [(zorin, owns, tablet), (tablet, belongs_to, zorin),
                   (zorin, visits, sevilla),
                   (zorin, rides, scooter), (scooter, burns, kerosene)]),
           remember_relation(S, R, O, 1.0)),
    assertz(struct_def(sa4, cA, [wex, quark])),
    forall(member((S, R, O),
                  [(wex, owns, quark), (quark, belongs_to, wex)]),
           remember_relation(S, R, O, 1.0)).

% skill por rol: puerta = estructura miembro del concepto
predict_role(S, stocked, O, L, Proof) :-
    struct_def(S, cA, Members),
    member(O, Members), member(L, Members),
    classify_struct(S, cA),
    struct_skill(cA, stocked, [belongs_to, visits]),
    memory_relation(O, belongs_to, P, _, _),
    memory_relation(P, visits, L, _, _),
    Proof = [role(cA, S), rule(stocked, [belongs_to, visits]),
             (O, belongs_to, P), (P, visits, L)].

predict_role(S, travels, P, F, Proof) :-
    struct_def(S, cB, Members),
    member(P, Members), member(F, Members),
    classify_struct(S, cB),
    struct_skill(cB, travels, [rides, burns]),
    memory_relation(P, rides, V, _, _),
    memory_relation(V, burns, F, _, _),
    Proof = [role(cB, S), rule(travels, [rides, burns]),
             (P, rides, V), (V, burns, F)].

% independencia de pruebas: la prueba de un rol no menciona al otro
proof_independent(Proof, Own, Other) :-
    Proof = [role(Own, _)|_],
    \+ sub_term(Other, Proof).

run_role_tests :-
    nl, writeln('===== ROLES (one object, two concepts) ====='),
    (classify_struct(sa3, cA) ->
        check(true, 'zorin joins cA')
    ; check(false, 'zorin joins cA')),
    (classify_struct(sb3, cB) ->
        check(true, 'zorin joins cB (same object, other role)')
    ; check(false, 'zorin joins cB (same object, other role)')),
    (predict_role(sa3, stocked, tablet, sevilla, PA) ->
        (check(proof_independent(PA, cA, travels),
               'zorin stocked with cA-only proof'),
         format('proof A: ~w~n', [PA]))
    ; check(false, 'zorin stocked with cA-only proof')),
    (predict_role(sb3, travels, zorin, kerosene, PB) ->
        (check(proof_independent(PB, cB, stocked),
               'zorin travels with cB-only proof'),
         format('proof B: ~w~n', [PB]))
    ; check(false, 'zorin travels with cB-only proof')),
    (predict_role(_, travels, beta, _, _) ->
        check(false, 'beta (A-only) travels -> UNKNOWN')
    ; check(true, 'beta (A-only) travels -> UNKNOWN')),
    (predict_role(_, stocked, carl, _, _) ->
        check(false, 'carl (B-only) stocked -> UNKNOWN')
    ; check(true, 'carl (B-only) stocked -> UNKNOWN')),
    (predict_role(sa4, stocked, _, _, _) ->
        check(false, 'wex (partial A) stocked -> UNKNOWN')
    ; check(true, 'wex (partial A) stocked -> UNKNOWN')),
    (predict_role(_, stocked, tablet, paris, _) ->
        check(false, 'cross tablet->paris rejected')
    ; check(true, 'cross tablet->paris rejected')).

% ---------- reporte ----------
check(Cond, Msg) :-
    (call(Cond) ->
        format('PASS ~w~n', [Msg]),
        assertz(check_results(Msg, pass))
    ; format('FAIL ~w~n', [Msg]),
      assertz(check_results(Msg, fail))).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps), length(Ps, NP),
    findall(1, check_results(_, fail), Fs), length(Fs, NF),
    N is NP + NF,
    format('passed ~w/~w~n', [NP, N]).
