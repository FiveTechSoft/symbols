% probe_bench2.pl — ingesta por grupos de capitulos con tiempos.
:- consult('corpus.pl').
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

probe_b2 :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    doc_reset,
    doc_scan('books/alice.txt', alice, Chapters),
    doc_census(Census),
    b2_groups(Chapters, Census, [[1, 2, 3], [4, 5, 6], [7, 8, 9, 10], [11, 12, 13]], 0, _).

b2_groups(_, _, [], S, S).
b2_groups(Chapters, Census, [G|Gs], S0, S) :-
    get_time(T0),
    b2_paras(Chapters, Census, G, S0, S1),
    get_time(T1),
    memory_size(NF),
    format('GROUP ~w ms=~2f memfacts=~w~n', [G, (T1 - T0) * 1000, NF]),
    b2_groups(Chapters, Census, Gs, S1, S).

b2_paras(_, _, [], S, S).
b2_paras(Chapters, Census, [CN|CNS], S0, S) :-
    ( member(ch(CN, Paras), Chapters) ->
        ingest_paras(alice, CN, Paras, Census, S0, S1)
    ; S1 = S0
    ),
    b2_paras(Chapters, Census, CNS, S1, S).
