% run_biblia.pl — Runner AUTOMATIZADO del corpus crudo.
% Uso: swipl -s run_biblia.pl -g main -t halt
% Lee corpus_biblia/kjv.txt tal cual (sin modificarlo): el motor detecta
% libros, versiculos y frases, extrae tripletas y publica metricas.
% Piloto: 1600 versiculos (~Genesis + inicio de Exodo). Produccion: main_full.
:- consult('corpus.pl').
:- consult('kjv_parse.pl').
:- consult('kjv_corpus.pl').

:- use_module(library(lists)).

main :- run_biblia(1600).
main_full :- run_biblia(0).

run_biblia(Limit) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    statistics(runtime, [T0, _]),
    kjv_load_limit(Limit),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    kjv_count(books, NB),
    kjv_count(verses, NV),
    kjv_count(sents, NS),
    kjv_count(stored, NSt),
    kjv_count(rejects, NR),
    memory_size(NF),
    format('BIBLIA books=~w verses=~w sents=~w stored=~w rejects=~w memfacts=~w ms=~w~n',
           [NB, NV, NS, NSt, NR, NF, Ms]),
    knowledge_stats(After),
    show_stats('BIBLIA-AFTER', After),
    nl, writeln('===== BOOKS ====='),
    kjv_list_books,
    nl, writeln('===== SAMPLE (15 con proveniencia) ====='),
    findall((S, R, O, Ref), prov(S, R, O, info(Ref, _, _)), All),
    length(All, N),
    format('stored triples: ~w~n', [N]),
    kjv_show(All, 15).

kjv_show(_, 0) :- !.
kjv_show([], _) :- !.
kjv_show([(S, R, O, Ref)|T], K) :-
    format('~w --~w--> ~w  [~w]~n', [S, R, O, Ref]),
    K1 is K - 1,
    kjv_show(T, K1).
