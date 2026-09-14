:- use_module(semantic_field).
:- use_module(parser_v2).
:- use_module(library(lists)).

:- dynamic failed/3.

run_composition :-
    retractall(failed(_,_,_)),
    Text = "mary bought a blue house in madrid yesterday. she has a red car in barcelona.",
    % Test 1: What did Mary buy?
    run_test(Text, "what did mary buy", house),
    % Test 2: What color was it?
    run_test(Text, "what color was it", blue),
    % Test 3: Where did she buy it?
    run_test(Text, "where did she buy it", madrid),
    % Test 4: When did she buy it?
    run_test(Text, "when did she buy it", yesterday),
    % Test 5: What does she have?
    run_test(Text, "what does she have", car),
    % Test 6: Where is the car?
    run_test(Text, "where is the car", barcelona),
    % Test 7: What color is the car?
    run_test(Text, "what color is the car", red),
    % Report
    nl,
    findall(f(Q,E,G), failed(Q,E,G), FailedList),
    length(FailedList, FL),
    Pass is 7 - FL,
    format('COMPOSITION SCORE: ~w/7~n', [Pass]),
    ( FL > 0 ->
        format('~nFailed:~n', []),
        forall(member(f(Q,Exp,Got), FailedList),
            format('  ~w => Exp:~w Got:~w~n', [Q,Exp,Got]))
    ; format('~n*** ALL PASSED ***~n', []) ).

run_test(Text, Query, Expected) :-
    ( catch(answer_query(Text, Query, Result, _), _, fail) ->
        ( Result == Expected ->
            format('  OK ~w => ~w~n', [Query, Result])
        ; format('  FAIL ~w => ~w (expected ~w)~n', [Query, Result, Expected]),
            assertz(failed(Query, Expected, Result))
        )
    ; format('  ERROR ~w (no result)~n', [Query]),
        assertz(failed(Query, Expected, error))
    ).

:- initialization(run_composition).
