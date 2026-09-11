% experiment18.pl
% EXPERIMENT 18 - COREFERENCE (abduccion de identidad linguistica)
% Menciones pronominales -> nodos provisionales -> SAME/UNKNOWN con traza.
% Nunca recencia, nunca azar. Reconstruccion del grafo + reintento.
% Contador global: he_1, she_2, he_3, she_4, he_5, he_6.
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('coreference.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.

experiment18 :-
    reset_experiment,
    phase_s1_single,
    phase_s4_novel,
    phase_s2_gender_pair,
    phase_s3_ambiguous,
    phase_s5_eliminated,
    final_audits,
    report_checks.

reset_experiment :-
    clear_memory,
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

% ---------- S1: un solo candidato ----------
phase_s1_single :-
    nl, writeln('===== S1: single candidate ====='),
    feed("Juan lives in Madrid"),
    feed("Juan visits Madrid"),
    feed("He visits Barcelona"),
    feed("Juan eats bread"),
    resolve_mention(he_1),
    check(verdict(he_1, same(juan), _), 'he_1 == juan SAME'),
    check(memory_relation(juan, visits, barcelona, _, _),
          'reconstructed: juan visits barcelona'),
    check(node_consumed(he_1), 'he_1 node consumed').

% ---------- S4: sin candidato (recien llegado) ----------
phase_s4_novel :-
    nl, writeln('===== S4: no candidate (newcomer) ====='),
    feed("She visits Sevilla"),
    resolve_mention(she_2),
    check(verdict(she_2, unknown, _), 'she_2 UNKNOWN (no candidate yet)'),
    check(memory_relation(she_2, visits, sevilla, _, _),
          'she_2 node preserved').

% ---------- S2: par de genero + reintento ----------
phase_s2_gender_pair :-
    nl, writeln('===== S2: gender pair + retry ====='),
    feed("Maria lives in Barcelona"),
    retry_unresolved,
    check(verdict(she_2, same(maria), _),
          'she_2 == maria SAME on retry (accumulated evidence)'),
    check(memory_relation(maria, visits, sevilla, _, _),
          'reconstructed: maria visits sevilla'),
    feed("He visits Sevilla"),
    resolve_mention(he_3),
    check(verdict(he_3, same(juan), _), 'he_3 == juan SAME (maria vetoed)'),
    feed("She visits Sevilla"),
    resolve_mention(she_4),
    check(verdict(she_4, same(maria), _), 'she_4 == maria SAME').

% ---------- S3: ambiguo real ----------
phase_s3_ambiguous :-
    nl, writeln('===== S3: genuine ambiguity ====='),
    feed("Pedro lives in Oslo"),
    feed("He visits Sevilla"),
    resolve_mention(he_5),
    check(verdict(he_5, unknown, R),
          'he_5 UNKNOWN (never random)'),
    check(( member(ambiguous(Survs), R),
            sort(Survs, [juan, pedro])
          ),
          'ambiguous exactly {juan,pedro}'),
    check(memory_relation(he_5, visits, sevilla, _, _),
          'he_5 node preserved'),
    check(( \+ same_as_link(he_5, _),
            \+ same_as_link(_, he_5)
          ),
          'no same_as stored for he_5').

% ---------- S5: eliminacion por funcionalidad ----------
phase_s5_eliminated :-
    nl, writeln('===== S5: elimination via functionality ====='),
    show_functionality,
    feed("He lives in Sevilla"),
    resolve_mention(he_6),
    check(verdict(he_6, unknown, R),
          'he_6 UNKNOWN (all candidates contradicted)'),
    check(( member(vetoed(juan, functional(lives_in)), R),
            member(vetoed(pedro, functional(lives_in)), R)
          ),
          'trace: 2 functional vetoes (lives_in)').

% ---------- auditorias finales ----------
final_audits :-
    nl, writeln('===== FINAL AUDITS ====='),
    expected_memory(Expected),
    findall((S, V, O), memory_relation(S, V, O, _, _), M0),
    sort(M0, M),
    sort(Expected, E),
    ( M == E ->
        format('memory exact: 10 triples, reconstruction local OK~n', []),
        assertz(check_results('memory exact (10 triples)', pass))
    ; format('MEMORY MISMATCH: ~w~n', [M]),
      assertz(check_results('memory exact (9 triples)', fail))
    ),
    findall((A, B), ( same_as_link(A, B) ; same_as_link(B, A) ), L0),
    sort(L0, Links),
    ExpectedLinks = [(he_1, juan), (he_3, juan), (juan, he_1),
                     (juan, he_3), (maria, she_2), (maria, she_4),
                     (she_2, maria), (she_4, maria)],
    sort(ExpectedLinks, EL),
    ( Links == EL ->
        format('same_as links exact (4 unordered) OK~n', []),
        assertz(check_results('same_as links exact', pass))
    ; format('LINKS MISMATCH: ~w~n', [Links]),
      assertz(check_results('same_as links exact', fail))
    ).

expected_memory([
    (juan, lives_in, madrid),
    (juan, visits, madrid),
    (juan, visits, barcelona),
    (juan, eats, bread),
    (juan, visits, sevilla),
    (maria, lives_in, barcelona),
    (maria, visits, sevilla),
    (pedro, lives_in, oslo),
    (he_5, visits, sevilla),
    (he_6, lives_in, sevilla)
]).

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
