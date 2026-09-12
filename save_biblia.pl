% save_biblia.pl — UNA sola ingesta completa (T0) persistida a kjv_memory.pl.
% Uso: swipl -s save_biblia.pl -g save_biblia -t halt
% Guarda memfact/5 + provfact/6 con writeq (recarga segura). Todo EXP54
% posterior carga este fichero en segundos en vez de re-ingerir (375 s).
:- consult('corpus.pl').
:- consult('kjv_parse.pl').
:- consult('kjv_corpus.pl').

save_biblia :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    statistics(runtime, [T0, _]),
    kjv_load_limit(0),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    open('kjv_memory.pl', write, Out),
    forall(memory_relation(S, R, O, W, U),
           format(Out, 'memfact(~q,~q,~q,~q,~q).~n', [S, R, O, W, U])),
    forall(prov(S, R, O, info(Ref, T, St)),
           format(Out, 'provfact(~q,~q,~q,~q,~q,~q).~n', [S, R, O, Ref, T, St])),
    close(Out),
    memory_size(NF),
    kjv_count(stored, NSt),
    kjv_count(sents, NS),
    format('SAVED memfacts=~w stored=~w sents=~w ms=~w -> kjv_memory.pl~n',
           [NF, NSt, NS, Ms]).
