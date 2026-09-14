% multi_head.pl — Symbolic Multi-Head Attention for Relation Extraction
%
% 5 heads scoring candidate relations:
%   Head 1: Relation (verb quality)
%   Head 2: Entity (plausibility)
%   Head 3: Position (proximity)
%   Head 4: Discourse (context)
%   Head 5: Novelty (new information)
%
% Attention(relation) = wR*relation + wE*entity + wP*position + wD*discourse + wN*novelty
%
% Attention selects. Prolog reasons. Graph remembers.

:- consult('relation_extractor.pl').

% ── Head weights (adjustable) ───────────────────────────────────────
head_weight(relation, 0.30).
head_weight(entity, 0.25).
head_weight(position, 0.15).
head_weight(discourse, 0.15).
head_weight(novelty, 0.15).

% ── multi_head_attention/3 ──────────────────────────────────────────
% multi_head_attention(+Candidates, -Scored, -Summary)
% Scores each candidate using all 5 heads, returns sorted by total score.

multi_head_attention(Candidates, Scored, Summary) :-
    % Score each candidate with all heads
    findall(scored_candidate(S, V, O, TotalScore, Heads), (
        member(Candidate, Candidates),
        Candidate = candidate(S, V, O, _, _, _),
        candidate_score_factors(Candidate, R, E, P, D, T, N),
        % Apply head weights
        head_weight(relation, WR),
        head_weight(entity, WE),
        head_weight(position, WP),
        head_weight(discourse, WD),
        head_weight(novelty, WN),
        TotalScore is R*WR + E*WE + P*WP + D*WD + N*WN,
        Heads = heads(R, E, P, D, T, N)
    ), Scored),
    % Sort by total score descending
    sort(4, @>=, Scored, Sorted),
    % Generate summary
    length(Candidates, NumCandidates),
    length(Sorted, NumScored),
    ( Sorted = [scored_candidate(_, _, _, TopScore, _)|_] ->
        TopScored = TopScore
    ;
        TopScored = 0
    ),
    Summary = summary(NumCandidates, NumScored, TopScored).

% ── top_k_candidates/3 ──────────────────────────────────────────────
% top_k_candidates(+Scored, +K, -Selected)

top_k_candidates(Scored, K, Selected) :-
    ( length(Scored, N), N =< K ->
        Selected = Scored
    ;
        length(Selected, K),
        append(Selected, _, Scored)
    ).

% ── attention_summary/2 ─────────────────────────────────────────────
% Pretty-print attention results

attention_summary(Scored, Lines) :-
    findall(Line, (
        member(scored_candidate(S, V, O, Score, heads(R, E, P, D, T, N)), Scored),
        format(atom(Line), '[~2f] ~w ~w ~w  (R:~2f E:~2f P:~2f D:~2f T:~2f N:~2f)',
               [Score, S, V, O, R, E, P, D, T, N])
    ), Lines).

% ── process_sentence/3 — Full pipeline ──────────────────────────────
% process_sentence(+Tokens, -Results, -Stats)
% Complete pipeline: tokens → candidates → attention → top-k → verify

process_sentence(Tokens, Results, stats(NumCandidates, NumTopK, NumVerified, NumNew, TimeMs)) :-
    % Time the process
    get_time(Start),
    % Step 1: Extract candidate relations
    extract_candidates(Tokens, Candidates),
    length(Candidates, NumCandidates),
    % Step 2: Multi-head attention
    multi_head_attention(Candidates, Scored, _),
    % Step 3: Top-K selection (K=5)
    top_k_candidates(Scored, 5, TopK),
    length(TopK, NumTopK),
    % Step 4: Verify each candidate
    findall(verified(S, V, O, Status), (
        member(scored_candidate(S, V, O, _, _), TopK),
        verify_candidate(S, V, O, Status)
    ), Verified),
    % Count results
    include(status_new, Verified, NewFacts),
    length(NewFacts, NumNew),
    include(status_verified, Verified, VerifiedFacts),
    length(VerifiedFacts, NumVerified),
    Results = results(Candidates, TopK, Verified),
    % Time
    get_time(End),
    TimeMs is round((End - Start) * 1000).

status_new(verified(_, _, _, new)).
status_verified(verified(_, _, _, known)).

% ── verify_candidate/4 ──────────────────────────────────────────────
% verify_candidate(+S, +V, +O, -Status)
% Checks if candidate is known, contradicted, or new.

verify_candidate(S, V, O, known) :-
    known_relation(S, V, O), !.
verify_candidate(S, V, O, contradicted) :-
    % Check for opposite relation
    ( known_relation(S, not_V, O) ; known_relation(O, V, S) ), !.
verify_candidate(S, V, O, new) :-
    % Not known, not contradicted → new information
    atom(S), atom(V), atom(O),
    % Basic sanity checks
    S \== O,
    V \== S,
    V \== O, !.
verify_candidate(_, _, _, rejected).

% ── store_verified/2 ────────────────────────────────────────────────
% store_verified(+Verified, -Stored)
% Stores new facts in the knowledge base.

store_verified(Verified, Stored) :-
    findall(stored(S, V, O), (
        member(verified(S, V, O, new), Verified),
        assertz(known_relation(S, V, O)),
        assertz(memory_relation(S, V, O, 1, learned))
    ), Stored).

% ── explain_decision/2 ──────────────────────────────────────────────
% explain_decision(+ScoredCandidate, -Explanation)

explain_decision(scored_candidate(S, V, O, Score, heads(R, E, P, D, T, N)), Explanation) :-
    format(atom(Explanation),
           '~w ~w ~w (score: ~2f)~n  Relation: ~2f  Entity: ~2f  Position: ~2f~n  Discourse: ~2f  Temporal: ~2f  Novelty: ~2f',
           [S, V, O, Score, R, E, P, D, T, N]).
