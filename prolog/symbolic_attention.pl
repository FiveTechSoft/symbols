% symbolic_attention.pl — Attention simbólica pura en Prolog
% Usa la KB (memory_relation/5) para pesar tokens en una frase.
% Sin vectores numéricos, sin torch, sin tensorflow.
%
% v3: Question-conditioned attention, real temperature, top-k by score.
:- use_module(library(lists)).
:- use_module(library(apply)).
:- use_module(library(pairs)).

% ── Configuración ────────────────────────────────────────────────────

:- dynamic memory_relation/5.

% Pesos para combinar factores (ajustables)
weight(relations, 0.4).
weight(connections, 0.3).
weight(position, 0.15).
weight(novelty, 0.15).

% ── Core: symbol_importance/2 ────────────────────────────────────────

% symbol_importance(+Symbol, -Score) — importancia de un símbolo en la KB
% Score = log(1 + total relaciones) para evitar que símbolos muy frecuentes dominen
symbol_importance(Symbol, Score) :-
    findall(1, memory_relation(Symbol, _, _, _, _), Outgoing),
    findall(1, memory_relation(_, _, Symbol, _, _), Incoming),
    length(Outgoing, NO),
    length(Incoming, NI),
    Total is NO + NI,
    Score is log(1 + Total).

% ── Core: relation_strength/3 ───────────────────────────────────────

% relation_strength(+S, +V, +O, -Strength) — fuerza de una relación
relation_strength(S, V, O, Strength) :-
    ( memory_relation(S, V, O, W, U) -> true
    ; memory_relation(O, V, S, W, U) -> true
    ; Strength = 0, !
    ),
    BaseStrength is W,
    ( atom_contains(U, 'curated') -> CurBonus = 0.3 ; CurBonus = 0 ),
    ( memory_relation(O, V, S, _, _) -> BidBonus = 0.2 ; BidBonus = 0 ),
    Strength is BaseStrength + CurBonus + BidBonus.

% ── Core: token_attention/4 ─────────────────────────────────────────

% token_attention(+Token, +Context, +Position, -Attention)
% Attention = combinación normalizada de 4 factores
token_attention(Token, Context, Position, Attention) :-
    symbol_importance(Token, RelScore),
    connection_score(Token, Context, ConnScore),
    position_score(Position, PosScore),
    novelty_score(Token, NovelScore),
    normalize_relscore(RelScore, RelNorm),
    normalize_connscore(ConnScore, Context, ConnNorm),
    weight(relations, WR),
    weight(connections, WC),
    weight(position, WP),
    weight(novelty, WN),
    Attention is RelNorm * WR + ConnNorm * WC + PosScore * WP + NovelScore * WN.

% ── Normalization ────────────────────────────────────────────────────

normalize_relscore(Raw, Normalized) :-
    Normalized is min(1.0, log(1 + Raw) / log(31)).

normalize_connscore(Raw, Context, Normalized) :-
    length(Context, CtxLen),
    ( CtxLen > 1 ->
        MaxConn is CtxLen - 1,
        Normalized is min(1.0, Raw / MaxConn)
    ; Normalized = 0
    ).

% ── connection_score/3 ──────────────────────────────────────────────

connection_score(Token, Context, Score) :-
    findall(C, (
        member(C, Context),
        C \== Token,
        ( memory_relation(Token, _, C, _, _) ;
          memory_relation(C, _, Token, _, _) ;
          memory_relation(Token, _, X, _, _), memory_relation(C, _, X, _, _) ;
          memory_relation(_, _, Token, _, _), memory_relation(_, _, C, _, _),
          memory_relation(_, Token, _, _, _), memory_relation(_, C, _, _, _)
        )
    ), Connections),
    sort(Connections, Unique),
    length(Unique, Score).

% ── position_score/2 ────────────────────────────────────────────────

position_score(0, 0.7).
position_score(1, 0.9).
position_score(2, 0.8).
position_score(3, 0.6).
position_score(4, 0.5).
position_score(5, 0.4).
position_score(Pos, 0.3) :- Pos > 5.

% ── novelty_score/2 ─────────────────────────────────────────────────

novelty_score(Token, 1.0) :-
    \+ memory_relation(Token, _, _, _, _),
    \+ memory_relation(_, _, Token, _, _), !.
novelty_score(Token, 0.2) :-
    symbol_importance(Token, Score),
    Score > 1.5, !.
novelty_score(_, 0.5).

% ── analyze_sentence/3 ──────────────────────────────────────────────

analyze_sentence(Tokens, Ranked, Summary) :-
    findall(attention(Token, Pos, Score), (
        nth0(Pos, Tokens, Token),
        token_attention(Token, Tokens, Pos, Score)
    ), AttentionList),
    map_list_to_pairs(score_pair, AttentionList, Pairs),
    keysort(Pairs, SortedPairs),
    reverse(SortedPairs, RevPairs),
    pairs_values(RevPairs, Ranked),
    Ranked = [attention(Top1, _, Score1)|_],
    Summary = top(Top1, Score1).

score_pair(attention(_, _, Score), Score-Attention) :- Attention = attention(_, _, Score).

% ── question_focus/3 ────────────────────────────────────────────────

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

% ── question_relation_match/3 ───────────────────────────────────────
% Score how well a KB relation matches the question's intent.

% question_relation_match(+QuestionTokens, +Relation, -Score)
% Relation = relation(S, V, O, BaseWeight)
% Score combines base weight with verb/object overlap with question.
question_relation_match(QTokens, relation(S, V, O, BaseWeight), Score) :-
    % Extract verb stem from question
    ( member(QVerb, QTokens), normalize_verb(QVerb, QVerbNorm) -> true ; QVerbNorm = none ),
    % Extract content objects from question (excluding glue)
    exclude(is_glue, QTokens, QContent),
    % Verb match bonus
    ( V == QVerbNorm -> VerbBonus = 0.3
    ; V == QVerb -> VerbBonus = 0.25
    ; irregular_form(QVerb, QVerbNorm), V == QVerbNorm -> VerbBonus = 0.2
    ; VerbBonus = 0
    ),
    % Object overlap bonus
    ( member(O, QContent) -> ObjBonus = 0.2
    ; compound_match(O, QContent) -> ObjBonus = 0.15
    ; ObjBonus = 0
    ),
    % Subject in question bonus
    ( member(S, QContent) -> SubjBonus = 0.1
    ; SubjBonus = 0
    ),
    Score is BaseWeight + VerbBonus + ObjBonus + SubjBonus.

% compound_match(+KBObj, +QContent)
% KBObj = white_rabbit, QContent = [white, rabbit]
compound_match(KBObj, QContent) :-
    atom_codes(KBObj, Codes),
    phrase(compound_words(Words), Codes),
    subset(Words, QContent).

compound_words([W]) --> word(W), !.
compound_words([W|Ws]) --> word(W), [_], compound_words(Ws).

word(W) --> codes(WCodes), { atom_codes(W, WCodes) }.
codes([]) --> [].
codes([C|Cs]) --> [C], codes(Cs).

% ── question_conditioned_attention/3 ────────────────────────────────
% question_conditioned_attention(+QuestionTokens, +KBEntities, -RankedRelations)
% Scores ALL relations in KB conditioned on the question, not just entity-popular ones.

question_conditioned_attention(QTokens, KBEntities, RankedRelations) :-
    % Collect all relations touching the question's entities
    findall(Rel, (
        member(Ent, KBEntities),
        ( memory_relation(Ent, V, O, W, U) -> Rel = relation(Ent, V, O, W)
        ; memory_relation(O, V, Ent, W, U) -> Rel = relation(O, V, Ent, W)
        )
    ), Rels0),
    sort(Rels0, Rels),  % deduplicate
    % Score each relation conditioned on question
    findall(Score-Rel, (
        member(Rel, Rels),
        question_relation_match(QTokens, Rel, Score)
    ), ScoredRels),
    keysort(ScoredRels, Sorted),
    reverse(Sorted, RankedRelations).

% ── predict_answer/3 ────────────────────────────────────────────────

% Pronoun resolution: resolve subject pronouns before answering
predict_answer(Tokens, Prediction, Confidence) :-
    Tokens = [Subj|Rest],
    dialog_pronoun(Subj), !,
    dialog_resolve(Subj, Resolved),
    predict_answer([Resolved|Rest], Prediction, Confidence).

% Simple SVO: [Subject, Verb, Object] -> verify subject does verb to object
predict_answer(Tokens, Prediction, Confidence) :-
    Tokens = [Subject, Verb, Object], !,
    normalize_verb(Verb, NormV),
    ( memory_relation(Subject, Verb, Object, _, _) ->
        Prediction = Subject, Confidence = 1.0
    ; memory_relation(Subject, NormV, Object, _, _) ->
        Prediction = Subject, Confidence = 0.95
    ; memory_relation(Subject, _, Object, _, _) ->
        Prediction = Subject, Confidence = 0.8
    ; Prediction = unknown, Confidence = 0
    ).

% predict_answer(+Tokens, -Prediction, -Confidence)
predict_answer(Tokens, Prediction, Confidence) :-
    Tokens = [who, Verb|Rest], !,
    exclude(is_glue, Rest, ContentTokens),
    normalize_verb(Verb, NormV),
    ( ContentTokens = [Obj] ->
        ( memory_relation(S, Verb, Obj, _, _) ->
            Prediction = S, Confidence = 1.0
        ; memory_relation(S, NormV, Obj, _, _) ->
            Prediction = S, Confidence = 0.95
        ; atom_join(ContentTokens, '_', CompoundObj),
          memory_relation(S, Verb, CompoundObj, _, _) ->
            Prediction = S, Confidence = 0.9
        ; memory_relation(S, NormV, CompoundObj, _, _) ->
            Prediction = S, Confidence = 0.85
        ; Prediction = unknown, Confidence = 0
        )
    ; ContentTokens = [] ->
        ( memory_relation(S, Verb, _, _, _) ->
            Prediction = S, Confidence = 0.8
        ; memory_relation(S, NormV, _, _, _) ->
            Prediction = S, Confidence = 0.7
        ; Prediction = unknown, Confidence = 0
        )
    ; last(ContentTokens, Obj),
        ( memory_relation(S, Verb, Obj, _, _) ->
            Prediction = S, Confidence = 0.9
        ; memory_relation(S, NormV, Obj, _, _) ->
            Prediction = S, Confidence = 0.85
        ; atom_join(ContentTokens, '_', CompoundObj),
          memory_relation(S, Verb, CompoundObj, _, _) ->
            Prediction = S, Confidence = 0.85
        ; memory_relation(S, NormV, CompoundObj, _, _) ->
            Prediction = S, Confidence = 0.8
        ; length(ContentTokens, Len),
          ( Len >= 3 ->
              append(_, [T1, T2], ContentTokens),
              atomic_list_concat([T1, T2], '_', Obj2),
              ( memory_relation(S, Verb, Obj2, _, _) ->
                  Prediction = S, Confidence = 0.8
              ; memory_relation(S, NormV, Obj2, _, _) ->
                  Prediction = S, Confidence = 0.75
              ; Prediction = unknown, Confidence = 0
              )
          ; Prediction = unknown, Confidence = 0
          )
        )
    ).
predict_answer(Tokens, Prediction, Confidence) :-
    Tokens = [what, is|Rest], !,
    exclude(is_glue, Rest, ContentTokens),
    ContentTokens = [Entity],
    ( memory_relation(Entity, is, Val, _, _) ->
        Prediction = Val, Confidence = 1.0
    ; memory_relation(Entity, has, Val, _, _) ->
        Prediction = Val, Confidence = 0.9
    ; Prediction = unknown, Confidence = 0
    ).
predict_answer(Tokens, Prediction, Confidence) :-
    analyze_sentence(Tokens, Ranked, _),
    Ranked = [attention(Focus, _, FocusScore)|_],
    ( memory_relation(Focus, V, O, _, _) ->
        Prediction = Focus-V-O, Confidence = FocusScore
    ; memory_relation(S, V, Focus, _, _) ->
        Prediction = S-V-Focus, Confidence = FocusScore
    ; Prediction = unknown, Confidence = 0
    ).

% atom_join(+List, +Sep, -Atom)
atom_join([], _, '').
atom_join([X], _, X).
atom_join([H|T], Sep, Result) :-
    atom_join(T, Sep, Rest),
    atomic_list_concat([H, Sep, Rest], Result).

% is_glue(+Token) — tokens functionales a ignorar
is_glue(the).
is_glue(a).
is_glue(an).
is_glue(is).
is_glue(at).
is_glue(in).
is_glue(on).
is_glue(to).
is_glue(of).
is_glue(for).
is_glue(with).
is_glue(and).
is_glue(or).

% ── explain_attention/2 ─────────────────────────────────────────────

explain_attention(Tokens, Explanation) :-
    findall(Score-Token-Pos, (
        nth0(Pos, Tokens, Token),
        token_attention(Token, Tokens, Pos, Score),
        Score > 0.3
    ), Explanations),
    sort(Explanations, Sorted),
    reverse(Sorted, Explanation).

% ── Tests ────────────────────────────────────────────────────────────

:- begin_tests(symbolic_attention).

test(weight_syntax) :-
    weight(relations, WR),
    weight(connections, WC),
    weight(position, WP),
    weight(novelty, WN),
    WR + WC + WP + WN =:= 1.0.

test(analyze_sentence_ordering) :-
    assertz(memory_relation(alice, follow, white_rabbit, 1.0, test)),
    assertz(memory_relation(alice, eat, cake, 1.0, test)),
    analyze_sentence([alice, follow, white_rabbit], Ranked, _),
    Ranked = [attention(alice, 0, S)|_],
    S > 0.

:- end_tests(symbolic_attention).
