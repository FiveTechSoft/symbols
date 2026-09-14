% benchmark_alice.pl — A/B test on alice.knowledge.pl (2901 facts)
% Proper triples like: alice hear rattle, grass rustled alice

:- consult('chat.pl').
:- consult('chat_attention.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('dialog_ref.pl').
:- bb_import('alice.knowledge.pl').

:- dynamic test_result/4.

run_benchmark :-
    dialog_reset,
    retractall(test_result(_,_,_,_)),
    write('========================================'), nl,
    write('  BENCHMARK: alice.knowledge.pl (2901 facts)'), nl,
    write('========================================'), nl, nl,
    Tests = [
        % Questions based on actual KB facts
        [[who, hear, rattle], alice, 'who hear rattle'],
        [[who, rustled], grass, 'who rustled'],
        [[who, sneezing], baby, 'who sneezing'],
        [[who, crashed], dishes, 'who crashed'],
        [[who, suppressed], choking, 'who suppressed'],
        [[who, believed], half, 'who believed'],
        [[who, pictured], wonderland, 'who pictured'],
        [[who, kissed], sister, 'who kissed'],
        [[who, clasped], hands, 'who clasped'],
        [[who, looking], eyes, 'who looking'],
        [[who, dreamed], alice, 'who dreamed'],
        [[who, repeated], white_rabbit, 'who repeated'],
        [[who, opened], narrator, 'who opened'],
        [[who, unfolded], king, 'who unfolded'],
        [[who, imitated], king, 'who imitated'],
        [[who, brightened], jury, 'who brightened'],
        [[who, turned], pale, 'who turned'],
        [[who, pushed], matter, 'who pushed'],
        [[who, returned], king, 'who returned'],
        [[who, shouted], queen, 'who shouted']
    ],
    run_category(baseline, Tests),
    run_category(attention_t1, Tests),
    run_category(attention_t03, Tests),
    run_category(attention_t01, Tests),
    summary,
    halt.

run_category(Category, Tests) :-
    configure_attention(Category),
    foldl(run_single(Category), Tests, _, _),
    true.

configure_attention(baseline) :-
    retractall(attention_temperature(_)),
    assertz(attention_temperature(1.0)),
    retractall(attention_top_k(_)),
    assertz(attention_top_k(5)),
    retractall(attention_min_score(_)),
    assertz(attention_min_score(0.0)).
configure_attention(attention_t1) :-
    retractall(attention_temperature(_)),
    assertz(attention_temperature(1.0)),
    retractall(attention_top_k(_)),
    assertz(attention_top_k(5)),
    retractall(attention_min_score(_)),
    assertz(attention_min_score(0.0)).
configure_attention(attention_t03) :-
    retractall(attention_temperature(_)),
    assertz(attention_temperature(0.3)),
    retractall(attention_top_k(_)),
    assertz(attention_top_k(5)),
    retractall(attention_min_score(_)),
    assertz(attention_min_score(0.0)).
configure_attention(attention_t01) :-
    retractall(attention_temperature(_)),
    assertz(attention_temperature(0.1)),
    retractall(attention_top_k(_)),
    assertz(attention_top_k(3)),
    retractall(attention_min_score(_)),
    assertz(attention_min_score(0.0)).

run_single(Category, Test, AccIn, AccOut) :-
    Test = [Query, Expected, Desc],
    ( chat_form(Query, _, answer([Ans|_], _)) ->
        true
    ; predict_answer(Query, Ans, _) ->
        true
    ;
        Ans = unknown
    ),
    ( Category \= baseline ->
        chat_attention_focus(Query, focus(_, Rels, _)),
        length(Rels, RelsExamined)
    ;
        RelsExamined = 0
    ),
    ( Ans == Expected ->
        assertz(test_result(Category, Query, ok, RelsExamined))
    ;
        assertz(test_result(Category, Query, fail(Expected, Ans), RelsExamined))
    ),
    AccOut = AccIn.

summary :-
    write('========================================'), nl,
    write('  RESULTS'), nl,
    write('========================================'), nl, nl,
    summary_category(baseline, 'BASELINE (no attention)'),
    summary_category(attention_t1, 'ATTENTION T=1.0 K=5'),
    summary_category(attention_t03, 'ATTENTION T=0.3 K=5'),
    summary_category(attention_t01, 'ATTENTION T=0.1 K=3'),
    nl,
    write('========================================'), nl,
    write('  COMPARISON'), nl,
    write('========================================'), nl,
    summary_comparison,
    nl,
    write('  DETAILED FAILURES'), nl,
    write('----------------------------------------'), nl,
    summary_failures,
    write('========================================'), nl.

summary_category(Cat, Label) :-
    findall(Q, test_result(Cat, Q, ok, _), OKs),
    findall(Q-R, test_result(Cat, Q, fail(_, _), R), Fails),
    length(OKs, OK),
    length(Fails, F),
    Total is OK + F,
    ( Total > 0 ->
        Pct is OK/Total*100,
        format('  ~w: ~w/~w (~1f%)~n', [Label, OK, Total, Pct])
    ;
        format('  ~w: no tests~n', [Label])
    ),
    findall(R, test_result(Cat, _, _, R), AllRels),
    ( AllRels \= [] ->
        sumlist(AllRels, SumRels),
        length(AllRels, LenRels),
        Avg is SumRels / LenRels,
        format('    Avg relations examined: ~1f~n', [Avg])
    ;
        true
    ).

summary_comparison :-
    findall(Q, test_result(baseline, Q, ok, _), B),
    length(B, BOK),
    findall(Q, test_result(attention_t1, Q, ok, _), A1),
    length(A1, A1OK),
    findall(Q, test_result(attention_t03, Q, ok, _), A03),
    length(A03, A03OK),
    findall(Q, test_result(attention_t01, Q, ok, _), A01),
    length(A01, A01OK),
    format('  Baseline:      ~w/20~n', [BOK]),
    format('  Attention T=1:  ~w/20~n', [A1OK]),
    format('  Attention T=0.3: ~w/20~n', [A03OK]),
    format('  Attention T=0.1: ~w/20~n', [A01OK]).

summary_failures :-
    test_result(baseline, Q, fail(Expected, Got), _),
    format('  BASELINE FAIL: ~w => ~w (expected ~w)~n', [Q, Got, Expected]),
    fail.
summary_failures :-
    test_result(attention_t1, Q, fail(Expected, Got), Rels),
    format('  ATT T=1 FAIL: ~w => ~w (expected ~w) [~w rels]~n', [Q, Got, Expected, Rels]),
    fail.
summary_failures :-
    test_result(attention_t03, Q, fail(Expected, Got), Rels),
    format('  ATT T=0.3 FAIL: ~w => ~w (expected ~w) [~w rels]~n', [Q, Got, Expected, Rels]),
    fail.
summary_failures :-
    test_result(attention_t01, Q, fail(Expected, Got), Rels),
    format('  ATT T=0.1 FAIL: ~w => ~w (expected ~w) [~w rels]~n', [Q, Got, Expected, Rels]),
    fail.
summary_failures.
