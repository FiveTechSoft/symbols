% experiment_world.pl â€” EXP-SYMBOLIC-REPLAY v0
% Meta-learning over the project's real experiment history.
% Question: can symbolic replay retrospectively distinguish
% exploration strategies from the real history alone?
%
% Run: swipl -f experiment_world.pl -g "criterio_final" -t halt

:- module(experiment_world, [
    experiment/5,
    strategy/2,
    score/2,
    branch_score/2,
    ancestor_or_self/2,
    replay/2,
    train_test_split/3
]).

% ============================================================
% 1. FACTS: REAL EXPERIMENT HISTORY (from STATE.md + AgentBrain)
%
% experiment(ID, Hypothesis, Result, Cost, Parent)
% Result: success | marginal | fail | closed
% Cost: relative units (session-days of work, integer >= 1 for
%       real experiments; 0 reserved for bookkeeping nodes)
% Parent: experiment this one derives from, or none
% ============================================================

% --- Phase 1: foundations (each was a working step) ---
experiment(exp1,  relations_basic,     success,  1, none).
experiment(exp2,  relations_inference, success,  2, exp1).
experiment(exp3,  embeddings_32d,      success,  3, exp2).
experiment(exp4,  persistence_v1,      success,  2, exp3).
experiment(exp5,  persistence_v2,      success,  3, exp4).

% --- Phase 2: learning ---
experiment(exp22, learn_new_relations, success,  5, exp5).
experiment(exp23, attributes_types,    success,  4, exp22).
experiment(exp24, multi_hop,           success,  6, exp23).
experiment(exp25, generalization,      success,  8, exp24).

% --- Phase 3: parser ---
experiment(r12c, parser_particle_skip, marginal, 12, exp25).

% --- Phase 4: attention ---
experiment(jonas,     attention_jonas,     success, 3, r12c).
experiment(multifact, attention_multifact, fail,    4, jonas).
experiment(kjv,       attention_kjv_noisy, fail,    5, multifact).

% --- bookkeeping closure (not a real experiment) ---
experiment(attention_closed, attention_line, closed, 0, kjv).

% ============================================================
% 2. STRATEGIES
% ============================================================

strategy(continue_last, [criterion:last_experiment]).
strategy(best_ratio,    [criterion:success_cost_ratio]).
strategy(deepest,       [criterion:max_depth]).
strategy(most_success,  [criterion:cumulative_success]).

% ============================================================
% 3. SCORE METRICS
% ============================================================

result_value(success,  1.0).
result_value(marginal, 0.3).
result_value(fail,     0.0).
result_value(closed,   0.0).   % bookkeeping node, no free value

score(ID, Score) :-
    experiment(ID, _, Result, Cost, _),
    result_value(Result, Value),
    (   Cost > 0
    ->  Score is Value / Cost
    ;   Score = Value
    ).

ancestor_or_self(ID, ID).
ancestor_or_self(ID, Anc) :-
    experiment(ID, _, _, _, Parent),
    Parent \= none,
    ancestor_or_self(Parent, Anc).

chain_list(none, []) :- !.
chain_list(ID, [ID|Rest]) :-
    experiment(ID, _, _, _, Parent),
    chain_list(Parent, Rest).

branch_score(ID, Total) :-
    findall(S, (ancestor_or_self(ID, A), score(A, S)), Ss),
    sum_list(Ss, Total).

depth(ID, 0) :-
    experiment(ID, _, _, _, none), !.
depth(ID, D) :-
    experiment(ID, _, _, _, Parent),
    Parent \= none,
    depth(Parent, PD),
    D is PD + 1.

% ============================================================
% 4. REPLAY (literal design v0): each strategy selects a
%    subset of experiments; score = sum of individual scores.
% ============================================================

replay(Strategy, TotalScore) :-
    strategy(Strategy, _),
    findall(ID, experiment(ID, _, _, _, _), AllIDs),
    simulate(Strategy, AllIDs, Selected),
    maplist(score, Selected, Scores),
    sum_list(Scores, TotalScore).

simulate(continue_last, _AllIDs, Selected) :-
    last_experiment(Last),
    ancestor_or_self(Last, Anc),
    chain_list(Anc, Selected).

simulate(best_ratio, AllIDs, Selected) :-
    findall(ID-S, (member(ID, AllIDs), score(ID, S)), Pairs),
    (   Pairs == []
    ->  Selected = []
    ;   sort(2, @>=, Pairs, Sorted),
        Sorted = [Best-_|_],
        Selected = [Best]
    ).

simulate(deepest, _AllIDs, Selected) :-
    findall(ID-D, (experiment(ID, _, _, _, _), depth(ID, D)), Pairs),
    (   Pairs == []
    ->  Selected = []
    ;   sort(2, @>=, Pairs, Sorted),
        Sorted = [Deepest-_|_],
        ancestor_or_self(Deepest, Anc),
        chain_list(Anc, Selected)
    ).

simulate(most_success, _AllIDs, Selected) :-
    findall(ID-S, (experiment(ID, _, _, _, _), branch_score(ID, S)), Pairs),
    (   Pairs == []
    ->  Selected = []
    ;   sort(2, @>=, Pairs, Sorted),
        Sorted = [Best-_|_],
        ancestor_or_self(Best, Anc),
        chain_list(Anc, Selected)
    ).

last_experiment(ID) :-
    findall(ID-D, (experiment(ID, _, _, _, _), depth(ID, D)), Pairs),
    sort(2, @>=, Pairs, [ID-_|_]).

train_test_split(Train, Test, Ratio) :-
    findall(ID, experiment(ID, _, _, _, _), AllIDs),
    length(AllIDs, N),
    Split is round(N * Ratio),
    length(Train, Split),
    append(Train, Test, AllIDs).

% ============================================================
% 5. FALSIFIABLE PREDICTION (v0)
%    Because the history is a single chain (no fork points),
%    continue_last == deepest == most_success by construction.
% ============================================================

criterio_final :-
    write('=== EXP-SYMBOLIC-REPLAY v0 (literal design) ==='), nl, nl,
    findall(S-Score, (strategy(S, _), replay(S, Score)), Pairs),
    forall(member(S-Score, Pairs),
           format('replay(~w) = ~2f~n', [S, Score])),
    findall(S, (member(S1-S1V, Pairs), member(S2-S2V, Pairs),
                S1 @< S2, S1V =\= S2V), Diffs),
    (   Diffs == []
    ->  write('PREDICTION CONFIRMED: all strategies identical (chain degeneracy).'), nl
    ;   write('UNEXPECTED: strategies differ: '), write(Diffs), nl
    ),
    halt.