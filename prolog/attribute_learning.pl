% attribute_learning.pl — EXP-23 attribute/type induction.
% Baseline fc64115 FROZEN + novel_relation.pl INTACT: this module only READS
% them, never modifies them. No content word is hardcoded below; category
% words, relation names and values all arrive as runtime DATA from observed
% Q&A regularities and copular sentences.
%
% Core ideas (all general mechanisms):
% - Category word = token right after what/which (positional deduction).
% - Category->slot binding by answer containment: the observed answer must
%   occur in exactly one structural slot (attribute edges | type facts).
% - Copular reader for [Art, X, Aux, Art, Y]: link name derived from the
%   observed linking words themselves.
% - Attributes are SLOT-OPEN (any fresh value in the attribute slot counts)
%   but closed against values already known under another category.
% - Types are SLOT-CLOSED per instance: no evidence -> unknown. UNKNOWN
%   stays a valid answer; the module never hallucinates a type.
%
% Public API:
%   learn_attributes(+Sentences, +QAPairs, -NewBindings)
%   answer_attr(+Text, +Query, -Answer)
%   reset_attr_learning/0
:- module(attribute_learning,
    [ learn_attributes/3,
      answer_attr/3,
      reset_attr_learning/0
    ]).

:- use_module(parser_v2).
:- use_module(semantic_field).
:- use_module(novel_relation).
:- use_module(library(lists)).

:- dynamic attr_binding/2.    % attr_binding(CategoryWord, Slot), Slot = attr | type
:- dynamic attr_evidence/4.   % attr_evidence(CategoryWord, Slot, Entity, Answer)
:- dynamic cat_member/2.      % cat_member(CategoryWord, Value)
:- dynamic known_type_rel/1.  % known_type_rel(LinkName)
:- dynamic type_fact/3.       % type_fact(LinkName, Entity, Type)
:- dynamic pending_type/3.    % pending_type(LinkName, Entity, Type)

binding_threshold(2).
induction_threshold(2).

reset_attr_learning :-
    retractall(attr_binding(_, _)),
    retractall(attr_evidence(_, _, _, _)),
    retractall(cat_member(_, _)),
    retractall(known_type_rel(_)),
    retractall(type_fact(_, _, _)),
    retractall(pending_type(_, _, _)).


% ════════════════════════════════════════════════════════════════════
%  CONTEXT GRAPH — frozen parse, read-only
% ════════════════════════════════════════════════════════════════════

% context_graph(+Text, -graph(Nodes, Edges))
context_graph(Text, graph(Nodes, Edges)) :-
    split_string(Text, ".", "", Parts),
    findall(R, ( member(P, Parts),
                 string_to_atom(P, At), atom_string(At, S),
                 string_length(S, L), L > 1,
                 parser_v2:parse_sentence(S, Rs),
                 member(R, Rs) ),
            Rels),
    maplist(rel_to_compact, Rels, Compact),
    semantic_field:build_graph(Compact, graph(Nodes, Edges)).

rel_to_compact(relation(T, P, A), rel(T, P, A)).


% ════════════════════════════════════════════════════════════════════
%  COPULAR READER — [Art, X, Aux, Art, Y], e.g. X is a Y.
%  Own general reader (frozen parser covers no copular shape).
%  Link name derived from the observed linking words.
% ════════════════════════════════════════════════════════════════════

% copular_obs(+SentenceString, -cop(Link, X, Y))
copular_obs(S, cop(Link, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, B, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:auxiliary(B),
    parser_v2:article(C),
    parser_v2:content_word(Y),
    atom_concat(B, '_', T),
    atom_concat(T, C, Link).

% ingest_sentence(+SentenceString) : transfer-time intake.
% Known type links -> store facts; unknown links -> pending (NO induction here;
% induction happens only inside learn_attributes/3, deliberately).
ingest_sentence(S) :-
    novel_relation:apply_sentences([S], _),
    ( copular_obs(S, cop(Link, X, Y)) ->
        ( known_type_rel(Link) ->
            ( type_fact(Link, X, Y) -> true
            ; assertz(type_fact(Link, X, Y)) )
        ; ( pending_type(Link, X, Y) -> true
          ; assertz(pending_type(Link, X, Y)) )
        )
    ; true
    ).

% induce_type_rels : promote pending links with >= threshold distinct pairs.
induce_type_rels :-
    findall(L, pending_type(L, _, _), Ls0),
    sort(Ls0, Links),
    forall(member(L, Links),
           ( findall(X-Y, pending_type(L, X, Y), Pairs0),
             sort(Pairs0, Pairs),
             induction_threshold(K), length(Pairs, N),
             ( N >= K, \+ known_type_rel(L) ->
                 assertz(known_type_rel(L)),
                 forall(member(X-Y, Pairs),
                        ( ( type_fact(L, X, Y) -> true
                          ; assertz(type_fact(L, X, Y)) ),
                          retract(pending_type(L, X, Y)) ))
             ; true
             ))).


% ════════════════════════════════════════════════════════════════════
%  CATEGORY INDUCTION from Q&A regularities
% ════════════════════════════════════════════════════════════════════

% learn_attributes(+Sentences, +QAPairs, -NewBindings)
% QAPairs = [qa(QuestionString, AnswerAtom, ContextText), ...].
% NewBindings = newly bound Category-Slot pairs this call.
learn_attributes(Sentences, QAs, NewBindings) :-
    forall(member(S, Sentences), ingest_sentence(S)),
    induce_type_rels,
    forall(member(qa(Q, A, Ctx), QAs), record_qa(Q, A, Ctx)),
    findall(Cat-Slot, distinct_evidence_key(Cat, Slot), Keys0),
    sort(Keys0, Keys),
    induce_bindings(Keys, New0),
    sort(New0, NewBindings).

% distinct_evidence_key(+Cat, +Slot) : ground keys with recorded evidence.
distinct_evidence_key(Cat, Slot) :-
    attr_evidence(Cat, Slot, _, _).

% induce_bindings(+Keys, -New) : bind keys reaching the threshold.
induce_bindings([], []) :- !.
induce_bindings([Cat-Slot|Rest], New) :-
    ( attr_binding(Cat, Slot) ->
        induce_bindings(Rest, New)
    ; findall(E-A, attr_evidence(Cat, Slot, E, A), Ev0),
      sort(Ev0, Ev),
      binding_threshold(K), length(Ev, N),
      ( N >= K ->
          assertz(attr_binding(Cat, Slot)),
          New = [Cat-Slot|More]
      ; New = More
      ),
      induce_bindings(Rest, More)
    ).

% record_qa(+Question, +Answer, +ContextText)
record_qa(Q, A, Ctx) :-
    parser_v2:normalize_text(Q, QT),
    QT = [Q0|_], member(Q0, [what, which]),
    QT = [_, Cat|_], atom(Cat), parser_v2:content_word(Cat),
    context_graph(Ctx, graph(Nodes, Edges)),
    resolve_entities(QT, Cat, Nodes, Ents),
    ( member(E, Ents),
      member(edge(E, _, A, attribute), Edges) ->
        note_evidence(Cat, attr, E, A)
    ; true
    ),
    ( member(E, Ents),
      type_fact(L, E, A), known_type_rel(L) ->
        note_evidence(Cat, type, E, A)
    ; true
    ).

note_evidence(Cat, Slot, E, A) :-
    ( attr_evidence(Cat, Slot, E, A) -> true
    ; assertz(attr_evidence(Cat, Slot, E, A)) ),
    ( Slot = attr, atom(A),
      ( cat_member(Cat, A) -> true ; assertz(cat_member(Cat, A)) )
    ; true
    ).

% newly_bound(+Cat, +Slot) : legacy single-key entry point (ground keys only).
% Batch induction goes through induce_bindings/2; kept for interactive use.
newly_bound(Cat, Slot) :-
    ground(Cat), ground(Slot),
    findall(E-A, attr_evidence(Cat, Slot, E, A), Ev0),
    sort(Ev0, Ev),
    binding_threshold(K), length(Ev, N), N >= K,
    \+ attr_binding(Cat, Slot),
    assertz(attr_binding(Cat, Slot)).


% ════════════════════════════════════════════════════════════════════
%  ENTITY RESOLUTION (grounding + type expansion, no word lists)
% ════════════════════════════════════════════════════════════════════

% grounded(+Word, +Nodes) : appears as a graph node or in type facts.
grounded(W, Nodes) :-
    ( member(W, Nodes) -> true
    ; type_fact(_, W, _) -> true
    ; type_fact(_, _, W) -> true
    ).

% resolve_entities(+QueryTokens, +CategoryWord, +Nodes, -Entities)
% Content words grounded in graph/memory, minus the category word itself;
% type values expand to their in-context instances.
resolve_entities(QT, Cat, Nodes, Ents) :-
    findall(E,
            ( member(E, QT), atom(E),
              parser_v2:content_word(E),
              E \== Cat,
              grounded(E, Nodes) ),
            Es0),
    expand_types(Es0, Nodes, Es1),
    sort(Es1, Ents).

expand_types([], _, []) :- !.
expand_types([E|Rest], Nodes, Out) :-
    ( type_fact(L, _, E), known_type_rel(L) ->
        findall(X, ( type_fact(L2, X, E),
                     known_type_rel(L2),
                     member(X, Nodes) ), Xs),
        expand_types(Rest, Nodes, More),
        append(Xs, More, Out)
    ; Out = [E|More],
      expand_types(Rest, Nodes, More)
    ).


% ════════════════════════════════════════════════════════════════════
%  ANSWERING — attr layer -> EXP-22 layer -> frozen baseline
% ════════════════════════════════════════════════════════════════════

% answer_attr(+Text, +Query, -Answer) ; unknown stays a valid answer.
answer_attr(Text, Query, Answer) :-
    split_string(Text, ".", "", Parts),
    forall(( member(P, Parts),
             string_to_atom(P, At), atom_string(At, S),
             string_length(S, L), L > 1 ),
           ingest_sentence(S)),
    parser_v2:normalize_text(Query, QT),
    ( QT = [Q0|_], member(Q0, [what, which]),
      QT = [_, Cat|_], atom(Cat), parser_v2:content_word(Cat),
      attr_binding(Cat, _) ->
        answer_category(Text, QT, Cat, Answer)
    ; ( member(QW, QT), type_value_word(QW) ->
          answer_via_type(Text, Query, QT, QW, Answer)
      ; novel_relation:answer_novel(Text, Query, Answer)
      )
    ).

% type_value_word(+W) : W occurs as a type value in learned type facts.
type_value_word(W) :-
    atom(W),
    type_fact(L, _, W), known_type_rel(L).

% answer_category(+Text, +QT, +Cat, -Answer)
answer_category(Text, QT, Cat, Answer) :-
    context_graph(Text, graph(Nodes, Edges)),
    resolve_entities(QT, Cat, Nodes, Ents),
    findall(Slot, attr_binding(Cat, Slot), Slots),
    findall(A,
            ( member(Slot, Slots),
              slot_answer(Slot, Ents, Edges, Cat, A) ),
            Ans0),
    sort(Ans0, Ans),
    ( Ans = [Answer] -> true ; Answer = unknown ).

% slot_answer(+Slot, +Entities, +Edges, +Cat, -Answer)
slot_answer(attr, Ents, Edges, Cat, A) :-
    member(E, Ents),
    member(edge(E, _, A, attribute), Edges),
    allow_value(Cat, A).
slot_answer(type, Ents, _Edges, _Cat, A) :-
    member(E, Ents),
    type_fact(L, E, A), known_type_rel(L).

% allow_value(+Cat, +Value) : values already known under a DIFFERENT
% category are excluded (anti cross-category hallucination); fresh values
% in the slot stay allowed (slot openness for attributes).
allow_value(Cat, V) :-
    \+ ( cat_member(Other, V), Other \== Cat ).

% answer_via_type(+Text, +Query, +QT, +TypeWord, -Answer) for who/what + type.
answer_via_type(Text, Query, QT, TypeWord, Answer) :-
    context_graph(Text, graph(Nodes, _)),
    findall(X, ( type_fact(L, X, TypeWord),
                 known_type_rel(L),
                 member(X, Nodes) ), Xs0),
    sort(Xs0, Xs),
    QT = [Q0|_],
    ( Q0 == who ->
        findall(Ag, ( member(X, Xs),
                      novel_relation:learned_fact(_, [Ag, X]) ), Ags0),
        sort(Ags0, Ags),
        ( Ags = [Answer] -> true ; Answer = unknown )
    ; member(Q0, [what, which]) ->
        findall(S-O, ( member(S, QT), atom(S),
                       parser_v2:content_word(S),
                       novel_relation:learned_fact(_, [S, O]),
                       member(O, Xs) ), Pairs0),
        sort(Pairs0, Pairs),
        findall(O, member(_-O, Pairs), Os0),
        sort(Os0, Os),
        ( Os = [Answer] -> true ; Answer = unknown )
    ; novel_relation:answer_novel(Text, Query, Answer)
    ).
