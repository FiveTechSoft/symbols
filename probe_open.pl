% probe_open.pl — frases del lector con 'opened' + id global.
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

probe_open :-
    doc_scan('books/alice.txt', alice, Chapters),
    walk(Chapters, 0, _).

walk([], S, S).
walk([ch(_, Paras)|Cs], S0, S) :-
    walk_p(Paras, S0, S1),
    walk(Cs, S1, S).

walk_p([], S, S).
walk_p([P|Ps], S0, S) :-
    doc_sentences(P, Sents),
    walk_s(Sents, S0, S1),
    walk_p(Ps, S1, S).

walk_s([], S, S).
walk_s([Sn|Sns], S0, S) :-
    S1 is S0 + 1,
    ( sub_string(Sn, _, _, _, "opened") ->
        format('s~w: ~w~n', [S1, Sn])
    ; true
    ),
    walk_s(Sns, S1, S).
