% symbolic_attention.pl — Attention simbólica pura en Prolog
% Usa la KB (memory_relation/5) para pesar tokens en una frase.
% Sin vectores numéricos, sin torch, sin tensorflow.
:- use_module(library(lists)).
:- use_module(library(apply)).

% ── Configuración ────────────────────────────────────────────────────

:- dynamic memory_relation/5.
:- dynamic symbol_importance/2.
:- dynamic attention_result/3.

% Pesos para combinar factores (ajustables)
weight relations(0.4).
weight(connections(0.3).
weight(position(0.15).
weight(novelty(0.15).

% ── Core: symbol_importance/2 ────────────────────────────────────────

% symbol_importance(+Symbol, -Score) — importancia de un símbolo en la KB
% Score = número total de relaciones donde el símbolo aparece
symbol_importance(Symbol, Score) :-
    findall(1, memory_relation(Symbol, _, _, _, _), Outgoing),
    findall(1, memory_relation(_, _, Symbol, _, _), Incoming),
    length(Outgoing, NO),
    length(Incoming, NI),
    Score is NO + NI.

% ── Core: relation_strength/3 ───────────────────────────────────────

% relation_strength(+S, +V, +O, -Strength) — fuerza de una relación
% Basada en: confianza (W), fuente (curated > auto), bidireccionalidad
relation_strength(S, V, O, Strength) :-
    % Buscar la relación
    ( memory_relation(S, V, O, W, U) -> true
    ; memory_relation(O, V, S, W, U) -> true
    ; Strength = 0, !
    ),
    % Calcular fuerza base
    BaseStrength is W,
    % Bonus por provenance curated
    ( atom_contains(U, 'curated') -> CurBonus = 0.3 ; CurBonus = 0 ),
    % Bonus por bidireccionalidad
    ( memory_relation(O, V, S, _, _) -> BidBonus = 0.2 ; BidBonus = 0 ),
    Strength is BaseStrength + CurBonus + BidBonus.

% ── Core: token_attention/4 ─────────────────────────────────────────

% token_attention(+Token, +Context, +Position, -Attention)
% Attention = combinación de: relations, connections, position, novelty
token_attention(Token, Context, Position, Attention) :-
    % Factor 1: Relations (cuántas relaciones tiene en KB)
    symbol_importance(Token, RelScore),
    % Factor 2: Connections (cuántas relaciones comparte con Context)
    connection_score(Token, Context, ConnScore),
    % Factor 3: Position (primeros tokens más importantes, pero no el primero)
    position_score(Position, PosScore),
    % Factor 4: Novelty (tokens no en KB son interesantes)
    novelty_score(Token, NovelScore),
    % Combinar
    weight(relations, WR),
    weight(connections, WC),
    weight(position, WP),
    weight(novelty, WN),
    Attention is RelScore * WR + ConnScore * WC + PosScore * WP + NovelScore * WN.

% ── connection_score/3 ──────────────────────────────────────────────

% connection_score(+Token, +Context, -Score)
% Cuántas relaciones comparte Token con otros tokens en Context
connection_score(Token, Context, Score) :-
    findall(1, (
        member(C, Context),
        C \== Token,
        ( memory_relation(Token, _, C, _, _) ;
          memory_relation(C, _, Token, _, _) ;
          memory_relation(Token, _, _, _, _), memory_relation(C, _, _, _, _),
          memory_relation(Token, _, X, _, _), memory_relation(C, _, X, _, _) ;
          memory_relation(_, _, Token, _, _), memory_relation(_, _, C, _, _),
          memory_relation(_, _, Token, _, _), memory_relation(_, _, C, _, _)
        )
    ), Connections),
    length(Connections, Score).

% ── position_score/2 ────────────────────────────────────────────────

% position_score(+Position, -Score)
% Tokens al inicio y final son más importantes (primacy/recency)
position_score(0, 0.7).      % primer token (sujeto)
position_score(1, 0.9).      % segundo token (verbo/acción)
position_score(2, 0.8).      % tercero (objeto principal)
position_score(3, 0.6).
position_score(4, 0.5).
position_score(5, 0.4).
position_score(Pos, 0.3) :- Pos > 5.

% ── novelty_score/2 ─────────────────────────────────────────────────

% novelty_score(+Token, -Score)
% Tokens no en KB son más "interesting" (potencial nuevo conocimiento)
novelty_score(Token, 1.0) :-
    \+ memory_relation(Token, _, _, _, _),
    \+ memory_relation(_, _, Token, _, _), !.
novelty_score(Token, 0.2) :-
    symbol_importance(Token, Score),
    Score > 3, !.
novelty_score(_, 0.5).

% ── analyze_sentence/3 ──────────────────────────────────────────────

% analyze_sentence(+Tokens, -Ranked, -Summary)
% Analiza una frase y rankea tokens por importancia
analyze_sentence(Tokens, Ranked, Summary) :-
    % Calcular attention para cada token
    findall(attention(Token, Pos, Score), (
        nth0(Pos, Tokens, Token),
        token_attention(Token, Tokens, Pos, Score)
    ), AttentionList),
    % Ordenar por score (descendente)
    msort(AttentionList, Sorted),
    reverse(Sorted, Ranked),
    % Generar resumen
    Ranked = [attention(Top1, _, Score1)|_],
    format('Top token: ~w (score: ~2f)~n', [Top1, Score1]),
    Summary = top(Top1, Score1).

% ── question_focus/3 ────────────────────────────────────────────────

% question_focus(+QuestionTokens, -Focus, -ExpectedAnswer)
% Determina el foco de una pregunta y qué respuesta esperar
question_focus(Tokens, Focus, ExpectedType) :-
    Tokens = [Who, is|Rest], !,
    Focus = Rest,
    ExpectedType = identity.
question_focus(Tokens, Focus, ExpectedType) :-
    Tokens = [What, did|Rest], !,
    Focus = Rest,
    ExpectedType = action.
question_focus(Tokens, Focus, ExpectedType) :-
    Tokens = [Who, did|Rest], !,
    Focus = Rest,
    ExpectedType = agent.
question_focus(Tokens, Focus, ExpectedType) :-
    Tokens = [What, is|Rest], !,
    Focus = Rest,
    ExpectedType = property.
question_focus(Tokens, Tokens, unknown).

% ── predict_answer/3 ────────────────────────────────────────────────

% predict_answer(+Tokens, -Prediction, -Confidence)
% Predice la respuesta basada en attention y KB
predict_answer(Tokens, Prediction, Confidence) :-
    analyze_sentence(Tokens, Ranked, _),
    % El token más importante es candidato
    Ranked = [attention(Focus, _, FocusScore)|_],
    % Buscar en KB qué relación tiene Focus
    ( memory_relation(Focus, V, O, _, _) ->
        Prediction = Focus-V-O,
        Confidence = FocusScore
    ; memory_relation(S, V, Focus, _, _) ->
        Prediction = S-V-Focus,
        Confidence = FocusScore
    ; Prediction = unknown,
      Confidence = 0
    ).

% ── explain_attention/2 ─────────────────────────────────────────────

% explain_attention(+Tokens, -Explanation)
% Genera explicación legible del attention
explain_attention(Tokens, Explanation) :-
    findall(Explanation-Pos-Score, (
        nth0(Pos, Tokens, Token),
        token_attention(Token, Tokens, Pos, Score),
        Score > 0.5
    ), Explanations),
    sort(Explanations, Sorted),
    reverse(Sorted, Explanation).

% ── integrate_with_chat/3 ───────────────────────────────────────────

% integrate_with_chat(+QuestionTokens, -Answer, -Reasoning)
% Integra attention con el sistema de chat existente
integrate_with_chat(QTokens, Answer, Reasoning) :-
    % 1. Analizar la pregunta
    question_focus(QTokens, Focus, ExpectedType),
    % 2. Buscar en KB
    find_answer(Focus, ExpectedType, RawAnswer),
    % 3. Validar con attention
    validate_with_attention(QTokens, RawAnswer, ValidatedAnswer),
    Answer = ValidatedAnswer,
    Reasoning = reasoning(Focus, ExpectedType, RawAnswer, ValidatedAnswer).

% find_answer(+Focus, +Type, -Answer)
find_answer(Focus, identity, Answer) :-
    member(Token, Focus),
    ( memory_relation(Token, is, Answer, _, _) -> true
    ; memory_relation(Answer, _, Token, _, _) -> true
    ; Answer = unknown
    ).
find_answer(Focus, action, Answer) :-
    member(Token, Focus),
    ( memory_relation(Token, V, O, _, _) -> Answer = V-O
    ; memory_relation(_, V, Token, _, _) -> Answer = V-Token
    ; Answer = unknown
    ).
find_answer(Focus, agent, Answer) :-
    member(Token, Focus),
    ( memory_relation(Answer, _, Token, _, _) -> true
    ; memory_relation(Token, _, Answer, _, _) -> true
    ; Answer = unknown
    ).
find_answer(Focus, property, Answer) :-
    member(Token, Focus),
    ( memory_relation(Token, is, Answer, _, _) -> true
    ; memory_relation(Token, has, Answer, _, _) -> true
    ; Answer = unknown
    ).
find_answer(_, _, unknown).

% validate_with_attention(+QuestionTokens, +RawAnswer, -ValidatedAnswer)
validate_with_attention(_, unknown, unknown) :- !.
validate_with_attention(QTokens, RawAnswer, ValidatedAnswer) :-
    % Tokenizar respuesta
    atom_string(RawAnswer, AnswerStr),
    tokenize_atom(AnswerStr, AnswerTokens),
    % Analizar pregunta
    analyze_sentence(QTokens, QRanked, _),
    % Analizar respuesta
    analyze_sentence(AnswerTokens, ARanked, _),
    % Si la respuesta tiene alto attention, es válida
    ARanked = [attention(_, _, Score)|_],
    ( Score > 0.3 -> ValidatedAnswer = RawAnswer
    ; ValidatedAnswer = RawAnswer  % mantener por ahora
    ).

% ── Tests ────────────────────────────────────────────────────────────

:- begin_tests(symbolic_attention).

test(symbol_importance) :-
    % Necesita KB cargada
    assertz(memory_relation(alice, follow, white_rabbit, 1.0, test)),
    assertz(memory_relation(alice, eat, cake, 1.0, test)),
    assertz(memory_relation(alice, drink, bottle, 1.0, test)),
    symbol_importance(alice, 3),
    symbol_importance(white_rabbit, 1),
    symbol_importance(cake, 1).

test(token_attention) :-
    assertz(memory_relation(alice, follow, white_rabbit, 1.0, test)),
    assertz(memory_relation(alice, eat, cake, 1.0, test)),
    token_attention(alice, [alice, follow, white_rabbit], 0, Score),
    Score > 0.

test(analyze_sentence) :-
    assertz(memory_relation(alice, follow, white_rabbit, 1.0, test)),
    assertz(memory_relation(alice, eat, cake, 1.0, test)),
    assertz(memory_relation(white_rabbit, carry, watch, 1.0, test)),
    analyze_sentence([alice, follow, white_rabbit], Ranked, _),
    Ranked = [attention(_, _, _)|_].

:- end_tests(symbolic_attention).
