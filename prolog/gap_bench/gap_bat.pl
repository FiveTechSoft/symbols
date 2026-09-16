:- consult('C:/symbols/prolog/chat.pl').

q(1, "who sent the wind").
q(2, "what came to jonah").
q(3, "who did the men fear").
q(4, "what did the lord send").
q(5, "who feared the lord").
q(6, "who believed god").
q(7, "what did the men fear").
q(8, "what did the waters compass").
q(9, "how did the mariners feel").
q(10, "who was afraid").
q(11, "the wind was sent by whom").
q(12, "to whom did the word come").
q(13, "were the mariners afraid").
q(14, "who compassed the soul").
q(15, "did the lord send a wind").
q(16, "did the people believe god").
q(17, "did the waters compass the soul").
q(18, "did the word come to jonah").
q(19, "who did the people fear").
q(20, "did the men fear the sea").
q(21, "what is the capital of france").
q(22, "who wrote romeo and juliet").
q(23, "h2o").
q(24, "days in a week").
q(25, "color of the sky").
q(26, "animal that says moo").
q(27, "2 plus 2").
q(28, "language spoken in spain").
q(29, "hello").
q(30, "how are you").
q(31, "thank you").
q(32, "goodbye").
q(33, "tell me a story about a fish").
q(34, "write a sentence about the sea").
q(35, "what is a fish").
q(36, "three animals").
q(37, "what are you").
q(38, "can you learn new facts").
q(39, "do you know jonah").
q(40, "what can you do").

compact(A, B) :-
    atom_codes(A, Cs0),
    maplist([C, C2]>>( memberchk(C, [10, 13, 9]) -> C2 = 32 ; C2 = C ), Cs0, Cs1),
    atom_codes(B, Cs1).

run :-
    forall(q(N, Q),
           ( bb_load("C:/Users/Anto/AppData/Local/Temp/opencode/jonah_r12c.knowledge.pl"),
             format('Q~w ||| ~w ||| ', [N, Q]),
             ( catch(with_output_to(atom(A), chat_line(Q)), _, A = '<error>') -> true ; A = '<fail>' ),
             compact(A, C),
             format('~w~n', [C]) )),
    halt.