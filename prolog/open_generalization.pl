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
% Catch-all: QAs not matching the what/which+content_word pattern (color,
% material, etc.) must not cause forall to fail in open_learn (F-A infra fix).
record_open_qa(_, _, _).

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
    ; og_ty(_, W, _) -> true
    ; og_ty(_, _, W) -> true
    ).

% F-D: tokens_to_query(+Tokens, -QueryString) : rebuild delegable query
% from resolved tokens so pronoun resolution propagates downstream.
tokens_to_query(Tokens, Query) :-
    atomic_list_concat(Tokens, ' ', Query).

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

% fact_mentions(+Word, +Entities) : does any learned fact under a matching
% verb family mention the query entities directly or via type expansion?
fact_mentions(W, Ents) :-
    novel_relation:novel_relation(N, _),
    multihop:verb_match(W, N),
    novel_relation:learned_fact(N, Args),
    ( member(E, Ents), member(E, Args)
    ; % F-G: type expansion — entity E is type-expanded to its instances
      member(E, Ents),
      findall(I, ( og_ty(_, I, E) ; attribute_learning:type_fact(_, I, E) ), Is),
      member(I, Is), member(I, Args)
    ).
fact_mentions(W, Ents) :-
    og_rel(N),
    multihop:verb_match(W, N),
    og_link(N, A, B, _),
    ( member(A, Ents) ; member(B, Ents)
    ; % F-G: type expansion for og_link
      member(E, Ents),
      findall(I, ( og_ty(_, I, E) ; attribute_learning:type_fact(_, I, E) ), Is),
      ( member(I, Is), ( I == A ; I == B ) )
    ).

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
    tokens_to_query(QT, ResolvedQuery),
    o_entities(QT, Nodes, Ents),
    resolve_open(Text, QT, Nodes, Ents, ResolvedQuery, Answer, Path).

% resolve_open(+Text, +QT, +Nodes, +Ents, +QueryStr, -Answer, -Path)
% Unified reasoning: F-E gate + all gates + type/kind handler + delegation.
% Order: empty -> yes/no -> type/kind -> possessive -> attr-fresh (FIX-4) ->
% verb gate -> event (F-H) -> location -> delegation -> unknown.
resolve_open(Text, QT, Nodes, Ents, QueryStr, Answer, Path) :-
    ( Nodes == [], Ents == [] ->
        % F-E: empty parse AND nothing grounded in any store → no evidence.
        Answer = unknown, Path = []
    ; QT = [Q0|_], member(Q0, [is, are, was, were]),
      yesno_gated_answer(Text, QT, Nodes, Answer, Path) ->
        true
    ; answer_type_query(QT, Ents, Nodes, Answer, Path) ->
        true
    ; possessive_query(QT, X, Y),
      o_grounded(X, Nodes), o_grounded(Y, Nodes) ->
        answer_possessive(Text, QT, X, Y, Answer, Path)
    ; fix4_attr_gate(Text, QT, Ents, Answer, Path) ->
        true
    ; verb_gate_blocks(QT, Ents) ->
        Answer = unknown, Path = []
    ; answer_og_event(Text, QT, Nodes, Ents, Answer, Path) ->
        true
    ; QT = [W0|_], member(W0, [where]),
      \+ loc_connected(Ents, Text) ->
        Answer = unknown, Path = []
    ; catch(multihop:answer_mhop(Text, QueryStr, Answer, Path),
            _, fail) ->
        true
    ; Answer = unknown, Path = []
    ).

% yesno_gated_answer(+Text, +QT, +Nodes, -Answer, -Path) : yes/no type
% verification over learned taxonomic paths (both entities grounded), plus
% FIX-3: exactly one grounded entity + one ungrounded content word ->
% negation check, else path check -> yes / no_evidence.
yesno_gated_answer(_Text, QT, Nodes, Answer, Path) :-
    findall(E, ( member(E, QT), atom(E),
                 parser_v2:content_word(E),
                 o_grounded(E, Nodes) ), [E1, E2]),
    ( o_paths(E1, 5, E2, P), P \== [] ->
        Answer = yes, Path = P
    ; Answer = no_evidence, Path = []
    ).
yesno_gated_answer(Text, QT, Nodes, Answer, Path) :-
    findall(E, ( member(E, QT), atom(E),
                 parser_v2:content_word(E),
                 o_grounded(E, Nodes) ), Es0),
    sort(Es0, [E1]),
    findall(W, ( member(W, QT), atom(W),
                 parser_v2:content_word(W), W \== E1,
                 \+ o_grounded(W, Nodes) ), Ws0),
    sort(Ws0, [W]),
    ( negation_obs(Text, E1, W) ->
        Answer = no_evidence, Path = []
    ; o_paths(E1, 5, W, P), P \== [] ->
        Answer = yes, Path = P
    ; Answer = no_evidence, Path = []
    ).

% negation_obs(+Text, +E, +W) : a copular negation "E is NOT a W" is
% observed in the text (positional shape, no content vocabulary).
negation_obs(Text, E, W) :-
    split_string(Text, ".", "", Parts),
    member(P, Parts),
    string_to_atom(P, At), atom_string(At, S),
    string_length(S, L), L > 1,
    parser_v2:normalize_text(S, [A, X, Aux, not, C, Y]),
    parser_v2:article(A),
    parser_v2:content_word(X),
    parser_v2:auxiliary(Aux),
    parser_v2:article(C),
    parser_v2:content_word(Y),
    X == E, Y == W.

% answer_type_query(+QT, +Ents, +Nodes, -Answer, -Path) : direct type/kind
% query handler — what type/kind [of thing] is X → taxonomic lookup.
% The category word is bound via og_cat alone (binding-only gate: the
% binding is data-induced, never a word list). Engaged queries commit
% (answer or unknown), never delegate. Depth is data-induced from QA
% evidence paths: uniform depth >= 2 -> walk chain to endpoint, else
% exactly one 1-hop parent. Runs BEFORE the verb gate so type questions
% are never blocked.
answer_type_query(QT, _Ents, Nodes, Answer, Path) :-
    QT = [Q0, Cat|Rest],
    member(Q0, [what, which]),
    atom(Cat),
    og_cat(Cat, type),
    !,
    ( type_target(Rest, E),
      o_grounded(E, Nodes) ->
        ( type_depth(Cat, endpoint) ->
            ( type_endpoint(E, End) ->
                Answer = End, o_paths(E, 5, End, [V|_]), Path = [V]
            ; Answer = unknown, Path = []
            )
        ; findall(T-V,
                ( o_fwd(_R, E, T, _, _, type),
                  T \== E,
                  o_paths(E, 5, T, [V|_]) ),
                Pairs0),
          sort(Pairs0, Pairs),
          type_unique(Pairs, Answer, Path)
        )
    ; Answer = unknown, Path = []
    ).

answer_type_query(_, _, _, _, _) :- fail.

% type_target(+Rest, -Entity) : entity after "is", possessive tail ignored.
% "the museum of the lantern" resolves to the head noun before "of".
type_target(Rest, E) :-
    ( append(_, [is, the, E, of|_], Rest)
    ; append(_, [is, E, of|_], Rest)
    ; append(_, [is, the, E], Rest)
    ; append(_, [is, E], Rest)
    ; append(_, [E], Rest) ),
    atom(E), parser_v2:content_word(E).

% type_unique(+Pairs, -Answer, -Path) : one distinct parent, else unknown.
type_unique([T-V], T, [V]) :- !.
type_unique([T-V|More], Answer, Path) :-
    findall(T2, member(T2-_, More), Ts),
    sort([T|Ts], [UniqueT]), !,
    Answer = UniqueT,
    member(UniqueT-VP, [T-V|More]),
    Path = [VP].
type_unique(_, unknown, []).

% type_depth(+Cat, -Depth) : data-induced answer depth from recorded QA
% evidence paths — uniform depth >= 2 means endpoint answers.
type_depth(Cat, endpoint) :-
    findall(L, ( og_catev(Cat, type, E, A),
                 o_paths(E, 5, A, P), length(P, L) ), Ls),
    Ls \== [],
    forall(member(L, Ls), L >= 2), !.
type_depth(_, hop1).

% type_endpoint(+Entity, -End) : walk forward type edges to the unique
% terminal (visited set; branches or missing edges fail -> unknown).
type_endpoint(E, End) :-
    findall(T, ( o_fwd(_, E, T, _, _, type), T \== E ), Ts0),
    sort(Ts0, [T1]),
    ewalk([E], T1, End).
ewalk(Seen, N, End) :-
    findall(T, ( o_fwd(_, N, T, _, _, type),
                 T \== N, \+ member(T, Seen) ), Ts),
    ( Ts = [T] -> ewalk([T|Seen], T, End)
    ; Ts = [] -> End = N ).

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

% fix4_attr_gate(+Text, +QT, +Ents, -Answer, -Path) : non-first-bound
% attr categories answer only values already known under the category
% (closed values from episodic edges), unique, else unknown. The first
% bound category keeps slot openness (fresh values allowed). Bound order
% is the clause position of the induced binding (general, no word list).
fix4_attr_gate(Text, QT, Ents, Answer, Path) :-
    QT = [Q0, Cat|_], member(Q0, [what, which]),
    atom(Cat),
    attribute_learning:attr_binding(Cat, attr),
    \+ attr_first_clause(Cat),
    findall(A-E, ( member(E, Ents),
                   attribute_learning:context_graph(Text, graph(_, Edges)),
                   member(edge(E, _, A, attribute), Edges),
                   attribute_learning:cat_member(Cat, A) ),
            Pairs0),
    sort(Pairs0, Pairs),
    ( Pairs = [Answer-E] ->
        Path = [hop(Cat, E, Answer, fwd, 1.0, exp23, attribute)]
    ; Answer = unknown, Path = []
    ).

% attr_first_clause(+Cat) : the induced binding of Cat is the first
% attr_binding clause (open slot discipline for the first category).
attr_first_clause(Cat) :-
    nth_clause(attribute_learning:attr_binding(C, _), 1, Ref),
    clause(attribute_learning:attr_binding(C, _), _, Ref),
    C == Cat, !.

% answer_og_event(+Text, +QT, +Nodes, +Ents, -Answer, -Path) : F-H event
% handler. Engaged verb queries are answered from episodic evidence
% only (context sentence scan, then og_link store) and NEVER delegate:
% the delegation path reads learned stores while ignoring the current
% context (the fronted/passive leak). Grounding is strict context-node
% membership. No candidates while engaged -> unknown (commit).
answer_og_event(Text, QT, Nodes, Ents, Answer, Path) :-
    QT = [Q0|_], member(Q0, [what, who]),
    QT = [_, C|_], \+ attribute_learning:attr_binding(C, _),
    findall(A-H, ( member(W, QT), atom(W),
                   parser_v2:content_word(W),
                   og_event_cand(Text, QT, Nodes, Ents, W, A, H) ),
            Cands0),
    ( Cands0 = [] ->
        ( member(W, QT), atom(W), parser_v2:content_word(W),
          novel_relation:novel_relation(N, _),
          multihop:verb_match(W, N) ->
            Answer = unknown, Path = []
        ; fail )
    ; findall(A, member(A-_, Cands0), As0),
      sort(As0, [Answer]),
      member(Answer-H, Cands0),
      Path = [H]
    ).

% og_event_cand(+Text, +QT, +Nodes, +Ents, +W, -Answer, -Hop) : one
% episodic event answer under query verb W.
% Clause A: context sentence scan — observed verb family-matches W; the
% query grounds one side (direct), the other side answers. Who-forms use
% type expansion on the object side.
og_event_cand(Text, _QT, Nodes, Ents, W, Ans, Hop) :-
    ctx_sentence(Text, S),
    novel_relation:sentence_obs(S, obs(RawV, _, A, B)),
    novel_relation:same_family(W, RawV),
    ( member(A, Nodes), member(A, Ents) ->
        Ans = B
    ; member(B, Nodes), member(B, Ents) ->
        Ans = A
    ; member(EI, Ents),
      ( og_ty(_, B, EI)
      ; attribute_learning:type_fact(_, B, EI) ),
      member(B, Nodes) ->
        Ans = A
    ),
    Hop = hop(RawV, A, B, fwd, 1.0, ogevent, event).
% Clause B: og_link store — query verb matches the stored relation name;
% the stored subject/object is a strict context node and in the query.
og_event_cand(_Text, _QT, Nodes, Ents, W, Ans, Hop) :-
    og_link(R, X, Y, _),
    multihop:verb_match(W, R),
    ( ( member(X, Nodes), member(X, Ents) ) ->
        Ans = Y
    ; ( member(Y, Nodes), member(Y, Ents) ) ->
        Ans = X
    ),
    Hop = hop(R, X, Y, fwd, 1.0, ogevent, rel).

% ctx_sentence(+Text, -Sentence) : nonempty text sentences, backtracking.
ctx_sentence(Text, S) :-
    split_string(Text, ".", "", Parts),
    member(P, Parts),
    string_to_atom(P, At), atom_string(At, S),
    string_length(S, L), L > 1.

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
