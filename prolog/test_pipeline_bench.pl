:- consult('relation_extractor.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('chat.pl').
:- consult('multi_head.pl').
:- consult('dialog_ref.pl').

:- bb_import('alice.knowledge.pl').

benchmark :-
    dialog_reset,
    write('========================================'), nl,
    write('  MULTI-HEAD PIPELINE BENCHMARK'), nl,
    write('  KB: alice.knowledge.pl (2901 facts)'), nl,
    write('========================================'), nl, nl,

    Sentences = [
        [alice, met, hatter],
        [alice, followed, white, rabbit],
        [alice, ate, mushroom],
        [queen, kissed, alice],
        [alice, played, croquet],
        [hatter, drank, tea],
        [alice, grew, tall],
        [rabbit, wore, coat],
        [queen, shouted, off, with, heads],
        [alice, fell, down]
    ],

    process_all(Sentences, 0, 0, 0, 0, 0),

    halt().

process_all([], NC, NT, NV, NN, TotalTime) :-
    nl,
    write('========================================'), nl,
    write('  SUMMARY'), nl,
    write('========================================'), nl,
    format('  Total sentences: 10~n'),
    format('  Total candidates generated: ~w~n', [NC]),
    format('  Total top-K selected: ~w~n', [NT]),
    format('  Total verified (new): ~w~n', [NV]),
    format('  Total new facts stored: ~w~n', [NN]),
    Avg is TotalTime / 10,
    format('  Avg time per sentence: ~w ms~n', [Avg]),
    !.

process_all([S|Rest], NC0, NT0, NV0, NN0, TT0) :-
    process_sentence(S, R, stats(NC, NT, NV, NN, Time)),
    R = results(Candidates, TopK, Verified),
    format('  Input: ~w~n', [S]),
    format('  Candidates: ~w, Top-K: ~w, Verified: ~w, New: ~w, Time: ~wms~n', [NC, NT, NV, NN, Time]),
    ( member(scored_candidate(ScoreS, ScoreV, ScoreO, Score, _), TopK),
      format('    [~2f] ~w ~w ~w~n', [Score, ScoreS, ScoreV, ScoreO]),
      fail ; true
    ), nl,
    NC1 is NC0 + NC,
    NT1 is NT0 + NT,
    NV1 is NV0 + NV,
    NN1 is NN0 + NN,
    TT1 is TT0 + Time,
    process_all(Rest, NC1, NT1, NV1, NN1, TT1).

:- initialization(benchmark).
