% experiment29.pl
% EXPERIMENT 29 - CORPUS 3: real transfer test (Q1/Q2/Q3 del diseno).
% Train: provides/imports/uses (vocabularios frescos, 100 cadenas c/u)
%   + loop admires (near-miss). Transfer: harvests (2 obs + 10 hidden).
% Q1: transferencia (meta, no memorizacion). Q2: distractores verificados
%   (pares minimos, runner confirma cero disparo). Q3: oculto inedito +
%   proof + atribucion (transferido vs inducido).
:- consult('corpus.pl').
:- consult('meta_pattern.pl').
:- consult('corpus3/heldout3.pl').
:- consult('corpus3/distractor3.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.

experiment29 :-
    reset_all,
    learn_file('corpus3/corpus3.txt'),
    memory_size(NFacts),
    check(NFacts =:= 986, 'ingested 986 facts'),
    knowledge_stats(AfterIngest),
    show_stats('ingest', AfterIngest),
    learn_cycle_guided([provides, imports, uses, admires], Rows),
    print_rows(Rows),
    knowledge_stats(AfterLearn),
    show_stats('learned', AfterLearn),
    discover_meta_phase,
    transfer_phase,
    l1_sanity,
    hidden_phase,
    distractor_phase,
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
    retractall(transferred_rule(_, _, _, _)),
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)),
    retractall(sentence_counter(_)),
    retractall(check_results(_, _)).

print_rows(Rows) :-
    forall(member(row(T, Path, F1, Gen, Ev, Ms), Rows),
           format('guided ~w :- ~w F1=~4f gen=~w eval=~w ~wms~n',
                  [T, Path, F1, Gen, Ev, Ms])).

discover_meta_phase :-
    nl, writeln('===== META (shapes, names erased) ====='),
    discover_meta,
    show_meta_rules,
    findall(M, meta_rule(M, 2, [1, 2, 3], _), Metas),
    check(Metas \== [], 'meta (2,[1,2,3]) exists'),
    forall(meta_rule(M, 2, [1, 2, 3], Ts),
           ( sort(Ts, Sorted),
             check(Sorted == [imports, provides, uses],
                   'meta members exactly the 3 train rules'))),
    check(\+ ( meta_rule(_, _, _, Ts),
               member(admires, Ts)
             ),
          'admires excluded from every meta (near-miss holds)').

transfer_phase :-
    nl, writeln('===== TRANSFER (harvests: 2 obs, 0 induction) ====='),
    ( meta_transfer(harvests, 3) ->
        check(transferred_rule(harvests, [plants, yields], [1, 2, 3], _),
              'harvests transferred via meta (labels discovered, shape licensed)')
    ; check(fail, 'transfer failed'), fail
    ).

l1_sanity :-
    nl, writeln('===== L1 SANITY (train retrieval, no hardcoding) ====='),
    memory_relation(P1, provides, M1, _, _),
    format(string(Q1), 'Where does ~w provide?', [P1]),
    ask(Q1, A1),
    check(A1 = answer([M1], retrieved, _), 'train provides retrieval'),
    memory_relation(P2, imports, M2, _, _),
    format(string(Q2), 'Where does ~w import?', [P2]),
    ask(Q2, A2),
    check(A2 = answer([M2], retrieved, _), 'train imports retrieval').

% nombres reales de la primera cadena de cada familia (del corpus)
% (se verifican en el run; si el generador cambia, preflight no aplica
% aqui: estos son Test Data, no codigo)

hidden_phase :-
    nl, writeln('===== HIDDEN (10 unseen combos + proof + attribution) ====='),
    findall((P, F), heldout(P, F, harvests), HK),
    length(HK, NH),
    check(NH =:= 10, '10 hidden pairs'),
    forall(member((P, F), HK),
           ( format(string(Q), 'Where does ~w harvest?', [P]),
             parse_question(Q, QP),
             answer_query(QP, A),
             ( A = answer([F], reasoned, Proofs) ->
                 Proofs = [Pr | _],
                 memberchk(rule(harvests, _, _), Pr),
                 transferred_rule(harvests, _, _, Meta),
                 format('PASS ~w -> ~w via ~w~n', [P, F, Meta]),
                 assertz(check_results(P, pass))
             ; format('FAIL ~w -> ~w~n', [P, A]),
               assertz(check_results(P, fail))
             )
           )).

distractor_phase :-
    nl, writeln('===== DISTRACTORS (20 verified false) ====='),
    findall((P, X, R), distractor(P, X, R), Ds),
    length(Ds, ND),
    check(ND =:= 20, '20 distractors loaded'),
    % pre-check: ninguno debe disparar NINGUNA regla (verificador)
    findall((P, X), ( member((P, X, R), Ds),
                      firing_rule(P, X, R)
                    ),
            Firing),
    ( Firing == [] ->
        format('verifier: 0/20 fire any rule OK~n', []),
        assertz(check_results('verifier clean', pass))
    ; format('VERIFIER FAIL (would fire): ~w~n', [Firing]),
      assertz(check_results('verifier clean', fail))
    ),
    forall(member((P, X, R), Ds),
           ( question_for(R, P, Q),
             ask(Q, A),
             ( A = answer([X], _, _) ->
                 format('FP ~w -> ~w~n', [P, X]),
                 assertz(check_results(distractor, fail))
             ; assertz(check_results(distractor, pass))
             )
           )).

% firing_rule: cualquier regla (concreta o transferida) predice el par.
firing_rule(P, X, R) :-
    constrained_rule(R, Path, Sig),
    full_bindings(P, X, Path, Full),
    eq_signature(Full, Sig).

question_for(harvests, P, Q) :-
    format(string(Q), 'Where does ~w harvest?', [P]).

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
