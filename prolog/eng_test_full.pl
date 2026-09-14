:- use_module(parser_v2).

:- dynamic failed/5.

run_all :-
    retractall(failed(_,_,_,_,_)),
    run_tests_seq(1),
    nl,
    findall(f(Ni,Si,Qi,Ei,Gi), failed(Ni,Si,Qi,Ei,Gi), FailedList),
    length(FailedList, FL),
    Pass is 100 - FL,
    Rate is Pass * 100 / 100,
    format('PASSED: ~w/100 (~1f%)~n', [Pass, Rate]),
    ( FL > 0 ->
        format('~nFailed:~n', []),
        forall(member(f(Idx,S,Q,Exp,Got), FailedList),
            format('  #~w: ~w / ~w  Exp:~w Got:~w~n', [Idx,S,Q,Exp,Got]))
    ; true ),
    ( Rate >= 95.0 -> format('~n*** TARGET MET ***~n', []) ; format('~n*** TARGET NOT MET ***~n', []) ).

run_tests_seq(N) :- N > 100, !.
run_tests_seq(N) :-
    ( test_data(N, S, Q, E) ->
        ( catch(parse_and_attend(S, Q, [scored(relation(TopType,_,_), _, _)|_], _), _, fail) ->
            ( TopType == E ->
                format('~w ', [N])
            ; format('X~w ', [N]),
                assertz(failed(N,S,Q,E,TopType))
            )
        ; format('?~w ', [N]),
            assertz(failed(N,S,Q,E,parse_error))
        )
    ; true ),
    N1 is N + 1,
    run_tests_seq(N1).

test_data(1, "John bought a red car in Madrid yesterday", "what did john buy", main).
test_data(2, "John bought a red car in Madrid yesterday", "where is the car", location).
test_data(3, "John bought a red car in Madrid yesterday", "what color is the car", attribute).
test_data(4, "John bought a red car in Madrid yesterday", "who made the purchase", main).
test_data(5, "John bought a red car in Madrid yesterday", "when was the purchase", temporal).
test_data(6, "John bought a red car in Madrid yesterday", "what shade is the vehicle", attribute).
test_data(7, "John bought a red car in Madrid yesterday", "john purchased something", main).
test_data(8, "John bought a red car in Madrid yesterday", "where did john buy it", location).
test_data(9, "John bought a red car in Madrid yesterday", "when did john buy the car", temporal).
test_data(10, "John bought a red car in Madrid yesterday", "what car did john buy", main).
test_data(11, "Mary lives in Paris", "where does mary live", location).
test_data(12, "Mary lives in Paris", "mary is in paris", main).
test_data(13, "Mary lives in Paris", "in which city does mary live", location).
test_data(14, "Mary lives in Paris", "who lives in france", main).
test_data(15, "Mary lives in Paris", "mary is not in london", main).
test_data(16, "Mary lives in Paris", "where is mary based", location).
test_data(17, "Mary lives in Paris", "mary has a home in paris", main).
test_data(18, "Mary lives in Paris", "what city is paris for mary", main).
test_data(19, "Mary lives in Paris", "does mary live abroad", main).
test_data(20, "Mary lives in Paris", "where did mary settle", location).
test_data(21, "Peter has a big house in London", "what does peter own", main).
test_data(22, "Peter has a big house in London", "where is the property", location).
test_data(23, "Peter has a big house in London", "what are the dimensions of the house", attribute).
test_data(24, "Peter has a big house in London", "who owns the house", main).
test_data(25, "Peter has a big house in London", "peter has a dwelling in london", main).
test_data(26, "Peter has a big house in London", "in which locality is the house", location).
test_data(27, "Peter has a big house in London", "the house is spacious", attribute).
test_data(28, "Peter has a big house in London", "peter does not have an apartment", main).
test_data(29, "Peter has a big house in London", "what type of property does peter have", attribute).
test_data(30, "Peter has a big house in London", "where is peter's house", location).
test_data(31, "Anne bought a blue book for Mary", "what did anne buy for mary", indirect).
test_data(32, "Anne bought a blue book for Mary", "for whom was the book", indirect).
test_data(33, "Anne bought a blue book for Mary", "what color is the book", attribute).
test_data(34, "Anne bought a blue book for Mary", "who bought the book for mary", indirect).
test_data(35, "Anne bought a blue book for Mary", "anne acquired a blue book", attribute).
test_data(36, "Anne bought a blue book for Mary", "the book is blue in tone", attribute).
test_data(37, "Anne bought a blue book for Mary", "who received the book", indirect).
test_data(38, "Anne bought a blue book for Mary", "what purchase did anne make", main).
test_data(39, "Anne bought a blue book for Mary", "for whom was the book intended", indirect).
test_data(40, "Anne bought a blue book for Mary", "anne bought something for mary", indirect).
test_data(41, "John does not live in London", "where does john live", negation).
test_data(42, "John does not live in London", "john is in london", negation).
test_data(43, "John does not live in London", "john does not reside in london", negation).
test_data(44, "John does not live in London", "john is absent from london", negation).
test_data(45, "John does not live in London", "in which place is john", negation).
test_data(46, "John does not live in London", "john does not dwell in london", negation).
test_data(47, "John does not live in London", "london is not his home", negation).
test_data(48, "John does not live in London", "john lives outside london", negation).
test_data(49, "John does not live in London", "where is john really", negation).
test_data(50, "John does not live in London", "john is not in the capital", negation).
test_data(51, "Mary visited Paris in 2025", "what destination did mary visit", main).
test_data(52, "Mary visited Paris in 2025", "when was the trip", temporal).
test_data(53, "Mary visited Paris in 2025", "where was mary", main).
test_data(54, "Mary visited Paris in 2025", "who visited the capital", main).
test_data(55, "Mary visited Paris in 2025", "in what period did mary visit", temporal).
test_data(56, "Mary visited Paris in 2025", "mary made a trip to paris", main).
test_data(57, "Mary visited Paris in 2025", "what year was the visit", temporal).
test_data(58, "Mary visited Paris in 2025", "mary did not visit london", main).
test_data(59, "Mary visited Paris in 2025", "when did mary travel", temporal).
test_data(60, "Mary visited Paris in 2025", "what city did mary visit", main).
test_data(61, "Peter bought a car and took it to Barcelona", "what did peter do with the car", main).
test_data(62, "Peter bought a car and took it to Barcelona", "where did peter take the car", location).
test_data(63, "Peter bought a car and took it to Barcelona", "what did peter buy first", main).
test_data(64, "Peter bought a car and took it to Barcelona", "where did the car go", location).
test_data(65, "Peter bought a car and took it to Barcelona", "peter acquired a vehicle", main).
test_data(66, "Peter bought a car and took it to Barcelona", "the car is in barcelona", location).
test_data(67, "Peter bought a car and took it to Barcelona", "who bought the car", main).
test_data(68, "Peter bought a car and took it to Barcelona", "peter took something to barcelona", main).
test_data(69, "Peter bought a car and took it to Barcelona", "where did the car stop", location).
test_data(70, "Peter bought a car and took it to Barcelona", "what car did peter buy", main).
test_data(71, "Anne has a house. The house is in Marbella.", "what property does anne have", main).
test_data(72, "Anne has a house. The house is in Marbella.", "where is anne's house", location).
test_data(73, "Anne has a house. The house is in Marbella.", "anne owns a dwelling", main).
test_data(74, "Anne has a house. The house is in Marbella.", "in which city is the property", location).
test_data(75, "Anne has a house. The house is in Marbella.", "who has a house in marbella", main).
test_data(76, "Anne has a house. The house is in Marbella.", "the house is on the coast", location).
test_data(77, "Anne has a house. The house is in Marbella.", "where is the house located", location).
test_data(78, "Anne has a house. The house is in Marbella.", "anne does not have an apartment", main).
test_data(79, "Anne has a house. The house is in Marbella.", "what house does anne have", main).
test_data(80, "Anne has a house. The house is in Marbella.", "marbella is its location", location).
test_data(81, "John works in London since 2020", "where does john work", main).
test_data(82, "John works in London since 2020", "when did john start", temporal).
test_data(83, "John works in London since 2020", "what does john do in london", main).
test_data(84, "John works in London since 2020", "since when does john work", temporal).
test_data(85, "John works in London since 2020", "in which city does john work", main).
test_data(86, "John works in London since 2020", "john has worked since 2020", temporal).
test_data(87, "John works in London since 2020", "who works in london", main).
test_data(88, "John works in London since 2020", "john does not work in paris", main).
test_data(89, "John works in London since 2020", "when did his employment begin", temporal).
test_data(90, "John works in London since 2020", "where is his job", main).
test_data(91, "Mary bought a black car, but Peter bought a red bicycle", "what did mary buy", main).
test_data(92, "Mary bought a black car, but Peter bought a red bicycle", "what did peter buy", main).
test_data(93, "Mary bought a black car, but Peter bought a red bicycle", "what shade is the bicycle", attribute).
test_data(94, "Mary bought a black car, but Peter bought a red bicycle", "who bought the car", main).
test_data(95, "Mary bought a black car, but Peter bought a red bicycle", "the car is dark in color", attribute).
test_data(96, "Mary bought a black car, but Peter bought a red bicycle", "mary acquired a vehicle", main).
test_data(97, "Mary bought a black car, but Peter bought a red bicycle", "peter bought something red", attribute).
test_data(98, "Mary bought a black car, but Peter bought a red bicycle", "what color is the bicycle", attribute).
test_data(99, "Mary bought a black car, but Peter bought a red bicycle", "mary bought a black car", attribute).
test_data(100, "Mary bought a black car, but Peter bought a red bicycle", "who bought a bicycle", main).
