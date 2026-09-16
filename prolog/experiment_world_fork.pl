% experiment_world_fork.pl — EXP-SYMBOLIC-REPLAY v1
% Question: can the real history, WITHOUT seeing the future,
% distinguish between real alternatives at the 3 frozen forks?
%
% Frozen design (user):
%   - exactly 3 forks: EXP-25, Jonás, MULTI-FACT
%   - historia_pre_fork/2 guarantees no post-fork info enters prediction
%   - predecir_alternativa/3 is isolated per fork
%   - actual is NEVER consulted during prediction (only in scoring)
%   - alternativa->area is decision-time metadata (what the option WAS),
%     NOT alternativa->posterior experiments (what it produced)
%   - LOO: prediction for fork F uses only events with index < F's index
%   - criterion applied ONCE: best replay > best baseline + 0.10
%
% Run: swipl -f experiment_world_fork.pl -g "experiment_world_fork:run_v1" -t halt

:- module(experiment_world_fork, [
    ev/5,
    forkpoint/5,
    alt_area/3,
    historia_pre_fork/2,
    predecir_alternativa/3,
    precision/2,
    run_v1/0
]).

% ============================================================
% 1. TIMELINE (real history; results/costs identical to
%    experiment_world.pl @ e1ec3de, cross-checked)
%    ev(ID, Area, Result, Cost, TimelineIndex)
% ============================================================

ev(exp1,  motor,       success,  1,  0).
ev(exp2,  motor,       success,  2,  1).
ev(exp3,  motor,       success,  3,  2).
ev(exp4,  motor,       success,  2,  3).
ev(exp5,  motor,       success,  3,  4).
ev(exp22, aprendizaje, success,  5,  5).
ev(exp23, aprendizaje, success,  4,  6).
ev(exp24, aprendizaje, success,  6,  7).
ev(exp25, aprendizaje, success,  8,  8).
% r5 = commit f1bcd79: r5_first_object, HUGE 71.41 -> 88.15 coverage
ev(r5,    parser,      success,  4,  9).
% --- FORK f1 (after EXP-25 closed; actual = parser: R12-C was executed
%     before any attention work, STATE.md delta section + session dates)
% --- FORK f1 @ 10
ev(r12c,  parser,      marginal, 12, 11).
% attint = attention module integrated + benchmark_ab 27/30 identical
% in all conditions (works, no delta demonstrated)
ev(attint, atencion,   success,  2, 12).
% Jonás A/B executed and diagnosed (18/20, fails attributed by level)
ev(jonas, atencion,    success,  3, 13).
% --- FORK f2 @ 14
% fixab = FIX-A in focus_answer (chat_attention.pl, attention-layer
% coverage) + FIX-B anti-alias; Jonás 20/20 (STATE.md FIX-A/B section)
ev(fixab, atencion,    success,  1, 15).
% MULTI-FACT A/B: 18/18 in all conditions, delta 0 on clean KB
ev(multifact, atencion, fail,    4, 16).
% --- FORK f3 @ 17
% KJV A/B: delta 0 once the direct path covered what-V-E
ev(kjv,   atencion,    fail,     5, 18).
ev(attclosed, atencion, closed,   0, 19).

% ============================================================
% 2. FORKS (frozen: exactly the 3 user-designated forks)
%    forkpoint(F, Anchor, [StayAlternative, ChangeAlternative],
%              Actual, TimelineIndex)
%    Actual is read ONLY by fork_actual/2 in scoring.
%    Sources: f1 = R12-C executed first (STATE.md git table);
%             f2 = FIX-A implemented in focus_answer = attention-layer
%                  coverage (STATE.md FIX-A); f3 = KJV A/B ran before
%                  the closure decision (STATE.md KJV + Próxima sesión).
% ============================================================

forkpoint(f1, exp25,     [continuar_parser, continuar_atencion],       continuar_parser,          10).
forkpoint(f2, jonas,     [ampliar_cobertura, fix_directo],             ampliar_cobertura,         14).
forkpoint(f3, multifact, [continuar_atencion_texto, cerrar_atencion],  continuar_atencion_texto,  17).

% Decision-time property of each alternative: which line it extends.
% This is what the option WAS at decision time (pre-fork metadata),
% not what it later produced.
alt_area(f1, continuar_parser,          parser).
alt_area(f1, continuar_atencion,        atencion).
alt_area(f2, ampliar_cobertura,         atencion).
alt_area(f2, fix_directo,               camino_directo).
alt_area(f3, continuar_atencion_texto,  atencion).
alt_area(f3, cerrar_atencion,           atencion).

fork_actual(F, A) :- forkpoint(F, _, _, A, _).

% ============================================================
% 3. PRE-FORK VIEW (leakage-free by construction)
% ============================================================

historia_pre_fork(F, Events) :-
    forkpoint(F, _, _, _, FIdx),
    findall(ev(ID, Area, Res, C, Idx),
            (ev(ID, Area, Res, C, Idx), Idx < FIdx),
            Events).

result_value(success,  1.0).
result_value(marginal, 0.3).
result_value(fail,     0.0).
result_value(closed,   0.0).

% ============================================================
% 4. STRATEGIES (frozen: S1 continue-last, S2 best value/cost).
%    Any ambiguity or missing evidence => abstain (counted wrong).
%    No heuristics to rescue predictions.
% ============================================================

% S1: alternative in the area of the most recent pre-fork event;
% requires a UNIQUE area match, else abstain.
predecir_alternativa(s1_inercia, F, Pred) :-
    historia_pre_fork(F, Es),
    Es \= [],
    last_event(Es, ev(_, Area, _, _, _)),
    alt_area(F, Pred, Area),
    findall(A, alt_area(F, A, Area), [Pred]).

% S2: alternative whose area has the best pre-fork value/cost ratio;
% areas with no pre-fork events are unusable; ties => abstain.
predecir_alternativa(s2_ratio, F, Pred) :-
    historia_pre_fork(F, Es),
    forkpoint(F, _, Alts, _, _),
    findall(Alt-R,
            (member(Alt, Alts),
             alt_area(F, Alt, Area),
             area_ratio(Es, Area, R)),
            Pairs),
    Pairs \= [],
    sort(2, @>=, Pairs, [Pred-R|Rest]),
    \+ (member(_-R2, Rest), R2 >= R).  % unique best, no tie

% Baselines (deterministic; random coin = 0.5 expected, reported as such)
predecir_alternativa(b_primera, F, Pred) :- forkpoint(F, _, [Pred|_], _, _).
predecir_alternativa(b_segunda, F, Pred) :- forkpoint(F, _, [_, Pred], _, _).

area_ratio(Es, Area, R) :-
    findall(V-C, (member(ev(_, Area, Res, C, _), Es),
                  result_value(Res, V)), Pairs),
    Pairs \= [],
    maplist(arg(1), Pairs, Vs),
    maplist(arg(2), Pairs, Cs),
    sum_list(Vs, SV),
    sum_list(Cs, SC),
    SC > 0,
    R is SV / SC.

last_event(Es, E) :-
    findall(Idx-Ev, (member(Ev, Es), Ev = ev(_, _, _, _, Idx)), Pairs),
    sort(1, @>=, Pairs, [_-E|_]).

% ============================================================
% 5. SCORING (actual used ONLY here)
% ============================================================

correct(S, F, 1) :-
    predecir_alternativa(S, F, Pred),
    fork_actual(F, Pred), !.
correct(_, _, 0).

precision(S, P) :-
    findall(C, (forkpoint(F, _, _, _, _), correct(S, F, C)), Cs),
    length(Cs, N),
    N > 0,
    sum_list(Cs, Sum),
    P is Sum / N.

% ============================================================
% 6. LEAKAGE TESTS (must pass BEFORE any result is computed)
% ============================================================

loo_no_ve_actual :-
    forall(forkpoint(F, _, _, _, _),
           (   historia_pre_fork(F, Es),
               forkpoint(F, _, _, _, FIdx),
               forall(member(E, Es),
                      (   E = ev(_, _, _, _, Idx),
                          Idx < FIdx
                      )),
               \+ member(forkpoint(F, _, _, _, _), Es),
               \+ member(actual(F, _), Es)
           )).

loo_no_descendientes :-
    forall(forkpoint(F, _, _, _, FIdx),
           (   historia_pre_fork(F, Es),
               \+ (member(E, Es), E = ev(_, _, _, _, Idx), Idx >= FIdx)
           )).

% ============================================================
% 7. RUNNER: criterion applied exactly once, at the end
% ============================================================

run_v1 :-
    write('=== EXP-SYMBOLIC-REPLAY v1 (fork points + LOO) ==='), nl, nl,
    t_run(loo_no_ve_actual, T1),
    t_run(loo_no_descendientes, T2),
    report_test(leakage_tests, [T1, T2]),
    (   member(fail, [T1, T2])
    ->  write('LEAKAGE DETECTED: aborting before any result.'), nl, halt(2)
    ;   true
    ),
    nl, write('--- predictions per fork ---'), nl,
    forall(forkpoint(F, _, _, A, _),
           (   write(F), write(' actual='), write(A), nl,
               forall(member(S, [s1_inercia, s2_ratio, b_primera, b_segunda]),
                      (   (   predecir_alternativa(S, F, P)
                          ->  true
                          ;   P = abstain
                          ),
                          write('  '), write(S), write(' -> '), write(P), nl)
               )
           )),
    nl, write('--- precisions (3 forks) ---'), nl,
    forall(member(S, [s1_inercia, s2_ratio, b_primera, b_segunda]),
           (   precision(S, P),
               format('  ~w = ~2f~n', [S, P])
           )),
    precision(s1_inercia, P1),
    precision(s2_ratio, P2),
    precision(b_primera, PB1),
    precision(b_segunda, PB2),
    BestReplay is max(P1, P2),
    BestBase is max(PB1, PB2),
    Random is 0.5,
    format('~nbest replay = ~2f | best baseline = ~2f | random expectation = ~2f~n',
           [BestReplay, BestBase, Random]),
    Margin is BestBase + 0.10,
    (   BestReplay > Margin
    ->  format('DECISION: CONTINUAR (replay ~2f > baseline ~2f + 0.10)~n',
               [BestReplay, BestBase]),
        halt(0)
    ;   format('DECISION: CERRAR (replay ~2f <= baseline ~2f + 0.10)~n',
               [BestReplay, BestBase]),
        halt(1)
    ).

report_test(Label, Tests) :-
    forall(member(T, Tests), report_one(T)),
    (   \+ member(fail, Tests)
    ->  write(Label), write(': ALL PASS'), nl
    ;   write(Label), write(': SOME FAIL'), nl
    ).

report_one(pass) :- write('PASS'), nl.
report_one(fail) :- write('FAIL'), nl.

t_run(Goal, pass) :-
    catch(once(Goal), Err, (write('error: '), write(Err), nl, fail)), !.
t_run(_, fail).