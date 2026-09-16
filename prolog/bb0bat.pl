:- consult('chat.pl').
:- use_module(library(lists)).

q(1, "who feared the lord").
q(2, "who came to him").
q(3, "who feared").
q(4, "who did the men fear").
q(5, "what did the men fear").
q(6, "did the men fear the lord").
q(7, "who feared the sea").
q(8, "what did jonah do").
q(9, "who fears the lord").
q(10, "the men feared the lord").
q(11, "why did the men fear the lord").

compact(A, B) :-
    atom_codes(A, Cs0),
    maplist([C, C2]>>( memberchk(C, [10, 13, 9]) -> C2 = 32 ; C2 = C ), Cs0, Cs1),
    atom_codes(B, Cs1).

run :-
    bb_load("C:/Users/Anto/AppData/Local/Temp/opencode/jonah_r12c.knowledge.pl"),
    forall(q(N, Q),
           ( format('Q~w ||| ~w ||| ', [N, Q]),
             ( catch(with_output_to(atom(A), chat_line(Q)), _, A = '<error>') -> true ; A = '<fail>' ),
             compact(A, C),
             format('~w~n', [C]) )),
    halt.