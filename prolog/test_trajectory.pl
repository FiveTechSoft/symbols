:- use_module(semantic_field).
:- use_module(parser_v2).

:- dynamic score/2.

test(Text, Query, Expected, QueryType) :-
    ( answer_query(Text, Query, R, _) ->
        ( R = Expected ->
            retract(score(C,T)), NewC is C+1, NewT is T+1, assert(score(NewC,NewT)),
            format('  OK ~w: ~w => ~w~n', [QueryType, Query, R])
        ;
            retract(score(C,T)), NewT is T+1, assert(score(C,NewT)),
            format('  FAIL ~w: ~w => ~w (expected ~w)~n', [QueryType, Query, R, Expected])
        )
    ;
        retract(score(C,T)), NewT is T+1, assert(score(C,NewT)),
        format('  ERROR ~w: ~w~n', [QueryType, Query])
    ),
    !.

run :-
    retractall(score(_,_)), assert(score(0,0)),
    format('=== SPANISH (multi-sentence) ===~n', []),
    test('Juan compro un coche rojo en Madrid. Juan vive en Malaga.', 'donde compro juan el coche', madrid, where_buy),
    test('Juan compro un coche rojo en Madrid. Juan vive en Malaga.', 'donde vive juan', malaga, where_live),
    test('Juan compro un coche rojo en Madrid. Juan vive en Malaga.', 'que compro juan', coche, what),
    test('Juan compro un coche rojo en Madrid. Juan vive en Malaga.', 'quien compro el coche', juan, who),
    test('Juan compro un coche rojo en Madrid. Juan vive en Malaga.', 'de que color es el coche', rojo, color),
    format('=== ENGLISH (multi-sentence) ===~n', []),
    test('John bought a red car in Madrid. John lives in London.', 'where did john buy the car', madrid, en_where_buy),
    test('John bought a red car in Madrid. John lives in London.', 'where does john live', london, en_where_live),
    test('John bought a red car in Madrid. John lives in London.', 'what did john buy', car, en_what),
    test('John bought a red car in Madrid. John lives in London.', 'who bought the car', john, en_who),
    test('John bought a red car in Madrid. John lives in London.', 'what color is the car', red, en_color),
    format('=== ENGLISH 4 (different persons) ===~n', []),
    test('Mary bought a blue house in Paris. Mary lives in Berlin.', 'where did mary buy the house', paris, en_where_buy2),
    test('Mary bought a blue house in Paris. Mary lives in Berlin.', 'where does mary live', berlin, en_where_live2),
    test('Mary bought a blue house in Paris. Mary lives in Berlin.', 'what did mary buy', house, en_what2),
    test('Mary bought a blue house in Paris. Mary lives in Berlin.', 'who bought the house', mary, en_who2),
    test('Mary bought a blue house in Paris. Mary lives in Berlin.', 'what color is the house', blue, en_color2),
    test('Peter bought a green book in London. Peter lives in Madrid.', 'where did peter buy the book', london, en_where_buy3),
    test('Peter bought a green book in London. Peter lives in Madrid.', 'where does peter live', madrid, en_where_live3),
    test('Peter bought a green book in London. Peter lives in Madrid.', 'what did peter buy', book, en_what3),
    test('Peter bought a green book in London. Peter lives in Madrid.', 'who bought the book', peter, en_who3),
    test('Peter bought a green book in London. Peter lives in Madrid.', 'what color is the book', green, en_color3),
    test('Anne bought a yellow phone in Berlin. Anne lives in Paris.', 'where did anne buy the phone', berlin, en_where_buy4),
    test('Anne bought a yellow phone in Berlin. Anne lives in Paris.', 'where does anne live', paris, en_where_live4),
    test('Anne bought a yellow phone in Berlin. Anne lives in Paris.', 'what did anne buy', phone, en_what4),
    test('Anne bought a yellow phone in Berlin. Anne lives in Paris.', 'who bought the phone', anne, en_who4),
    test('Anne bought a yellow phone in Berlin. Anne lives in Paris.', 'what color is the phone', yellow, en_color4),
    score(C,T),
    format('~n=== FINAL SCORE: ~w/~w ===~n', [C,T]).

:- initialization(run).
