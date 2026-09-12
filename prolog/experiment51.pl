% experiment51.pl
% EXPERIMENT 51 - SCALING BENCHMARK (misma memoria, mismo motor)
% Niveles acumulativos de ontologias supply contadas (0/10/50/200/500)
% sobre el mundo EXP43: la memoria SOLO crece. Por nivel se mide:
% hechos, conceptos, skills, ms de descubrimiento, ms de consulta,
% transferencia (nueva ontologia), UNKNOWN y cordura del corpus.
% Sin umbrales: benchmark honesto. Ningun modulo tocado.
:- consult('conversation.pl').
:- consult('experiment46.pl').
:- consult('experiment50.pl').

:- use_module(library(lists)).

:- dynamic scale_row/7.

benchmark51 :-
    load_demo,
    nl, writeln('===== BENCHMARK 51 (scaling, cumulative) ====='),
    scale_level(l0, 0),
    scale_level(l1, 10),
    scale_level(l2, 50),
    scale_level(l3, 200),
    scale_level(l4, 500),
    report_scale,
    report_checks.

% ---------- niveles: cuenta ontologias NUEVAS hasta el total ----------
scale_level(Name, Total) :-
    tell_ontologies_until(Total),
    statistics(walltime, _),
    discover_concepts,
    statistics(walltime, [_, MsAbs]),
    statistics(walltime, _),
    discover_composition(reaches, 3),
    statistics(walltime, [_, MsDisc]),
    facts_now(Facts),
    concepts_now(Concepts),
    skills_now(Skills),
    probe_queries(MsQ),
    ( newest_ontology(P, O, C) ->
        ( learn_map50([P, O, C]),
          ( ask_stored50(O, C, yes(_)) ->
              check(true, Name-transfer)
          ; format('FAIL ~w transfer ~w ~w~n', [Name, O, C]),
            check(false, Name-transfer)) )
    ; check(true, Name-transfer-vacuous) ),
    ( ask_stored50(zzz, nowhere, unknown) ->
        check(true, Name-unknown)
    ; format('FAIL ~w unknown~n', [Name]),
      check(false, Name-unknown)),
    ( ask("Does alba reach norway?", yes(_)) ->
        check(true, Name-sanity)
    ; format('FAIL ~w sanity~n', [Name]),
      check(false, Name-sanity)),
    assertz(scale_row(Name, Facts, Concepts, Skills, MsAbs, MsDisc, MsQ)),
    format('row ~w facts=~w concepts=~w skills=~w abs=~wms disc=~wms q=~wms~n',
           [Name, Facts, Concepts, Skills, MsAbs, MsDisc, MsQ]).

% cuenta ontologias pN/oN/cN hasta Total (monotono: nunca borra).
tell_ontologies_until(Total) :-
    told_ontologies(N0),
    ( N0 >= Total -> true
    ; N1 is N0 + 1,
      tell_ontology(N1),
      tell_ontologies_until(Total)
    ).

told_ontologies(N) :-
    findall(I, told_ontology_mark(I), Is),
    sort(Is, S),
    length(S, N).

:- dynamic told_ontology_mark/1.

tell_ontology(I) :-
    atomic_list_concat([p, I], P),
    atomic_list_concat([o, I], O),
    Cities = [oslo, roma, lima, paris, madrid, dublin, bern, lisboa,
              atenea, quito],
    length(Cities, NC),
    Idx is (I mod NC) + 1,
    nth1(Idx, Cities, C),
    remember_relation(P, keeps, O, 1.0),
    remember_relation(O, held_by, P, 1.0),
    remember_relation(P, tours, C, 1.0),
    assertz(told_ontology_mark(I)).

newest_ontology(P, O, C) :-
    told_ontologies(N),
    N > 0,
    tell_ontology_lookup(N, P, O, C).
newest_ontology(_, _, _) :-
    told_ontologies(0),
    fail.

tell_ontology_lookup(I, P, O, C) :-
    atomic_list_concat([p, I], P),
    atomic_list_concat([o, I], O),
    Cities = [oslo, roma, lima, paris, madrid, dublin, bern, lisboa,
              atenea, quito],
    length(Cities, NC),
    Idx is (I mod NC) + 1,
    nth1(Idx, Cities, C).

% ---------- metricas ----------
facts_now(N) :-
    findall(1, memory_relation(_, _, _, _, _), Fs),
    length(Fs, N).

concepts_now(N) :-
    findall(C, concept(C, _, _), Cs),
    length(Cs, N).

skills_now(N) :-
    findall(1, composed_rule(_, _, _), R1),
    findall(1, constrained_rule(_, _, _), R2),
    findall(1, found_rule(_, _, _), R3),
    length(R1, A), length(R2, B), length(R3, C),
    N is A + B + C.

% bateria fija x20 repeticiones (ms de consulta).
probe_queries(Ms) :-
    statistics(walltime, _),
    forall(between(1, 20, _),
           ( ask("Does alba reach norway?", _),
             ask("Does quinn reach ecuador?", _),
             ask("Does zorin visit madrid?", _) )),
    statistics(walltime, [_, Ms]).

% ---------- informe ----------
report_scale :-
    nl, writeln('===== SCALE TABLE ====='),
    writeln('level facts concepts skills abs_ms disc_ms query20_ms'),
    forall(scale_row(N, F, C, S, A, D, Q),
           format('~w ~w ~w ~w ~w ~w ~w~n', [N, F, C, S, A, D, Q])).

% ask/2, check/2, report_checks/0 de conversation.pl;
% ask_stored50/3, learn_map50/1 de experiment50.pl (mapa ya inducido
% en e1 y reutilizado: la transferencia no reinduce).
