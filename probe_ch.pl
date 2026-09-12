% probe_ch.pl — lista capitulos detectados.
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

probe_ch :-
    doc_scan('books/alice.txt', alice, Chapters),
    forall(member(ch(N, Paras), Chapters),
           ( Paras = [P|_],
             string_length(P, PL),
             L is min(PL, 70),
             sub_string(P, 0, L, _, H),
             format('ch(~w): ~w~n', [N, H])
           )).
