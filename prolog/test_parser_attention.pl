:- use_module(parser_attention).

run :-
    Text = "Juan compro un coche rojo en Madrid ayer",

    format("~n================================~n", []),
    format("FRASE: ~w~n", [Text]),
    format("================================~n", []),

    parse_sentence(Text, Relations),

    format("~nRELACIONES DETECTADAS:~n", []),
    print_relations(Relations),

    parse_and_attend(Text, Selected, All),

    format("~nATENCION:~n", []),
    print_scored(All),

    format("~nTOP-K:~n", []),
    print_scored(Selected).


print_relations([]).

print_relations([H|T]) :-
    format("  ~w~n", [H]),
    print_relations(T).


print_scored([]).

print_scored([
    scored(Relation, Score, Heads)|T
]) :-
    format("~n  ~w~n", [Relation]),
    format("    score    = ~2f~n", [Score]),
    format("    heads    = ~w~n", [Heads]),
    print_scored(T).
