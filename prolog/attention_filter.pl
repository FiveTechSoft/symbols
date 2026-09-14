% attention_filter.pl — Attention-based filtering for KB extraction
% Scores candidate triples and keeps only those above threshold.
%
% Usage:
%   filter_triples(Candidates, Filtered, Stats)
%
% Stats = stats(Total, Kept, Removed, AvgScore)

:- consult('symbolic_attention.pl').

:- dynamic memory_relation/5.

% ── triple_score/2 ──────────────────────────────────────────────────
% Score a candidate triple (S, V, O) for quality/importance.

triple_score(S, V, O, Score) :- !,
    % Factor 1: Entity importance (log scale)
    entity_score(S, SS),
    entity_score(O, OS),
    % Factor 2: Relation clarity (verb is a real verb, not noise)
    verb_score(V, VS),
    % Factor 3: Coherence (connects to existing KB)
    coherence_score(S, O, CS),
    % Factor 4: Simplicity (short entities are cleaner)
    simplicity_score(S, O, SimS),
    % Weighted combination
    Score is SS * 0.25 + OS * 0.25 + VS * 0.20 + CS * 0.20 + SimS * 0.10.

% ── entity_score/2 ──────────────────────────────────────────────────
% Score an entity based on its connections in KB.

entity_score(Entity, Score) :-
    findall(1, memory_relation(Entity, _, _, _, _), Out),
    findall(1, memory_relation(_, _, Entity, _, _), In),
    length(Out, NO),
    length(In, NI),
    Total is NO + NI,
    ( Total > 0 ->
        Score is min(1.0, log(1 + Total) / log(10))
    ;
        Score = 0.1  % unknown entity, low but not zero
    ).

% ── verb_score/2 ────────────────────────────────────────────────────
% Score a verb: real action verbs score high, noise scores low.

verb_score(V, Score) :-
    atom(V), !,
    % Check if verb has conjugations or is a known word
    ( irregular(V, _) ; irregular(_, V) ->
        Score = 0.9
    ; normalize_verb(V, Norm), Norm \== V ->
        Score = 0.8
    ; % Check length and structure
      atom_length(V, Len),
      ( Len >= 3, Len =< 15 ->
          % Check if it looks like a verb (no special chars)
          atom_codes(V, Codes),
          \+ member(95, Codes),  % no underscores
          \+ member(32, Codes),  % no spaces
          Score = 0.6
      ;
          Score = 0.3
      )
    ).
verb_score(_, 0.2) :- !.

% ── coherence_score/3 ───────────────────────────────────────────────
% Score how well S and O connect to existing knowledge.

coherence_score(S, O, Score) :-
    ( memory_relation(S, _, O, _, _) -> !,
        Score = 1.0
    ; findall(1, (
        memory_relation(S, _, X, _, _),
        memory_relation(O, _, X, _, _)
      ), Shared1),
      findall(1, (
        memory_relation(X, _, S, _, _),
        memory_relation(X, _, O, _, _)
      ), Shared2),
      append(Shared1, Shared2, AllShared),
      length(AllShared, SharedCnt),
      ( SharedCnt > 0 -> !,
          Score is min(1.0, 0.5 + SharedCnt * 0.1)
      ; !,
          Score = 0.2
      )
    ).
coherence_score(_, _, 0.2) :- !.

% ── simplicity_score/3 ──────────────────────────────────────────────
% Score based on entity length (shorter = cleaner).

simplicity_score(S, O, Score) :-
    atom_length(S, SL),
    atom_length(O, OL),
    AvgLen is (SL + OL) / 2,
    ( AvgLen =< 10 -> Score = 1.0
    ; AvgLen =< 20 -> Score = 0.7
    ; AvgLen =< 30 -> Score = 0.4
    ; Score = 0.2
    ).

% ── filter_triples/3 ────────────────────────────────────────────────
% Filter a list of candidate triples using attention scoring.

filter_triples(Candidates, Filtered, stats(Total, Kept, 0, 0)) :-
    length(Candidates, Total),
    % Score each candidate
    findall(Key-Val, (
        member(S-V-O, Candidates),
        triple_score(S, V, O, Score),
        atomic_list_concat([S,V,O], '_', Key),
        Val = triple(S, V, O, Score)
    ), Scored),
    % Sort by score descending
    keysort(Scored, AscSorted),
    reverse(AscSorted, Sorted),
    % Filter by threshold
    include(above_threshold(0.3), Sorted, KeptPairs),
    % Extract clean triples
    findall(triple(S, V, O), member(_-triple(S, V, O, _), KeptPairs), Filtered),
    length(KeptPairs, Kept).

above_threshold(Threshold, _-triple(_, _, _, Score)) :- Score >= Threshold.

% ── filter_kb/2 ─────────────────────────────────────────────────────
% Filter all facts in current KB using attention.

filter_kb(FilteredKB, Stats) :-
    findall(S-V-O, memory_relation(S, V, O, _, _), AllTriples),
    filter_triples(AllTriples, FilteredKB, Stats).

% ── export_filtered/2 ───────────────────────────────────────────────
% Export filtered KB to a .knowledge.pl file.

export_filtered(FilteredKB, Filename) :-
    open(Filename, write, Stream),
    format(Stream, '% Auto-filtered by symbolic attention~n', []),
    format(Stream, '% Threshold: 0.3~n~n', []),
    forall(
        member(S-V-O, FilteredKB),
        format(Stream, 'memfact(~w,~w,~w,1,filtered).~n', [S, V, O])
    ),
    close(Stream).

% ── analyze_noise/1 ─────────────────────────────────────────────────
% Analyze what the filter removed.

analyze_noise(Candidates, Filtered) :-
    % Find removed triples
    subtract(Candidates, Filtered, Removed),
    length(Removed, RemCnt),
    length(Filtered, KeptCnt),
    format('Removed ~w triples (kept ~w)~n', [RemCnt, KeptCnt]),
    nl,
    % Show worst removed (lowest score)
    findall(Score-S-V-O, (
        member(S-V-O, Removed),
        triple_score(S, V, O, Score)
    ), ScoredRemoved),
    sort(4, @=<, ScoredRemoved, WorstRemoved),
    write('Worst removed (noise):'), nl,
    (   member(scored(S, V, O, Score), WorstRemoved),
        I < 10,
        format('  [~2f] ~w ~w ~w~n', [Score, S, V, O]),
        I2 is I + 1,
        fail ; true
    ),
    nl,
    % Show best kept
    findall(Score-S-V-O, (
        member(S-V-O, Filtered),
        triple_score(S, V, O, Score)
    ), ScoredFiltered),
    sort(4, @>=, ScoredFiltered, BestKept),
    write('Best kept (signal):'), nl,
    (   member(scored(S, V, O, Score), BestKept),
        I2 < 10,
        format('  [~2f] ~w ~w ~w~n', [Score, S, V, O]),
        I3 is I2 + 1,
        fail ; true
    ).
