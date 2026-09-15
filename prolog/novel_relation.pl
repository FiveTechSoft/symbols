% novel_relation.pl — EXP-22 Novel Relation Learning (baseline fc64115 FROZEN).
%
% This module READS the frozen pipeline (parser_v2, semantic_field) but never
% modifies it. All induction uses general mechanisms; NO verb is hardcoded
% anywhere below (no repair/rescue/have mentions). Relation names that appear
% (e.g. repair) do so only as runtime DATA produced by induction.
%
% Pipeline: EXPERIENCE -> OBSERVATION -> PATTERN -> INDUCTION -> MEMORY -> VERIFY.
%
% Public API:
%   learn_from_sentences(+Sentences, -Induced)  experience intake + induction
%   apply_sentences(+Sentences, -Stored)        transfer intake (known verbs only)
%   answer_novel(+Text, +Query, -Answer)        learned memory, else frozen baseline
%   verify_novel(+Rel, +Args, -Verdict)         yes | no_evidence
%   reset_learning/0, reset_facts/0             test isolation helpers
%
% Induced representation (example values, produced at runtime):
%   novel_relation(repair, 2). agent(repair, 1). object(repair, 2).
%   learned_fact(repair, [elena, bicycle]).
:- module(novel_relation,
    [ learn_from_sentences/2,
      apply_sentences/2,
      answer_novel/3,
      verify_novel/3,
      reset_learning/0,
      reset_facts/0
    ]).

:- use_module(parser_v2).
:- use_module(semantic_field).
:- use_module(library(lists)).

:- dynamic novel_relation/2.
:- dynamic agent/2.
:- dynamic object/2.
:- dynamic learned_fact/2.
:- dynamic pending_obs/4.   % pending_obs(RawVerb, SlotPred, Arg1, Arg2)

% At least this many consistent observations before inducing a relation.
induction_threshold(2).

% Interrogative/query-language words: never verbs, never entities.
% Closed function-word-like set (NOT content vocabulary).
query_keyword(what). query_keyword(who). query_keyword(whom).
query_keyword(where). query_keyword(when). query_keyword(how).
query_keyword(color). query_keyword(colour).
query_keyword(shade). query_keyword(tone).

reset_learning :-
    retractall(novel_relation(_, _)),
    retractall(agent(_, _)),
    retractall(object(_, _)),
    reset_facts.

reset_facts :-
    retractall(learned_fact(_, _)),
    retractall(pending_obs(_, _, _, _)).


% ════════════════════════════════════════════════════════════════════
%  OBSERVATION — verb = token at V position, slot = frozen main/2 edge
% ════════════════════════════════════════════════════════════════════

% sentence_obs(+SentenceString, -obs(RawVerb, SlotPred, Arg1, Arg2))
% Verb deduced by POSITION (token after a content-word subject), never by list.
sentence_obs(S, obs(RawV, Pred, A, B)) :-
    parser_v2:normalize_text(S, [Subj, RawV|_]),
    parser_v2:content_word(Subj),
    parser_v2:content_word(RawV),
    parser_v2:parse_sentence(S, Rels),
    member(relation(main, Pred, [A, B]), Rels),
    A == Subj.


% ════════════════════════════════════════════════════════════════════
%  VERB FAMILIES — string-level grouping, no morphology tables,
%  no verb lists. "repaired"~"repair", "rescued"~"rescue";
%  "has"!~"have" (shared prefix too short -> stay distinct).
% ════════════════════════════════════════════════════════════════════

same_family(A, B) :- A == B, !.
same_family(A, B) :-
    atom_chars(A, CA), atom_chars(B, CB),
    common_prefix_len(CA, CB, L), L >= 4,
    length(CA, LA), length(CB, LB),
    abs(LA - LB) =< 2.

common_prefix_len([], _, 0) :- !.
common_prefix_len(_, [], 0) :- !.
common_prefix_len([H|T1], [H|T2], N) :- !,
    common_prefix_len(T1, T2, M), N is M + 1.
common_prefix_len(_, _, 0).

% canonical_name(+Members:list(atom), -Name) : shortest member wins
% (base form is normally shortest: repair < repaired).
canonical_name([M], M) :- !.
canonical_name(Members, Name) :-
    maplist(atom_length_pair, Members, Pairs),
    keysort(Pairs, [_-Name|_]).

atom_length_pair(A, L-A) :- atom_length(A, L).

% cluster_verbs(+Verbs, -Clusters) : greedy same_family clustering.
cluster_verbs([], []) :- !.
cluster_verbs([V|Rest], Clusters) :-
    cluster_verbs(Rest, Temp),
    ( select(C, Temp, Others),
      member(M, C), same_family(V, M) ->
        Clusters = [[V|C]|Others]
    ; Clusters = [[V]|Temp]
    ).

% family_canon(+RawVerb, +KnownNames, -Canon) : map a verb token to its
% family canonical name, preferring an already-induced name when compatible.
family_canon(RawV, Known, Canon) :-
    member(K, Known), same_family(RawV, K), !,
    Canon = K.
family_canon(RawV, Known, Canon) :-
    cluster_verbs([RawV|Known], Clusters),
    member(C, Clusters), member(RawV, C), !,
    canonical_name(C, Canon).


% ════════════════════════════════════════════════════════════════════
%  INDUCTION — learn_from_sentences/2
% ════════════════════════════════════════════════════════════════════

% learn_from_sentences(+Sentences, -Induced)
% 1. Observe (verb position + frozen slot) for each sentence.
% 2. Stash observations as pending (cumulative across calls).
% 3. Move pending obs of ALREADY-known verbs into learned facts.
% 4. Induce relations with >= threshold consistent obs (same family+slot).
learn_from_sentences(Sentences, Induced) :-
    findall(obs(RV, P, A, B),
            ( member(S, Sentences), sentence_obs(S, obs(RV, P, A, B)) ),
            Obs),
    forall(member(obs(RV, P, A, B), Obs),
           assertz(pending_obs(RV, P, A, B))),
    findall(N, novel_relation(N, _), Known),
    % Step 3: pending obs of already-known verbs -> learned facts.
    findall(p(RV, P, A, B)-Canon,
            ( pending_obs(RV, P, A, B),
              family_canon(RV, Known, Canon),
              novel_relation(Canon, 2) ),
            KnownPend),
    forall(member(p(RV, P, A, B)-Canon, KnownPend),
           ( ( learned_fact(Canon, [A, B]) -> true
             ; assertz(learned_fact(Canon, [A, B])) ),
             retract(pending_obs(RV, P, A, B)) )),
    % Step 4: induction over remaining pending obs.
    findall(RV-P, pending_obs(RV, P, _, _), Keys0),
    sort(Keys0, Keys),
    induce_keys(Keys, Induced0),
    sort(Induced0, Induced).

% induce_keys(+Keys, -Induced) : one induction per (family, slot) group
% reaching the threshold. Groups already covered are skipped.
induce_keys([], []) :- !.
induce_keys([RV-P|Rest], Induced) :-
    ( novel_relation(C, _), same_family(RV, C) ->
        induce_keys(Rest, Induced)
    ; findall(A-B, pending_obs_family(RV, P, A, B), Pairs0),
      sort(Pairs0, Pairs),
      induction_threshold(K), length(Pairs, N),
      ( N >= K ->
          canonical_from_pending(RV, P, Canon),
          assertz(novel_relation(Canon, 2)),
          assertz(agent(Canon, 1)),
          assertz(object(Canon, 2)),
          forall(member(A-B, Pairs),
                 ( ( learned_fact(Canon, [A, B]) -> true
                   ; assertz(learned_fact(Canon, [A, B])) ),
                   retract(pending_obs(_, P, A, B)) )),
          Induced = [Canon|More]
      ; Induced = More
      ),
      induce_keys(Rest, More)
    ).

% pending_obs_family(+RV, +Pred, -A, -B) : pending obs whose verb is in
% RV's family and shares the same structural slot predicate.
pending_obs_family(RV, P, A, B) :-
    pending_obs(RV2, P, A, B),
    same_family(RV2, RV).

% canonical name for a to-be-induced family: shortest raw form in its group.
canonical_from_pending(RV, P, Canon) :-
    findall(RV2, pending_obs(RV2, P, _, _), Vs0),
    sort(Vs0, Vs),
    cluster_verbs(Vs, Clusters),
    member(C, Clusters), member(RV, C), !,
    canonical_name(C, Canon).


% ════════════════════════════════════════════════════════════════════
%  TRANSFER INTAKE — apply_sentences/2 (known verbs only, never induces)
% ════════════════════════════════════════════════════════════════════

apply_sentences(Sentences, Stored) :-
    findall(N-Q, novel_relation(N, Q), KnownPairs),
    findall(Canon-[A, B],
            ( member(S, Sentences),
              sentence_obs(S, obs(RV, _, A, B)),
              member(Canon-2, KnownPairs),
              same_family(RV, Canon),
              ( learned_fact(Canon, [A, B]) -> true
              ; assertz(learned_fact(Canon, [A, B])) )
            ),
            Stored0),
    sort(Stored0, Stored).


% ════════════════════════════════════════════════════════════════════
%  ANSWERING — learned memory first, frozen baseline otherwise
% ════════════════════════════════════════════════════════════════════

% answer_novel(+Text, +Query, -Answer)
answer_novel(Text, Query, Answer) :-
    split_string(Text, ".", "", Parts),
    forall(( member(P, Parts),
             string_to_atom(P, At), atom_string(At, Str),
             string_length(Str, L), L > 1 ),
           apply_sentences([Str], _)),
    parser_v2:normalize_text(Query, QT),
    ( question_verb(QT, Canon), novel_relation(Canon, 2) ->
        answer_learned(Text, Query, QT, Canon, Answer)
    ; semantic_field:answer_query(Text, Query, Answer, _)
    ).

% question_verb(+QueryTokens, -Canon) : a content word (minus query
% keywords) whose family matches an induced relation name.
question_verb(QT, Canon) :-
    novel_relation(Canon, 2),
    member(W, QT), atom(W),
    parser_v2:content_word(W),
    \+ query_keyword(W),
    same_family(W, Canon).

% query_entities(+QueryTokens, +Canon, -Entities) : content words that are
% neither keywords nor the relation verb itself, in query order.
query_entities(QT, Canon, Ents) :-
    findall(E,
            ( member(E, QT), atom(E),
              parser_v2:content_word(E),
              \+ query_keyword(E),
              \+ same_family(E, Canon) ),
            Ents).

% Yes/no questions start with an auxiliary (frozen function-word set).
yn_aux(QT) :-
    QT = [First|_],
    parser_v2:auxiliary(First).

answer_learned(_Text, _Query, QT, Canon, Answer) :-
    yn_aux(QT), !,
    query_entities(QT, Canon, Ents),
    ( Ents = [E1, E2|_] ->
        ( learned_fact(Canon, [E1, E2]) -> Answer = yes
        ; Answer = no_evidence )
    ; Answer = unknown
    ).
answer_learned(_Text, _Query, QT, Canon, Answer) :-
    member(what, QT), !,
    query_entities(QT, Canon, Ents),
    ( select(Subj, Ents, _),
      learned_fact(Canon, [Subj, _]) ->
        learned_fact(Canon, [Subj, Answer])
    ; Answer = unknown
    ).
answer_learned(Text, Query, _QT, _Canon, Answer) :-
    % Other query types (where/when/color): frozen baseline owns them.
    semantic_field:answer_query(Text, Query, Answer, _).


% ════════════════════════════════════════════════════════════════════
%  VERIFICATION
% ════════════════════════════════════════════════════════════════════

% verify_novel(+Rel, +Args, -Verdict) : yes | no_evidence
verify_novel(Rel, Args, yes) :-
    learned_fact(Rel, Args), !.
verify_novel(_, _, no_evidence).
