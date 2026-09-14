:- use_module(parser_v2).

run :-
    format("~n========================================~n", []),
    format("  COMPARISON: PARSER vs ATTENTION~n", []),
    format("========================================~n~n", []),

    % Test cases: sentence + query + expected top relation type
    Tests = [
        test(1, "Juan compro un coche rojo en Madrid ayer",
             "donde compro juan el coche", location),
        test(2, "Maria vive en Barcelona",
             "donde vive maria", main),
        test(3, "Pedro tiene una casa grande en Malaga",
             "que tiene pedro", main),
        test(4, "Ana compro un libro azul para Maria",
             "para quien compro ana el libro", indirect),
        test(5, "Juan no vive en Madrid",
             "donde vive juan", negation),
        test(6, "Maria visito Madrid en 2025",
             "cuando visito maria madrid", temporal),
        test(7, "Pedro compro un coche y lo llevo a Barcelona",
             "donde llevo pedro el coche", location),
        test(8, "Ana tiene una casa. La casa esta en Marbella.",
             "donde esta la casa de ana", location),
        test(9, "Juan trabaja en Madrid desde 2020",
             "cuando empezo juan a trabajar en madrid", temporal),
        test(10, "Maria compro un coche negro, pero Pedro compro una bicicleta roja",
             "que compro maria y pedro", main)
    ],

    compare_all(Tests, 0, 0, 0, 0),
    halt().

compare_all([], P, A, PA, AA) :-
    nl,
    format("~n========================================~n", []),
    format("  RESULTS~n", []),
    format("========================================~n~n", []),
    format("Parser (first relation):     ~w/~w correct (~1f%)~n", [P, 10, P*10.0]),
    format("Attention (top-1):           ~w/~w correct (~1f%)~n", [A, 10, A*10.0]),
    format("Parser (has answer):         ~w/~w (~1f%)~n", [PA, 10, PA*10.0]),
    format("Attention (has answer):      ~w/~w (~1f%)~n", [AA, 10, AA*10.0]),
    nl,
    ( A > P ->
        format("VERDICT: Attention improves over parser by ~w cases~n", [A-P])
    ; A == P ->
        format("VERDICT: Attention equals parser~n", [])
    ; format("VERDICT: Parser better (attention has regression)~n", [])
    ).

compare_all([test(N, Text, Query, ExpectedType)|Rest], P0, A0, PA0, AA0) :-
    parse_sentence(Text, Rels),
    parse_and_attend(Text, Query, Selected, _All),

    % Parser: first relation type
    Rels = [relation(FirstType, _, _)|_],

    % Attention: top-1 type
    Selected = [scored(relation(AttType, _, _), AttScore, _)|_],

    % Check if answer exists anywhere
    ( member(relation(ExpectedType, _, _), Rels) -> PA1 = 1 ; PA1 = 0 ),
    ( member(scored(relation(ExpectedType, _, _), _, _), Selected) -> AA1 = 1 ; AA1 = 0 ),

    % Check if first/top matches
    ( FirstType == ExpectedType -> P1 = 1 ; P1 = 0 ),
    ( AttType == ExpectedType -> A1 = 1 ; A1 = 0 ),

    P2 is P0 + P1,
    A2 is A0 + A1,
    PA2 is PA0 + PA1,
    AA2 is AA0 + AA1,

    % Print detail
    ( P1 == 1, A1 == 1 -> Icon = "="
    ; P1 == 0, A1 == 1 -> Icon = "+"
    ; P1 == 1, A1 == 0 -> Icon = "-"
    ; Icon = " "
    ),

    format("~w ~w. \"~w\"~n", [Icon, N, Text]),
    format("   Query: \"~w\"  Expected: ~w~n", [Query, ExpectedType]),
    format("   Parser first:  ~w (score=---)~n", [FirstType]),
    format("   Attention top: ~w (score=~3f)~n", [AttType, AttScore]),
    nl,

    compare_all(Rest, P2, A2, PA2, AA2).

:- initialization(run).
