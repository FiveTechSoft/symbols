% experiment17.pl
% EXPERIMENT 17 - IDENTITY (SAME / DIFFERENT / UNKNOWN / CONTRADICTION)
% Estructuras directas; declaraciones de identidad VIA LENGUAJE
% ("alias1 is alpha"). Sin same_entity magico en los datos.
% Funcionalidad descubierta, nunca programada.
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('identity.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.

experiment17 :-
    reset_experiment,
    phase1_base,
    show_functionality,
    phase1_queries,
    phase2_evidence,
    phase2_verdicts,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(same_as_link(_, _)),
    retractall(check_results(_, _)).

remember_triple((S, V, O)) :-
    remember_relation(S, V, O, 1.0).

% ---------- Phase 1: base facts (direct) ----------
phase1_facts([
    (alpha, visits, madrid), (alpha, visits, paris), (alpha, eats, bread),
    (alias1, visits, madrid), (alias1, eats, bread),
    (beta, visits, madrid),
    (gamma, visits, madrid),
    (delta, visits, paris), (epsilon, visits, paris),
    (juan, eats, bread), (pedro, eats, bread),
    (alias3, visits, paris)
]).

phase1_base :-
    nl, writeln('===== PHASE 1: base evidence ====='),
    phase1_facts(Fs),
    forall(member(T, Fs), remember_triple(T)),
    discover_concepts,
    show_concepts_brief.

phase1_queries :-
    nl, writeln('===== PHASE 1 verdicts ====='),
    check(identity_status(beta, gamma, unknown),
          'beta ?= gamma UNKNOWN (indistinguishable)'),
    check(identity_status(delta, epsilon, unknown),
          'delta ?= epsilon UNKNOWN'),
    check(identity_status(juan, pedro, unknown),
          'juan ?= pedro UNKNOWN (same structure, not same entity)'),
    check(( concept_member(C, juan, _),
            concept_member(C, pedro, _)
          ),
          'juan & pedro share concept (membership, not identity)'),
    check(\+ functional_candidate(visits),
          'visits discovered non-functional'),
    check(\+ functional_candidate(lives_in),
          'lives_in not yet decidable (no data in phase 1)').

% ---------- Phase 2: discriminants + declarations via language ----------
declare_sentences([
    "alias1 is alpha",
    "alias3 is beta",
    "alias2 is beta"
]).

phase2_evidence :-
    nl, writeln('===== PHASE 2: discriminants + lives + declarations ====='),
    remember_triple((beta, eats, bread)),
    remember_triple((gamma, eats, cheese)),
    remember_triple((beta, lives_in, madrid)),
    remember_triple((alias1, lives_in, madrid)),
    remember_triple((alias2, lives_in, paris)),
    declare_sentences(Ss),
    forall(member(S, Ss),
           ( symbolize_open(S, (X, same_as, Y), identity) ->
               ( remember_identity(X, Y),
                 format('declared: ~w == ~w  (from "~w")~n', [X, Y, S])
               )
           ; format('PARSE FAIL: ~w~n', [S]), fail
           )),
    discover_concepts,
    show_concepts_brief.

phase2_verdicts :-
    nl, writeln('===== PHASE 2 verdicts ====='),
    check(functional_candidate(lives_in),
          'lives_in discovered functional in phase 2'),
    check(identity_status(alias1, alpha, same),
          'alias1 == alpha SAME (declared)'),
    check(identity_status(alias3, beta, same),
          'alias3 == beta SAME (visits merge, non-functional, clean)'),
    check(identity_status(beta, gamma, different),
          'beta != gamma DIFFERENT (eats discriminant)'),
    check(identity_status(delta, epsilon, unknown),
          'delta ?= epsilon still UNKNOWN'),
    check(identity_status(beta, gamma, different),
          'UNKNOWN != DIFFERENT (strict, both verdicts coexist)'),
    check(identity_status(alias1, alpha, same),
          'SAME despite differing evidence (declaration beats structure)'),
    check(identity_status(beta, alias2, contradiction),
          'beta+alias2 CONTRADICTION (2 lives_in, functional)'),
    check(merged_has(alpha, out(lives_in, madrid)),
          'merged alpha memory gained lives_in via alias1'),
    check(identity_status(juan, pedro, unknown),
          'EXP17.5 holds: same concept, UNKNOWN identity').

merged_has(E, Fact) :-
    merged_profile(E, P),
    member(Fact, P).

% ---------- checks ----------
check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).

% --- discovery (threshold 0.80, idempotent) ---

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
    ( Best >= 0.80 -> add_member(BC, E, Best)
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

show_concepts_brief :-
    findall(C, concept(C, _, _), Cs),
    length(Cs, N),
    format('concepts=~w~n', [N]),
    forall(concept(C, Sig, _),
           ( findall(M, concept_member(C, M, _), Ms0),
             sort(Ms0, Ms),
             format('  ~w ~w ~w~n', [C, Sig, Ms])
           )).
