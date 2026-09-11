% experiment43.pl
% EXPERIMENT 43 - OPEN CORPUS, MULTIPLE ABSTRACTIONS (v0.2)
% Mismo motor intacto; corpus_natural2 (519 frases) con TRES estructuras
% compartiendo personas: visits/in->reaches, works_at/located->based,
% owns/belongs_to (evidencia). Dos skills latentes descubiertas sin
% fusionarse; la tarea selecciona la estructura (reach vs based);
% proofs independientes por skill; transferencia en entidades nuevas.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('natural_parse.pl').
:- consult('corpus_natural2/gold.pl').
:- consult('corpus_natural2/expected.pl').
:- consult('corpus_natural2/distractor_natural2.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic composed_rule/3.
:- dynamic distinct_rule/3.
:- dynamic constrained_rule/3.
:- dynamic found_rule/3.
:- dynamic parsed_line/2.
:- dynamic check_results/2.

experiment43 :-
    reset_experiment,
    ingest_corpus,
    extraction_report,
    discover_concepts,
    discover_concept_relations,
    discover_latent,
    run_queries,
    run_distractors,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(parsed_line(_, _)),
    retractall(found_rule(_, _, _)),
    reset_natural.

% ---------- ingesta ----------
ingest_corpus :-
    open('corpus_natural2/corpus.txt', read, S, [encoding(utf8)]),
    ingest_lines(S, 1),
    close(S),
    findall(1, parsed_line(_, _), Ps), length(Ps, NP),
    format('ingested lines: ~w~n', [NP]).

ingest_lines(S, N) :-
    get_char(S, C),
    ( C == end_of_file -> true
    ; read_line_rest(S, C, Chars),
      string_chars(Line, Chars),
      ( Line == "" -> true
      ; ( symbolize_natural(Line, (Sub, Rel, Obj)) ->
            ( remember_relation(Sub, Rel, Obj, 1.0),
              assertz(parsed_line(N, (Sub, Rel, Obj))) )
        ; assertz(parsed_line(N, none)) )
      ),
      N1 is N + 1,
      ingest_lines(S, N1)
    ).

read_line_rest(S, C, [C|Cs]) :-
    C \== end_of_file, C \== '\n', !,
    get_char(S, C2),
    read_line_rest(S, C2, Cs).
read_line_rest(_, _, []).

% ---------- extraccion ----------
extraction_report :-
    findall(N, gold_fact(N, _, _, _), GNs), length(GNs, NG),
    findall(N, (parsed_line(N, T), T \== none,
                gold_fact(N, S, R, O), T == (S, R, O)), TPs),
    length(TPs, TP),
    findall(N, (parsed_line(N, T), T \== none), PPos),
    length(PPos, NPpos),
    ( NG > 0 -> P is TP / NG ; P = 0 ),
    ( NPpos > 0 -> R is TP / NPpos ; R = 0 ),
    format('extraction: TP=~w gold=~w parsedpos=~w P=~4f R=~4f~n',
           [TP, NG, NPpos, P, R]),
    check(P >= 0.95, 'extraction precision >= 0.95'),
    check(R >= 0.95, 'extraction recall >= 0.95'),
    findall(N-T, (parsed_line(N, T), T \== none,
                  \+ gold_fact(N, _, _, _)), Halls),
    length(Halls, NH),
    ( NH =:= 0 ->
        check(true, 'hallucination = 0 (no invented triplets)')
    ; format('HALLUCINATIONS: ~w~n', [Halls]),
      check(false, 'hallucination = 0 (no invented triplets)')).

% ---------- conceptos (maquinaria local) ----------
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

% ---------- dos latentes, sin fusion ----------
% NOTA: discover_composition/2 deja solo la ULTIMA regla (retractall
% global del motor, que no tocamos). El runner conserva cada hallazgo
% en found_rule/3: las dos skills conviven a nivel experimento.
keep_rule(Target) :-
    composed_rule(Target, Path, F1),
    assertz(found_rule(Target, Path, F1)).

discover_latent :-
    discover_composition(reaches, 3),
    ( composed_rule(reaches, [visits, in], F1) ->
        ( format('discovered: reaches :- [visits,in] F1=~4f~n', [F1]),
          keep_rule(reaches),
          check(true, 'latent reaches :- [visits,in]') )
    ; check(false, 'latent reaches :- [visits,in]')),
    discover_composition(based, 3),
    ( composed_rule(based, [works_at, located, in], F2) ->
        ( format('discovered: based :- [works_at,located,in] F1=~4f~n', [F2]),
          keep_rule(based),
          check(true, 'latent based 3-hop discovered') )
    ; check(false, 'latent based 3-hop discovered')),
    findall(P, found_rule(_, P, _), Paths),
    sort(Paths, SP),
    check(SP == [[visits, in], [works_at, located, in]],
          'no fusion: exactly two latent paths (per-skill)').

% ---------- queries con seleccion por tarea ----------
answer_visits(P, C, retrieved) :-
    memory_relation(P, visits, C, _, _), !.
answer_visits(_, _, unknown).

answer_owns(P, O, retrieved) :-
    memory_relation(P, owns, O, _, _), !.
answer_owns(_, _, unknown).

answer_reach(P, K, reasoned, Proof) :-
    found_rule(reaches, [visits, in], _),
    memory_relation(P, visits, C, _, _),
    memory_relation(C, in, K, _, _),
    Proof = [rule(reaches, [visits, in]), (P, visits, C), (C, in, K)].

answer_based(P, K, reasoned, Proof) :-
    found_rule(based, [works_at, located, in], _),
    memory_relation(P, works_at, G, _, _),
    memory_relation(G, located, C, _, _),
    memory_relation(C, in, K, _, _),
    Proof = [rule(based, [works_at, located, in]), (P, works_at, G),
             (G, located, C), (C, in, K)].

person_known(X) :-
    memory_relation(X, _, _, _, _), !.
person_known(X) :-
    memory_relation(_, _, X, _, _), !.

run_queries :-
    nl, writeln('===== QUERIES (two tasks, shared persons) ====='),
    forall(expected(Id, Zone, Kind, A, B, How),
           run_query(Id, Zone, Kind, A, B, How)).

run_query(Id, Zone, visits, P, C, retrieved) :-
    ( answer_visits(P, C, retrieved) ->
        check(true, Id-Zone-visits)
    ; format('FAIL q~w ~w visits ~w~n', [Id, P, C]),
      check(false, Id-Zone-visits)).
run_query(Id, Zone, owns, P, O, retrieved) :-
    ( answer_owns(P, O, retrieved) ->
        check(true, Id-Zone-owns)
    ; format('FAIL q~w ~w owns ~w~n', [Id, P, O]),
      check(false, Id-Zone-owns)).
run_query(Id, Zone, reach, P, K, reasoned) :-
    ( answer_reach(P, K, reasoned, Proof) ->
        ( check(true, Id-Zone-reach),
          ( Id =< 2 -> format('proof q~w: ~w~n', [Id, Proof]) ; true ) )
    ; format('FAIL q~w ~w reach ~w~n', [Id, P, K]),
      check(false, Id-Zone-reach)).
run_query(Id, Zone, based, P, K, reasoned) :-
    ( answer_based(P, K, reasoned, Proof) ->
        ( check(true, Id-Zone-based),
          ( Id =< 20, Id >= 19 -> format('proof q~w: ~w~n', [Id, Proof]) ; true ) )
    ; format('FAIL q~w ~w based ~w~n', [Id, P, K]),
      check(false, Id-Zone-based)).
run_query(Id, Zone, unknown_person, Z, _) :-
    ( \+ person_known(Z) ->
        check(true, Id-Zone-unknown)
    ; format('FAIL q~w ~w should be unknown~n', [Id, Z]),
      check(false, Id-Zone-unknown)).

run_distractors :-
    nl, writeln('===== DISTRACTORS (verified false) ====='),
    forall(distractor(P, X, R),
           ( ( R == reach, \+ answer_reach(P, X, _, _) ->
                 check(true, distractor-reject-reach)
             ; R == based, \+ answer_based(P, X, _, _) ->
                 check(true, distractor-reject-based)
             ; R == visits, \+ answer_visits(P, X, retrieved) ->
                 check(true, distractor-reject-visits)
             ; R == owns, \+ answer_owns(P, X, retrieved) ->
                 check(true, distractor-reject-owns)
             ; format('FP ~w ~w ~w~n', [P, R, X]),
               check(false, distractor-fp) ) )).

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
