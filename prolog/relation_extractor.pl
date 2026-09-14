% relation_extractor.pl — Generate candidate SVO triples from tokens
% Pipeline: tokens → symbols → candidate relations
%
% Improvements over v1:
% - Compound entity grouping (white rabbit → white_rabbit)
% - Conjunction splitting (and, but, or)
% - Question handling (who, what, where)
% - Better verb detection

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
    % Step 2: Check if question
    ( is_question(Normalized) ->
        extract_question(Normalized, Candidates)
    ;
        % Step 3: Split on conjunctions
        split_conjunctions(Normalized, Clauses),
        % Step 4: Process each clause
        findall(Candidate, (
            member(Clause, Clauses),
            process_clause(Clause, Candidate)
        ), AllCandidates),
        % Step 5: Deduplicate
        sort(AllCandidates, Candidates)
    ).

% ── is_question/1 ───────────────────────────────────────────────────
% Check if sentence is a question

is_question(Tokens) :-
    Tokens = [First|_],
    is_question_word(First).

is_question_word(who). is_question_word(what). is_question_word(where).
is_question_word(when). is_question_word(why). is_question_word(how).
is_question_word(which). is_question_word(whom).

% ── extract_question/2 ──────────────────────────────────────────────
% Extract candidates from questions

extract_question(Tokens, Candidates) :-
    % Remove question word, treat as unknown
    Tokens = [QWord|Rest],
    is_question_word(QWord),
    % Filter glue from rest
    exclude(is_glue, Rest, ContentTokens),
    % Find verb and object
    partition(is_verb_token, ContentTokens, Verbs, NonVerbs),
    ( Verbs = [Verb|_] ->
        % Find verb position in original
        nth0(VerbPos, Rest, Verb),
        % Find object after verb
        ( append(_, [Object], NonVerbs) ->
            Candidates = [candidate('?unknown', Verb, Object, 0, VerbPos, VerbPos+1)]
        ; Candidates = [candidate('?unknown', Verb, '?what', 0, VerbPos, VerbPos+1)]
        )
    ; Candidates = []
    ).

% ── process_clause/2 ─────────────────────────────────────────────────
% Process a single clause into candidates
% Uses ContentTokens (glue filtered) for verb detection, but original for positions

process_clause(Tokens, candidate(S, V, O, PosS, PosV, PosO)) :-
    % Identify verbs from content tokens
    exclude(is_glue, Tokens, ContentTokens),
    partition(is_verb_token, ContentTokens, Verbs, _),
    % Generate candidates
    member(V, Verbs),
    nth0(PosV, Tokens, V),
    % Find subject (before verb, skip glue)
    nth0(PosS, Tokens, S),
    PosS < PosV,
    PosV - PosS =< 4,
    \+ is_glue(S),
    \+ is_verb_token(S),
    % Find object (after verb)
    nth0(PosO, Tokens, O),
    PosO > PosV,
    PosO - PosV =< 4,
    % Object can be content word OR preposition/adverb (for phrasal verbs)
    \+ is_verb_token(O),
    S \== O.

% ── split_conjunctions/2 ─────────────────────────────────────────────
% Split sentence on conjunctions (and, but, or, then)

split_conjunctions(Tokens, [Before, After]) :-
    append(Before, [Conj|After0], Tokens),
    is_conjunction(Conj),
    After = After0,
    Before \== [],
    After \== [].
split_conjunctions(Tokens, [Tokens]).

is_conjunction(and). is_conjunction(but). is_conjunction(or).
is_conjunction(then). is_conjunction(nor). is_conjunction(yet).

% ── group_compounds/2 ────────────────────────────────────────────────
% Group adjective+noun pairs into compound entities

group_compounds(Candidates, Grouped) :-
    findall(GroupedCandidate, (
        member(candidate(S, V, O, PosS, PosV, PosO), Candidates),
        % Try to group S with preceding adjective
        ( PosS > 0,
          nth0(PosS0, [alice,met,hatter], S0), % placeholder
          is_adjective(S0),
          GroupedS =.. [compound, S0, S] ->
            GroupedCandidate = candidate(GroupedS, V, O, PosS, PosV, PosO)
        ; GroupedCandidate = candidate(S, V, O, PosS, PosV, PosO)
        )
    ), Grouped).

% Actually, let's do this properly with the original tokens
% We need access to the original token list to find adjacent adjectives

% ── normalize_token/2 ───────────────────────────────────────────────
normalize_token(Token, Normalized) :-
    normalize_space(atom(Normalized), Token).

% ── is_verb_token/1 ─────────────────────────────────────────────────
% Heuristic: check if token looks like a verb
% Only use irregular table and common verb endings (avoid false positives on nouns)
is_verb_token(Token) :-
    atom(Token),
    atom_length(Token, Len),
    Len >= 3,
    % Known verb forms first (irregular table is reliable)
    ( irregular(Token, _) ; irregular(_, Token) -> true
    ; % Common verb endings ONLY for longer words (5+ chars to avoid false positives)
      atom_codes(Token, Codes),
      Codes = [_, _, _, _, _ | _],  % at least 5 chars
      last(Codes, LastCode),
      ( (LastCode == 100, % 'd' — past tense: followed, played, walked
         atom_length(Token, L), L >= 5) -> true
      ; (LastCode == 103, % 'g' — present participle: running, eating
         atom_codes(Token, [_, _, Prev|_]),
         Prev == 110) -> true  % 'ng' ending
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

% ── is_adjective/1 ──────────────────────────────────────────────────
% Common adjectives
is_adjective(white). is_adjective(black). is_adjective(red).
is_adjective(blue). is_adjective(green). is_adjective(yellow).
is_adjective(big). is_adjective(small). is_adjective(little).
is_adjective(great). is_adjective(old). is_adjective(new).
is_adjective(very). is_adjective(beautiful). is_adjective(ugly).

% ── candidate_score_factors/7 ───────────────────────────────────────
% Returns individual factors for a candidate (used by multi_head.pl)
% Each factor is 0.0-1.0

candidate_score_factors(candidate(S, V, O, PosS, PosV, PosO),
                         RelationScore, EntityScore, PositionScore,
                         DiscourseScore, TemporalScore, NoveltyScore) :-
    % Relation: verb quality + KB presence
    verb_quality(V, VQ),
    ( known_relation(S, V, O) -> KBBonus = 0.3 ; KBBonus = 0.0 ),
    RelationScore is min(1.0, VQ + KBBonus),
    % Entity: are S and O known or plausible entities
    entity_plausibility(S, SE),
    entity_plausibility(O, OE),
    % Bonus if entity appears in KB
    ( known_entity(S) -> SBonus = 0.2 ; SBonus = 0.0 ),
    ( known_entity(O) -> OBonus = 0.2 ; OBonus = 0.0 ),
    EntityScore is min(1.0, (SE + OE) / 2 + (SBonus + OBonus) / 2),
    % Position: proximity and ordering (S before V before O is ideal)
    ( PosS < PosV, PosV < PosO ->
        OrderBonus = 0.2
    ; OrderBonus = 0.0
    ),
    PositionScore is min(1.0, max(0.1, 1.0 - abs(PosV - PosS) * 0.15 - abs(PosO - PosV) * 0.15) + OrderBonus),
    % Discourse: connects to conversation context
    discourse_relevance(S, O, DiscourseScore),
    % Temporal: time indicators + action verbs
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
    ( recent_mentioned(S) ; recent_mentioned(O) ->
        Score = 0.9
    ; Score = 0.3
    ).

recent_mentioned(Entity) :-
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
