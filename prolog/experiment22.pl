% experiment22.pl
% EXPERIMENT 22 - CLOSED CYCLE: learn -> forget -> relearn + 2nd family
% S1 (lenguaje): reaches (2 cadenas) -> regla -> disco -> WIPE experiencia
%   (reglas+conceptos+meta se conservan).
% S2 (lenguaje, entidades nuevas): transferencia reaches a zorin;
%   familia travels/arrives (2 obs + 1 hidden) -> regla nueva;
%   retencion: reaches intacta y redisparando; no-interferencia.
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

experiment22 :-
    reset_experiment,
    session1_learn,
    wipe_experience,
    session2_transfer_newrule,
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

% ---------- S1: reaches por lenguaje ----------
session1_learn :-
    nl, writeln('===== SESSION 1: learn reaches ====='),
    forall(member(S, [
        "Maria visits Madrid",
        "Madrid is in Spain",
        "Maria reaches Spain",
        "Luis visits Paris",
        "Paris is in France",
        "Luis reaches France"
    ]), feed(S)),
    learn_target(reaches, [visits, in], [1, 2, 3]),
    export_rules('longterm22.pl').

learn_target(Target, Path, Sig) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup,
    discover_composition_scoped(Target, 3),
    induce_constrained(Target, Path),
    constrained_rule(Target, Path, Sig),
    format('learned ~w :- ~w + ~w~n', [Target, Path, Sig]).

discover_rules_dedup :-
    forall(concept_relation(SC, R, OC, Sc),
           ( learned_rule(rule(SC, R, OC), _, _, _, _) -> true
           ; assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))
           )).

% discover_composition_scoped local (solo su Target; resto sobrevive).
discover_composition_scoped(Target, MaxLen) :-
    retractall(composed_rule(Target, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(R, (memory_relation(_, R, _, _, _), R \== Target), Rs0),
    sort(Rs0, Vocab),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(composed_rule(Target, BestPath, BestF1)),
    format('Scoped: ~w :- ~w (F1=~4f)~n', [Target, BestPath, BestF1]).

% ---------- WIPE: solo experiencia (reglas+conceptos+meta quedan) ----------
wipe_experience :-
    nl, writeln('===== WIPE EXPERIENCE (rules+concepts+meta stay) ====='),
    kb_counts(Before),
    clear_memory,
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    memory_size(0),
    check(constrained_rule(reaches, [visits, in], [1, 2, 3]),
          'reaches rule survives wipe'),
    findall(1, concept(_, _, _), Cs),
    length(Cs, NC),
    check(NC > 0, 'concepts survive wipe'),
    format('kept: ~w~n', [Before]).

kb_counts([Mem, C, CR, R, Comp]) :-
    memory_size(Mem),
    findall(1, concept(_, _, _), LC), length(LC, C),
    findall(1, concept_relation(_, _, _, _), LCR), length(LCR, CR),
    findall(1, learned_rule(_, _, _, _, _), LR), length(LR, R),
    findall(1, composed_rule(_, _, _), LComp), length(LComp, Comp).

% ---------- S2: transferencia + familia nueva ----------
session2_transfer_newrule :-
    nl, writeln('===== SESSION 2: transfer + arrives family ====='),
    forall(member(S, [
        "Zorin visits Rome",
        "Rome is in Italy"
    ]), feed(S)),
    ask("Where does Zorin reach?",
        answer([italy], reasoned, _),
        "zorin reaches italy."),
    forall(member(S, [
        "Kadir travels through Paris",
        "Paris is in France",
        "Kadir arrives in France",
        "Leo travels through Oslo",
        "Oslo is in Norway",
        "Leo arrives in Norway",
        "Mia travels through Lima",
        "Lima is in Peru"
    ]), feed(S)),
    learn_target(arrives, [travels, in], [1, 2, 3]),
    % retencion: reaches intacta y redispara tras la 2a regla
    check(constrained_rule(reaches, [visits, in], [1, 2, 3]),
          'reaches intact after arrives learned (retention)'),
    ask("Where does Zorin reach?",
        answer([italy], reasoned, _),
        "zorin reaches italy."),
    ask("Where does Mia arrive?",
        answer([peru], reasoned, _),
        "mia arrives in peru."),
    ask("Where does Kadir arrive?",
        answer([france], retrieved, _),
        "kadir arrives in france."),
    ask_yn("Does Zorin reach Spain?", no),
    ask_yn("Does Mia arrive in France?", no),
    ask_yn("Does Kadir arrive in Italy?", no),
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
