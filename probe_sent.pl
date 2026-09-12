% probe_sent.pl — parsea una frase dada con LastEnt controlado.
:- consult('gen_parse.pl').

probe_sent :-
    Censo = [alice, footman, door],
    probe_one("It was opened by another footman in livery", Censo, alice),
    probe_one("At this moment the door of the house opened", Censo, alice).

probe_one(Str, Censo, LE) :-
    format('=== ~w~n', [Str]),
    gen_tokenize(Str, Lower, Raw),
    format('lower=~w~n', [Lower]),
    findall(I, gen_is_verb(Lower, Raw, I), VIs),
    format('verbs=~w~n', [VIs]),
    gen_np_runs(Lower, Raw, Runs),
    format('runs=~w~n', [Runs]),
    gen_frames(Lower, Raw, Censo, LE, Frames, St, Ments),
    format('frames=~w~n~w ~w~n', [Frames, St, Ments]).
