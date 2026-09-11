% experiment15.pl
% EXPERIMENT 15 - OPEN VOCABULARY
% Unknown words -> UNKNOWN symbols (kept, never type-hallucinated).
% P1: usage only (zorin/velara/felicia/kadir/wex) -> NO is_a may exist.
% P2: is_a evidence -> exact typing (4/4), wex stays untyped.
% P3: rejected verb sentences -> nothing enters memory.
% P4: reaches rule over mixed known/unknown + hidden incl. all-unknown
%     chain; metrics: recall / type-precision / generalization / halluc=0.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('open_vocab.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment15 :-
    reset_experiment,
    phase1_usage,
    phase2_typing,
    phase3_rejections,
    phase4_rules_tests.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

remember_triple((S, V, O)) :-
    remember_relation(S, V, O, 1.0).

% ---------- P1: usage sentences, unknowns enter WITHOUT types ----------
phase1_usage_sentences([
    "lina visits roma",
    "roma is in italy",
    "lina reaches italy",
    "mario visits paris",
    "paris is in france",
    "mario reaches france",
    "anna visits lima",
    "lima is in peru",
    "anna reaches peru",
    "zorin visits velara",
    "velara is in felicia",
    "kadir visits oslo",
    "oslo is in norway",
    "wex visits lima"
]).

phase1_usage :-
    nl, writeln('===== PHASE 1: usage (unknowns untyped) ====='),
    phase1_usage_sentences(Ss),
    findall(T, ( member(S, Ss),
                 symbolize_open(S, T, usage)
               ),
            Ts),
    length(Ss, NS),
    length(Ts, NT),
    format('parsed ~w/~w usage sentences~n', [NT, NS]),
    forall(member(T, Ts), remember_triple(T)),
    ( memory_relation(_, is_a, _, _, _) ->
        format('HALLUCINATION: is_a without evidence!~n', []), fail
    ; format('no is_a facts: unknowns truly untyped OK~n', [])
    ).

% ---------- P2: explicit typing evidence ----------
phase2_typing_sentences([
    "zorin is a person",
    "velara is a city",
    "felicia is a country",
    "kadir is a person"
]).

phase2_typing :-
    nl, writeln('===== PHASE 2: typing evidence ====='),
    phase2_typing_sentences(Ss),
    forall(( member(S, Ss),
             symbolize_open(S, T, typed)
           ),
           remember_triple(T)),
    findall((X, Y), memory_relation(X, is_a, Y, _, _), IsA0),
    sort(IsA0, IsA),
    format('is_a facts: ~w~n', [IsA]),
    Expected = [(felicia, country), (kadir, person),
                (velara, city), (zorin, person)],
    ( IsA == Expected ->
        format('TYPE PRECISION 4/4 exact OK~n', [])
    ; format('TYPE MISMATCH~n', []), fail
    ),
    ( memory_relation(wex, is_a, _, _, _) ->
        format('HALLUCINATION: wex typed without evidence!~n', []), fail
    ; format('wex stays untyped OK~n', [])
    ).

% ---------- P3: unknown verbs rejected, nothing stored ----------
phase3_bad_sentences([
    "zorin glimps velara",
    "kadir dwells oslo"
]).

phase3_rejections :-
    nl, writeln('===== PHASE 3: rejections ====='),
    phase3_bad_sentences(Ss),
    findall(S, ( member(S, Ss),
                 symbolize_open(S, _, _)
               ),
            Parsed),
    ( Parsed == [] ->
        format('rejected 2/2 unknown-verb sentences OK~n', [])
    ; format('LEAK: parsed ~w~n', [Parsed]), fail
    ),
    ( memory_relation(_, glimps, _, _, _) ->
        format('LEAK: glimps in memory!~n', []), fail
    ; true
    ),
    ( memory_relation(_, dwells, _, _, _) ->
        format('LEAK: dwells in memory!~n', []), fail
    ; format('no verb leaks OK~n', [])
    ).

% ---------- P4: rules over mixed entities + audits ----------
phase4_rules_tests :-
    nl, writeln('===== PHASE 4: rules + audits ====='),
    discover_concepts,
    show_concept_notes,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    show_composed_rules,
    induce_constrained(reaches, [visits, in]),
    show_constrained_rules,
    run_tests,
    run_audits.

show_concept_notes :-
    nl, writeln('--- structural roles vs asserted types ---'),
    forall(concept_member(C, E, _),
           ( (E == zorin ; E == kadir ; E == felicia) ->
               format('typed-but-split: ~w in ~w~n', [E, C])
           ; true
           )).

run_tests :-
    HPos = [(zorin, reaches, felicia), (kadir, reaches, norway),
            (wex, reaches, peru)],
    HNeg = [(zorin, reaches, norway), (kadir, reaches, felicia),
            (lina, reaches, france), (wex, reaches, italy)],
    findall(1, (member((S, V, O), HPos), reuse_predict(S, V, O)), TPL),
    length(TPL, TP),
    findall(1, (member((S, V, O), HPos), \+ reuse_predict(S, V, O)), FNL),
    length(FNL, FN),
    findall(1, (member((S, V, O), HNeg), reuse_predict(S, V, O)), FPL),
    length(FPL, FP),
    findall(1, (member((S, V, O), HNeg), \+ reuse_predict(S, V, O)), TNL),
    length(TNL, TN),
    format('generalization TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 3,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    format('Precision=~4f Recall=~4f F1=~4f~n', [P, R, F1]).

run_audits :-
    nl, writeln('===== AUDITS ====='),
    % 1. unknown recall: accepted unknowns all present as nodes
    findall(E, ( memory_relation(E, _, _, _, _) ;
                 memory_relation(_, _, E, _, _)
               ),
            E0),
    sort(E0, Nodes),
    ExpectedU = [felicia, kadir, velara, wex, zorin],
    ( forall(member(U, ExpectedU), member(U, Nodes)) ->
        format('UNKNOWN RECALL 5/5 OK~n', [])
    ; format('UNKNOWN RECALL FAIL~n', []), fail
    ),
    % 2. type precision already checked in P2; re-assert count
    findall(1, memory_relation(_, is_a, _, _, _), IL),
    length(IL, NI),
    format('is_a count=~w (expected 4)~n', [NI]),
    % 3. rejected words absent
    ( memory_relation(_, _, glimps, _, _) ->
        format('HALLUCINATION: glimps node!~n', []), fail
    ; true
    ),
    findall(E, ( ( E = glimps ; E = dwells ),
                 ( memory_relation(E, _, _, _, _) ;
                   memory_relation(_, _, E, _, _) )
               ),
            Leaks),
    ( Leaks == [] ->
        format('HALLUCINATION RATE 0%% OK~n', [])
    ; format('HALLUCINATION: ~w~n', [Leaks]), fail
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
