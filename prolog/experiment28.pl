% experiment28.pl
% EXPERIMENT 28 - GUIDED CORPUS LEARNING (bloques + metrica marginal)
% b1: visits/in/reaches+eats (exhaustive Y guided: igualdad exigida).
% b2: travels/in/arrives.  b3: reads/in/borrows.
% b4: ruido owns/likes (control negativo: 0 reglas nuevas).
% Por bloque: sentences/symbols/facts/concepts/rules + exhaustive-count
% vs guided-evals + tiempo. Final: held-out 15 + distractores 15.
:- consult('corpus.pl').
:- consult('heldout2.pl').
:- consult('distractor2.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.

experiment28 :-
    reset_all,
    block(b1, 'corpus2/b1.txt', [reaches], true),
    block(b2, 'corpus2/b2.txt', [arrives], false),
    block(b3, 'corpus2/b3.txt', [borrows], false),
    block(b4, 'corpus2/b4.txt', [], false),
    run_heldout,
    print_trend,
    report_checks.

reset_all :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(skill(_, _, _)),
    retractall(meta_rule(_, _, _, _)),
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)),
    retractall(sentence_counter(_)),
    retractall(check_results(_, _)).

block(Name, File, Targets, ExhaustiveToo) :-
    format('~n===== BLOCK ~w (~w) =====~n', [Name, File]),
    learn_file(File),
    knowledge_stats(AfterIngest),
    show_stats('ingest', AfterIngest),
    discover_all,
    ( ExhaustiveToo == true ->
        run_discovery(exhaustive, reaches, 3, SE),
        SE = stats(PE, FE, _, GenE, EvE, _, MsE),
        format('exhaustive: winner=~w F1=~4f gen=~w eval=~w ~wms~n',
               [PE, FE, GenE, EvE, MsE]),
        assertz(exh_witness(PE, FE))
    ; true
    ),
    learn_cycle_guided(Targets, Rows),
    forall(member(row(T, Path, F1, Gen, Ev, Ms), Rows),
           format('guided ~w :- ~w F1=~4f gen=~w eval=~w ~wms~n',
                  [T, Path, F1, Gen, Ev, Ms])),
    ( ExhaustiveToo == true ->
        exh_witness(PE, FE),
        member(row(reaches, PG, FG, _, _, _), Rows),
        check(PE == PG, 'guided == exhaustive winner'),
        check(abs(FE - FG) < 0.0001, 'guided == exhaustive F1')
    ; true
    ),
    ( Targets == [] ->
        findall(T-P, constrained_rule(T, P, _), CRs),
        sort(CRs, CRsS),
        check(CRsS == [arrives-[travels, in],
                       borrows-[reads, in],
                       reaches-[visits, in]],
              'b4: zero new constrained rules (negative control)')
    ; true
    ),
    knowledge_stats(AfterLearn),
    show_stats('learned', AfterLearn),
    AfterIngest = stats(F1f, _, _, _, _, _, _, _, _),
    AfterLearn = stats(F2f, Sym2, _, C2, R2, _, _, _, _),
    exhaustive_count_block(Targets, ExhCount),
    record_block(Name, F1f, F2f, Sym2, C2, R2, ExhCount, Rows).

:- dynamic block_row/8.
:- dynamic exh_witness/2.
% block_row(Name, FactsBefore, FactsAfter, Symbols, Concepts, Rules,
%           ExhaustiveCount, GuidedEvals)

exhaustive_count_block([], 0) :- !.
exhaustive_count_block([T|_], Count) :-
    exhaustive_count(T, 3, Count).

record_block(Name, F1f, F2f, Sym2, C2, R2, ExhCount, Rows) :-
    findall(Ev, member(row(_, _, _, _, Ev, _), Rows), Evs),
    sum_evs(Evs, GuidedEvals),
    assertz(block_row(Name, F1f, F2f, Sym2, C2, R2, ExhCount,
                      GuidedEvals)).

sum_evs([], 0).
sum_evs([H|T], S) :-
    sum_evs(T, S0),
    S is S0 + H.

run_heldout :-
    nl, writeln('===== HELD-OUT (15, never in corpus) ====='),
    findall((P, K, R), heldout(P, K, R), HK),
    length(HK, NH),
    format('heldout pairs: ~w~n', [NH]),
    forall(member((P, K, R), HK),
           ( question_for(R, P, Q),
             ask(Q, A),
             ( A = answer([K], reasoned, _) ->
                 assertz(check_results(P, pass))
             ; format('FAIL ~w -> ~w~n', [P, A]),
               assertz(check_results(P, fail))
             )
           )),
    nl, writeln('===== DISTRACTORS (verified false in generator) ====='),
    findall((P, K2, R), distractor(P, K2, R), Ds),
    length(Ds, ND),
    format('distractors: ~w~n', [ND]),
    forall(member((P, K2, R), Ds),
           ( question_for(R, P, Q),
             ask(Q, A),
             ( A = answer([K2], _, _) ->
                 format('FP ~w -> ~w~n', [P, K2]),
                 assertz(check_results(distractor, fail))
             ; assertz(check_results(distractor, pass))
             )
           )).

question_for(reaches, P, Q) :-
    format(string(Q), 'Where does ~w reach?', [P]).
question_for(arrives, P, Q) :-
    format(string(Q), 'Where does ~w arrive?', [P]).
question_for(borrows, P, Q) :-
    format(string(Q), 'Where does ~w borrow?', [P]).

print_trend :-
    nl, writeln('===== MARGINAL TREND ====='),
    writeln('block facts rules exh-count guided-eval'),
    forall(block_row(N, F1f, F2f, _, _, R2, Exh, Gev),
           format('~w ~w->~w rules=~w exh=~w guided=~w~n',
                  [N, F1f, F2f, R2, Exh, Gev])),
    findall(E, block_row(_, _, _, _, _, _, _, E), Evs),
    sum_evs(Evs, TE),
    findall(F, block_row(_, _, F, _, _, _, _, _), Fs),
    sort(Fs, SortedFs),
    reverse(SortedFs, [FinalFacts|_]),
    format('cumulative facts=~w total guided-evals=~w evals/fact=~4f~n',
           [FinalFacts, TE, TE / FinalFacts]).

% --- checks ---

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
