% bookbrain/bookbrain.pl — Producto: documento -> document.knowledge.
% Uso desde bookbrain/: swipl -s bookbrain.pl -g "bookbrain('../books/alice.txt', alice)" -t halt
% Pipeline diferido (rapido): scan -> ingest -> index -> replay -> save.
% El .knowledge.pl guarda memfact/provfact + resumen. Motor intacto.
:- consult('../corpus.pl').
:- consult('../gen_parse.pl').
:- consult('../doc_corpus.pl').

:- use_module(library(lists)).

bookbrain(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    doc_reset,
    dx_reset,
    dx_deferred_on,
    get_time(T0),
    doc_scan(File, Alias, Chapters),
    doc_ingest(Alias, Chapters),
    dx_build_indexes,
    dx_replay,
    dx_deferred_off,
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
    findall(1, doc_ent(_, _, _), Es),
    length(Es, NE),
    format('BOOKBRAIN ~w facts=~w entities=~w ms=~w -> ~w~n',
           [Alias, NF, NE, Ms, Out]).
