% experiment19.pl
% EXPERIMENT 19 - QUESTIONS OVER MEMORY (retrieval vs reasoning + proof)
% Solo lenguaje de entrada (con correferencia); el texto se descarta.
% Q1 directa, Q2 inversa, Q3 compuesta (NO almacenada: reasoning),
% Q4 negativa (No), Q5 desconocida (UNKNOWN), Q6 inversa, Q7 por que (prueba).
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('coreference.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('question_parser.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.

experiment19 :-
    reset_experiment,
    feed_experience,
    resolve_all,
    learn_rules,
    run_questions,
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
    retractall(same_as_link(_, _)),
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)),
    retractall(resolution_verdict(_, _, _)),
    retractall(check_results(_, _)).

feed(S) :-
    ( symbolize_open(S, (A, V, B), usage) ->
        remember_relation(A, V, B, 1.0)
    ; format('PARSE FAIL: ~w~n', [S]), fail
    ).

% experiencia solo-lenguaje (el texto vive en esta clausula y se descarta).
% Resolucion incremental: cada bloque se resuelve antes del siguiente
% (si no, pedro llegaria a tiempo de ambiguizar he_2).
feed_experience :-
    nl, writeln('===== EXPERIENCE (language only, then discarded) ====='),
    forall(member(S, [
        "Maria lives in Madrid",
        "She visits Barcelona",
        "Maria visits Madrid",
        "Madrid is in Spain"
    ]), feed(S)),
    retry_unresolved,
    forall(member(S, [
        "Juan lives in Paris",
        "He visits London",
        "London is in England",
        "Juan reaches England"
    ]), feed(S)),
    retry_unresolved,
    forall(member(S, [
        "Pedro visits Oslo",
        "Oslo is in Norway",
        "Pedro reaches Norway"
    ]), feed(S)),
    retry_unresolved,
    format('text discarded; memory holds the graph~n', []).

resolve_all :-
    retry_unresolved,
    findall(P-V, resolution_verdict(P, V, _), Vs),
    format('resolutions: ~w~n', [Vs]).

learn_rules :-
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    show_composed_rules,
    induce_constrained(reaches, [visits, in]),
    show_constrained_rules.

run_questions :-
    nl, writeln('===== QUESTIONS ====='),
    % Q1 directa (retrieval)
    ask("Where does Maria live?",
        answer([madrid], retrieved, _),
        "maria lives in madrid."),
    % Q2 inversa (retrieval)
    ask("Who visits Barcelona?",
        answer([maria], retrieved, _),
        "maria visits barcelona."),
    % Q3 compuesta (reasoning: NO almacenada)
    ask("Where does Maria reach?",
        answer([spain], reasoned, Proof),
        "maria reaches spain."),
    check(\+ memory_relation(maria, reaches, _, _, _),
          'Q3 answer NOT stored: reasoning, not retrieval'),
    check(( Proof = [P | _],
            memberchk(rule(reaches, [visits, in], _), P)
          ),
          'Q3 proof cites rule + chain'),
    % Q4 negativa
    ask_yn("Does Maria live in Paris?", no),
    % Q5 desconocida
    ask("Where does Pedro live?", unknown, "I don't know."),
    % Q6 inversa 2
    ask("Who lives in Madrid?",
        answer([maria], retrieved, _),
        "maria lives in madrid."),
    % Q7 por que
    ask_why("Why does Maria reach Spain?",
            explanation(maria, reaches, spain, Proof7)),
    check(Proof7 \== [], 'Q7 proof non-empty').

ask(Q, Expected, ExpectedText) :-
    parse_question(Q, QP),
    answer_query(QP, A),
    verbalize(A, QP, Text),
    ( A = Expected, Text == ExpectedText ->
        format('PASS ~w -> ~w~n', [Q, Text]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w -> ~w (~w)~n', [Q, Text, A]),
      assertz(check_results(Q, fail))
    ).

ask_yn(Q, Expected) :-
    parse_question(Q, QP),
    answer_query(QP, A),
    verbalize(A, QP, Text),
    ( A == Expected ->
        format('PASS ~w -> ~w~n', [Q, Text]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w -> ~w~n', [Q, Text]),
      assertz(check_results(Q, fail))
    ).

ask_why(Q, Expected) :-
    parse_question(Q, QP),
    answer_query(QP, A),
    verbalize(A, QP, Text),
    ( A = Expected ->
        format('PASS ~w -> ~w~n', [Q, Text]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w -> ~w~n', [Q, Text]),
      assertz(check_results(Q, fail))
    ).

% --- checks ---

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

% --- discovery (threshold 0.70, idempotent) ---

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
