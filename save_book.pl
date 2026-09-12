% save_book.pl — ingesta Alice + persistencia a alice_memory.pl (una vez).
% Uso: swipl -s save_book.pl -g "save_book('books/alice.txt', alice)" -t halt
:- consult('corpus.pl').
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

save_book(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    doc_reset,
    doc_scan(File, Alias, Chapters),
    doc_ingest(Alias, Chapters),
    atom_concat(Alias, '_memory.pl', Out),
    open(Out, write, S),
    forall(memory_relation(A, R, O, W, U),
           format(S, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
    forall(prov(A, R, O, info(Ref, T, St)),
           format(S, 'provfact(~q,~q,~q,~q,~q,~q).~n', [A, R, O, Ref, T, St])),
    close(S),
    memory_size(NF),
    format('SAVED-BOOK ~w memfacts=~w -> ~w~n', [Alias, NF, Out]).
