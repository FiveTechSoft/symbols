% experiment20.pl
% EXPERIMENT 20 - CONFLICT, TIME & REVIEW
% Confirmacion (refuerzo), contradiccion (contested, UNKNOWN),
% tiempo (el ultimo gana), fuentes en conflicto (pending).
% Reglas temporales GENERALES (ninguna por persona):
%   ahora / antes / en-ANO (pasado interpola, futuro -> UNKNOWN).
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('conflict.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.
:- dynamic sentence_counter/1.

experiment20 :-
    reset_experiment,
    phase_background,
    phase_confirm,
    phase_contradiction,
    phase_time,
    phase_visits,
    phase_sources,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)),
    retractall(check_results(_, _)),
    retractall(sentence_counter(_)),
    assertz(sentence_counter(0)).

next_sentence_id(Src) :-
    ( retract(sentence_counter(N)) -> true ; N = 0 ),
    N1 is N + 1,
    assertz(sentence_counter(N1)),
    atomic_list_concat([sentence_, N1], Src).

feed(S) :-
    next_sentence_id(Src),
    ( symbolize_timed(S, (A, V, O), usage, Time) ->
        remember_tracked(A, V, O, Src, Time)
    ; format('PARSE FAIL: ~w~n', [S]), fail
    ).

% ---------- reglas temporales generales (ninguna por persona) ----------
where_now(X, P) :-
    current_belief(X, lives_in, P).

where_before(X, P) :-
    prov(X, lives_in, P, info(_, T, superseded)),
    number(T),
    \+ ( prov(X, lives_in, _, info(_, T2, superseded)),
         number(T2), T2 > T
       ).

% where_in: pasado interpola (ultimo T =< Year); futuro -> falla (UNKNOWN).
where_in(X, Year, P) :-
    integer(Year),
    findall(T, ( prov(X, lives_in, _, info(_, T, _)),
                 number(T)
               ),
            Ts),
    Ts \== [],
    max_list(Ts, MaxT),
    Year =< MaxT,
    findall(T-P2, ( prov(X, lives_in, P2, info(_, T, _)),
                    number(T), T =< Year
                  ),
            Cands),
    Cands \== [],
    keysort(Cands, Sorted),
    reverse(Sorted, [_-P|_]).

query_now(X, P) :- where_now(X, P), !.
query_now(_, unknown).
query_before(X, P) :- where_before(X, P), !.
query_before(_, unknown).
query_in(X, Year, P) :- where_in(X, Year, P), !.
query_in(_, _, unknown).

% ---------- fases ----------
phase_background :-
    nl, writeln('===== BACKGROUND ====='),
    feed("Juan lives in Roma"),
    feed("Ana lives in Lima").

phase_confirm :-
    nl, writeln('===== 1. CONFIRMATION ====='),
    feed("Maria lives in Madrid"),
    feed("Maria lives in Madrid"),
    check(( memory_relation(maria, lives_in, madrid, _, Uses),
            Uses >= 2
          ),
          'madrid reinforced (uses>=2)'),
    check(query_now(maria, madrid), 'belief: madrid').

phase_contradiction :-
    nl, writeln('===== 2. CONTRADICTION (held) ====='),
    feed("Maria lives in Paris"),
    check(prov(maria, lives_in, madrid, info(_, _, contested)),
          'madrid contested'),
    check(prov(maria, lives_in, paris, info(_, _, contested)),
          'paris contested'),
    check(query_now(maria, unknown),
          'query now -> UNKNOWN (conflict pending, never random)'),
    check(prov_log(conflict, _),
          'conflict logged').

phase_time :-
    nl, writeln('===== 3. TIME resolves ====='),
    feed("Maria lived in Madrid in 2020"),
    feed("Maria lives in Paris in 2026"),
    resolve_time_conflicts,
    check(prov(maria, lives_in, paris, info(_, 2026, active)),
          'paris active @2026'),
    check(prov(maria, lives_in, madrid, info(_, 2020, superseded)),
          'madrid superseded @2020'),
    check(query_now(maria, paris), 'now -> paris'),
    check(query_before(maria, madrid), 'before -> madrid'),
    check(query_in(maria, 2020, madrid), 'in 2020 -> madrid'),
    check(query_in(maria, 2026, paris), 'in 2026 -> paris'),
    check(query_in(maria, 2030, unknown), 'in 2030 -> UNKNOWN (no future)'),
    check(( why_belief(maria, lives_in, paris, Why),
            member(source(sentence_5), Why),
            member(time_evidence([((maria, lives_in, paris),
                                   sentence_7, 2026)]), Why),
            member(superseded([(maria, lives_in, madrid)]), Why)
          ),
          'why paris: claim source + time evidence + superseded madrid').

phase_visits :-
    nl, writeln('===== visits: no conflict (undecided relation) ====='),
    feed("Maria visits Madrid"),
    feed("Maria visits Barcelona"),
    check(prov(maria, visits, madrid, info(_, _, active)),
          'visits madrid active'),
    check(prov(maria, visits, barcelona, info(_, _, active)),
          'visits barcelona active (no false conflict)').

phase_sources :-
    nl, writeln('===== 4. CONFLICTING SOURCES (pending) ====='),
    feed("Pedro lives in Oslo"),
    feed("Pedro lives in Bergen"),
    check(prov_log(conflict, ((pedro, lives_in, oslo, sentence_10),
                              (pedro, lives_in, bergen, sentence_11))),
          'conflict holds both sources'),
    check(query_now(pedro, unknown),
          'query pedro -> UNKNOWN (pending, never arbitrary)').

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
