% multihop.pl — EXP-24 multi-hop learned reasoning.
% Baselines FROZEN (fc64115) + novel_relation.pl + attribute_learning.pl
% INTACT: this module only READS them, never modifies them.
%
% What is induced here vs merely stored:
% - Stored (episodic, closed-world): observed instance pairs (has/part_of/
%   plural-copular). Only observed pairs ever answer; unobserved -> unknown.
% - Induced abstractions: category/slot bindings (reused from EXP-23),
%   relation-name registry, and PATH COMPOSITION itself. A 3-hop answer is
%   never stored anywhere: it exists only as a trajectory. That is the
%   experiment's induction target.
%
% Hop information is never blind: every hop carries
%   hop(Relation, From, To, Direction, Confidence, Source, Type)
% with Direction=fwd|bwd, Confidence=1.0 for observed pairs (minimum over
% paths), Source=provenance (exp23 | observed | plural), Type in
% {type, poss, mero, event} (mechanism tags, same status as the frozen
% main/location/attribute tags). Backward traversal is allowed only for
% taxonomic-class links (navigable hierarchy); possession and parthood
% traverse forward (documented relation-semantics rule, not per-case).
%
% Provisional heuristics (documented, NOT definitive mechanisms):
% - prefix verb families (shared with EXP-22's approach, not stemming);
% - relaxed verb match (common prefix >=2 plus edit distance =<2);
% - possessive "'s" strip and plural "s" strip (character rules);
% - query-scope words (who/what/which/ultimately) are query-syntax on the
%   same level as interrogatives/auxiliaries, never content answers.
%
% Public API:
%   learn_multihop(+Sentences, +QAPairs, -InducedNames)
%   answer_mhop(+Text, +Query, -Answer, -Path)
%   reset_mhop/0
:- module(multihop,
    [ learn_multihop/3,
      answer_mhop/4,
      reset_mhop/0
    ]).

:- use_module(parser_v2).
:- use_module(semantic_field).
:- use_module(novel_relation).
:- use_module(attribute_learning).
:- use_module(library(lists)).

:- dynamic mh_link/5.   % mh_link(RelName, Arg1, Arg2, SourceTag, Class)
:- dynamic mh_rel/1.    % registered relation names (runtime data only)
:- dynamic mh_pending/2.% mh_pending(X, Y) plural-copular pairs awaiting a link

reset_mhop :-
    retractall(mh_link(_, _, _, _, _)),
    retractall(mh_rel(_)),
    retractall(mh_pending(_, _)).


% ════════════════════════════════════════════════════════════════════
%  GENERAL READERS — shapes, never words
% ════════════════════════════════════════════════════════════════════

% possessive_obs(+S, -poss(Verb, X, Y)) : [Art, X, Verb, Y].
poss_obs(S, poss(V, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, V, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:content_word(V),
    parser_v2:content_word(Y).

% relcop_obs(+S, -relcop(Link, X, Y)) : [Art, X, Aux, Art, RelNoun, P, Art, Y].
% Link name derived from the observed linking words themselves.
relcop_obs(S, relcop(Link, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, B, C, RN, P, D, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:auxiliary(B),
    parser_v2:article(C),
    parser_v2:content_word(RN),
    parser_v2:content_word(P),
    parser_v2:article(D),
    parser_v2:content_word(Y),
    atom_concat(RN, '_', T),
    atom_concat(T, P, Link).

% copular_obs(+S, -cop(Link, X, Y)) : [Art, X, Aux, Art, Y].
% Link name derived from the linking words; indefinite-article allomorphs
% (a/an) normalize to one form (provisional, function-level like lowercase).
copular_obs(S, cop(Link, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, B, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:auxiliary(B),
    parser_v2:article(C),
    parser_v2:content_word(Y),
    link_article(C, CN),
    atom_concat(B, '_', T),
    atom_concat(T, CN, Link).

link_article(an, a) :- !.
link_article(A, A).

% plural_cop(+S, -pl(Xs, Ys)) : [Xplural, Aux, Yplural], singularized.
plural_cop(S, pl(Xs, Ys)) :-
    parser_v2:normalize_text(S, [X, B, Y]),
    parser_v2:content_word(X),
    parser_v2:auxiliary(B),
    parser_v2:content_word(Y),
    singularize(X, Xs),
    singularize(Y, Ys).

% singularize(+Plural, -Singular) : minimal plural rule (provisional).
% NOTE: single-quoted char LISTS ([s], not "s": double quotes are string
% objects in SWI-Prolog and never unify with atom_chars output).
singularize(W, S) :-
    atom_chars(W, Cs),
    append(R, [s], Cs),
    length(R, L), L > 3,
    \+ append(_, [s, s], Cs), !,
    atom_chars(S, R).
singularize(W, W).

% strip_possessive(+Token, -Base) : character rule for X's -> X.
strip_possessive(W, B) :-
    atom_chars(W, Cs),
    append(R, ['\'', s], Cs),
    R \== [], !,
    atom_chars(B, R).
strip_possessive(W, W).


% ════════════════════════════════════════════════════════════════════
%  RELAXED VERB MATCHING — general string relations, no verb lists
% ════════════════════════════════════════════════════════════════════

verb_match(A, B) :- A == B, !.
verb_match(A, B) :- prefix_family(A, B), !.
verb_match(A, B) :- prefix_of(A, B), !.
verb_match(A, B) :- prefix_of(B, A), !.
verb_match(A, B) :-
    common_prefix_len2(A, B, L), L >= 2,
    lev_dist(A, B, D), D =< 2.

% prefix_of(+Short, +Long) : Short is a proper prefix of Long (>=3 chars).
prefix_of(A, B) :-
    A \== B,
    atom_chars(A, CA), atom_chars(B, CB),
    append(CA, Rest, CB), Rest \== [],
    length(CA, L), L >= 3.

% prefix_family: shared prefix >=4, length gap =<2 (provisional heuristic).
prefix_family(A, B) :-
    atom_chars(A, CA), atom_chars(B, CB),
    common_prefix_len2(A, B, L), L >= 4,
    length(CA, LA), length(CB, LB),
    abs(LA - LB) =< 2.

common_prefix_len2(A, B, L) :-
    atom_chars(A, CA), atom_chars(B, CB),
    cpl(CA, CB, L).

cpl([], _, 0) :- !.
cpl(_, [], 0) :- !.
cpl([H|T1], [H|T2], N) :- !,
    cpl(T1, T2, M), N is M + 1.
cpl(_, _, 0).

% lev_dist(+A, +B, -D) : plain Levenshtein on short tokens.
lev_dist(A, B, D) :-
    atom_chars(A, CA), atom_chars(B, CB),
    length(CA, LA), length(CB, LB),
    LA =< 12, LB =< 12,
    lev(CA, CB, D).

lev([], B, D) :- !, length(B, D).
lev(A, [], D) :- !, length(A, D).
lev([H|T1], [H|T2], D) :- !, lev(T1, T2, D).
lev([_|T1], [_|T2], D) :- !,
    lev(T1, T2, D1), D is D1 + 1.
lev([_|T1], B, D) :-
    lev(T1, B, D1), D is D1 + 1.
lev(A, [_|T2], D) :-
    lev(A, T2, D1), D is D1 + 1.


% ════════════════════════════════════════════════════════════════════
%  EXPERIENCE INTAKE — learn_multihop/3 (induction lives here only)
% ════════════════════════════════════════════════════════════════════

% learn_multihop(+Sentences, +QAPairs, -InducedNames)
% - Copular sentences + QAs go through EXP-23 intake (reuse, intact).
% - Possessive / relational-copular / plural shapes stored here (episodic).
learn_multihop(Sentences, QAs, Induced) :-
    attribute_learning:learn_attributes(Sentences, QAs, _),
    forall(member(S, Sentences), ingest_mhop(S)),
    findall(N, mh_rel(N), Ns0),
    sort(Ns0, Induced).

% covered_elsewhere(+X, +Y) : the taxonomic pair already lives in EXP-23
% memory (singular copular intake). Skipping keeps the two stores disjoint;
% traversal reads both, so nothing is lost and single-result patterns stay
% sound. Never silently drop: only exact X,Y pairs are skipped.
covered_elsewhere(X, Y) :-
    attribute_learning:type_fact(_, X, Y).

ingest_mhop(S) :-
    novel_relation:apply_sentences([S], _),
    ( copular_obs(S, cop(Link, X, Y)),
      \+ covered_elsewhere(X, Y) ->
        store_link(Link, X, Y, observed_copular, type)
    ; true
    ),
    ( poss_obs(S, poss(V, X, Y)) ->
        store_link(V, X, Y, observed, poss)
    ; true
    ),
    ( relcop_obs(S, relcop(Link, X, Y)) ->
        store_link(Link, X, Y, observed, mero)
    ; true
    ),
    ( plural_cop(S, pl(X, Y)) ->
        ( covered_elsewhere(X, Y) -> true
        ; setof(L, attribute_learning:known_type_rel(L), [Only]) ->
            store_link(Only, X, Y, plural, type)
        ; ( mh_pending(X, Y) -> true
          ; assertz(mh_pending(X, Y)) )
        )
    ; true
    ).

% store_link(+Rel, +X, +Y, +Src, +Class) : episodic storage + name registry.
store_link(Rel, X, Y, Src, Class) :-
    ( mh_link(Rel, X, Y, _, _) -> true
    ; assertz(mh_link(Rel, X, Y, Src, Class)) ),
    ( mh_rel(Rel) -> true
    ; assertz(mh_rel(Rel)) ).


% ════════════════════════════════════════════════════════════════════
%  ANNOTATED GRAPH TRAVERSAL — never blind
% ════════════════════════════════════════════════════════════════════

% fwd_edge(-Rel, +From, -To, -Conf, -Src, -Type)
fwd_edge(R, F, T, 1.0, exp23, type) :-
    attribute_learning:type_fact(R, F, T),
    attribute_learning:known_type_rel(R).
fwd_edge(R, F, T, 1.0, Src, type) :-
    mh_link(R, F, T, Src, type),
    mh_rel(R).
fwd_edge(R, F, T, 1.0, Src, poss) :-
    mh_link(R, F, T, Src, poss).
fwd_edge(R, F, T, 1.0, Src, mero) :-
    mh_link(R, F, T, Src, mero).
fwd_edge(R, F, T, 1.0, exp22, event) :-
    novel_relation:learned_fact(R, [F, T]).

% Backward traversal only for taxonomic class (navigable hierarchy).
bwd_allowed(R) :-
    attribute_learning:known_type_rel(R).
bwd_allowed(R) :-
    mh_link(R, _, _, _, type).

% neighbor(+Node, -Hop, -Next)
neighbor(F, hop(R, F, T, fwd, C, S, Ty), T) :-
    fwd_edge(R, F, T, C, S, Ty).
neighbor(F, hop(R, T, F, bwd, C, S, Ty), T) :-
    fwd_edge(R, T, F, C, S, Ty),
    bwd_allowed(R).

% mh_paths(+From, +MaxHops, +To, -Path) : annotated BFS, visited set.
mh_paths(From, MaxH, To, Path) :-
    bfs_mhop([[hop(start, none, From, fwd, 1.0, query, none)]], To, MaxH, R),
    strip_start(R, Path).

strip_start(R, Path) :-
    reverse(R, [hop(start, _, _, _, _, _, _)|Path]), !.
strip_start(P, P).

% type_edge_any(-Rel, +From, -To) : taxonomic edges from every learned
% store (EXP-23 facts union own copular intake). Read-only unification:
% the a/an split fractured the chain across stores, so traversal and all
% lookups must consult both, or 3-hop conclusions silently vanish.
type_edge_any(L, X, Y) :-
    attribute_learning:type_fact(L, X, Y),
    attribute_learning:known_type_rel(L).
type_edge_any(L, X, Y) :-
    mh_link(L, X, Y, _, type),
    mh_rel(L).

% Base case: goal on top of a NON-EMPTY extension (the start marker alone
% never counts). Deliberately NO cut: bound-goal callers take the first
% (shortest, BFS order) solution, while free-goal enumeration (closure)
% needs every reachable node. Termination via visited set + MaxH.
bfs_mhop([Path|_], Goal, _, Path) :-
    Path = [hop(_, _, Goal, _, _, _, _)|Rest],
    Rest \== [].
bfs_mhop([Path|Rest], Goal, MaxH, Result) :-
    Path = [hop(_, _, Current, _, _, _, _)|_],
    length(Path, Len), Len < MaxH + 1,
    findall([Hop|Path],
            ( neighbor(Current, Hop, Next),
              \+ member_hop_node(Next, Path) ),
            NewPaths),
    append(Rest, NewPaths, Queue),
    Queue \== [],
    bfs_mhop(Queue, Goal, MaxH, Result).

member_hop_node(N, Path) :-
    member(hop(_, _, N, _, _, _, _), Path).

% taxonomic_closure(+Entity, -Reachable) : forward-reachable via type class.
% FORWARD-ONLY on purpose: admitting backward edges here lets closure wander
% up-down detours (bicycle->vehicle->machine->motorcycle...), which then
% corrupts longest/furthest. Backward traversal stays available for targeted
% inversion queries through mh_paths/4 (bound goals).
taxonomic_closure(E, Reach) :-
    findall(T, mh_path_type(E, T), Ts0),
    sort(Ts0, Reach).

mh_path_type(E, T) :-
    mh_paths_fwd(E, 5, T, P),
    P \== [],
    forall(member(hop(_, _, _, _, _, _, Ty), P), Ty == type).

% mh_paths_fwd(+From, +MaxHops, +To, -Path) : forward-only annotated BFS.
mh_paths_fwd(From, MaxH, To, Path) :-
    bfs_fwd([[hop(start, none, From, fwd, 1.0, query, none)]], To, MaxH, R),
    strip_start(R, Path).

bfs_fwd([Path|_], Goal, _, Path) :-
    Path = [hop(_, _, Goal, _, _, _, _)|Rest],
    Rest \== [].
bfs_fwd([Path|Rest], Goal, MaxH, Result) :-
    Path = [hop(_, _, Current, _, _, _, _)|_],
    length(Path, Len), Len < MaxH + 1,
    findall([Hop|Path],
            ( neighbor_fwd(Current, Hop, Next),
              \+ member_hop_node(Next, Path) ),
            NewPaths),
    append(Rest, NewPaths, Queue),
    Queue \== [],
    bfs_fwd(Queue, Goal, MaxH, Result).

neighbor_fwd(F, hop(R, F, T, fwd, C, S, Ty), T) :-
    fwd_edge(R, F, T, C, S, Ty).

% path_confidence(+Path, -Conf) : minimum over hops (future energy hook).
path_confidence([], 1.0) :- !.
path_confidence(P, C) :-
    findall(E, member(hop(_, _, _, _, E, _, _), P), Es),
    min_list(Es, C).


% ════════════════════════════════════════════════════════════════════
%  ENTITY GROUNDING (graph + all learned memories, no word lists)
% ════════════════════════════════════════════════════════════════════

grounded_mh(W, Nodes) :-
    ( member(W, Nodes) -> true
    ; attribute_learning:type_fact(_, W, _) -> true
    ; attribute_learning:type_fact(_, _, W) -> true
    ; mh_link(_, W, _, _, _) -> true
    ; mh_link(_, _, W, _, _) -> true
    ; novel_relation:learned_fact(_, Args), member(W, Args) -> true
    ).

% resolve_mh(+QT, +Nodes, -Entities) : grounded content words, possessives
% reduced to base form; interrogatives/auxiliaries/articles never qualify
% as content words, glue words fail grounding automatically.
resolve_mh(QT, Nodes, Ents) :-
    findall(E,
            ( member(W, QT), atom(W),
              parser_v2:content_word(W),
              strip_possessive(W, B),
              grounded_mh(B, Nodes),
              E = B ),
            Es0),
    sort(Es0, Ents).


% ════════════════════════════════════════════════════════════════════
%  ANSWERING — multihop layer -> EXP-22/23 -> frozen baseline
% ════════════════════════════════════════════════════════════════════

% answer_mhop(+Text, +Query, -Answer, -Path)
answer_mhop(Text, Query, Answer, Path) :-
    split_string(Text, ".", "", Parts),
    forall(( member(P, Parts),
             string_to_atom(P, At), atom_string(At, S),
             string_length(S, L), L > 1 ),
           ingest_mhop(S)),
    parser_v2:normalize_text(Query, QT),
    ( QT = [Q0|_], member(Q0, [what, which]),
      QT = [_, Cat|_], atom(Cat), parser_v2:content_word(Cat),
      attribute_learning:attr_binding(Cat, _) ->
        answer_mhop_category(Text, Query, QT, Cat, Answer, Path)
    ; ( member(QW, QT), mh_type_value(QW) ->
          answer_mhop_typeword(Text, Query, QT, QW, Answer, Path)
      ; mh_relation_word(QT, Rel) ->
          answer_mhop_relword(Text, Query, QT, Rel, Answer, Path)
      ; novel_relation:answer_novel(Text, Query, Answer),
        Path = [hop(delegate, na, Answer, fwd, 1.0, stack, other)]
      )
    ).

% mh_type_value(+W) : W occurs as a type value in learned type knowledge.
mh_type_value(W) :-
    atom(W),
    type_edge_any(_, _, W).

% mh_relation_word(+QT, -Rel) : a query word matching a learned relation
% (EXP-22 registry first: its verbs stay authoritative for their facts).
mh_relation_word(QT, Rel) :-
    member(W, QT), atom(W), parser_v2:content_word(W),
    novel_relation:novel_relation(C, _),
    verb_match(W, C), !,
    Rel = exp22(C).
mh_relation_word(QT, Rel) :-
    member(W, QT), atom(W), parser_v2:content_word(W),
    mh_rel(R),
    verb_match(W, R), !,
    Rel = mine(R).

% answer_mhop_category(+Text, +Query, +QT, +Cat, -Answer, -Path)
% Possessive focus + type step > path-constrained target > ultimately
% furthest > plain EXP-23 delegation.
answer_mhop_category(Text, Query, QT, Cat, Answer, Path) :-
    attribute_learning:context_graph(Text, graph(Nodes, Edges)),
    resolve_mh(QT, Nodes, Ents),
    findall(Slot, attribute_learning:attr_binding(Cat, Slot), Slots),
    ( possessive_focus(Ents, Slots, Edges, Focus, FHop),
      focus_slot_answer(Slots, Edges, Focus, A, SH) ->
        Answer = A, Path = [FHop, SH]
    ; constrained_target(Ents, Cat, Edges, Target, Path) ->
        Answer = Target
    ; member(ultimately, QT) ->
        furthest_type(Ents, Nodes, Answer, Path)
    ; attribute_learning:attr_binding(Cat, type),
      direct_type_single(Ents, Answer, Path) ->
        true
    ; attribute_learning:answer_attr(Text, Query, Answer),
      Path = [hop(delegate, na, Answer, fwd, 1.0, exp23layer, other)]
    ).

% direct_type_single(+Entities, -Answer, -Path) : exactly one learned
% type value for the mentioned entities themselves (NO instance expansion:
% taxonomy answers attach per-node; expansion is the fallback owned by the
% attribute layer for attribute slots). Stored 1-hop: never novel.
direct_type_single(Ents, Answer, Path) :-
    findall(L-E-A,
            ( member(E, Ents),
              type_edge_any(L, E, A) ),
            Triples0),
    sort(Triples0, [L-E-A]),
    ( attribute_learning:type_fact(L, E, A) ->
        Src = exp23
    ; Src = learned
    ),
    Path = [hop(L, E, A, fwd, 1.0, Src, type)],
    Answer = A.

% possessive_focus(+Entities, -Focus, -FocusHop) : two grounded entities
% sharing exactly one direct stored link; focus = link target.
possessive_focus(Ents, Slots, Edges, Focus, Hop) :-
    findall(E1-E2-H,
            ( member(E1, Ents), member(E2, Ents), E1 \== E2,
              direct_stored_link(E1, E2, H) ),
            Ls0),
    sort(Ls0, [E1-E2-H]),
    \+ direct_slot_value(Slots, Edges, E1, _),
    Focus = E2, Hop = H.

direct_stored_link(E1, E2, hop(R, E1, E2, fwd, 1.0, Src, Ty)) :-
    fwd_edge(R, E1, E2, 1.0, Src, Ty).

% direct_slot_value(+Slots, +Edges, +Entity, -Value) : stored slot value.
direct_slot_value(Slots, Edges, E, A) :-
    member(Slot, Slots),
    slot_value(Slot, Edges, E, A, _).

% slot_value(+Slot, +Edges, +Entity, -Value, -Hop)
slot_value(type, _Edges, E, A, hop(L, E, A, fwd, 1.0, Src, type)) :-
    ( attribute_learning:type_fact(L, E, A),
      attribute_learning:known_type_rel(L), Src = exp23
    ; mh_link(L, E, A, Src, type),
      mh_rel(L)
    ).
slot_value(attr, Edges, E, A, hop(R, E, A, fwd, 1.0, frozen, attribute)) :-
    member(edge(E, R, A, attribute), Edges).

% focus_slot_answer(+Slots, +Edges, +Focus, -Answer, -SlotHop) : exactly
% one slot value for the focus entity across the queried slots.
focus_slot_answer(Slots, Edges, Focus, A, SH) :-
    findall(A-SH,
            ( member(Slot, Slots),
              slot_value(Slot, Edges, Focus, A, SH) ),
            Pairs0),
    sort(Pairs0, [A-SH]).

% constrained_target(+Entities, +Cat, +Edges, -Target, -Path) : a type-word
% entity that terminates in the queried slot and is reachable from another
% entity via learned paths.
constrained_target(Ents, Cat, Edges, Target, Path) :-
    member(Target, Ents),
    slot_value_member(Cat, Edges, Target),
    member(Start, Ents), Start \== Target,
    mh_paths(Start, 5, Target, Path),
    Path \== [].

% slot_value_member(+Cat, +Edges, +Target) : Target occurs as a value in
% one of the category's bound slots (types: learned memory; attributes:
% current episodic context).
slot_value_member(Cat, _Edges, Target) :-
    attribute_learning:attr_binding(Cat, type),
    type_edge_any(_, _, Target).
slot_value_member(Cat, Edges, Target) :-
    attribute_learning:attr_binding(Cat, attr),
    member(edge(_, _, Target, attribute), Edges).

% furthest_type(+Entities, +Nodes, -Answer, -Path) : taxonomic endpoint
% (LAST hop's target: paths are root-first after strip_start).
furthest_type(Ents, _Nodes, Answer, Path) :-
    member(E, Ents),
    taxonomic_closure(E, Reach),
    Reach \== [],
    longest_path_to(E, Reach, Path),
    last(Path, hop(_, _, Answer, _, _, _, _)).

longest_path_to(E, Reach, Best) :-
    findall(Len-P,
            ( member(T, Reach),
              mh_paths_fwd(E, 5, T, P), P \== [],
              length(P, Len) ),
            Pairs0),
    sort(Pairs0, Sorted),
    reverse(Sorted, [_-Best|_]).

% text_mentions(+Text, +Entity) : entity name occurs in the text (episodic
% scoping for instance disambiguation; case-insensitive, general).
text_mentions(Text, X) :-
    string_lower(Text, LT),
    atom_string(LA, LT),
    sub_atom(LA, _, _, _, X).

% answer_mhop_typeword(+Text, +Query, +QT, +TypeWord, -Answer, -Path)
answer_mhop_typeword(Text, Query, QT, TW, Answer, Path) :-
    findall(X, ( mh_instance_of(TW, X), text_mentions(Text, X) ), Xs0),
    sort(Xs0, Xs),
    QT = [Q0|_],
    ( Q0 == who ->
        findall(Ag-P,
                ( member(X, Xs),
                  learned_agent_of(X, Ag, P) ),
                AgPs0),
        sort(AgPs0, AgPs),
        ( AgPs = [Ag-P] ->
            Answer = Ag, Path = P
        ; Answer = unknown, Path = []
        )
    ; member(Q0, [what, which]) ->
        ( Xs = [Single] ->
            Answer = Single,
            Path = [hop(instance_of, TW, Single, bwd, 1.0, learned, type)]
        ; Answer = unknown, Path = []
        )
    ; novel_relation:answer_novel(Text, Query, Answer),
      Path = [hop(delegate, na, Answer, fwd, 1.0, stack, other)]
    ).

% mh_instance_of(+TypeWord, -Instance) : learned type subjects of a value.
mh_instance_of(TW, X) :-
    type_edge_any(_, X, TW).

% learned_agent_of(+Instance, -Agent, -Path) : agents via learned relations.
learned_agent_of(X, Ag, [hop(R, Ag, X, fwd, 1.0, exp22, event)]) :-
    novel_relation:learned_fact(R, [Ag, X]).
learned_agent_of(X, Ag, [hop(R, Ag, X, fwd, 1.0, learned, poss)]) :-
    mh_link(R, Ag, X, _, poss).

% answer_mhop_relword(+Text, +Query, +QT, +Rel, -Answer, -Path)
answer_mhop_relword(Text, Query, _QT, exp22(_), Answer, Path) :-
    !,
    novel_relation:answer_novel(Text, Query, Answer),
    Path = [hop(delegate, na, Answer, fwd, 1.0, exp22layer, other)].
answer_mhop_relword(Text, _Query, QT, mine(R), Answer, Path) :-
    attribute_learning:context_graph(Text, graph(Nodes, _)),
    resolve_mh(QT, Nodes, Ents),
    ( member(Q0, QT), member(Q0, [what, which]) ->
        % "What does X have?" / "What is Y part of?": subject present.
        findall(V-P,
                ( member(E, Ents),
                  mh_link(R, E, V, Src, Class),
                  P = [hop(R, E, V, fwd, 1.0, Src, Class)] ),
                VPs0),
        sort(VPs0, VPs),
        ( VPs = [_-P] ->
            P = [hop(_, _, Answer, _, _, _, _)|_], Path = P
        ; VPs = [] ->
            % Inverse lookup: value present, subject missing ("What has Y?").
            findall(S-P,
                    ( member(E, Ents),
                      mh_link(R, S, E, Src, Class),
                      P = [hop(R, S, E, bwd, 1.0, Src, Class)] ),
                    SPs0),
            sort(SPs0, SPs),
            ( SPs = [_-P2] ->
                P2 = [hop(_, Answer, _, _, _, _, _)|_], Path = P2
            ; Answer = unknown, Path = []
            )
        ; Answer = unknown, Path = []
        )
    ; Answer = unknown, Path = []
    ).
