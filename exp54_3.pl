% exp54_3.pl — EXP54.3: rejilla direccion x min-support sobre KJV.
% Uso: swipl -s exp54_3.pl -g exp54_3 -t halt
% Ejes: direccion {ot2nt, nt2ot} x k {5,10,20}. Mismos parametros que
% EXP54.2 salvo: split por libros (no 80/20) y filtro Support>=k.
% Bucle guiado REPLICADO aqui (sin tocar guided_search.pl) para separar
% T_gen (enumerar+filtrar patrones) de T_score (puntuar). Budgets como
% dato honesto: TIMEOUT / SKIPPED-BUDGET no abortan la corrida.
:- consult('corpus.pl').

:- use_module(library(lists)).

:- dynamic e54t_held/3.      % hechos del lado test (no ingeridos)
:- dynamic e54t_composed/4.  % e54t_composed(Dir, K, Target, Path)
:- dynamic e54_l2budget/1.   % budget ms acumulado len2 por celda

exp54_3 :-
    consult('kjv_memory.pl'),
    forall(member(Dir, [ot2nt, nt2ot]),
           forall(member(K, [5, 10, 20]),
                  cell(Dir, K))),
    writeln('E54-DONE').

% ---------- celda ----------
cell(Dir, K) :-
    format('E54-CELL dir=~w k=~w~n', [Dir, K]),
    cell_reset,
    dir_split(Dir, NTrain, NTest),
    format('E54-CELL-SPLIT dir=~w train=~w test=~w~n', [Dir, NTrain, NTest]),
    timed_phase(600, discover_concepts, Cms, Cst),
    e54_counts(NC, NM),
    format('E54-CELL-PHASE dir=~w k=~w phase=CONCEPTS concepts=~w members=~w ms=~w status=~w~n',
           [Dir, K, NC, NM, Cms, Cst]),
    timed_phase(300, discover_concept_relations, Rms, Rst),
    format('E54-CELL-PHASE dir=~w k=~w phase=CRELS ms=~w status=~w~n',
           [Dir, K, Rms, Rst]),
    train_targets(8, Targets),
    retractall(e54_l2budget(_)),
    assertz(e54_l2budget(200000)),
    format('E54-CELL-TARGETS dir=~w k=~w ~w~n', [Dir, K, Targets]),
    forall(member(T, Targets),
           (cell_len(Dir, K, T, 1), cell_len(Dir, K, T, 2))),
    cell_rules(Dir, K),
    cell_holdout(Dir, K).

cell_reset :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(e54t_held(_, _, _)),
    retractall(e54t_composed(_, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)).

% ---------- split por libros via proveniencia ----------
% OT = BI 1..39, NT = 40..66. Test = hechos del lado test (todos).
dir_split(ot2nt, NTrain, NTest) :-
    split_books(1, 39, NTrain, NTest).
dir_split(nt2ot, NTrain, NTest) :-
    split_books(40, 66, NTrain, NTest).

split_books(Lo, Hi, NTrain, NTest) :-
    findall((S, R, O, W, U, BI),
            (memfact(S, R, O, W, U),
             provfact(S, R, O, Ref, _, _),
             e54_book(Ref, BI)),
            All),
    split_books_assert(All, Lo, Hi, 0, 0, NTrain, NTest).

split_books_assert([], _, _, NTrain, NTest, NTrain, NTest).
split_books_assert([(S, R, O, W, U, BI)|T], Lo, Hi, A0, B0, NTrain, NTest) :-
    ( BI >= Lo, BI =< Hi ->
        assertz(memory_relation(S, R, O, W, U)),
        ( provfact(S, R, O, Ref, Tm, St) ->
            assertz(prov(S, R, O, info(Ref, Tm, St)))
        ; true
        ),
        A1 is A0 + 1, B1 = B0
    ; assertz(e54t_held(S, R, O)),
      A1 = A0, B1 is B0 + 1
    ),
    split_books_assert(T, Lo, Hi, A1, B1, NTrain, NTest).

e54_book(Ref, BI) :-
    atom(Ref),
    atom_concat(b, Rest, Ref),
    split_string(Rest, "_", "", [BIS|_]),
    number_string(BI, BIS), !.
e54_book(_, 0).

% ---------- fases temporizadas ----------
timed_phase(Secs, Goal, Ms, Status) :-
    statistics(runtime, [T0, _]),
    ( catch(call_with_time_limit(Secs, Goal), E, true) ->
        ( var(E) -> Status = ok ; Status = timeout )
    ; Status = failed
    ),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0.

e54_counts(C, M) :-
    findall(1, concept(_, _, _), Cs), length(Cs, C),
    findall(1, concept_member(_, _, _), Ms), length(Ms, M).

% ---------- targets top por train ----------
train_targets(Max, Targets) :-
    findall(R, memory_relation(_, R, _, _, _), Rs),
    msort(Rs, S),
    e54_clump(S, C),
    e54_sort_desc(C, Desc),
    take_targets(Desc, Max, Targets).

e54_clump([], []).
e54_clump([H|T], [N-H|R]) :-
    e54_run(H, T, N, Rest),
    e54_clump(Rest, R).

e54_run(H, [], 1, []).
e54_run(H, [H|T], N, R) :- !, e54_run(H, T, N0, R), N is N0 + 1.
e54_run(_, L, 1, L).

e54_sort_desc(C, Desc) :-
    findall(N-R, member(N-R, C), P),
    keysort(P, A),
    reverse(A, Desc).

take_targets([], _, []) :- !.
take_targets(_, 0, []) :- !.
take_targets([_-R|T], K, Out) :-
    ( concept_relation(_, R, _, _) ->
        Out = [R|Rest], K1 is K - 1, take_targets(T, K1, Rest)
    ; format('E54-CELL-SKIP-NOCONCEPT ~w~n', [R]),
      take_targets(T, K, Out)
    ).

% ---------- discovery guiado con T_gen/T_score separados ----------
% Budget len2 por celda: 200s acumulados; al agotarse, SKIPPED-BUDGET
% (dato honesto, no aborta la celda).
cell_len(Dir, K, T, 1) :- !,
    cell_len_run(Dir, K, T, 1).
cell_len(Dir, K, T, 2) :-
    e54_l2budget(B),
    ( B =< 0 ->
        format('E54-CELL-COMP dir=~w k=~w target=~w len=2 status=skipped-budget~n',
               [Dir, K, T])
    ; cell_len_run(Dir, K, T, 2)
    ).

cell_len_run(Dir, K, T, Len) :-
    statistics(runtime, [T0, _]),
    discover_split(Dir, K, T, Len, Res),
    statistics(runtime, [T1, _]),
    Wall is T1 - T0,
    ( Res = ok(Path, F1, Sup, Gen, Ev, Pr, MsScore, MsGen) ->
        St = ok
    ; Res = filtered(Sup) ->
        St = filtered-support, Path = [], F1 = 0, Sup = Sup, Gen = 0, Ev = 0, Pr = 0, MsScore = 0, MsGen = 0
    ; Res = no_margin ->
        St = no-margin, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, MsScore = 0, MsGen = 0
    ; St = timeout, Path = [], F1 = 0, Sup = 0, Gen = 0, Ev = 0, Pr = 0, MsScore = 0, MsGen = 0
    ),
    ( St == ok ->
        retractall(e54t_composed(Dir, K, T, _)),
        retractall(composed_rule(T, _, _)),
        assertz(e54t_composed(Dir, K, T, Path)),
        assertz(composed_rule(T, Path, F1))
    ; true
    ),
    format('E54-CELL-COMP dir=~w k=~w target=~w len=~w path=~w f1=~4f sup=~w gen=~w eval=~w pruned=~w tgen=~w tscore=~w wall=~w status=~w~n',
           [Dir, K, T, Len, Path, F1, Sup, Gen, Ev, Pr, MsGen, MsScore, Wall, St]),
    ( Len == 2 ->
        retract(e54_l2budget(B0)), B1 is B0 - Wall, assertz(e54_l2budget(B1))
    ; true
    ).

% discover_split: misma semantica que run_discovery(guided) pero con
% temporizadores separados y filtro Support>=K. Siempre sucede.
discover_split(Dir, K, T, Len, Res) :-
    ( concept_relation(SC, T, OC, _) ->
        findall(S, concept_member(SC, S, _), SS0),
        findall(O, concept_member(OC, O, _), OS0),
        sort(SS0, SS), sort(OS0, OS),
        get_time(G0),
        ( catch(call_with_time_limit(120,
                    ( incident_vocab(SS, OS, T, Vocab),
                      findall(L, between(1, Len, L), Lens),
                      findall(P, (member(Ln, Lens),
                                  pattern(Ln, Vocab, P)), Gen0),
                      include(pattern_ok_ss_os(SS, OS), Gen0, Valid)
                    )), EG, true) ->
            ( var(EG) -> Gstat = ok ; Gstat = timeout )
        ; Gstat = failed
        ),
        get_time(G1),
        MsGen is round((G1 - G0) * 1000),
        ( Gstat \== ok -> Res = timeout
        ; length(Gen0, Gen),
          length(Valid, Ev),
          Pr is Gen - Ev,
          get_time(S0),
          ( catch(call_with_time_limit(120,
                      findall(F1-P-Sup,
                              (member(P, Valid),
                               score_path(T, SS, OS, P, F1, Sup)),
                              Scored)), ES, true) ->
              ( var(ES) -> Sstat = ok ; Sstat = timeout )
          ; Sstat = failed
          ),
          get_time(S1),
          MsScore is round((S1 - S0) * 1000),
          ( Sstat \== ok -> Res = timeout
          ; keysort(Scored, Sorted),
            reverse(Sorted, Ranked),
            ( Ranked = [BF-BP-BS|Rest] ->
                ( Rest = [SF-_-_|_] -> true ; SF = 0.0 ),
                Margin is BF - SF,
                ( Margin >= 0.30, BF >= 0.70 ->
                    ( BS >= K ->
                        Res = ok(BP, BF, BS, Gen, Ev, Pr, MsScore, MsGen)
                    ; Res = filtered(BS)
                    )
                ; Res = no_margin
                )
            ; Res = no_margin
            )
          )
        )
    ; Res = no_margin
    ).

% ---------- reglas ----------
cell_rules(Dir, K) :-
    findall((T, P), e54t_composed(Dir, K, T, P), Ws),
    format('E54-CELL-RULES dir=~w k=~w winners=~w~n', [Dir, K, Ws]),
    forall(member((T, P), Ws), cell_rule(Dir, K, T, P)).

cell_rule(Dir, K, T, Path) :-
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
        findall(BI, (member((S-O), SupL),
                     prov(S, T, O, info(Ref, _, _)),
                     e54_book(Ref, BI)), BI0),
        sort(BI0, BIs),
        findall(SC-OC, concept_relation(SC, T, OC, _), CRs),
        length(CRs, NC)
    ; NSup = 0, BIs = [], NC = 0
    ),
    format('E54-CELL-RULE dir=~w k=~w target=~w path=~w sig=~w support=~w books=~w concepts=~w ms=~w status=~w~n',
           [Dir, K, T, Path, Sig, NSup, BIs, NC, Ms, St]).

% ---------- holdout direccional ----------
cell_holdout(Dir, K) :-
    findall((T, P), (e54t_composed(Dir, K, T, P),
                     constrained_rule(T, P, _)), Rules),
    length(Rules, NR),
    statistics(runtime, [T0, _]),
    findall(TP-FP-FN, (member((T, P), Rules), rule_scores(Dir, K, T, P, TP, FP, FN)), Rows),
    sum3(Rows, TTP, TFP, TFN),
    e54_f1(TTP, TFP, TFN, P, R, F1),
    statistics(runtime, [T1, _]),
    Ms is T1 - T0,
    format('E54-CELL-HOLDOUT dir=~w k=~w rules=~w tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f ms=~w~n',
           [Dir, K, NR, TTP, TFP, TFN, P, R, F1, Ms]).

rule_scores(Dir, K, T, Path, TP, FP, FN) :-
    findall((S-O), reuse_predict(S, T, O), Pred0),
    sort(Pred0, Pred),
    findall(1, (member((S-O), Pred), e54t_held(S, T, O)), TPs),
    length(TPs, TP),
    findall(1, (member((S-O), Pred),
                \+ memfact(S, T, O, _, _)), FPs),
    length(FPs, FP),
    findall(1, (e54t_held(S, T, O),
                once(full_bindings(S, O, Path, _)),
                \+ member((S-O), Pred)), FNs),
    length(FNs, FN),
    e54_f1(TP, FP, FN, P, R, F1),
    findall(BI, (member((S-O), Pred), e54t_held(S, T, O),
                 provfact(S, T, O, Ref, _, _), e54_book(Ref, BI)), TBI0),
    sort(TBI0, TBIs),
    format('E54-CELL-RULE-HOLDOUT dir=~w k=~w target=~w path=~w tp=~w fp=~w fn=~w P=~4f R=~4f F1=~4f testbooks=~w~n',
           [Dir, K, T, Path, TP, FP, FN, P, R, F1, TBIs]).

e54_f1(TP, FP, FN, P, R, F1) :-
    ( TP + FP =:= 0 -> P = 0.0 ; P is TP / (TP + FP) ),
    ( TP + FN =:= 0 -> R = 0.0 ; R is TP / (TP + FN) ),
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ).

sum3([], 0, 0, 0).
sum3([A-B-C|T], SA, SB, SC) :-
    sum3(T, RA, RB, RC),
    SA is RA + A, SB is RB + B, SC is RC + C.
