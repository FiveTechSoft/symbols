% probe_plataforma.pl — Alice + Plataforma en la misma memoria.
:- encoding(utf8).
:- consult('chat.pl').

p(L, S) :- format('--- ~w ---~n', [L]), chat_line(S).

probe :-
    bb_load('alice_clean.knowledge.pl'),
    bb_add('plataforma_clean.knowledge.pl'),
    p('que libro', "que libro?"),
    p('who found key', "who found key?"),
    p('quien es michel', "quien es michel?"),
    p('quien ama valerie', "quien ama valerie?"),
    p('que paso con valerie', "que paso con valerie?"),
    p('hablame de plataforma', "hablame de plataforma"),
    p('appears in plataforma', "appears_in plataforma?"),
    p('appears in alice', "appears_in alice_in_wonderland?"),
    p('who is alice', "who is alice?"),
    p('de que trata', "de que trata?"),
    p('did alice eat cake', "did alice eat cake?"),
    p('michel queda donde', "queda pattaya?"),
    halt.
