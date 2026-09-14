:- consult('positional.pl').

test_full :-
    % Clean sentences
    Tests = [
        'Alice followed the white rabbit down the hole.',
        'The white rabbit dropped his fan.',
        'Alice ate the cake and drank the bottle.',
        'The Cheshire Cat has a grin.',
        'Alice met the Caterpillar on a mushroom.',
        'The Cat left only its grin.',
        'The baby turned into a pig.',
        'Alice found a little golden key.',
        'The gardeners painted the roses red.',
        'Alice and the Rabbit went to the garden.'
    ],
    maplist([S]>>(
        tokenize_pos(S, T),
        parse_svo_clean(T, SVO),
        format('~w~n  -> ~w~n~n', [S, SVO])
    ), Tests),

    % Real Alice lines (first few chapters)
    format('=== Real Alice lines ===~n'),
    RealTests = [
        'down the rabbit hole',
        'alice began to get very tired',
        'the rabbit hole went straight on like a tunnel',
        'she fell past it',
        'the white rabbit ran past her',
        'she found herself in a long hallway',
        'the door was locked',
        'she found a little golden key',
        'the key fit a tiny door',
        'she drank from a bottle labeled orange marmalade',
        'she ate a cake labeled eat me',
        'the cat grinned from ear to ear',
        'the hatter was having tea with the march hare',
        'the dormouse was sleeping',
        'the queen of hearts shouted off with their heads',
        'the gardeners were painting the roses red',
        'alice grew very tall',
        'the baby turned into a pig',
        'the mock turtle sang a song',
        'the gryphon led alice to the mock turtle'
    ],
    maplist([S]>>(
        tokenize_pos(S, T),
        parse_svo_clean(T, SVO),
        format('~w~n  -> ~w~n~n', [S, SVO])
    ), RealTests).

:- test_full, halt.
