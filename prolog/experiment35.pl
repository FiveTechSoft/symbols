% experiment35.pl
% EXPERIMENT 35 - TASK-ORIENTED SSE (un objeto, varias preguntas)
% alpha responde lives_in/visits/reaches; cada objetivo induce una SSE
% distinta auditada (la exclusion importa; cero atomos; sin fuga).
% reaches sale por REGLA con prueba (no por la firma).
:- consult('memory.pl').
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

experiment35 :-
    reset_experiment,
    build_world,
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, [visits, in]),
    run_queries,
    run_sse_audits,
    run_distractors,
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

% 5 completos (reaches oculto en alpha,beta) + zorin visitas-solo.
% espana existe (pais de eva) para el truco No.
build_world :-
    forall(member((X, C, F, H, K),
                  [(alpha, ca1, fa1, ha1, ka1),
                   (beta, cb1, fb1, hb1, kb1),
                   (carl, ca3, fa3, ha3, ka3),
                   (dora, ca4, fa4, ha4, ka4),
                   (eva, ca5, fa5, ha5, spain)]),
           ( remember_relation(X, visits, C, 1.0),
             remember_relation(X, eats, F, 1.0),
             remember_relation(X, lives_in, H, 1.0),
             remember_relation(C, in, K, 1.0),
             ( (X == alpha ; X == beta) -> true
             ; remember_relation(X, reaches, K, 1.0)
             )
           )),
    remember_relation(zorin, visits, vz, 1.0),
    memory_size(N),
    format('world facts: ~w~n', [N]).

% ---------- SSE con exclusion ----------
sse_excluding(E, Exclude, sig(O, I, ON, IN)) :-
    findall(X, ( memory_relation(E, R, X, _, _),
                 \+ member(R, Exclude)
               ),
            Outs),
    length(Outs, O),
    findall(X, ( memory_relation(_, R, E, _, _),
                 \+ member(R, Exclude)
               ),
            Ins),
    length(Ins, I),
    findall((A, B), ( member(N, Outs),
                      deg_excluding(N, Exclude, A, B)
                    ),
            ON0),
    sort(ON0, ON),
    findall((A, B), ( member(N, Ins),
                      deg_excluding(N, Exclude, A, B)
                    ),
            IN0),
    sort(IN0, IN).

deg_excluding(E, Exclude, O, I) :-
    findall(X, ( memory_relation(E, R, X, _, _),
                 \+ member(R, Exclude)
               ),
            L1),
    length(L1, O),
    findall(X, ( memory_relation(_, R, E, _, _),
                 \+ member(R, Exclude)
               ),
            L2),
    length(L2, I).

signature_clean(Sig) :-
    \+ ( sub_term(T, Sig),
         atom(T)
       ).

% ---------- queries ----------
run_queries :-
    nl, writeln('===== QUERIES (same object, three targets) ====='),
    ask("Where does Alpha live?",
        answer([ha1], retrieved, _),
        "alpha lives in ha1."),
    ask("Who visits Ca1?",
        answer([alpha], retrieved, _),
        "alpha visits ca1."),
    ask("Where does Alpha reach?",
        answer([ka1], reasoned, Proof),
        "alpha reaches ka1."),
    check(( Proof = [P | _],
            memberchk(rule(reaches, [visits, in], _), P)
          ),
          'reaches answer cites rule (inference, not leakage)'),
    ask("Where does Beta reach?",
        answer([kb1], reasoned, _),
        "beta reaches kb1.").

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

% ---------- auditorias SSE ----------
run_sse_audits :-
    nl, writeln('===== SSE AUDITS (one object, three tasks) ====='),
    sse_excluding(alpha, [lives_in], SL),
    sse_excluding(alpha, [visits], SV),
    sse_excluding(alpha, [reaches], SR),
    sse_excluding(alpha, [], SF),
    check(SL \== SF, 'SSE|lives_in differs (exclusion mattered)'),
    check(SV \== SF, 'SSE|visits differs (exclusion mattered)'),
    check(SR == SF, 'SSE|reaches == full (hidden: nothing to leak, honest)'),
    check(( SL \== SV, SV \== SR, SL \== SR ),
          'three tasks, three different signatures'),
    check(( signature_clean(SL),
            signature_clean(SV),
            signature_clean(SR)
          ),
          'zero atoms in all three (audit)').

% ---------- distractores ----------
run_distractors :-
    nl, writeln('===== DISTRACTORS ====='),
    ask_yn("Does Alpha reach Spain?", no),
    ask("Where does Zorin reach?", unknown, "I don't know.").

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

% --- checks & discovery (threshold 0.80, idempotent) ---

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
