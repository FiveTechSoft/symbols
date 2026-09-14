:- consult('relation_extractor.pl').
:- consult('stemmer.pl').
:- consult('positional.pl').
:- consult('chat.pl').
:- consult('multi_head.pl').
:- consult('dialog_ref.pl').

:- bb_import('alice_clean.knowledge.pl').

test_pipeline :-
    dialog_reset,
    write('========================================'), nl,
    write('  FULL PIPELINE TEST'), nl,
    write('========================================'), nl, nl,

    % Test 1
    write('--- Test 1: alice met hatter ---'), nl,
    process_sentence([alice, met, hatter], R1, S1),
    write('  R: '), write(R1), nl,
    write('  S: '), write(S1), nl, nl,

    % Test 2
    write('--- Test 2: alice followed white rabbit ---'), nl,
    process_sentence([alice, followed, white, rabbit], R2, S2),
    write('  R: '), write(R2), nl,
    write('  S: '), write(S2), nl, nl,

    % Test 3
    write('--- Test 3: alice ate mushroom ---'), nl,
    process_sentence([alice, ate, mushroom], R3, S3),
    write('  R: '), write(R3), nl,
    write('  S: '), write(S3), nl, nl,

    halt().

:- initialization(test_pipeline).
