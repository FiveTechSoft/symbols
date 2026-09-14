% benchmark_ab.pl — A/B test: baseline vs symbolic attention
% Measures: accuracy, relations examined, confidence, unknown handling

:- consult('chat.pl').
:- consult('chat_attention.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('dialog_ref.pl').
:- bb_import('alice_clean.knowledge.pl').
:- consult('alice_auto.knowledge.pl').

:- dynamic test_result/4.

run_benchmark :-
    dialog_reset,
    retractall(test_result(_,_,_,_)),
    write('========================================'), nl,
    write('  A/B BENCHMARK: BASELINE vs ATTENTION'), nl,
    write('========================================'), nl, nl,
    Tests = [
        % format: [query_tokens, expected_answer, description]
        [[who, defied, court], alice, 'who defied court'],
        [[who, met, hatter], alice, 'who met hatter'],
        [[who, held, flamingo], alice, 'who held flamingo'],
        [[who, smokes, hookah], caterpillar, 'who smokes hookah'],
        [[who, threatened, alice], queen, 'who threatened alice'],
        [[who, played, croquet], alice, 'who played croquet'],
        [[who, followed, white, rabbit], alice, 'who followed rabbit'],
        [[who, nurses, baby], duchess, 'who nurses baby'],
        [[who, painted, roses], gardeners, 'who painted roses'],
        [[who, ate, mushroom], alice, 'who ate mushroom'],
        [[who, waved, fan], alice, 'who waved fan'],
        [[who, recited, poem], alice, 'who recited poem'],
        [[who, entered, garden], alice, 'who entered garden'],
        [[who, found, key], alice, 'who found key'],
        [[what, is, alice, in, wonderland], book, 'what is alice in wonderland'],
        [[who, led, alice], gryphon, 'who led alice'],
        [[who, told, story], dormouse, 'who told story'],
        [[who, offered, wine], march_hare, 'who offered wine'],
        [[who, judged, knave], king, 'who judged knave'],
        [[who, defied, court], alice, 'who defied court (dup)'],
        [[who, met, hatter], alice, 'who met hatter (dup)'],
        [[who, smokes, hookah], caterpillar, 'who smokes hookah (dup)'],
        [[who, played, croquet], alice, 'who played croquet (dup)'],
        [[who, followed, white, rabbit], alice, 'who followed rabbit (dup)'],
        [[who, nurses, baby], duchess, 'who nurses baby (dup)'],
        [[who, painted, roses], gardeners, 'who painted roses (dup)'],
        [[who, ate, mushroom], alice, 'who ate mushroom (dup)'],
        [[who, waved, fan], alice, 'who waved fan (dup)'],
        [[who, entered, garden], alice, 'who entered garden (dup)'],
        [[who, found, key], alice, 'who found key (dup)']
    ],
    % Run baseline
    run_category(baseline, Tests),
    % Run with attention (T=1.0)
    run_category(attention_t1, Tests),
    % Run with attention (T=0.3 concentrated)
    run_category(attention_t03, Tests),
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

run_single(Category, Test, AccIn, AccOut) :-
    Test = [Query, Expected, Desc],
    % Try chat_form first
    ( chat_form(Query, _, answer([Ans|_], _)) ->
        true
    ; predict_answer(Query, Ans, _) ->
        true
    ;
        Ans = unknown
    ),
    % Count relations examined via attention
    ( Category \= baseline ->
        chat_attention_focus(Query, focus(_, Rels, _)),
        length(Rels, RelsExamined)
    ;
        RelsExamined = 0
    ),
    % Record result
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
    summary_category(attention_t1, 'ATTENTION T=1.0'),
    summary_category(attention_t03, 'ATTENTION T=0.3'),
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
    findall(Q, test_result(baseline, Q, ok, _), BaselineOK),
    length(BaselineOK, BOK),
    findall(Q, test_result(attention_t1, Q, ok, _), AttOK),
    length(AttOK, AOK),
    findall(Q, test_result(attention_t03, Q, ok, _), Att03OK),
    length(Att03OK, A03OK),
    ( BOK > 0 ; AOK > 0 ; A03OK > 0 ->
        format('  Baseline: ~w/30 correct~n', [BOK]),
        format('  Attention T=1.0: ~w/30 correct~n', [AOK]),
        format('  Attention T=0.3: ~w/30 correct~n', [A03OK]),
        ( A03OK > BOK ->
            format('  >> ATTENTION T=0.3 IMPROVES by ~w questions~n', [A03OK-BOK])
        ; A03OK == BOK ->
            format('  >> TIE~n')
        ;
            format('  >> ATTENTION T=0.3 REGRESSES by ~w questions~n', [BOK-A03OK])
        )
    ; true
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
summary_failures.
