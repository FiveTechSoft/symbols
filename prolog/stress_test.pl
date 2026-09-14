:- use_module(parser_v2).

run :-
    format("~n========================================~n", []),
    format("  STRESS TEST: 100 QUERIES~n", []),
    format("========================================~n~n", []),

    findall(test(N,S,Q,E), test_data(N,S,Q,E), Tests),
    run_tests(Tests, [], FailList),
    length(Tests, Total),
    length(FailList, Failures),
    Pass is Total - Failures,
    PassRate is Pass * 100 / Total,

    nl,
    format("~n========================================~n", []),
    format("  FINAL RESULTS~n", []),
    format("========================================~n~n", []),
    format("Total:   ~w~n", [Total]),
    format("Passed:  ~w (~1f%)~n", [Pass, PassRate]),
    format("Failed:  ~w~n", [Failures]),
    ( Failures > 0 ->
        format("~nFailed cases:~n", []),
        forall(member(fail(N,S,Q,Exp,Got), FailList),
            format("  #~w: \"~w\"~n    Query: \"~w\"  Expected: ~w  Got: ~w~n", [N,S,Q,Exp,Got]))
    ; true ),
    ( PassRate >= 95.0 ->
        format("~n*** TARGET MET: >=95% ***~n", [])
    ; format("~n*** TARGET NOT MET: <95% ***~n", [])
    ),
    halt().

run_tests([], F, F).
run_tests([test(N,S,Q,E)|Rest], F0, FFinal) :-
    parse_and_attend(S, Q, Selected, _All),
    Selected = [scored(relation(TopType,_,_), TopScore, _)|_],
    ( TopType == E ->
        F1 = F0
    ; F1 = [fail(N,S,Q,E,TopType)|F0]
    ),
    run_tests(Rest, F1, FFinal).


% S1: Juan compro un coche rojo en Madrid ayer
test_data(101, "Juan compro un coche rojo en Madrid ayer", "que compro juan", main).
test_data(102, "Juan compro un coche rojo en Madrid ayer", "que compro juan ayer", main).
test_data(103, "Juan compro un coche rojo en Madrid ayer", "donde compro juan el coche", location).
test_data(104, "Juan compro un coche rojo en Madrid ayer", "cuando compro juan el coche", temporal).
test_data(105, "Juan compro un coche rojo en Madrid ayer", "de que color es el coche", attribute).
test_data(106, "Juan compro un coche rojo en Madrid ayer", "que coche compro juan", main).
test_data(107, "Juan compro un coche rojo en Madrid ayer", "quien compro un coche", main).
test_data(108, "Juan compro un coche rojo en Madrid ayer", "juan compro algo", main).
test_data(109, "Juan compro un coche rojo en Madrid ayer", "el coche es rojo", attribute).
test_data(110, "Juan compro un coche rojo en Madrid ayer", "que coche tiene juan", main).

% S2: Maria vive en Barcelona
test_data(201, "Maria vive en Barcelona", "donde vive maria", main).
test_data(202, "Maria vive en Barcelona", "que hace maria", main).
test_data(203, "Maria vive en Barcelona", "vive maria en madrid", main).
test_data(204, "Maria vive en Barcelona", "quien vive en barcelona", main).
test_data(205, "Maria vive en Barcelona", "maria vive en barcelona", main).
test_data(206, "Maria vive en Barcelona", "donde esta maria", main).
test_data(207, "Maria vive en Barcelona", "en que ciudad vive maria", main).
test_data(208, "Maria vive en Barcelona", "maria no vive en madrid", main).
test_data(209, "Maria vive en Barcelona", "vive maria en barcelona", main).
test_data(210, "Maria vive en Barcelona", "que ciudad es barcelona", main).

% S3: Pedro tiene una casa grande en Malaga
test_data(301, "Pedro tiene una casa grande en Malaga", "que tiene pedro", main).
test_data(302, "Pedro tiene una casa grande en Malaga", "donde esta la casa", location).
test_data(303, "Pedro tiene una casa grande en Malaga", "que tipo de casa tiene pedro", attribute).
test_data(304, "Pedro tiene una casa grande en Malaga", "quien tiene una casa", main).
test_data(305, "Pedro tiene una casa grande en Malaga", "pedro tiene una casa en malaga", main).
test_data(306, "Pedro tiene una casa grande en Malaga", "donde tiene pedro una casa", location).
test_data(307, "Pedro tiene una casa grande en Malaga", "la casa es grande", attribute).
test_data(308, "Pedro tiene una casa grande en Malaga", "en que ciudad esta la casa", location).
test_data(309, "Pedro tiene una casa grande en Malaga", "que size es la casa", attribute).
test_data(310, "Pedro tiene una casa grande en Malaga", "pedro no tiene casa", main).

% S4: Ana compro un libro azul para Maria
test_data(401, "Ana compro un libro azul para Maria", "que compro ana", main).
test_data(402, "Ana compro un libro azul para Maria", "para quien compro ana", indirect).
test_data(403, "Ana compro un libro azul para Maria", "de que color es el libro", attribute).
test_data(404, "Ana compro un libro azul para Maria", "quien compro el libro", indirect).
test_data(405, "Ana compro un libro azul para Maria", "ana compro un libro para maria", indirect).
test_data(406, "Ana compro un libro azul para Maria", "que libro compro ana", main).
test_data(407, "Ana compro un libro azul para Maria", "el libro es azul", attribute).
test_data(408, "Ana compro un libro azul para Maria", "ana no compro nada", main).
test_data(409, "Ana compro un libro azul para Maria", "quien recibio el libro", indirect).
test_data(410, "Ana compro un libro azul para Maria", "para谁compro ana", main).

% S5: Juan no vive en Madrid
test_data(501, "Juan no vive en Madrid", "donde vive juan", negation).
test_data(502, "Juan no vive en Madrid", "juan vive en madrid", negation).
test_data(503, "Juan no vive en Madrid", "no vive juan en madrid", negation).
test_data(504, "Juan no vive en Madrid", "que pasa con juan", negation).
test_data(505, "Juan no vive en Madrid", "juan no esta en madrid", negation).
test_data(506, "Juan no vive en Madrid", "vive juan en madrid", negation).
test_data(507, "Juan no vive en Madrid", "donde no vive juan", negation).
test_data(508, "Juan no vive en Madrid", "juan vive en barcelona", negation).
test_data(509, "Juan no vive en Madrid", "madrid no es de juan", negation).
test_data(510, "Juan no vive en Madrid", "que ciudad no es de juan", negation).

% S6: Maria visito Madrid en 2025
test_data(601, "Maria visito Madrid en 2025", "que hizo maria", main).
test_data(602, "Maria visito Madrid en 2025", "donde visito maria", main).
test_data(603, "Maria visito Madrid en 2025", "cuando visito maria madrid", temporal).
test_data(604, "Maria visito Madrid en 2025", "quien visito madrid", main).
test_data(605, "Maria visito Madrid en 2025", "maria visito madrid en 2025", main).
test_data(606, "Maria visito Madrid en 2025", "en que ano visito maria madrid", temporal).
test_data(607, "Maria visito Madrid en 2025", "que ciudad visito maria", main).
test_data(608, "Maria visito Madrid en 2025", "maria no visito madrid", main).
test_data(609, "Maria visito Madrid en 2025", "cuando fue maria a madrid", temporal).
test_data(610, "Maria visito Madrid en 2025", "visitó maria madrid", main).

% S7: Pedro compro un coche y lo llevo a Barcelona
test_data(701, "Pedro compro un coche y lo llevo a Barcelona", "que hizo pedro", main).
test_data(702, "Pedro compro un coche y lo llevo a Barcelona", "donde llevo pedro el coche", location).
test_data(703, "Pedro compro un coche y lo llevo a Barcelona", "que compro pedro", main).
test_data(704, "Pedro compro un coche y lo llevo a Barcelona", "pedro compro un coche", main).
test_data(705, "Pedro compro un coche y lo llevo a Barcelona", "donde esta el coche de pedro", location).
test_data(706, "Pedro compro un coche y lo llevo a Barcelona", "quien compro un coche", main).
test_data(707, "Pedro compro un coche y lo llevo a Barcelona", "pedro llevo el coche a barcelona", main).
test_data(708, "Pedro compro un coche y lo llevo a Barcelona", "que coche compro pedro", main).
test_data(709, "Pedro compro un coche y lo llevo a Barcelona", "pedro no compro nada", main).
test_data(710, "Pedro compro un coche y lo llevo a Barcelona", "a donde llevo pedro el coche", location).

% S8: Ana tiene una casa. La casa esta en Marbella.
test_data(801, "Ana tiene una casa. La casa esta en Marbella.", "que tiene ana", main).
test_data(802, "Ana tiene una casa. La casa esta en Marbella.", "donde esta la casa de ana", location).
test_data(803, "Ana tiene una casa. La casa esta en Marbella.", "ana tiene una casa en marbella", main).
test_data(804, "Ana tiene una casa. La casa esta en Marbella.", "quien tiene casa en marbella", main).
test_data(805, "Ana tiene una casa. La casa esta en Marbella.", "en que ciudad esta la casa", location).
test_data(806, "Ana tiene una casa. La casa esta en Marbella.", "la casa de ana esta en marbella", location).
test_data(807, "Ana tiene una casa. La casa esta en Marbella.", "donde vive ana", location).
test_data(808, "Ana tiene una casa. La casa esta en Marbella.", "ana no tiene casa", main).
test_data(809, "Ana tiene una casa. La casa esta en Marbella.", "que casa tiene ana", main).
test_data(810, "Ana tiene una casa. La casa esta en Marbella.", "marbella es donde esta la casa", location).

% S9: Juan trabaja en Madrid desde 2020
test_data(901, "Juan trabaja en Madrid desde 2020", "que hace juan", main).
test_data(902, "Juan trabaja en Madrid desde 2020", "donde trabaja juan", main).
test_data(903, "Juan trabaja en Madrid desde 2020", "cuando empezo juan a trabajar en madrid", temporal).
test_data(904, "Juan trabaja en Madrid desde 2020", "quien trabaja en madrid", main).
test_data(905, "Juan trabaja en Madrid desde 2020", "juan trabaja en madrid desde 2020", temporal).
test_data(906, "Juan trabaja en Madrid desde 2020", "desde cuando trabaja juan en madrid", temporal).
test_data(907, "Juan trabaja en Madrid desde 2020", "en que ciudad trabaja juan", main).
test_data(908, "Juan trabaja en Madrid desde 2020", "juan no trabaja en madrid", main).
test_data(909, "Juan trabaja en Madrid desde 2020", "cuando empezo juan en madrid", temporal).
test_data(910, "Juan trabaja en Madrid desde 2020", "juan trabaja desde 2020", temporal).

% S10: Maria compro un coche negro, pero Pedro compro una bicicleta roja
test_data(1001, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "que compro maria", main).
test_data(1002, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "que compro pedro", main).
test_data(1003, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "de que color es el coche", attribute).
test_data(1004, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "de que color es la bicicleta", attribute).
test_data(1005, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "quien compro un coche", main).
test_data(1006, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "quien compro una bicicleta", main).
test_data(1007, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "maria compro algo", main).
test_data(1008, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "pedro compro algo", main).
test_data(1009, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "el coche es negro", attribute).
test_data(1010, "Maria compro un coche negro, pero Pedro compro una bicicleta roja", "la bicicleta es roja", attribute).

:- initialization(run).
