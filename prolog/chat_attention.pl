% chat_attention.pl — Adaptador: chat.pl → symbolic_attention.pl
% Temperature simbólica + Top-K + Threshold como parámetros de comportamiento.
%
% Parámetros configurables (assert/retract):
%   attention_temperature(T)  — T < 1 concentra, T > 1 dispersa (default 1.0)
%   attention_top_k(K)        — número fijo de candidatos (default 5)
%   attention_min_score(S)    — threshold mínimo de score (default 0.0)
%
% Uso:
%   assertz(attention_temperature(0.3)).   % focalizar
%   assertz(attention_top_k(3)).           % limitar candidatos
%   assertz(attention_min_score(0.25)).    % descartar ruido

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
% Aplica escala tipo softmax: p_i = exp(score_i / T) / Σ exp(score_j / T)
% Pero normalizado a [0, 1] relativo al máximo.

symbolic_temperature([], _T, []).
symbolic_temperature([attention(N, P, S)|Rest], T, [attention(N, P, Scaled)|ScaledRest]) :-
    symbolic_temperature(Rest, T, ScaledRest),
    Scaled is S / T.   % Escalamiento directo (no necesitamos exp para el caso simbólico)

% Versión para listas de relation(S, V, O, Score)
temperature_relations([], _T, []).
temperature_relations([relation(S, V, O, Score)|Rest], T, [relation(S, V, O, Scaled)|ScaledRest]) :-
    temperature_relations(Rest, T, ScaledRest),
    Scaled is Score / T.

% ── symbolic_topk/3 ─────────────────────────────────────────────────
% Modo K fijo: take_top(K, List, Result)
% Modo threshold: filtra por score >= MinScore

symbolic_topk(List, TopK, MinScore, Result) :-
    % Primero filtrar por threshold
    include(above_min_score(MinScore), List, Filtered),
    % Luego limitar por K
    take_top(TopK, Filtered, Result).

above_min_score(MinScore, attention(_, _, Score)) :- Score >= MinScore.
above_min_score(MinScore, relation(_, _, _, Score)) :- Score >= MinScore.

% ── Main: chat_attention_focus/2 ─────────────────────────────────────

chat_attention_focus(Tokens, focus(Entities, Relations, Context)) :-
    % 1. Analizar atención de la pregunta
    analyze_sentence(Tokens, Ranked, _),
    % 2. Obtener parámetros
    attention_temperature(T),
    attention_top_k(K),
    attention_min_score(MinScore),
    % 3. Aplicar temperature a tokens
    symbolic_temperature(Ranked, T, TempRanked),
    % 4. Seleccionar top-K por threshold
    symbolic_topk(TempRanked, K, MinScore, SelectedTokens),
    % 5. Construir entidades desde tokens seleccionados
    entities_from_ranked(SelectedTokens, Entities),
    % 6. Buscar relaciones relevantes y aplicar temperature
    find_relevant_relations(Tokens, Entities, RelUnsorted),
    temperature_relations(RelUnsorted, T, TempRels),
    symbolic_topk(TempRels, K, MinScore, Relations),
    % 7. Contexto de pregunta
    question_focus(Tokens, _, ExpectedType),
    question_kind(Tokens, Kind),
    Context = context(ExpectedType, Kind).

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

% ── find_relevant_relations/3 ────────────────────────────────────────

find_relevant_relations(_Tokens, Entities, Relations) :-
    entity_names(Entities, EntNames),
    find_rels_for_entities(EntNames, Rels),
    sort(4, @>=, Rels, Relations).

entity_names([], []).
entity_names([entity(Name, _, _, _, _)|Rest], [Name|Names]) :-
    entity_names(Rest, Names).

find_rels_for_entities([], []).
find_rels_for_entities([E|Es], Rels) :-
    findall(relation(E, V, O, W), memory_relation(E, V, O, W, _), RelsE),
    findrels_tail(Es, RelsE, Rels).

findrels_tail([], R, R).
findrels_tail([E|Es], Acc, Rels) :-
    findall(relation(E, V, O, W), memory_relation(E, V, O, W, _), RelsE),
    append(Acc, RelsE, Acc2),
    findrels_tail(Es, Acc2, Rels).

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

% ── focus_answer/3 — construye respuesta desde el foco ──────────────

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
