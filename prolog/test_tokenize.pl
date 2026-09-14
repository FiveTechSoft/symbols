:- consult('positional.pl').

test_tokenization :-
    Sentences = [
        'Alice followed the white rabbit down the hole.',
        'The white rabbit dropped his fan.',
        'Alice ate the cake and drank the bottle.',
        'The Cheshire Cat has a grin.',
        'She picked up the fan and gloves.',
        'The Queen plays croquet with flamingos.',
        'The Hatter hosted a tea party.',
        'The caterpillar smoked a hookah.',
        'The gardeners painted the roses red.',
        'Alice found a little golden key.',
        'The baby turned into a pig.',
        'The Dormouse told a story.',
        'The March Hare offered wine.',
        'The Duchess nurses a baby.',
        'Alice met the Caterpillar on a mushroom.',
        'The Cat left only its grin.'
    ],
    maplist([S]>>(
        tokenize_pos(S, T),
        parse_svo_clean(T, SVO),
        format('~w~n  tokens: ~w~n  svo:    ~w~n~n', [S, T, SVO])
    ), Sentences).

:- test_tokenization, halt.
