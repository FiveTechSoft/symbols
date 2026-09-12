% run_biblia_pilot.pl — PILOTO Genesis (linea base honesta, sin extension verbal).
% Uso: swipl -s run_biblia_pilot.pl -g main -t halt
% Mide: tiempo de ingesta, hechos aceptados, muestra de tripletas.
:- consult('corpus.pl').

main :-
    statistics(runtime, [T0, _]),
    learn_corpus('corpus_pilot'),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    memory_size(NFacts),
    format('PILOT facts=~w ms=~w~n', [NFacts, Ms]),
    knowledge_stats(After),
    show_stats('PILOT-AFTER-INGEST', After),
    nl, writeln('===== SAMPLE (10) ====='),
    findall((S, R, O), memory_relation(S, R, O, _, _), All),
    length(All, N),
    format('stored triples: ~w~n', [N]),
    show_n(All, 10).

show_n(_, 0) :- !.
show_n([], _) :- !.
show_n([(S, R, O)|T], K) :-
    format('~w --~w--> ~w~n', [S, R, O]),
    K1 is K - 1,
    show_n(T, K1).
