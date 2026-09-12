% probe_gen.pl — caso minimo "she tried".
:- consult('gen_parse.pl').

probe_gen :-
    probe_sent("she tried"),
    probe_sent("Alice opened the door").

probe_sent(Str) :-
    format('=== ~w~n', [Str]),
    gen_tokenize(Str, Lower, Raw),
    format('lower=~w raw=~w~n', [Lower, Raw]),
    findall(I, gen_is_verb(Lower, Raw, I), VIs),
    format('verbs=~w~n', [VIs]),
    gen_np_runs(Lower, Raw, Runs),
    format('runs=~w~n', [Runs]),
    gen_frames(Lower, Raw, [alice], none, Frames, St, Ments),
    format('frames=~w st=~w ments=~w~n', [Frames, St, Ments]),
    ( Lower = [alice|_] ->
        gen_np_runs(Lower, Raw, R2),
        gen_side(R2, Lower, Raw, [alice], 1, before, none, S, _, _),
        format('side-b=~w~n', [S]),
        gen_side(R2, Lower, Raw, [alice], 1, after, none, O, _, _),
        format('side-a=~w~n', [O]),
        gen_np_class(Raw, Lower, [0], [alice], Cl),
        format('class0=~w~n', [Cl])
    ; true
    ).
