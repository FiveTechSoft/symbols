:- consult('relation_extractor.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('chat.pl').
:- consult('multi_head.pl').
:- consult('dialog_ref.pl').

:- bb_import('alice.knowledge.pl').

test_all :-
    dialog_reset,
    write('========================================'), nl,
    write('  EXTRACTOR v2 TEST'), nl,
    write('========================================'), nl, nl,

    test('alice met hatter'),
    test('alice followed white rabbit'),
    test('who met hatter'),
    test('alice met hatter and drank tea'),
    test('queen kissed alice'),
    test('queen shouted off with heads'),
    test('alice fell down'),

    nl,
    write('========================================'), nl,
    write('  FULL PIPELINE BENCHMARK'), nl,
    write('========================================'), nl, nl,

    bench('alice met hatter'),
    bench('alice followed white rabbit'),
    bench('queen kissed alice'),
    bench('hatter drank tea'),
    bench('alice fell down'),
    bench('who met hatter'),
    bench('alice played croquet'),
    bench('rabbit wore coat'),
    bench('alice grew tall'),
    bench('alice ate mushroom'),

    halt().

test(S) :-
    tokenize_pos(S, Tokens),
    extract_candidates(Tokens, Candidates),
    length(Candidates, L),
    format('  "~w": ~w candidates~n', [S, L]),
    ( member(Cand, Candidates),
      write('    '), write(Cand), nl,
      fail ; true
    ).

bench(S) :-
    tokenize_pos(S, Tokens),
    process_sentence(Tokens, R, stats(NC, NT, NV, NN, Time)),
    R = results(Candidates, TopK, Verified),
    format('  "~w":~n', [S]),
    format('    Candidates: ~w, Top-K: ~w, New: ~w, Time: ~wms~n', [NC, NT, NN, Time]),
    ( member(scored_candidate(CS, CV, CO, Score, heads(Rel, Ent, Pos, Disc, Tem, Nov)), TopK),
      format('    [~2f] ~w ~w ~w  (R:~2f E:~2f P:~2f D:~2f T:~2f N:~2f)~n',
             [Score, CS, CV, CO, Rel, Ent, Pos, Disc, Tem, Nov]),
      fail ; true
    ).

:- initialization(test_all).
