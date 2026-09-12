% exp55_1.pl — EXP55.1: discovery sobre representacion AUTONOMA (Alice).
% Uso: swipl -s exp55_1.pl -g exp55_1 -t halt
% Mismo protocolo que EXP54.2 (fases, timeouts, holdout) salvo el split:
% TRAIN = caps 1..10 (preambulo + libro I-VIII), TEST = caps 11..13
% (libro IX-XII). Generalizacion intra-libro. Parser congelado.
:- consult('corpus.pl').

:- use_module(library(lists)).

:- dynamic e55_held/3.
:- dynamic e55_composed/3.

exp55_1 :-
    load_split,
    phase_concepts,
    phase_crels,
    phase_comp,
    phase_rules,
    phase_holdout,
    writeln('E55-DONE').

% ---------- carga + split por capitulos ----------
load_split :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(e55_held(_, _, _)),
    retractall(e55_composed(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(composed_rule(_, _, _)),
    consult('alice_memory.pl'),
    findall((S, R, O, W, U, CN),
            (memfact(S, R, O, W, U),
             provfact(S, R, O, Ref, _, _),
             e55_chapter(Ref, CN)),
            All),
    length(All, N),
    split_assert(All, 0, 0, NH, NT),
    format('E55-SPLIT total=~w train=~w test=~w~n', [N, NT, NH]).

% TEST = libro IX-XII = caps 11..13 (cap 1 = preambulo va a train).
split_assert([], NH, NT, NH, NT).
split_assert([(S, R, O, W, U, CN)|T], NH0, NT0, NH, NT) :-
    ( CN > 10 ->
        assertz(e55_held(S, R, O)),
        NH1 is NH0 + 1, NT1 = NT0
    ; assertz(memory_relation(S, R, O, W, U)),
      ( provfact(S, R, O, Ref, Tm, St) ->
          assertz(prov(S, R, O, info(Ref, Tm, St)))
      ; true
      ),
      NT1 is NT0 + 1, NH1 = NH0
    ),
    split_assert(T, NH1, NT1, NH, NT).

e55_chapter(Ref, CN) :-
    atom(Ref),
    split_string(Ref, "_", "", Parts),
    member(C, Parts),
    atom_concat('ch', N, C),
    atom_number(N, CN), !.
e55_chapter(_, 0).

phase_run(Name, Secs, Goal) :-
    statistics(runtime, [T0, _]),
    ( catch(call_with_time_limit(Secs, Goal), E, true) ->
        ( var(E) -> Status = ok ; Status = timeout )
    ; Status = failed
    ),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    memory_size(F),
    e55_counts(C, M),
    format('E55-PHASE ~w facts=~w concepts=~w members=~w ms=~w status=~w~n',
           [Name, F, C, M, Ms, Status]).

e55_counts(C, M) :-
    findall(1, concept(_, _, _), Cs), length(Cs, C),
    findall(1, concept_member(_, _, _), Ms), length(Ms, M).

phase_concepts :-
    phase_run('CONCEPTS', 600, discover_concepts).

phase_crels :-
    phase_run('CRELS', 300, discover_concept_relations).

% ---------- COMP ----------
phase_comp :-
    train_rel_counts(Counts),
    take_targets(Counts, 8, Targets),
    format('E55-TARGETS ~w~n', [Targets]),
    forall(member(T, Targets), comp_target(T)).

train_rel_counts(Desc) :-
    findall(R, memory_relation(_, R, _, _, _), Rs),
    msort(Rs, S),
    e55_clump(S, C),
    e55_sort_desc(C, Desc).

e55_clump([], []).
e55_clump([H|T], [N-H|R]) :-
    e55_run(H, T, N, Rest),
    e55_clump(Rest, R).

e55_run(H, [], 1, []).
e55_run(H, [H|T], N, R) :- !, e55_run(H, T, N0, R), N is N0 + 1.
e55_run(_, L, 1, L).

e55_sort_desc(C, Desc) :-
    findall(N-R, member(N-R, C), P),
    keysort(P, A),
    reverse(A, Desc).

take_targets([], _, []) :- !.
take_targets(_, 0, []) :- !.
take_targets([_-R|T], K, Out) :-
    ( concept_relation(_, R, _, _) ->
        Out = [R|Rest], K1 is K - 1, take_targets(T, K1, Rest)
    ; format('E55-SKIP-NOCONCEPT ~w~n', [R]),
      take_targets(T, K, Out)
    ).

comp_target(T) :-
    comp_len(T, 1),
    comp_len(T, 2).

comp_len(T, Len) :-
    statistics(runtime, [T0, _]),
    ( catch(call_with_time_limit(180, run_discovery(guided, T, Len, Stats)), E, true) ->
        ( var(E) ->
            ( Stats = stats(Path, F1, Sup, Gen, Ev, Pr, Ms) ->
                St = ok,
                retractall(e55_composed(T, _, _)),
                retractall(composed_rule(T, _, _)),
                assertz(e55_composed(T, Path, F1)),
                assertz(composed_rule(T, Path, F1))
            ; St = no-margin, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
            )
        ; St = timeout, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
        )
    ; St = failed, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, Ms = 0
    ),
    statistics(runtime, [T1, _]),
    Wall is T1 - T0,
    format('E55-COMP target=~w len=~w path=~w f1=~4f sup=~w gen=~w eval=~w pruned=~w ms=~w wall=~w status=~w~n',
           [T, Len, Path, F1, Sup, Gen, Ev, Pr, Ms, Wall, St]).

% ---------- RULES ----------
phase_rules :-
    findall((T, P), e55_composed(T, P, _), Ws),
    length(Ws, NW),
    format('E55-RULES winners=~w~n', [NW]),
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
    format('E55-RULE target=~w path=~w sig=~w support=~w concepts=~w ms=~w status=~w~n',
           [T, Path, Sig, NSup, NC, Ms, St]).

% ---------- HOLDOUT intra-libro ----------
phase_holdout :-
    findall((T, P, S), constrained_rule(T, P, S), Rules),
    length(Rules, NR),
    format('E55-HOLDOUT rules=~w~n', [NR]),
    statistics(runtime, [T0, _]),
    findall(TP-FP-FN, (member((T, P, _), Rules), rule_scores(T, P, TP, FP, FN)), Rows),
    sum3(Rows, TTP, TFP, TFN),
    e55_f1(TTP, TFP, TFN, P, R, F1),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    format('E55-HOLDOUT-MICRO tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f ms=~w~n',
           [TTP, TFP, TFN, P, R, F1, Ms]).

rule_scores(T, Path, TP, FP, FN) :-
    findall((S-O), reuse_predict(S, T, O), Pred0),
    sort(Pred0, Pred),
    length(Pred, NP),
    findall(1, (member((S-O), Pred), e55_held(S, T, O)), TPs),
    length(TPs, TP),
    findall(1, (member((S-O), Pred),
                \+ memfact(S, T, O, _, _)), FPs),
    length(FPs, FP),
    findall(1, (e55_held(S, T, O),
                once(full_bindings(S, O, Path, _)),
                \+ member((S-O), Pred)), FNs),
    length(FNs, FN),
    e55_f1(TP, FP, FN, P, R, F1),
    findall(CN, (member((S-O), Pred), e55_held(S, T, O),
                 provfact(S, T, O, Ref, _, _), e55_chapter(Ref, CN)), C0),
    sort(C0, CNs),
    format('E55-HOLDOUT-RULE target=~w path=~w npred=~w tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f testchs=~w~n',
           [T, Path, NP, TP, FP, FN, P, R, F1, CNs]),
    rule_proofs(T, Path, Pred).

e55_f1(TP, FP, FN, P, R, F1) :-
    ( TP + FP =:= 0 -> P = 0.0 ; P is TP / (TP + FP) ),
    ( TP + FN =:= 0 -> R = 0.0 ; R is TP / (TP + FN) ),
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ).

sum3([], 0, 0, 0).
sum3([A-B-C|T], SA, SB, SC) :-
    sum3(T, RA, RB, RC),
    SA is RA + A, SB is RB + B, SC is RC + C.

rule_proofs(T, Path, Pred) :-
    findall((S-O), (member((S-O), Pred), e55_held(S, T, O)), Hits),
    show_proofs(T, Path, Hits, 3).

show_proofs(_, _, [], _) :- !.
show_proofs(_, _, _, 0) :- !.
show_proofs(T, Path, [(S-O)|R], K) :-
    full_bindings(S, O, Path, _),
    path_proof(S, O, Path, Ev),
    format('E55-PROOF ~w(~w,~w) :- ~w :: ~w~n', [T, S, O, Path, Ev]),
    K1 is K - 1,
    show_proofs(T, Path, R, K1).

path_proof(S, O, [R], [(S, R, O, Ref)]) :-
    prov(S, R, O, info(Ref, _, _)), !.
path_proof(S, O, [R], [(S, R, O, noref)]) :- !.
path_proof(S, O, [R1|Rs], [(S, R1, M, Ref)|Ev]) :-
    memory_relation(S, R1, M, _, _),
    ( prov(S, R1, M, info(Ref, _, _)) -> true ; Ref = noref ),
    path_proof(M, O, Rs, Ev).
