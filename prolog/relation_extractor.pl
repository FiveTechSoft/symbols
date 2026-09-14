% relation_extractor.pl — Generate candidate SVO triples from tokens
% Pipeline: tokens → symbols → candidate relations
%
% No intenta "entender" la frase completa.
% Genera CANDIDATOS que la atención luego puntuará.

:- consult('stemmer.pl').
:- consult('positional.pl').

:- dynamic known_entity/1.
:- dynamic known_relation/3.

% ── extract_candidates/2 ─────────────────────────────────────────────
% extract_candidates(+Tokens, -Candidates)
% Generates candidate SVO triples from a tokenized sentence.

extract_candidates(Tokens, Candidates) :-
    % Step 1: Normalize tokens
    maplist(normalize_token, Tokens, Normalized),
    % Step 2: Filter glue words
    exclude(is_glue, Normalized, ContentTokens),
    % Step 3: Identify potential entities and verbs
    partition(is_verb_token, ContentTokens, Verbs, NonVerbs),
    % Step 4: Generate SVO candidates
    findall(candidate(S, V, O, PosS, PosV, PosO), (
        % Pattern: Subject Verb Object (consecutive)
        nth0(PosS, ContentTokens, S),
        nth0(PosV, ContentTokens, V),
        nth0(PosO, ContentTokens, O),
        PosV is PosS + 1,
        PosO is PosV + 1,
        is_verb_token(V),
        \+ is_glue(S),
        \+ is_glue(O)
    ), ConsecutiveCandidates),
    % Step 5: Generate verb-argument candidates (verb before/after entity)
    findall(candidate(S, V, O, PosS, PosV, PosO), (
        member(V, Verbs),
        nth0(PosV, ContentTokens, V),
        % Object after verb
        nth0(PosO, ContentTokens, O),
        PosO > PosV,
        PosO - PosV =< 3,
        \+ is_glue(O),
        % Subject before verb
        nth0(PosS, ContentTokens, S),
        PosS < PosV,
        PosV - PosS =< 3,
        \+ is_glue(S),
        S \== V,
        O \== V
    ), ArgCandidates),
    % Step 6: Generate compound entity candidates
    findall(candidate(S, V, O, PosS, PosV, PosO), (
        member(V, Verbs),
        nth0(PosV, ContentTokens, V),
        % Compound subject: "the X" or "X Y"
        nth0(PosS, ContentTokens, S),
        PosS < PosV,
        PosV - PosS =< 4,
        \+ is_glue(S),
        % Compound object after verb
        nth0(PosO, ContentTokens, O),
        PosO > PosV,
        PosO - PosV =< 4,
        \+ is_glue(O),
        S \== O
    ), CompoundCandidates),
    % Combine and deduplicate
    append(ConsecutiveCandidates, ArgCandidates, All1),
    append(All1, CompoundCandidates, AllCandidates),
    sort(AllCandidates, Candidates).

% ── normalize_token/2 ───────────────────────────────────────────────
normalize_token(Token, Normalized) :-
    atom_codes(Token, Codes),
    % Remove leading/trailing whitespace
    normalize_space(atom(Normalized), Token).

% ── is_verb_token/1 ─────────────────────────────────────────────────
% Heuristic: check if token looks like a verb
is_verb_token(Token) :-
    atom(Token),
    atom_length(Token, Len),
    Len >= 2,
    % Known verb forms
    ( irregular(Token, _) ; irregular(_, Token) -> true
    ; normalize_verb(Token, Norm), Norm \== Token -> true
    % Common verb endings
    ; atom_codes(Token, Codes),
      last(Codes, LastCode),
      ( LastCode == 101 % 'e'
      ; LastCode == 115 % 's'
      ; LastCode == 100 % 'd'
      ; LastCode == 103 % 'g' (running)
      ; LastCode == 110 % 'n' (taken)
      )
    ).

% ── is_entity_token/1 ───────────────────────────────────────────────
% Heuristic: check if token looks like an entity
is_entity_token(Token) :-
    atom(Token),
    atom_length(Token, Len),
    Len >= 2,
    % Capitalized or known entity
    ( sub_atom(Token, 0, 1, _, First),
      char_type(First, upper) -> true
    ; known_entity(Token) -> true
    ; % Not a common word
      \+ is_glue(Token),
      \+ is_verb_token(Token)
    ).

% ── candidate_score_factors/7 ───────────────────────────────────────
% Returns individual factors for a candidate (used by multi_head.pl)

candidate_score_factors(candidate(S, V, O, PosS, PosV, PosO),
                         RelationScore, EntityScore, PositionScore,
                         DiscourseScore, TemporalScore, NoveltyScore) :-
    % Relation: verb quality
    verb_quality(V, RelationScore),
    % Entity: are S and O known or plausible entities
    entity_plausibility(S, SE),
    entity_plausibility(O, OE),
    EntityScore is (SE + OE) / 2,
    % Position: proximity and ordering
    PositionScore is max(0.1, 1.0 - abs(PosV - PosS) * 0.1 - abs(PosO - PosV) * 0.1),
    % Discourse: connects to conversation context
    discourse_relevance(S, O, DiscourseScore),
    % Temporal: time indicators
    temporal_relevance(V, TemporalScore),
    % Novelty: is this new information
    novelty_score_candidates(S, V, O, NoveltyScore).

% ── verb_quality/2 ──────────────────────────────────────────────────
verb_quality(V, 0.9) :- irregular(V, _), !.
verb_quality(V, 0.9) :- irregular(_, V), !.
verb_quality(V, 0.8) :- normalize_verb(V, Norm), Norm \== V, !.
verb_quality(V, 0.6) :-
    atom_length(V, Len),
    Len >= 3, Len =< 15,
    atom_codes(V, Codes),
    \+ member(95, Codes), !.
verb_quality(_, 0.3).

% ── entity_plausibility/2 ───────────────────────────────────────────
entity_plausibility(E, 1.0) :- known_entity(E), !.
entity_plausibility(E, 0.8) :-
    atom(E),
    atom_length(E, Len),
    Len >= 2, Len =< 20,
    sub_atom(E, 0, 1, _, First),
    char_type(First, upper), !.
entity_plausibility(E, 0.5) :-
    atom(E),
    \+ is_glue(E),
    \+ is_verb_token(E), !.
entity_plausibility(_, 0.2).

% ── discourse_relevance/3 ───────────────────────────────────────────
discourse_relevance(S, O, Score) :-
    % Check if S or O appeared in recent conversation
    ( recent_mentioned(S) ; recent_mentioned(O) ->
        Score = 0.9
    ; Score = 0.3
    ).

recent_mentioned(Entity) :-
    % Check dialog stack
    dialog_stack(subj, Subjs),
    member(Entity, Subjs), !.
recent_mentioned(Entity) :-
    dialog_stack(obj, Objs),
    member(Entity, Objs), !.

% ── temporal_relevance/2 ────────────────────────────────────────────
temporal_relevance(Verb, 0.8) :-
    ( irregular(Verb, _) ; irregular(_, Verb) ),
    member(Verb, [was, were, had, has, did, does, went, came, saw,
                  took, gave, found, thought, said, made, ate, drank,
                  ran, began, sang, rang, spoke, stole, broke, woke]),
    !.
temporal_relevance(_, 0.3).

% ── novelty_score_candidates/4 ──────────────────────────────────────
novelty_score_candidates(S, V, O, 1.0) :-
    \+ known_relation(S, V, O), !.
novelty_score_candidates(_, _, _, 0.2).
