% holdout_1000.pl — P4 benchmark: 1000 hold-out NL queries through answer_query.
% Usage: swipl -s holdout_1000.pl -g run_holdout -t halt
% Data: holdout_1000_data.pl (test_data(N, Lang, QType, Text, Query, Expected)).
% Output: holdout_1000_results.txt, one line per test: N|Lang|QType|Expected|Got|Status
%   Status = pass | fail | error (exception or no result).
:- use_module(semantic_field).
:- consult('holdout_1000_data.pl').

:- use_module(library(lists)).
:- dynamic res/6.

run_holdout :-
    retractall(res(_, _, _, _, _, _)),
    forall(test_data(N, Lang, QT, Text, Query, Expected),
           run_one(N, Lang, QT, Text, Query, Expected)),
    write_results_file,
    print_summary.

run_one(N, Lang, QT, Text, Query, Expected) :-
    ( catch(answer_query(Text, Query, Got, _), _, fail) ->
        ( Got == Expected ->
            assertz(res(N, Lang, QT, Expected, Got, pass))
        ; assertz(res(N, Lang, QT, Expected, Got, fail))
        )
    ; assertz(res(N, Lang, QT, Expected, error, error))
    ),
    ( N mod 100 =:= 0 -> format('~w ', [N]) ; true ).

write_results_file :-
    open('holdout_1000_results.txt', write, S),
    forall(res(N, Lang, QT, Exp, Got, St),
           format(S, '~w|~w|~w|~w|~w|~w~n', [N, Lang, QT, Exp, Got, St])),
    close(S).

print_summary :-
    nl,
    findall(1, res(_, _, _, _, _, pass), Ps), length(Ps, NP),
    findall(1, res(_, _, _, _, _, fail), Fs), length(Fs, NF),
    findall(1, res(_, _, _, _, _, error), Es), length(Es, NE),
    Total is NP + NF + NE,
    format('TOTAL: ~w  PASS: ~w  FAIL: ~w  ERROR: ~w~n', [Total, NP, NF, NE]),
    format('~nBy language:~n', []),
    forall(member(L, [es, en]),
           ( findall(1, res(_, L, _, _, _, pass), LP), length(LP, NLP),
             findall(1, res(_, L, _, _, _, _), LT), length(LT, NLT),
             format('  ~w: ~w/~w~n', [L, NLP, NLT])
           )),
    format('~nBy query type:~n', []),
    forall(member(T, [what, who, where, when, color]),
           ( findall(1, res(_, _, T, _, _, pass), TP), length(TP, NTP),
             findall(1, res(_, _, T, _, _, _), TT), length(TT, NTT),
             format('  ~w: ~w/~w~n', [T, NTP, NTT])
           )),
    format('~nBy language x type (failures+errors):~n', []),
    forall((member(L, [es, en]), member(T, [what, who, where, when, color])),
           ( findall(N, (res(N, L, T, _, _, St), St \== pass), Bad),
             length(Bad, NB),
             ( NB > 0 ->
                 findall(Exp-Got, res(_, L, T, Exp, Got, fail), Fails),
                 sort(Fails, UFail),
                 format('  ~w/~w: ~w bad e.g. ~w~n', [L, T, NB, UFail])
             ; true
             )
           )).
