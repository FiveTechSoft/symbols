% benchmark_auto.pl — A/B test on auto-extracted KB (2348 facts)
% Measures: accuracy, relations examined, noise filtering

:- consult('chat.pl').
:- consult('chat_attention.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('dialog_ref.pl').
:- consult('alice_auto.knowledge.pl').

:- dynamic test_result/4.

run_benchmark :-
    dialog_reset,
    retractall(test_result(_,_,_,_)),
    write('========================================'), nl,
write('  BENCHMARK: AUTO-EXTRACTED KB (2348 facts)'), nl,
    write('========================================'), nl, nl,
    Tests = [
        % These should be answerable from auto-extracted KB
        [[who, fell, down, rabbit, hole], alice, 'who fell down rabbit hole'],
        [[who, found, little, bottle], alice, 'who found little bottle'],
        [[who, grew, tall], alice, 'who grew tall'],
        [[who, cried, tears], alice, 'who cried tears'],
        [[who, met, white, rabbit], alice, 'who met white rabbit'],
        [[who, ate, cake], alice, 'who ate cake'],
        [[who, played, croquet], alice, 'who played croquet'],
        [[who, met, hatter], alice, 'who met hatter'],
        [[who, met, caterpillar], alice, 'who met caterpillar'],
        [[who, met, queen], alice, 'who met queen'],
        [[who, ran, away], alice, 'who ran away'],
        [[who, walked, sadly], alice, 'who walked sadly'],
        [[who, dried, eyes], alice, 'who dried eyes'],
        [[who, came, near], rabbit, 'who came near'],
        [[who, sat, still], sister, 'who sat still'],
        [[who, began, dreaming], sister, 'who began dreaming'],
        [[who, rustled], grass, 'who rustled'],
        [[who, splashed], mouse, 'who splashed'],
        [[who, sneezed], baby, 'who sneezed'],
        [[who, crashed], plates, 'who crashed']
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
    write('  RESULTS (AUTO-EXTRACTED KB)'), nl,
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
    format('  Attention T=0.1: ~w/20~n', [A01OK]),
    ( A03OK > BOK ->
        format('  >> T=0.3 IMPROVES by ~w~n', [A03OK-BOK])
    ; A03OK == BOK ->
        format('  >> T=0.3 TIE~n')
    ;
        format('  >> T=0.3 REGRESSES by ~w~n', [BOK-A03OK])
    ).

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
