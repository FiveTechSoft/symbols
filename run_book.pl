% run_book.pl — Runner LIBRO AUTONOMO (fase 1: cobertura sin ayuda).
% Uso: swipl -s run_book.pl -g "run_book('books/alice.txt', alice)" -t halt
% Cero lexicón de contenido: gen_parse + doc_corpus. Al final BOOK REPORT.
:- consult('corpus.pl').
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

run_book(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    doc_reset,
    statistics(runtime, [T0, _]),
    doc_scan(File, Alias, Chapters),
    doc_ingest(Alias, Chapters),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    memory_size(MF),
    format('BOOK-DONE ~w memfacts=~w ms=~w~n', [Alias, MF, Ms]),
    doc_report(Alias).
