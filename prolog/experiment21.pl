% experiment21.pl
% EXPERIMENT 21 - RULE TRANSFER ACROSS WIPED MEMORY
% Fase A: experiencia (lenguaje) -> regla reaches (2 cadenas observadas).
% Fase B: regla a disco, BORRADO TOTAL factual, recarga (solo la regla).
% Fase C: entidades nuevas (sofia/rome/italy, leo/oslo/norway), sin reaches.
% La respuesta solo puede venir de EJECUTAR la regla, nunca de retrieval.
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('meta_pattern.pl').
:- consult('rule_instantiation.pl').
:- consult('question_parser.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.

experiment21 :-
    reset_experiment,
    phase_a_experience,
    phase_b_wipe_reload,
    phase_c_new_world,
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
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)),
    retractall(check_results(_, _)).

feed(S) :-
    ( symbolize_open(S, (A, V, B), usage) ->
        remember_relation(A, V, B, 1.0)
    ; format('PARSE FAIL: ~w~n', [S]), fail
    ).

% ---------- Fase A: experiencia -> regla ----------
phase_a_experience :-
    nl, writeln('===== PHASE A: experience -> rule ====='),
    forall(member(S, [
        "Ana visits Madrid",
        "Madrid is in Spain",
        "Ana reaches Spain",
        "Luis visits Paris",
        "Paris is in France",
        "Luis reaches France"
    ]), feed(S)),
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, [visits, in]),
    constrained_rule(reaches, Path, Sig),
    format('learned reaches :- ~w + ~w~n', [Path, Sig]),
    check(constrained_rule(reaches, [visits, in], [1, 2, 3]),
          'rule reaches:[visits,in]+[1,2,3] induced').

% ---------- Fase B: disco + borrado total + recarga ----------
phase_b_wipe_reload :-
    nl, writeln('===== PHASE B: persist, wipe factual memory, reload ====='),
    export_rules('longterm21.pl'),
    constrained_rule(reaches, Path, Sig),
    clear_memory,
    wipe_all_rules,
    % el wipe debe llevarse tambien artefactos de induccion (rederivables)
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    memory_size(M),
    check(M =:= 0, 'factual memory wiped (0 facts)'),
    check(\+ constrained_rule(_, _, _), 'no rules survive wipe'),
    import_rules('longterm21.pl'),
    check(constrained_rule(reaches, Path, Sig),
          'same rule reloaded from disk (experience gone, program stays)'),
    check(\+ composed_rule(_, _, _),
          'no induction artifacts reloaded (application only)').

% ---------- Fase C: mundo nuevo, solo ejecucion ----------
phase_c_new_world :-
    nl, writeln('===== PHASE C: new entities, execution only ====='),
    forall(member(S, [
        "Sofia visits Rome",
        "Rome is in Italy",
        "Leo visits Oslo",
        "Oslo is in Norway"
    ]), feed(S)),
    check(\+ memory_relation(_, reaches, _, _, _),
          'zero reaches facts: retrieval impossible'),
    ask("Where does Sofia reach?",
        answer([italy], reasoned, _),
        "sofia reaches italy."),
    ask("Where does Leo reach?",
        answer([norway], reasoned, Proof),
        "leo reaches norway."),
    check(( Proof = [P | _],
            memberchk(rule(reaches, [visits, in], _), P)
          ),
          'proof executes stored rule on new symbols'),
    ask_yn("Does Sofia reach Spain?", no),
    ask("Where does Ana reach?", unknown, "I don't know.").

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

% --- checks ---

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
