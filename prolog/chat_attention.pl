% chat_attention.pl — Adaptador: chat.pl → symbolic_attention.pl
% v3: Real temperature, question-conditioned relations, top-k by score.
%
% Temperature:
%   T < 1 → concentrate (fewer candidates, higher confidence)
%   T > 1 → spread (more candidates, lower confidence)
%   T = 1 → neutral
%
% Top-K:
%   Always returns the K highest-scored candidates, guaranteed sorted.
%
% Parameters (assert/retract):
%   attention_temperature(T)  — default 1.0
%   attention_top_k(K)        — default 5
%   attention_min_score(S)    — default 0.0

:- consult('symbolic_attention.pl').

% ── Defaults ─────────────────────────────────────────────────────────
:- dynamic attention_enabled/0.
:- dynamic attention_temperature/1.
:- dynamic attention_top_k/1.
:- dynamic attention_min_score/1.

attention_temperature(1.0).
attention_top_k(5).
attention_min_score(0.0).

% ── symbolic_temperature/3 ──────────────────────────────────────────
% Real temperature: softmax-like normalization.
% Handles both attention(Name,Pos,Score) and relation(S,V,O,Score).

symbolic_temperature([], _T, []).
symbolic_temperature(List, T, Result) :-
    List \= [],
    % Step 1: extract scores from any element type
    findall(score_pair(Elem, ExpVal), (
        member(Elem, List),
        extract_score(Elem, Score),
        ExpVal is exp(Score / T)
    ), PairList),
    % Step 2: sum all exp values
    foldl(plus_exp_pair, PairList, 0, TotalExp),
    % Step 3: compute normalized probability for each
    maplist(normalize_pair(TotalExp), PairList, NormPairs),
    % Step 4: rebuild list with normalized scores
    pairs_values(NormPairs, Result).

extract_score(attention(_, _, Score), Score).
extract_score(relation(_, _, _, Score), Score).

plus_exp_pair(score_pair(_, E), Acc, Result) :- Result is Acc + E.

normalize_pair(Total, score_pair(Elem, E), NormElem-Elem) :-
    Norm is E / Total,
    % Rebuild element with normalized score
    rebuild_elem(Elem, Norm, NormElem).

rebuild_elem(attention(N, P, _), Norm, attention(N, P, Norm)).
rebuild_elem(relation(S, V, O, _), Norm, relation(S, V, O, Norm)).

% ── symbolic_topk/4 — Top-K guaranteed by score ─────────────────────
% Precondition: List must be sorted by score descending.
% Postcondition: Result = K highest-scoring items, sorted descending.

symbolic_topk(List, TopK, MinScore, Result) :-
    % Filter by threshold
    include(above_min_score(MinScore), List, Filtered),
    % Sort descending by score (safety: re-sort to guarantee invariant)
    sort_by_score_desc(Filtered, Sorted),
    % Take top K
    take_top(TopK, Sorted, Result).

above_min_score(MinScore, attention(_, _, Score)) :- Score >= MinScore.
above_min_score(MinScore, relation(_, _, _, Score)) :- Score >= MinScore.

% sort_by_score_desc/2 — sort attention or relation lists by score descending
sort_by_score_desc(List, Sorted) :-
    maplist(score_key, List, Pairs),
    keysort(Pairs, AscSorted),
    reverse(AscSorted, DescSorted),
    pairs_values(DescSorted, Sorted).

score_key(attention(N, P, S), S-attention(N, P, S)).
score_key(relation(S, V, O, S2), S2-relation(S, V, O, S2)).

% ── Main: chat_attention_focus/2 ─────────────────────────────────────
% NOW: question-conditioned relations, not just entity-popular ones.

chat_attention_focus(Tokens, focus(Entities, Relations, Context)) :-
    % 1. Analyze token attention from the question
    analyze_sentence(Tokens, Ranked, _),
    % 2. Get parameters
    attention_temperature(T),
    attention_top_k(K),
    attention_min_score(MinScore),
    % 3. Apply temperature to tokens
    symbolic_temperature(Ranked, T, TempRanked),
    % 4. Select top-K tokens
    symbolic_topk(TempRanked, K, MinScore, SelectedTokens),
    % 5. Build entities from selected tokens
    entities_from_ranked(SelectedTokens, Entities),
    entity_names(Entities, EntNames),
    % 6. Question-conditioned relations (KEY CHANGE)
    % Returns Score-Rel pairs, extract relation with Q-conditioned score
    question_conditioned_attention(Tokens, EntNames, ScoredRels),
    % Convert Score-Rel pairs to relation(S,V,O,QCScore) for temperature
    findall(relation(S, V, O, QCScore), (
        member(QCScore-relation(S, V, O, _), ScoredRels)
    ), RelList),
    % Apply temperature to relations
    ( RelList \= [] ->
        symbolic_temperature(RelList, T, TempRels)
    ; TempRels = []
    ),
    symbolic_topk(TempRels, K, MinScore, Relations),
    % 7. Question context
    question_focus(Tokens, _, ExpectedType),
    question_kind(Tokens, Kind),
    Context = context(ExpectedType, Kind).

% ── entity_names/2 ───────────────────────────────────────────────────
entity_names([], []).
entity_names([entity(Name, _, _, _, _)|Rest], [Name|Names]) :-
    entity_names(Rest, Names).

% ── entities_from_ranked/2 ───────────────────────────────────────────

entities_from_ranked([], []).
entities_from_ranked([attention(Name, Pos, Score)|Rest], [Entity|Entities]) :-
    entity_from_kb(Name, Score, Pos, Entity),
    entities_from_ranked(Rest, Entities).

entity_from_kb(Name, Score, Pos, entity(Name, Type, Score, Pos, Relations)) :-
    ( memory_relation(Name, _, _, _, _) -> Type = entity
    ; memory_relation(_, _, Name, _, _) -> Type = entity
    ; Type = unknown
    ),
    findall(R, memory_relation(Name, R, _, _, _), OutRels),
    findall(R, memory_relation(_, R, Name, _, _), InRels),
    append(OutRels, InRels, AllRels),
    sort(AllRels, Relations).

% ── question_kind/2 ──────────────────────────────────────────────────

question_kind(Tokens, who) :- member(who, Tokens), !.
question_kind(Tokens, what) :- member(what, Tokens), !.
question_kind(Tokens, when) :- member(when, Tokens), !.
question_kind(Tokens, where) :- member(where, Tokens), !.
question_kind(Tokens, why) :- member(why, Tokens), !.
question_kind(Tokens, how) :- member(how, Tokens), !.
question_kind(Tokens, yes_no) :- member(is, Tokens), !.
question_kind(Tokens, yes_no) :- member(did, Tokens), !.
question_kind(_, other).

% ── take_top/3 ───────────────────────────────────────────────────────

take_top(N, List, Top) :-
    take_top_acc(N, List, [], Top).

take_top_acc(0, _, Acc, Top) :- !, reverse(Acc, Top).
take_top_acc(_, [], Acc, Top) :- !, reverse(Acc, Top).
take_top_acc(N, [H|T], Acc, Top) :-
    N > 0,
    N1 is N - 1,
    take_top_acc(N1, T, [H|Acc], Top).

% ── focus_answer/3 — builds answer from focus ───────────────────────

focus_answer(focus(Entities, Relations, context(_ExpectedType, Kind)), Tokens, Answer) :-
    ( Kind = who,
      Relations = [relation(Subject, Verb, Object, Score)|_],
      Score > 0.3 ->
        Answer = answer([Subject], [(Subject, Verb, Object)])
    ; Kind = what,
      Tokens = [what, is|Rest],
      exclude(is_glue_token, Rest, [Entity]),
      member(entity(Entity, _, _, _, _), Entities),
      memory_relation(Entity, is, Value, _, _) ->
        Answer = answer([Value], [(Entity, is, Value)])
    ; Entities = [entity(Name, _, Score, _, _)|_],
      Score > 0.3,
      memory_relation(Name, V, O, _, _) ->
        Answer = answer([Name], [(Name, V, O)])
    ; fail
    ).

is_glue_token(X) :- is_glue(X).
