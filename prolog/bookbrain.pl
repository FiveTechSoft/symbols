% bookbrain/bookbrain.pl — Producto: documento -> document.knowledge.
% Uso desde prolog/: swipl -s bookbrain.pl -g "bookbrain('ruta/libro.txt', alias)" -t halt
% Pipeline directo sobre corpus.pl: clear -> learn_file -> dump.
% (El pipeline diferido doc_scan/doc_ingest/dx_* no existe en este arbol;
% las refs de provenance son sentence_N. Mismo formato memfact/5 +
% provfact/6 que cargan chat.pl y ask.pl. Motor intacto.)
:- consult('corpus.pl').

:- use_module(library(lists)).

bookbrain(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    get_time(T0),
    learn_file(File),
    get_time(T1),
    Ms is round((T1 - T0) * 1000),
    atom_concat(Alias, '.knowledge.pl', Out),
    open(Out, write, S),
    forall(memory_relation(A, R, O, W, U),
           format(S, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
    forall(prov(A, R, O, info(Ref, T, St)),
           format(S, 'provfact(~q,~q,~q,~q,~q,~q).~n', [A, R, O, Ref, T, St])),
    close(S),
    memory_size(NF),
    format('BOOKBRAIN ~w facts=~w ms=~w -> ~w~n',
           [Alias, NF, Ms, Out]).
