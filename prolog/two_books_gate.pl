% two_books_gate.pl — Alice y Plataforma conviven, se distinguen y
% se recargan ya procesados.
% Uso: swipl -s two_books_gate.pl -g two_books_gate -t halt
:- encoding(utf8).
:- consult('chat.pl').

two_books_gate :-
    ( catch(run_two, E,
            ( format('FAIL exception: ~w~n', [E]), halt(1) )) ->
        writeln('TWO BOOKS GATE PASS')
    ; writeln('TWO BOOKS GATE FAIL'), halt(1)
    ).

run_two :-
    bb_load('alice_clean.knowledge.pl'),
    bb_add('plataforma_clean.knowledge.pl'),
    capture("que libro?", OutLib),
    ( sub_string(OutLib, _, _, _, "alice_in_wonderland") -> true
    ; format('FAIL books missing alice: ~w~n', [OutLib]), fail
    ),
    ( sub_string(OutLib, _, _, _, "plataforma") -> true
    ; format('FAIL books missing plataforma: ~w~n', [OutLib]), fail
    ),
    expect("who found key?", "alice found key"),
    expect("quien ama valerie?", "michel ama valerie"),
    expect("appears_in plataforma?", "michel"),
    expect("appears_in alice_in_wonderland?", "alice"),
    expect("que paso con valerie?", "muere"),
    expect("who found key?", "alice"),
    \+ ( capture("who found key?", OutA),
         sub_string(OutA, _, _, _, "michel") ),
    chat_save(two_books_tmp),
    bb_load('two_books_tmp.knowledge.pl'),
    expect("who found key?", "alice found key"),
    expect("quien ama valerie?", "michel ama valerie"),
    expect("que libro?", "plataforma"),
    expect("who ate cake?", "alice eat cake"),
    expect("loves means ama", "Learned"),
    expect("does valerie love michel?", "Yes"),
    expect("who is houellebecq?", "plataforma"),
    capture("who found key?", _),
    capture("did she die?", OutDie),
    \+ sub_string(OutDie, _, _, _, "defied"),
    expect("de que trata plataforma?", "houellebecq"),
    expect("de que trata plataforma?", "michel"),
    expect("quienes son los protagonistas de plataforma?", "valerie"),
    expect("cual me recomiendas?", "plataforma"),
    capture("ok", OutOk),
    \+ sub_string(OutOk, _, _, _, "Learned"),
    \+ sub_string(OutOk, _, _, _, "I don't know"),
    delete_file('two_books_tmp.knowledge.pl').

capture(Line, Out) :-
    with_output_to(string(Out), chat_line(Line)).

expect(Line, Sub) :-
    capture(Line, Out),
    ( sub_string(Out, _, _, _, Sub) -> true
    ; format('FAIL expect ~q in answer to ~q~n  got: ~w~n',
             [Sub, Line, Out]),
      fail
    ).
