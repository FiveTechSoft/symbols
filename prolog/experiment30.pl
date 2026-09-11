% experiment30.pl
% EXPERIMENT 30 - FIRST NATURAL CORPUS (cero lexicón declarado)
% Parser posicional SVO (positional.pl): sin listas de verbos ni tipos.
% Corpus A en bloques (a1/a2/a3) + secreto temporal (secret/, jamas
% ingerido al aprender). Retrieve / reason / unknown + truco + proofs.
:- consult('memory.pl').
:- consult('positional.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('continuous.pl').
:- consult('question_parser.pl').

:- consult('guided_search.pl').
:- consult('secret/secret_expected.pl').
:- consult('heldoutA.pl').
:- consult('distractorA.pl').

:- use_module(library(lists)).
:- use_module(library(readutil)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.
:- dynamic sentence_counter/1.

experiment30 :-
    reset_experiment,
    ingest_block('corpusA/a1.txt', [comes_from]),
    ingest_block('corpusA/a2.txt', [provides]),
    ingest_block('corpusA/a3.txt', [uses]),
    run_heldout,
    run_distractors,
    run_secret,
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
    retractall(sentence_counter(_)),
    retractall(check_results(_, _)),
    assertz(sentence_counter(0)).

% ---------- ingesta posicional (sin lexicón) ----------
feed_pos(S) :-
    sentence_counter(N),
    N1 is N + 1,
    retract(sentence_counter(N)),
    assertz(sentence_counter(N1)),
    ( symbolize_text(S, (A, V, O)) ->
        remember_relation(A, V, O, 1.0)
    ; format('PARSE FAIL: ~w~n', [S]), fail
    ).

ingest_file(File, Count) :-
    open(File, read, St, [encoding(utf8)]),
    read_lines_loop(St, Lines),
    close(St),
    exclude(empty_line, Lines, NonEmpty),
    length(NonEmpty, Count),
    forall(member(S, NonEmpty), feed_pos(S)).

read_lines_loop(St, Lines) :-
    catch(read_line_to_string(St, L), _, L = end_of_file),
    ( L == end_of_file -> Lines = []
    ; Lines = [L|Ls], read_lines_loop(St, Ls)
    ).

empty_line("").
empty_line(S) :- atom_string(A, S), atom_length(A, 0).

% ---------- bloque: ingiere + ciclo guiado + metricas ----------
ingest_block(File, Targets) :-
    format('~n===== BLOCK ~w =====~n', [File]),
    ingest_file(File, Count),
    memory_size(M),
    format('sentences=~w facts=~w~n', [Count, M]),
    learn_cycle_guided_local(Targets, Rows),
    forall(member(row(T, Path, F1, Gen, Ev, Ms), Rows),
           format('guided ~w :- ~w F1=~4f gen=~w eval=~w ~wms~n',
                  [T, Path, F1, Gen, Ev, Ms])),
    knowledge_stats_block.

knowledge_stats_block :-
    memory_size(F),
    findall(E, ( memory_relation(E, _, _, _, _) ;
                 memory_relation(_, _, E, _, _)
               ),
            E0),
    sort(E0, Es),
    length(Es, S),
    findall(1, concept(_, _, _), Cs),
    length(Cs, C),
    findall(1, constrained_rule(_, _, _), Rs),
    length(Rs, R),
    format('stats: facts=~w symbols=~w concepts=~w rules=~w~n',
           [F, S, C, R]).

% ciclo guiado local (descubre + induce por target con alcance)
learn_cycle_guided_local(Targets, Rows) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup,
    findall(Row, ( member(T, Targets),
                   guided_target_local(T, Row)
                 ),
            Rows).

guided_target_local(T, row(T, Path, F1, Gen, Ev, Ms)) :-
    run_discovery(guided, T, 3, Stats),
    Stats = stats(Path, F1, _Sup, Gen, Ev, _Pr, Ms),
    retractall(composed_rule(T, _, _)),
    assertz(composed_rule(T, Path, F1)),
    induce_constrained(T, Path).

discover_rules_dedup :-
    forall(concept_relation(SC, R, OC, Sc),
           ( learned_rule(rule(SC, R, OC), _, _, _, _) -> true
           ; assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))
           )).

% (guided_search consultado arriba, junto al resto)

% ---------- held-out del generador (conclusions retenidas) ----------
run_heldout :-
    nl, writeln('===== HELD-OUT (never in corpus) ====='),
    findall((P, E, R), heldout(P, E, R), HK),
    length(HK, NH),
    check(NH =:= 6, '6 held-out pairs loaded'),
    forall(member((P, E, R), HK),
           ( question_for(R, P, Q),
             parse_question(Q, QP),
             answer_query(QP, A),
             ( A = answer([E], reasoned, Proofs),
               Proofs \== [] ->
                 format('PASS ~w --~w--> ~w~n', [P, R, E]),
                 assertz(check_results(P, pass))
             ; format('FAIL ~w -> ~w~n', [P, A]),
               assertz(check_results(P, fail))
             )
           )).

% ---------- distractores del generador (pares minimos probados falsos) ----------
run_distractors :-
    nl, writeln('===== DISTRACTORS (verified false) ====='),
    findall((P, X, R), distractor(P, X, R), Ds),
    length(Ds, ND),
    format('distractors: ~w~n', [ND]),
    findall((P, X), ( member((P, X, R), Ds),
                      firing_any(P, X, R)
                    ),
            Firing),
    ( Firing == [] ->
        format('verifier: 0 fire any rule OK~n', []),
        assertz(check_results('verifier clean', pass))
    ; format('VERIFIER FAIL: ~w~n', [Firing]),
      assertz(check_results('verifier clean', fail))
    ),
    forall(member((P, X, R), Ds),
           ( question_for(R, P, Q),
             parse_question(Q, QP),
             answer_query(QP, A),
             ( A = answer([X], _, _) ->
                 format('FP ~w -> ~w~n', [P, X]),
                 assertz(check_results(distractor, fail))
             ; assertz(check_results(distractor, pass))
             )
           )).

firing_any(P, X, R) :-
    constrained_rule(R, Path, Sig),
    full_bindings(P, X, Path, Full),
    eq_signature(Full, Sig).

question_for(comes_from, P, Q) :-
    format(string(Q), 'Where does ~w come from?', [P]).
question_for(provides, P, Q) :-
    format(string(Q), 'Where does ~w provide?', [P]).
question_for(uses, P, Q) :-
    format(string(Q), 'Where does ~w use?', [P]).

% ---------- secreto temporal (jamas ingerido al aprender) ----------
run_secret :-
    nl, writeln('===== SECRET (temporal, never learned) ====='),
    open('secret/secret_text.txt', read, St, [encoding(utf8)]),
    read_lines_loop(St, Lines),
    close(St),
    exclude(empty_line, Lines, Secret),
    length(Secret, NS),
    format('secret sentences: ~w~n', [NS]),
    findall(T-P, constrained_rule(T, P, _), Before),
    sort(Before, BB),
    forall(member(S, Secret), feed_pos(S)),
    findall(T-P, constrained_rule(T, P, _), After),
    sort(After, AA),
    check(BB == AA, 'no induction during secret (application only)'),
    secret_expects(zara, spain, comes_from),
    secret_expects(yago, steel, provides),
    secret_expects(teo, pork, uses),
    ask_secret("Where does Zara come from?", [spain], comes_from),
    ask_secret("Where does Yago provide?", [steel], provides),
    ask_secret("Where does Teo use?", [pork], uses),
    ask("Where does Zorin live?", unknown, "I don't know."),
    ask_yn("Does Ana live in Paris?", no).

ask_secret(Q, ExpectedXs, Rel) :-
    parse_question(Q, QP),
    answer_query(QP, A),
    ( A = answer(ExpectedXs, reasoned, Proofs),
      Proofs \== [] ->
        format('PASS ~w -> ~w (reasoned, ~w)~n', [Q, ExpectedXs, Rel]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w -> ~w~n', [Q, A]),
      assertz(check_results(Q, fail))
    ).

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
