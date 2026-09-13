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
    expect("quien es michel?", "Esto se de michel"),
    expect("y quien es valerie?", "Esto se de valerie"),
    expect("de que trata el libro?", "houellebecq"),
    expect("que lecciones enseña el libro?", "plataforma"),
    capture("en que se parece un libro al otro?", OutCmp),
    ( sub_string(OutCmp, _, _, _, "author") -> true
    ; format('FAIL compare: ~w~n', [OutCmp]), fail
    ),
    \+ sub_string(OutCmp, _, _, _, "I don't know"),
    expect("wrote means author", "Learned"),
    capture("who wrote plataforma?", OutW),
    ( sub_string(OutW, _, _, _, "houellebecq") -> true
    ; format('FAIL wrote: ~w~n', [OutW]), fail
    ),
    \+ sub_string(OutW, _, _, _, "appears in"),
    capture("did michel ama valerie and valerie ama michel?", OutAnd),
    ( sub_string(OutAnd, _, _, _, "Yes") -> true
    ; format('FAIL conj-and: ~w~n', [OutAnd]), fail
    ),
    ( sub_string(OutAnd, _, _, _, "valerie ama michel") -> true
    ; format('FAIL conj missing 2nd: ~w~n', [OutAnd]), fail
    ),
    expect("what about the baby that turned pig?", "turned"),
    expect("is michel in plataforma?", "appears"),
    capture("is alice in plataforma?", OutIn),
    ( sub_string(OutIn, _, _, _, "No") -> true
    ; format('FAIL alice-in-plat: ~w~n', [OutIn]), fail
    ),
    expect("how many books?", "2"),
    capture("did alice never eat cake?", OutNev),
    ( sub_string(OutNev, _, _, _, "No") -> true
    ; format('FAIL never: ~w~n', [OutNev]), fail
    ),
    expect("did alice or michel find the key?", "alice"),
    capture("de que trata plataforma?", _),
    capture("hablame del otro libro", OutOtr),
    ( sub_string(OutOtr, _, _, _, "alice_in_wonderland") -> true
    ; format('FAIL otro libro: ~w~n', [OutOtr]), fail
    ),
    capture("who besides alice met the hatter?", OutBes),
    ( sub_string(OutBes, _, _, _, "No one else") -> true
    ; format('FAIL besides: ~w~n', [OutBes]), fail
    ),
    capture("did alice eat cake or find the key?", OutOrVp),
    ( sub_string(OutOrVp, _, _, _, "Yes") -> true
    ; format('FAIL or-vp: ~w~n', [OutOrVp]), fail
    ),
    capture("is alice not in plataforma?", OutNot),
    ( sub_string(OutNot, _, _, _, "Yes") -> true
    ; format('FAIL not-in: ~w~n', [OutNot]), fail
    ),
    expect("where is plataforma set?", "set in"),
    capture("list the characters in plataforma", OutList),
    \+ sub_string(OutList, _, _, _, "Learned"),
    ( sub_string(OutList, _, _, _, "michel") -> true
    ; format('FAIL list chars: ~w~n', [OutList]), fail
    ),
    expect("did only alice find the key?", "Yes"),
    capture("did alice meet both the hatter and the queen?", OutBoth),
    ( sub_string(OutBoth, _, _, _, "Yes") -> true
    ; format('FAIL both: ~w~n', [OutBoth]), fail
    ),
    capture("did neither alice nor michel eat the key?", OutNei),
    ( sub_string(OutNei, _, _, _, "Yes") -> true
    ; format('FAIL neither: ~w~n', [OutNei]), fail
    ),
    capture("is alice and valerie in plataforma?", OutAB),
    ( sub_string(OutAB, _, _, _, "No") -> true
    ; format('FAIL alice+valerie in: ~w~n', [OutAB]), fail
    ),
    expect("how many people appear in plataforma?", "4"),
    capture("does alice_in_wonderland have the same author as plataforma?", OutSame),
    ( sub_string(OutSame, _, _, _, "No") -> true
    ; format('FAIL same author: ~w~n', [OutSame]), fail
    ),
    capture("who found key?", _),
    capture("who else met the hatter?", OutElse),
    ( sub_string(OutElse, _, _, _, "No one else") -> true
    ; format('FAIL else: ~w~n', [OutElse]), fail
    ),
    capture("forget that", OutFg),
    \+ sub_string(OutFg, _, _, _, "Because"),
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
