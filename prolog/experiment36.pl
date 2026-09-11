% experiment36.pl
% EXPERIMENT 36 - STRUCTURAL SSE (composicion estructural)
% De SSE(object|task) a SSE(structure|task): la estructura completa
% {P owns O, O belongs_to P, P visits L} se reconoce como unidad aunque
% ningun objeto individual sea conocido. La relacion objetivo (stocked)
% NO forma parte de la firma (exclusion EXP34, por construccion).
% Skill stocked :- [belongs_to, visits] adherida al concepto de
% estructura y heredada solo por estructuras miembro.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic struct_def/2.
 :- dynamic struct_sig/2.
:- dynamic struct_member/2.
:- dynamic struct_skill/3.
:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.

experiment36 :-
    reset_experiment,
    build_train,
    discover_concepts,
    discover_concept_relations,
    discover_skill,
    derive_struct_concepts,
    attach_struct_skill,
    build_test,
    run_transfer_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(struct_def(_, _)),
    retractall(struct_sig(_, _)),
    retractall(struct_member(_, _)),
    retractall(struct_skill(_, _, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% ---------- conceptos relacionales (maquinaria, como EXP33) ----------
% Solo alimentan a discover_composition (sujeto/objeto de stocked).
% La SSE estructural de abajo sigue siendo label-free y auditada.
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

% ---------- train: 2 estructuras, misma topologia ----------
build_train :-
    assertz(struct_def(s1, [alpha, book, madrid])),
    assertz(struct_def(s2, [beta, pen, paris])),
    forall(member((S, R, O),
                  [(alpha, owns, book), (book, belongs_to, alpha),
                   (alpha, visits, madrid), (book, stocked, madrid),
                   (beta, owns, pen), (pen, belongs_to, beta),
                   (beta, visits, paris), (pen, stocked, paris)]),
           remember_relation(S, R, O, 1.0)).

% ---------- skill: stocked(O,L) :- O belongs_to P, P visits L ----------
discover_skill :-
    discover_composition(stocked, 3),
    induce_constrained(stocked, [belongs_to, visits]),
    check(composed_rule(stocked, [belongs_to, visits], _),
          'skill stocked :- [belongs_to, visits] discovered').

% ---------- SSE estructural (label-free, stocked excluido) ----------
% member_sig(E, sig(O,I,ON,IN)): grados WL-1 ignorando stocked.
member_sig(E, sig(O, I, ON, IN)) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stocked), Outs),
    length(Outs, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stocked), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_excl(N, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_excl(N, A, B)), IN0),
    sort(IN0, IN).

deg_excl(E, O, I) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stocked), L1),
    length(L1, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stocked), L2),
    length(L2, I).

% struct_sse(S, stsig(SortedMemberSigs, InternalEdges))
struct_sse(S, stsig(MS, E)) :-
    struct_def(S, Members),
    findall(M, (member(X, Members), member_sig(X, M)), M0),
    sort(M0, MS),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _), R \== stocked),
            Edges),
    length(Edges, E).

% auditoria: cero atomos en la firma (solo estructura)
sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

derive_struct_concepts :-
    findall(S, struct_def(S, _), Ss),
    forall(member(S, Ss),
           (struct_sse(S, Sig),
            (struct_sig(C, Sig) -> assertz(struct_member(C, S))
            ; (findall(N, struct_sig(struct_c(N), _), Ns),
               (Ns == [] -> N = 1 ; (max_list(Ns, M), N is M + 1)),
               C = struct_c(N),
               assertz(struct_sig(C, Sig)),
               assertz(struct_member(C, S)))))),
    struct_sse(s1, Sig1), struct_sse(s2, Sig2),
    check(Sig1 == Sig2, 'train structures share structural SSE'),
    check(sig_clean(Sig1), 'structural SSE has zero atoms'),
    findall(X, struct_member(_, X), Ms0),
    sort(Ms0, Ms),
    check(Ms == [s1, s2], 'one structure-concept {s1,s2}').

attach_struct_skill :-
    struct_sse(s1, Sig),
    struct_sig(C, Sig),
    assertz(struct_skill(C, stocked, [belongs_to, visits])),
    check(true, 'skill stocked attached to structure-concept').

% ---------- test: estructura nueva (vocabulario nuevo) + distractores ----------
build_test :-
    % s3: recien llegada, stocked OCULTO
    assertz(struct_def(s3, [zorin, tablet, sevilla])),
    forall(member((S, R, O),
                  [(zorin, owns, tablet), (tablet, belongs_to, zorin),
                   (zorin, visits, sevilla)]),
           remember_relation(S, R, O, 1.0)),
    % s4: casi-estructura (falta visits)
    assertz(struct_def(s4, [wex, quark])),
    forall(member((S, R, O),
                  [(wex, owns, quark), (quark, belongs_to, wex)]),
           remember_relation(S, R, O, 1.0)),
    % s5: estructuralmente cercana pero con arista extra
    assertz(struct_def(s5, [yago, cake, oslo])),
    forall(member((S, R, O),
                  [(yago, owns, cake), (cake, belongs_to, yago),
                   (yago, visits, oslo), (yago, visits, rome)]),
           remember_relation(S, R, O, 1.0)).

% clasificar estructura por igualdad exacta de firma
classify_struct(S, C) :-
    struct_sse(S, Sig),
    struct_sig(C, Sig),
    struct_skill(C, _, _).

% predecir stocked solo dentro de estructuras miembro del concepto
predict_stocked(O, L, Proof) :-
    struct_def(S, Members),
    member(O, Members), member(L, Members),
    classify_struct(S, C),
    struct_skill(C, stocked, [belongs_to, visits]),
    memory_relation(O, belongs_to, P, _, _),
    memory_relation(P, visits, L, _, _),
    Proof = [struct_match(S, C), rule(stocked, [belongs_to, visits]),
             (O, belongs_to, P), (P, visits, L)].

run_transfer_tests :-
    nl, writeln('===== TRANSFER (structure-level) ====='),
    (classify_struct(s3, _) ->
        check(true, 's3 joins (no new concept)')
    ; check(false, 's3 joins (no new concept)')),
    (predict_stocked(tablet, sevilla, Proof) ->
        (check(true, 's3 inherits stocked -> sevilla (never observed)'),
         format('proof: ~w~n', [Proof]))
    ; check(false, 's3 inherits stocked -> sevilla (never observed)')),
    (classify_struct(s4, _) ->
        check(false, 's4 excluded (missing visits)')
    ; check(true, 's4 excluded (missing visits)')),
    (classify_struct(s5, _) ->
        check(false, 's5 excluded (extra edge)')
    ; check(true, 's5 excluded (extra edge)')),
    (predict_stocked(quark, _, _) ->
        check(false, 'wex/quark -> UNKNOWN')
    ; (check(true, 'wex/quark -> UNKNOWN'),
       format('wex/quark -> UNKNOWN~n', []))),
    (predict_stocked(tablet, paris, _) ->
        check(false, 'cross tablet->paris rejected')
    ; check(true, 'cross tablet->paris rejected')),
    struct_sse(s3, Sig3),
    check(sig_clean(Sig3), 'newcomer SSE has zero atoms (audit)').

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
