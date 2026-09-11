% experiment14.pl
% EXPERIMENT 14 - CUMULATIVE LINGUISTIC LEARNING (English session)
% ARM A: symbols  -> knowledge.  ARM B: language -> knowledge. A =?= B ?
% SESSION 2: wipe runtime, load rules, NEW persons/cities/countries as
%   language only, infer via loaded rule (application, no re-induction).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('meta_pattern.pl').
:- consult('rule_instantiation.pl').
:- consult('english_graph.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment14 :-
    reset_all,
    session1_facts(Gold),
    session1_sentences(Gold, Sentences),
    arm_symbols(Gold, RuleA),
    arm_language(Sentences, Gold, RuleB),
    ( RuleA == RuleB ->
        format('CONVERGENCE OK: symbols == language (~w)~n', [RuleA])
    ; format('CONVERGENCE FAIL: ~w vs ~w~n', [RuleA, RuleB]),
      fail
    ),
    run_session1_tests,
    session2.

reset_all :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% ---------- session 1 data: 6 chains, reaches hidden in 2 ----------
chain(lina, roma, italy).
chain(mario, paris, france).
chain(sofia, madrid, spain).
chain(anna, oslo, norway).
chain(paul, quito, ecuador).
chain(elena, lima, peru).
hidden(sofia). hidden(paul).

session1_facts(Gold) :-
    findall((P, visits, C), chain(P, C, _), V),
    findall((C, in, K), chain(_, C, K), I),
    findall((P, reaches, K),
            ( chain(P, _, K), \+ hidden(P) ),
            R),
    append([V, I, R], All),
    sort(All, Gold).

session1_sentences(Gold, Ss) :-
    findall(S, ( member(F, Gold),
                 fact_sentence(F, Gold, S)
               ),
            Ss).

fact_sentence(Fact, Gold, S) :-
    nth0(K, Gold, Fact),
    sentence_tpl(Fact, K, S).

sentence_tpl((X, visits, Y), K, S) :-
    K2 is K mod 4,
    ( K2 =:= 0 -> format(string(S), "~w visits ~w.", [X, Y])
    ; K2 =:= 1 -> format(string(S), "~w visited ~w.", [X, Y])
    ; K2 =:= 2 -> format(string(S), "~w was visited by ~w.", [Y, X])
    ; format(string(S), "yesterday ~w visited ~w.", [X, Y])
    ).
sentence_tpl((X, in, Y), K, S) :-
    K2 is K mod 3,
    ( K2 =:= 0 -> format(string(S), "~w is in ~w.", [X, Y])
    ; K2 =:= 1 -> format(string(S), "~w lies in ~w.", [X, Y])
    ; format(string(S), "~w was in ~w.", [X, Y])
    ).
sentence_tpl((X, reaches, Y), K, S) :-
    K2 is K mod 2,
    ( K2 =:= 0 -> format(string(S), "~w reaches ~w.", [X, Y])
    ; format(string(S), "~w has reached ~w.", [X, Y])
    ).

% ---------- ARM A: direct symbols ----------
arm_symbols(Gold, rule(Path, Sig)) :-
    nl, writeln('===== ARM A (symbols) ====='),
    reset_all,
    forall(member((S, V, O), Gold),
           remember_relation(S, V, O, 1.0)),
    learn_reaches(Path, Sig).

% ---------- ARM B: language only ----------
arm_language(Sentences, Gold, rule(Path, Sig)) :-
    nl, writeln('===== ARM B (language) ====='),
    reset_all,
    findall(T, ( member(S, Sentences),
                 symbolize_en(S, T)
               ),
            Ext0),
    sort(Ext0, Ext),
    sort(Gold, G),
    ( Ext == G ->
        format('extraction exact: ~w triples~n', [G])
    ; format('EXTRACTION MISMATCH~n', []), fail
    ),
    forall(member((S, V, O), Ext),
           remember_relation(S, V, O, 1.0)),
    learn_reaches(Path, Sig).

learn_reaches(Path, Sig) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, Path),
    constrained_rule(reaches, Path, Sig),
    format('learned reaches :- ~w + ~w~n', [Path, Sig]).

% ---------- SESSION 2: new experience, loaded rule, no re-induction ----------
session2_sentences([
    "ruth visits dublin",
    "dublin is in ireland",
    "ivan visits bern",
    "bern lies in switzerland",
    "yesterday ruth visited dublin"
]).

session2 :-
    nl, writeln('===== SESSION 2 (new persons, loaded rule) ====='),
    export_rules('longterm14.pl'),
    reset_all,
    retractall(rule(_, _, _, _, _)),
    retractall(meta(_, _, _, _)),
    import_rules('longterm14.pl'),
    ( composed_rule(_, _, _) ->
        format('application-only VIOLATED (composed present)~n', []), fail
    ; format('application-only confirmed (no induction ran)~n', [])
    ),
    session2_sentences(Ss),
    forall(( member(S, Ss),
             symbolize_en(S, T)
           ),
           ( T = (A, V, B),
             remember_relation(A, V, B, 1.0)
           )),
    run_session2_tests.

run_session1_tests :-
    nl, writeln('--- session 1 hidden (language knowledge) ---'),
    HPos = [(sofia, reaches, spain), (paul, reaches, ecuador)],
    HNeg = [(sofia, reaches, france), (paul, reaches, spain),
            (lina, reaches, france), (mario, reaches, italy)],
    suite(HPos, HNeg, TP, FN, FP, TN),
    format('S1 TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    TP =:= 2, TN =:= 4.

run_session2_tests :-
    HPos = [(ruth, reaches, ireland), (ivan, reaches, switzerland)],
    HNeg = [(ruth, reaches, italy), (ruth, reaches, france),
            (ivan, reaches, ireland), (ivan, reaches, italy)],
    suite(HPos, HNeg, TP, FN, FP, TN),
    format('S2 TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    TP =:= 2, TN =:= 4.

suite(HPos, HNeg, TP, FN, FP, TN) :-
    findall(1, (member((S, V, O), HPos), reuse_predict(S, V, O)), TPL),
    length(TPL, TP),
    findall(1, (member((S, V, O), HPos), \+ reuse_predict(S, V, O)), FNL),
    length(FNL, FN),
    findall(1, (member((S, V, O), HNeg), reuse_predict(S, V, O)), FPL),
    length(FPL, FP),
    findall(1, (member((S, V, O), HNeg), \+ reuse_predict(S, V, O)), TNL),
    length(TNL, TN),
    forall(member((S, V, O), HPos),
           ( reuse_predict(S, V, O) ->
               format('PASS  ~w --~w--> ~w~n', [S, V, O])
           ; format('FAIL  ~w --~w--> ~w~n', [S, V, O])
           )),
    forall(member((S, V, O), HNeg),
           ( reuse_predict(S, V, O) ->
               format('ERROR ~w --~w--> ~w~n', [S, V, O])
           ; format('reject ~w --~w--> ~w~n', [S, V, O])
           )).

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
