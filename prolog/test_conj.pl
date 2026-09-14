:- consult('positional.pl').

test_conj :-
    Sentences = [
        'Alice ate the cake and drank the bottle.',
        'Alice followed the white rabbit and found a key.',
        'The Queen played croquet and threatened Alice.',
        'The Hatter hosted a tea party and asked a riddle.',
        'Alice and the Rabbit went to the garden.'
    ],
    maplist([S]>>(
        tokenize_pos(S, T),
        parse_svo_clean(T, SVO),
        format('~w~n  tokens: ~w~n  svo:    ~w~n~n', [S, T, SVO])
    ), Sentences).

:- test_conj, halt.
