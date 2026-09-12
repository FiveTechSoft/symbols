% exp58.pl — EXP58: aprendizaje end-to-end sobre Alice (diferido).
% Uso: swipl -s exp58.pl -g exp58 -t halt
% Disenado para TERMINAR: ingesta diferida (~2s), 5 targets, min-support>=3,
% budget 400s en COMP, timeouts por fase. Split: train caps<=10 (libro I-VIII),
% test caps>10 (libro IX-XII). Criterio de salida: skills + holdout F1.
:- consult('corpus.pl').
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

:- use_module(library(lists)).

:- dynamic e58_held/4.       % e58_held(S,R,O,Ref) hechos test (fuera de memoria)
:- dynamic e58_composed/3.
:- dynamic e58_budget/1.

exp58 :-
    ingest_deferred,
    split_chapters,
    phase_concepts,
    phase_crels,
    phase_comp,
    phase_rules,
    phase_holdout,
    writeln('E58-DONE').

% ---------- ingesta diferida completa ----------
ingest_deferred :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    doc_reset,
    dx_reset,
    dx_deferred_on,
    get_time(T0),
    doc_scan('books/alice.txt', alice, Chapters),
    doc_ingest(alice, Chapters),
    dx_build_indexes,
    dx_replay,
    dx_deferred_off,
    get_time(T1),
    Ms is round((T1 - T0) * 1000),
    memory_size(NF),
    format('E58-INGEST memfacts=~w ms=~w~n', [NF, Ms]).

% ---------- split train/test por capitulos ----------
split_chapters :-
    retractall(e58_held(_, _, _, _)),
    retractall(e58_composed(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(composed_rule(_, _, _)),
    findall((S, R, O, Ref),
            (memory_relation(S, R, O, _, _),
             prov(S, R, O, info(Ref, _, _)),
             e58_chapter(Ref, CN), CN > 10),
            Test),
    length(Test, NTest),
    forall(member((S, R, O, Ref), Test),
           ( retract(memory_relation(S, R, O, _, _)),
             retract(prov(S, R, O, _)),
             assertz(e58_held(S, R, O, Ref))
           )),
    memory_size(NTrain),
    format('E58-SPLIT train=~w test=~w~n', [NTrain, NTest]).

e58_chapter(Ref, CN) :-
    atom(Ref),
    split_string(Ref, "_", "", Parts),
    member(C, Parts),
    atom_concat('ch', N, C),
    atom_number(N, CN), !.
e58_chapter(_, 0).

phase_run(Name, Secs, Goal) :-
    statistics(runtime, [T0, _]),
    ( catch(call_with_time_limit(Secs, Goal), E, true) ->
        ( var(E) -> Status = ok ; Status = timeout )
    ; Status = failed
    ),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    memory_size(F),
    e58_counts(C, M),
    format('E58-PHASE ~w facts=~w concepts=~w members=~w ms=~w status=~w~n',
           [Name, F, C, M, Ms, Status]).

e58_counts(C, M) :-
    findall(1, concept(_, _, _), Cs), length(Cs, C),
    findall(1, concept_member(_, _, _), Ms), length(Ms, M).

phase_concepts :-
    phase_run('CONCEPTS', 600, discover_concepts).

phase_crels :-
    phase_run('CRELS', 300, discover_concept_relations).

% ---------- COMP con budget y min-support ----------
phase_comp :-
    train_rel_counts(Counts),
    take_targets(Counts, 5, Targets),
    format('E58-TARGETS ~w~n', [Targets]),
    retractall(e58_budget(_)),
    assertz(e58_budget(400000)),
    forall(member(T, Targets), (comp_len(T, 1), comp_len(T, 2))).

train_rel_counts(Desc) :-
    findall(R, memory_relation(_, R, _, _, _), Rs),
    msort(Rs, S),
    e58_clump(S, C),
    e58_sort_desc(C, Desc).

e58_clump([], []).
e58_clump([H|T], [N-H|R]) :-
    e58_run(H, T, N, Rest),
    e58_clump(Rest, R).

e58_run(H, [], 1, []).
e58_run(H, [H|T], N, R) :- !, e58_run(H, T, N0, R), N is N0 + 1.
e58_run(_, L, 1, L).

e58_sort_desc(C, Desc) :-
    findall(N-R, member(N-R, C), P),
    keysort(P, A),
    reverse(A, Desc).

take_targets([], _, []) :- !.
take_targets(_, 0, []) :- !.
take_targets([_-R|T], K, Out) :-
    ( concept_relation(_, R, _, _) ->
        Out = [R|Rest], K1 is K - 1, take_targets(T, K1, Rest)
    ; format('E58-SKIP-NOCONCEPT ~w~n', [R]),
      take_targets(T, K, Out)
    ).

comp_len(T, Len) :-
    e58_budget(B),
    ( B =< 0 ->
        format('E58-COMP target=~w len=~w status=skipped-budget~n', [T, Len])
    ; statistics(runtime, [T0, _]),
      ( catch(call_with_time_limit(120, run_discovery(guided, T, Len, Stats)), E, true) ->
          ( var(E) ->
              ( Stats = stats(Path, F1, Sup, Gen, Ev, Pr, Ms) ->
                  ( Sup >= 3 -> St = ok ; St = filtered-support )
              ; St = no-margin, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
              )
          ; St = timeout, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
          )
      ; St = failed, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
      ),
      statistics(runtime, [T1, _]),
      Wall is T1 - T0,
      retract(e58_budget(B0)),
      B1 is B0 - Wall,
      assertz(e58_budget(B1)),
      ( St == ok ->
          retractall(e58_composed(T, _, _)),
          retractall(composed_rule(T, _, _)),
          assertz(e58_composed(T, Path, F1)),
          assertz(composed_rule(T, Path, F1))
      ; true
      ),
      format('E58-COMP target=~w len=~w path=~w f1=~4f sup=~w gen=~w eval=~w pruned=~w ms=~w wall=~w status=~w~n',
             [T, Len, Path, F1, Sup, Gen, Ev, Pr, Ms, Wall, St])
    ).

% ---------- RULES ----------
phase_rules :-
    findall((T, P), e58_composed(T, P, _), Ws),
    length(Ws, NW),
    format('E58-RULES winners=~w~n', [NW]),
    forall(member((T, P), Ws), rule_one(T, P)).

rule_one(T, Path) :-
    statistics(runtime, [T0, _]),
    ( induce_constrained(T, Path) ->
        constrained_rule(T, Path, Sig), St = accepted
    ; St = refused, Sig = []
    ),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    ( St == accepted ->
        findall(S-O, (memory_relation(S, T, O, _, _),
                      full_bindings(S, O, Path, _)), Sup0),
        sort(Sup0, SupL),
        length(SupL, NSup),
        findall(SC-OC, concept_relation(SC, T, OC, _), CRs),
        length(CRs, NC)
    ; NSup = 0, NC = 0
    ),
    format('E58-RULE target=~w path=~w sig=~w support=~w concepts=~w ms=~w status=~w~n',
           [T, Path, Sig, NSup, NC, Ms, St]).

% ---------- HOLDOUT intra-libro ----------
phase_holdout :-
    findall((T, P, S), constrained_rule(T, P, S), Rules),
    length(Rules, NR),
    format('E58-HOLDOUT rules=~w~n', [NR]),
    statistics(runtime, [T0, _]),
    findall(TP-FP-FN, (member((T, P, _), Rules), rule_scores(T, P, TP, FP, FN)), Rows),
    sum3(Rows, TTP, TFP, TFN),
    e58_f1(TTP, TFP, TFN, P, R, F1),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    format('E58-HOLDOUT-MICRO tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f ms=~w~n',
           [TTP, TFP, TFN, P, R, F1, Ms]).

rule_scores(T, Path, TP, FP, FN) :-
    findall((S-O), reuse_predict(S, T, O), Pred0),
    sort(Pred0, Pred),
    length(Pred, NP),
    findall(1, (member((S-O), Pred), e58_held(S, T, O, _)), TPs),
    length(TPs, TP),
    findall(1, (member((S-O), Pred),
                \+ e58_anyfact(S, T, O)), FPs),
    length(FPs, FP),
    findall(1, (e58_held(S, T, O, _),
                once(full_bindings(S, O, Path, _)),
                \+ member((S-O), Pred)), FNs),
    length(FNs, FN),
    e58_f1(TP, FP, FN, P, R, F1),
    findall(CN, (member((S-O), Pred), e58_held(S, T, O, Ref),
                 e58_chapter(Ref, CN)), C0),
    sort(C0, CNs),
    format('E58-HOLDOUT-RULE target=~w path=~w npred=~w tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f testchs=~w~n',
           [T, Path, NP, TP, FP, FN, P, R, F1, CNs]),
    rule_proofs(T, Path, Pred).

% FP = predicho pero ausente de train Y de test (grafo completo conocido).
e58_anyfact(S, T, O) :-
    memory_relation(S, T, O, _, _), !.
e58_anyfact(S, T, O) :-
    e58_held(S, T, O, _), !.

e58_f1(TP, FP, FN, P, R, F1) :-
    ( TP + FP =:= 0 -> P = 0.0 ; P is TP / (TP + FP) ),
    ( TP + FN =:= 0 -> R = 0.0 ; R is TP / (TP + FN) ),
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ).

sum3([], 0, 0, 0).
sum3([A-B-C|T], SA, SB, SC) :-
    sum3(T, RA, RB, RC),
    SA is RA + A, SB is RB + B, SC is RC + C.

rule_proofs(T, Path, Pred) :-
    findall((S-O), (member((S-O), Pred), e58_held(S, T, O, _)), Hits),
    show_proofs(T, Path, Hits, 3).

show_proofs(_, _, [], _) :- !.
show_proofs(_, _, _, 0) :- !.
show_proofs(T, Path, [(S-O)|R], K) :-
    full_bindings(S, O, Path, _),
    path_proof(S, O, Path, Ev),
    format('E58-PROOF ~w(~w,~w) :- ~w :: ~w~n', [T, S, O, Path, Ev]),
    K1 is K - 1,
    show_proofs(T, Path, R, K1).

path_proof(S, O, [R], [(S, R, O, Ref)]) :-
    prov(S, R, O, info(Ref, _, _)), !.
path_proof(S, O, [R], [(S, R, O, noref)]) :- !.
path_proof(S, O, [R1|Rs], [(S, R1, M, Ref)|Ev]) :-
    memory_relation(S, R1, M, _, _),
    ( prov(S, R1, M, info(Ref, _, _)) -> true ; Ref = noref ),
    path_proof(M, O, Rs, Ev).
