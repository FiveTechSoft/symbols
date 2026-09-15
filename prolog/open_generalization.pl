% open_generalization.pl — EXP-25 open generalization.
% Baselines FROZEN (fc64115) + novel_relation.pl + attribute_learning.pl +
% multihop.pl INTACT: this module only READS them, never modifies them.
%
% What this layer owns (all general mechanisms, zero content words):
% - Art-initial transitive readers: [Art,X,V,Prep,Art,Y] and [Art,X,V,Art,Y].
%   Verb identity preserved from verb position; relation name = observed
%   verb token. Frozen/EXP-22 observation needs content subjects, so these
%   shapes never overlap with prior intake: disjoint by construction.
% - Possessive scoping (X's Y / Y of [the] X): candidates restricted to
%   X-linked instances; unlinked -> unknown HALTS (no fallback leakage).
%   "of"/"the" here are documented structural glue, same status as the
%   interrogatives below.
% - Verb gate: verb matching an induced family with zero learned facts
%   mentioning the query entities -> unknown (closed world over verbs).
% - Location gate: where-questions need a location edge touching the
%   query entities (direct or via-main), else unknown.
% - Yes/no type verification over learned taxonomic paths.
% - Pronoun resolution mirroring the frozen positional rule (first main
%   edge subject/object of the text).
% Query-syntax words (who/what/which/where/when/how, auxiliaries, of/the,
% ultimately) and mechanism tags are design vocabulary, explicitly allowed;
% content vocabulary never appears below (m0 self-check in the test file).
%
% Everything else delegates down: multihop:answer_mhop -> novel ->
% frozen baseline. Answers that need no new machinery cost no new code.
%
% Public API:
%   open_learn(+Sentences, +QAPairs, -InducedNames)
%   answer_open(+Text, +Query, -Answer, -Path)
%   reset_open/0
:- module(open_generalization,
    [ open_learn/3,
      answer_open/4,
      reset_open/0
    ]).

:- use_module(parser_v2).
:- use_module(semantic_field).
:- use_module(novel_relation).
:- use_module(attribute_learning).
:- use_module(multihop).
:- use_module(library(lists)).

:- dynamic og_link/4.   % og_link(RelName, Arg1, Arg2, SourceTag)
:- dynamic og_rel/1.    % registered relation names (runtime data only)
:- dynamic og_ty/3.     % og_ty(RelName, Entity, Type): allomorph-normalized
:- dynamic og_tyrel/1.  % copular relation names (runtime data only)
:- dynamic og_cat/2.    % og_cat_binding(CategoryWord, Slot), Slot=type only
:- dynamic og_catev/4.  % og_catev(CategoryWord, Slot, Entity, Answer)

reset_open :-
    retractall(og_link(_, _, _, _)),
    retractall(og_rel(_)),
    retractall(og_ty(_, _, _)),
    retractall(og_tyrel(_)),
    retractall(og_cat(_, _)),
    retractall(og_catev(_, _, _, _)).


% ════════════════════════════════════════════════════════════════════
%  ART-INITIAL READERS — shapes the frozen pipeline never takes
%  (it requires content subjects), hence disjoint intake by construction
% ════════════════════════════════════════════════════════════════════

% poss6_obs(+S, -poss6(Verb, X, Y)) : [Art, X, Verb, Prep, Art, Y].
% Prep accepts frozen prepositions (to/for/of/...) OR content words:
% the frozen set must not silently drop relations (F-C).
poss6_obs(S, poss6(V, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, V, P, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:content_word(V),
    ( parser_v2:preposition(P) ; parser_v2:content_word(P) ),
    parser_v2:article(C),
    parser_v2:content_word(Y).

% poss5_obs(+S, -poss5(Verb, X, Y)) : [Art, X, Verb, Art, Y].
poss5_obs(S, poss5(V, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, V, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:content_word(V),
    parser_v2:article(C),
    parser_v2:content_word(Y).

% store_og(+Verb, +X, +Y, +Src) : episodic storage + name registry.
% Single-observation naming (like multihop episodic stores): closed world
% over stored pairs preserves unknown; induction proper lives in EXP-22/23.
store_og(V, X, Y, Src) :-
    ( og_link(V, X, Y, _) -> true
    ; assertz(og_link(V, X, Y, Src)) ),
    ( og_rel(V) -> true
    ; assertz(og_rel(V)) ).

% norm_indef(+Article, -Canonical) : indefinite-article allomorph map.
% a/an are phonological variants of one morpheme; normalizing them before
% copular intake is function-level preprocessing (same status as the frozen
% lowercase/accent normalization), never content knowledge. Literals a/an
% below are articles, explicitly excluded from the m0 content stoplist.
norm_indef(an, a) :- !.
norm_indef(A, A).

% copular_norm(+S, -cop(Link, X, Y)) : [Art, X, Aux, Art, Y] with a/an
% unified, so "A chest is a container" and "A lantern is an artifact"
% induce ONE relation instead of fragmenting is_a/is_an (F-A).
copular_norm(S, cop(Link, X, Y)) :-
    parser_v2:normalize_text(S, [A, X, B, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:auxiliary(B),
    parser_v2:article(C),
    parser_v2:content_word(Y),
    norm_indef(C, CN),
    atom_concat(B, '_', T),
    atom_concat(T, CN, Link).

% covered_taxo(+X, +Y) : pair already lives in another taxonomic store
% (EXP-23 facts or multihop links, any link name). Keeps stores disjoint
% so single-result patterns stay sound (EXP-24 lesson).
covered_taxo(X, Y) :-
    attribute_learning:type_fact(_, X, Y), !.
covered_taxo(X, Y) :-
    multihop:mh_link(_, X, Y, _, type), !.

% store_ogty(+Link, +X, +Y) : normalized copular pair, disjoint stores.
store_ogty(L, X, Y) :-
    ( covered_taxo(X, Y) -> true
    ; ( og_ty(L, X, Y) -> true
      ; assertz(og_ty(L, X, Y)) )
    ),
    ( og_tyrel(L) -> true
    ; assertz(og_tyrel(L)) ).


% ════════════════════════════════════════════════════════════════════
%  EXPERIENCE INTAKE — open_learn/3 (induction lives here only)
% ════════════════════════════════════════════════════════════════════

% open_learn(+Sentences, +QAPairs, -InducedNames)
% - S-V-O sentences ride EXP-22 induction (thresholds intact, reused).
% - Copular sentences + QAs ride EXP-23 induction (bindings reused) AND the
%   normalized copular intake below (allomorph-unified, F-A).
% - Art-initial transitives land in og_link (episodic, immediately usable).
% - Path-mediated QA evidence binds type/kind words in og_cat (F-B):
%   direct containment cannot confirm 2-hop answers, so evidence here is
%   reachability through taxonomic links (reuses the multihop-proven
%   traversal shape over the union view).
open_learn(Sentences, QAs, Induced) :-
    novel_relation:learn_from_sentences(Sentences, Ind22),
    attribute_learning:learn_attributes(Sentences, QAs, _),
    forall(member(S, Sentences), ingest_open(S)),
    forall(member(qa(Q, A, Ctx), QAs), record_open_qa(Q, A, Ctx)),
    induce_open_bindings(Bound),
    findall(N, og_rel(N), Ns0),
    sort(Ns0, NsOg),
    append([Ind22, NsOg, Bound], Ind0),
    flatten(Ind0, Ind1),
    sort(Ind1, Induced).

% ingest_open(+Sentence) : transfer-time intake, induction-free.
ingest_open(S) :-
    novel_relation:apply_sentences([S], _),
    ( poss6_obs(S, poss6(V, X, Y)) ->
        store_og(V, X, Y, poss6)
    ; true
    ),
    ( poss5_obs(S, poss5(V, X, Y)) ->
        store_og(V, X, Y, poss5)
    ; true
    ),
    ( copular_norm(S, cop(Link, X, Y)) ->
        store_ogty(Link, X, Y)
    ; true
    ).

% record_open_qa(+Question, +Answer, +ContextText) : path-mediated evidence.
% Category word = token after what/which (positional, same convention as
% EXP-23). Evidence = answer reachable from a grounded entity through
% taxonomic links (any length >= 1, union view).
record_open_qa(Q, A, Ctx) :-
    parser_v2:normalize_text(Q, QT),
    QT = [Q0|_], member(Q0, [what, which]),
    QT = [_, Cat|_], atom(Cat), parser_v2:content_word(Cat),
    attribute_learning:context_graph(Ctx, graph(Nodes, _)),
    ground_taxo(QT, Nodes, Ents),
    member(E, Ents),
    o_taxo_reachable(E, A),
    ( og_catev(Cat, type, E, A) -> true
    ; assertz(og_catev(Cat, type, E, A)) ).

% ground_taxo(+Tokens, +Nodes, -Entities) : grounded content words that
% participate in taxonomic knowledge (as instance or as value).
ground_taxo(QT, Nodes, Ents) :-
    findall(E,
            ( member(E, QT), atom(E),
              parser_v2:content_word(E),
              ( member(E, Nodes) -> true
              ; o_taxo_mentions(E) -> true
              ) ),
            Es0),
    sort(Es0, Ents).

o_taxo_mentions(E) :-
    ( attribute_learning:type_fact(_, E, _) -> true
    ; attribute_learning:type_fact(_, _, E) -> true
    ; og_ty(_, E, _) -> true
    ; og_ty(_, _, E) -> true
    ).

% o_taxo_reachable(+Entity, +Answer) : taxonomic path, union view.
% Visited set: taxonomies are DAGs in practice, cycles must not hang us.
o_taxo_reachable(E, A) :-
    o_taxo_reach(E, A, [E]).

o_taxo_reach(E, A, _) :-
    o_taxo_step(E, A).
o_taxo_reach(E, A, Seen) :-
    o_taxo_step(E, M),
    \+ member(M, Seen),
    o_taxo_reach(M, A, [M|Seen]).

o_taxo_step(E, A) :-
    attribute_learning:type_fact(R, E, A),
    attribute_learning:known_type_rel(R).
o_taxo_step(E, A) :-
    og_ty(R, E, A),
    og_tyrel(R).

% induce_open_bindings(-New) : bind categories with >= 2 distinct evidences.
induce_open_bindings(New) :-
    findall(Cat-Slot, og_catev(Cat, Slot, _, _), Keys0),
    sort(Keys0, Keys),
    findall(Cat-Slot,
            ( member(Cat-Slot, Keys),
              \+ og_cat(Cat, Slot),
              findall(E-A, og_catev(Cat, Slot, E, A), Ev0),
              sort(Ev0, Ev),
              length(Ev, N), N >= 2,
              assertz(og_cat(Cat, Slot)) ),
            New0),
    sort(New0, New).


% ════════════════════════════════════════════════════════════════════
%  UNION EDGE VIEW — reads only, every store stays authoritative
% ════════════════════════════════════════════════════════════════════

% o_fwd(-Rel, +From, -To, -Conf, -Src, -Type)
o_fwd(R, F, T, 1.0, exp23, type) :-
    attribute_learning:type_fact(R, F, T),
    attribute_learning:known_type_rel(R).
o_fwd(R, F, T, 1.0, Src, type) :-
    multihop:mh_link(R, F, T, Src, type),
    multihop:mh_rel(R).
o_fwd(R, F, T, 1.0, ogty, type) :-
    og_ty(R, F, T),
    og_tyrel(R).
o_fwd(R, F, T, 1.0, Src, rel) :-
    og_link(R, F, T, Src).
o_fwd(R, F, T, 1.0, exp22, event) :-
    novel_relation:learned_fact(R, [F, T]).

% o_paths(+From, +MaxHops, +To, -Path) : annotated BFS, visited set.
o_paths(From, MaxH, To, Path) :-
    obfs([[hop(start, none, From, fwd, 1.0, query, none)]], To, MaxH, R),
    strip_o(R, Path).

strip_o(R, Path) :-
    reverse(R, [hop(start, _, _, _, _, _, _)|Path]), !.
strip_o(P, P).

obfs([Path|_], Goal, _, Path) :-
    Path = [hop(_, _, Goal, _, _, _, _)|Rest],
    Rest \== [].
obfs([Path|Rest], Goal, MaxH, Result) :-
    Path = [hop(_, _, Current, _, _, _, _)|_],
    length(Path, Len), Len < MaxH + 1,
    findall([Hop|Path],
            ( o_neighbor(Current, Hop, Next),
              \+ o_visited(Next, Path) ),
            NewPaths),
    append(Rest, NewPaths, Queue),
    Queue \== [],
    obfs(Queue, Goal, MaxH, Result).

o_neighbor(F, hop(R, F, T, fwd, C, S, Ty), T) :-
    o_fwd(R, F, T, C, S, Ty).
o_neighbor(F, hop(R, T, F, bwd, C, S, Ty), T) :-
    o_fwd(R, T, F, C, S, Ty),
    o_bwd_allowed(R).

% Backward only for taxonomic class (same documented rule as multihop).
o_bwd_allowed(R) :-
    attribute_learning:known_type_rel(R).
o_bwd_allowed(R) :-
    multihop:mh_link(R, _, _, _, type).
o_bwd_allowed(R) :-
    og_tyrel(R).

o_visited(N, Path) :-
    member(hop(_, _, N, _, _, _, _), Path).


% ════════════════════════════════════════════════════════════════════
%  QUERY UTILITIES — pronouns, grounding, gates (all general)
% ════════════════════════════════════════════════════════════════════

% first_main(+Relations, -Subject, -Object) : positional mirror of the
% frozen coreference rule (first main edge establishes referents).
first_main(Rels, S, O) :-
    member(relation(main, _, [S, O]), Rels), !.

% resolve_pronouns(+Tokens, +Text, -Resolved) : she/he->subject, it->object.
resolve_pronouns(QT, Text, Resolved) :-
    split_string(Text, ".", "", Parts),
    findall(R, ( member(P, Parts),
                 string_to_atom(P, At), atom_string(At, S),
                 string_length(S, L), L > 1,
                 parser_v2:parse_sentence(S, Rs),
                 member(R, Rs) ),
            Rels),
    ( first_main(Rels, Subj, Obj) ->
        maplist(subst_pron(Subj, Obj), QT, Resolved)
    ; Resolved = QT
    ).

subst_pron(Subj, _, she, Subj) :- Subj \== unknown, !.
subst_pron(Subj, _, he, Subj) :- Subj \== unknown, !.
subst_pron(_, Obj, it, Obj) :- Obj \== unknown, !.
subst_pron(_, _, W, W).

% o_grounded(+Word, +Nodes) : baseline nodes or any learned memory.
o_grounded(W, Nodes) :-
    ( member(W, Nodes) -> true
    ; attribute_learning:type_fact(_, W, _) -> true
    ; attribute_learning:type_fact(_, _, W) -> true
    ; novel_relation:learned_fact(_, Args), member(W, Args) -> true
    ; og_link(_, W, _, _) -> true
    ; og_link(_, _, W, _) -> true
    ; multihop:mh_link(_, W, _, _, _) -> true
    ; multihop:mh_link(_, _, W, _, _) -> true
    ).

% o_entities(+Tokens, +Nodes, -Entities) : grounded content words.
o_entities(QT, Nodes, Ents) :-
    findall(E,
            ( member(E, QT), atom(E),
              parser_v2:content_word(E),
              o_grounded(E, Nodes) ),
            Es0),
    sort(Es0, Ents).

% context_nodes(+Text, -Nodes) : frozen baseline graph nodes, read-only.
context_nodes(Text, Nodes) :-
    split_string(Text, ".", "", Parts),
    findall(R, ( member(P, Parts),
                 string_to_atom(P, At), atom_string(At, S),
                 string_length(S, L), L > 1,
                 parser_v2:parse_sentence(S, Rs),
                 member(R, Rs) ),
            Rels),
    maplist(rel_to_compact_o, Rels, Compact),
    semantic_field:build_graph(Compact, graph(Nodes, _)).

rel_to_compact_o(relation(T, P, A), rel(T, P, A)).

% loc_connected(+Entities, +Text) : some entity touches a location edge
% directly or via a main edge (general anti-hallucination gate for where).
loc_connected(Ents, Text) :-
    split_string(Text, ".", "", Parts),
    findall(R, ( member(P, Parts),
                 string_to_atom(P, At), atom_string(At, S),
                 string_length(S, L), L > 1,
                 parser_v2:parse_sentence(S, Rs),
                 member(R, Rs) ),
            Rels),
    maplist(rel_to_compact_o, Rels, Compact),
    semantic_field:build_graph(Compact, graph(_, Edges)),
    member(E, Ents),
    ( member(edge(E, _, _, location), Edges) -> true
    ; member(edge(E, _, M, main), Edges),
      member(edge(M, _, _, location), Edges)
    ), !.

% verb_gate_blocks(+Tokens, +Entities) : a query verb matches an induced
% family but no learned fact under it mentions the entities -> unknown.
% (Prevents verb-agnostic baseline leakage on cross-episode queries.)
verb_gate_blocks(QT, Ents) :-
    member(W, QT), atom(W),
    parser_v2:content_word(W),
    induced_family_of(W, _),
    \+ fact_mentions(W, Ents).

induced_family_of(W, N) :-
    novel_relation:novel_relation(N, _),
    multihop:verb_match(W, N).
induced_family_of(W, N) :-
    og_rel(N),
    multihop:verb_match(W, N).

fact_mentions(W, Ents) :-
    novel_relation:novel_relation(N, _),
    multihop:verb_match(W, N),
    novel_relation:learned_fact(N, Args),
    member(E, Ents), member(E, Args).
fact_mentions(W, Ents) :-
    og_rel(N),
    multihop:verb_match(W, N),
    og_link(N, A, B, _),
    ( member(A, Ents) ; member(B, Ents) ).

% possessive_query(+Tokens, -X, -Y) : X's Y form or Y of [the] X form.
% "of"/"the" are documented structural glue (function-level).
possessive_query(QT, X, Y) :-
    member(W, QT),
    multihop:strip_possessive(W, X), W \== X, atom(X),
    member(Y, QT), Y \== W, atom(Y),
    parser_v2:content_word(Y).
possessive_query(QT, X, Y) :-
    ( append(_, [Y, of, the, X|_], QT)
    ; append(_, [Y, of, X|_], QT) ),
    atom(X), atom(Y),
    parser_v2:content_word(X),
    parser_v2:content_word(Y).


% ════════════════════════════════════════════════════════════════════
%  ANSWERING — open layer gates, then down the frozen stack
% ════════════════════════════════════════════════════════════════════

% answer_open(+Text, +Query, -Answer, -Path)
answer_open(Text, Query, Answer, Path) :-
    split_string(Text, ".", "", Parts),
    forall(( member(P, Parts),
             string_to_atom(P, At), atom_string(At, S),
             string_length(S, L), L > 1 ),
           ingest_open(S)),
    parser_v2:normalize_text(Query, QT0),
    context_nodes(Text, Nodes),
    resolve_pronouns(QT0, Text, QT),
    o_entities(QT, Nodes, Ents),
    ( possessive_query(QT, X, Y),
      o_grounded(X, Nodes), o_grounded(Y, Nodes) ->
        answer_possessive(Text, QT, X, Y, Answer, Path)
    ; QT = [Q0|_], member(Q0, [is, are, was, were]),
      findall(E, ( member(E, QT), atom(E),
                   parser_v2:content_word(E),
                   o_grounded(E, Nodes) ), [E1, E2] ) ->
        ( o_paths(E1, 5, E2, P), P \== [] ->
            Answer = yes, Path = P
        ; Answer = no_evidence, Path = []
        )
    ; verb_gate_blocks(QT, Ents) ->
        Answer = unknown, Path = []
    ; QT = [W0|_], member(W0, [where]),
      \+ loc_connected(Ents, Text) ->
        Answer = unknown, Path = []
    ; catch(multihop:answer_mhop(Text, Query, Answer, Path),
            _, fail) ->
        true
    ; Answer = unknown, Path = []
    ).

% answer_possessive(+Text, +QT, +X, +Y, -Answer, -Path) : Y must link to
% X's episode (any stored path reaching it); then exactly one slot value
% on Y within the queried (bound) category. No fallback: an unlinked
% possessive or an ambiguous value is invalid, never guessed.
answer_possessive(Text, QT, X, Y, Answer, Path) :-
    ( QT = [_, Cat|_], atom(Cat), parser_v2:content_word(Cat),
      attribute_learning:attr_binding(Cat, _) ->
        ( o_paths(X, 5, Y, [LP|_]) ->
            findall(A-SH, poss_slot_value(Text, Cat, Y, A, SH), Pairs0),
            sort(Pairs0, Pairs),
            ( Pairs = [Answer-SH] ->
                Path = [LP, SH]
            ; Answer = unknown, Path = []
            )
        ; Answer = unknown, Path = []
        )
    ; Answer = unknown, Path = []
    ).

% poss_slot_value(+Text, +Cat, +Entity, -Value, -Hop) : single-category
% slot lookup. Types: learned memory (closed). Attributes: current
% episodic context, novel values allowed, other-category members excluded
% (same discipline as EXP-23, read through its memory, never written).
poss_slot_value(_Text, Cat, E, A, hop(R, E, A, fwd, 1.0, exp23, type)) :-
    attribute_learning:attr_binding(Cat, type),
    attribute_learning:type_fact(R, E, A),
    attribute_learning:known_type_rel(R).
poss_slot_value(Text, Cat, E, A, hop(R, E, A, fwd, 1.0, frozen, attribute)) :-
    attribute_learning:attr_binding(Cat, attr),
    attribute_learning:context_graph(Text, graph(_, Edges)),
    member(edge(E, R, A, attribute), Edges),
    \+ ( attribute_learning:cat_member(Other, A), Other \== Cat ).
