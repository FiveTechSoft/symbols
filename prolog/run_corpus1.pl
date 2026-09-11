% run_corpus1.pl
% Driver Corpus 1: ingesta -> ciclo -> metricas antes/despues -> held-out.
% Uso desde prolog/: swipl -s run_corpus1.pl -g main -t halt
:- consult('corpus.pl').
:- consult('heldout1.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.

main :-
    knowledge_stats(Before),
    show_stats('BEFORE', Before),
    learn_corpus('.'),
    memory_size(NFacts),
    check(NFacts =:= 150, 'ingested 150 facts (template coverage total)'),
    knowledge_stats(AfterIngest),
    show_stats('AFTER-INGEST', AfterIngest),
    learn_cycle([reaches]),
    knowledge_stats(AfterLearn),
    show_stats('AFTER-LEARN', AfterLearn),
    run_heldout,
    save_knowledge('longterm_c1.pl'),
    report_checks.

run_heldout :-
    nl, writeln('===== HELD-OUT (10 persons, never in corpus) ====='),
    forall(heldout(P, K),
           ( format(string(Q), 'Where does ~w reach?', [P]),
             ask(Q, A),
             ( A = answer([K], reasoned, _) ->
                 format('PASS ~w -> ~w (reasoned)~n', [P, K]),
                 assertz(check_results(P, pass))
             ; format('FAIL ~w -> ~w~n', [P, A]),
               assertz(check_results(P, fail))
             )
           )),
    nl, writeln('===== DISTRACTORS (next-person country, cyclic) ====='),
    findall(P-K, heldout(P, K), HK),
    length(HK, NH),
    findall((P, K2), ( nth0(I, HK, P-_),
                       J is (I + 1) mod NH,
                       nth0(J, HK, _-K2)
                     ),
            Pairs),
    forall(member((P, K2), Pairs),
           ( format(string(Q), 'Where does ~w reach?', [P]),
             ask(Q, A),
             ( A = answer([K2], _, _) ->
                 format('FP ~w -> ~w~n', [P, K2]),
                 assertz(check_results(distractor, fail))
             ; assertz(check_results(distractor, pass))
             )
           )).

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
