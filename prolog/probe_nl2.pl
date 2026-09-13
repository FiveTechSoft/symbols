% probe_nl2.pl — segunda pasada: huecos naturales sobre alice_clean.
:- encoding(utf8).
:- consult('chat.pl').

p(Label, Line) :-
    format('--- ~w ---~n', [Label]),
    chat_line(Line).

probe :-
    bb_load('alice_clean.knowledge.pl'),
    p('who is alice', "who is alice?"),
    p('tell me about the white rabbit', "tell me about the white rabbit?"),
    p('what did the queen do', "what did the queen do?"),
    p('did the baby turn into a pig', "did the baby turn into a pig?"),
    p('what happened to the baby', "what happened to the baby?"),
    p('how many people did alice meet', "how many people did alice meet?"),
    p('who carries a watch', "who carries a watch?"),
    p('who met the hatter', "who met the hatter?"),
    p('did alice drink the bottle', "did alice drink the bottle?"),
    p('why?', "why?"),
    p('por que', "por que?"),
    p('and the hatter', "And the hatter?"),
    p('who painted the roses', "who painted the roses?"),
    p('did alice eat the key', "did alice eat the key?"),
    p('what did she play', "what did she play?"),
    p('the duchess nurses a baby', "the duchess nurses a baby."),
    p('who nurses the baby', "who nurses the baby?"),
    p('she threatened alice', "she threatened alice?"),
    p('quien es alice', "quien es alice?"),
    p('de que trata', "de que trata?"),
    p('que paso con el baby', "que paso con el baby?"),
    p('alice found a key and ate cake', "alice found a key and ate cake."),
    p('who woke alice', "who woke alice?"),
    p('did the sister read a book', "did the sister read a book?"),
    p('what does the cheshire cat have', "what does the cheshire cat have?"),
    p('explain', "explain"),
    p('more', "more"),
    halt.
