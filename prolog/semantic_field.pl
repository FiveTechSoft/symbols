:- module(semantic_field,
    [ build_graph/2,
      init_energy/4,
      propagate/5,
      extract_trajectory/5,
      answer_query/4,
      field_to_string/2,
      resolve_sentences/3,
      resolve_sentence_pronouns/3,
      tokens_to_string/2,
      extract_referents/3,
      resolve_query_pronouns/4
    ]).

:- use_module(library(lists)).
:- use_module(library(apply)).
:- use_module(parser_v2).


% ════════════════════════════════════════════════════════════════════
%  GRAPH CONSTRUCTION
% ════════════════════════════════════════════════════════════════════

build_graph(Relations, graph(Nodes, Edges)) :-
    extract_nodes_edges(Relations, Nodes0, Edges0),
    sort(Nodes0, Nodes),
    sort(Edges0, EdgesRaw),
    dedup_main_edges(EdgesRaw, Edges).

build_graph(Relations, Tokens, graph(Nodes, Edges)) :-
    extract_nodes_edges(Relations, Nodes0, Edges0),
    sort(Nodes0, Nodes),
    sort(Edges0, EdgesRaw),
    dedup_main_edges(EdgesRaw, Tokens, Edges).

% Two-pass dedup: identify attribute descriptors vs nouns
dedup_main_edges(Edges, Cleaned) :-
    dedup_main_edges(Edges, [], Cleaned).

dedup_main_edges(Edges, Tokens, Cleaned) :-
    findall(T, member(edge(_, _, T, main), Edges), MainTargets0),
    sort(MainTargets0, MainTargets),
    % Check for bidirectional attribute pairs between main targets
    findall(A-B, (
        member(A, MainTargets), member(B, MainTargets), A @< B,
        member(edge(A, _, B, attribute), Edges),
        member(edge(B, _, A, attribute), Edges)
    ), BiPairs),
    ( BiPairs = [_|_] ->
        % Break cycles using article-based language detection
        % (per-sentence tokens: single noun per sentence is correct)
        ( Tokens \= [], detect_noun_from_article(Tokens, MainTargets, Noun) ->
            findall(E, (
                member(E, Edges),
                ( E = edge(_, _, Target, main) -> Target = Noun ; true )
            ), Cleaned)
        ;
            % Fallback: use original descriptor logic
            dedup_fallback(Edges, MainTargets, Cleaned)
        )
    ;
        % No bidirectional pairs: use original descriptor logic
        dedup_fallback(Edges, MainTargets, Cleaned)
    ).

dedup_fallback(Edges, MainTargets, Cleaned) :-
    findall(Node, (
        member(Node, MainTargets),
        member(edge(Node, _, Other, attribute), Edges),
        member(Other, MainTargets),
        Node \= Other,
        \+ (member(edge(Another, _, Node, attribute), Edges),
            member(Another, MainTargets),
            Another \= Node)
    ), Descriptors0),
    sort(Descriptors0, Descriptors),
    findall(E, (
        member(E, Edges),
        ( E = edge(_, _, Target, main) -> \+ member(Target, Descriptors) ; true )
    ), Cleaned).

% Detect noun using article position in token list
% English: "a blue house" → article before adjective → noun is LAST content word after article
% Spanish: "un coche rojo" → article before noun → noun is FIRST content word after article
detect_noun_from_article(Tokens, MainTargets, Noun) :-
    % Find English articles
    member(Article, [a, an, the]),
    member(Article, Tokens),
    nth0(ArtIdx, Tokens, Article),
    !,
    % Find main targets that appear after the article
    findall(Pos-C, (
        nth0(Pos, Tokens, C),
        Pos > ArtIdx,
        member(C, MainTargets)
    ), AfterArticle),
    keysort(AfterArticle, Sorted),
    % English: adjective first, noun second → noun is LAST (farthest from article)
    last(Sorted, _-Noun).

detect_noun_from_article(Tokens, MainTargets, Noun) :-
    % Find Spanish articles
    member(Article, [un, una, el, la, los, las, unos, unas]),
    member(Article, Tokens),
    nth0(ArtIdx, Tokens, Article),
    !,
    % Find main targets that appear after the article
    findall(Pos-C, (
        nth0(Pos, Tokens, C),
        Pos > ArtIdx,
        member(C, MainTargets)
    ), AfterArticle),
    keysort(AfterArticle, Sorted),
    % Spanish: noun first, adjective second → noun is FIRST (closest to article)
    Sorted = [_-Noun|_].

extract_nodes_edges([], [], []).
extract_nodes_edges([rel(Type, Pred, [A,B])|Rest], Nodes, [edge(A, Pred, B, Type)|Edges]) :-
    atom(A), atom(B), !,
    extract_nodes_edges(Rest, NodesRest, Edges),
    sort([A,B|NodesRest], Nodes).
extract_nodes_edges([rel(Type, Pred, [Compound,B])|Rest], Nodes, [edge(Compound, Pred, B, Type)|Edges]) :-
    compound(Compound), atom(B), !,
    extract_nodes_edges(Rest, NodesRest, Edges),
    sort([B|NodesRest], Nodes1),
    add_compound_parts(Compound, Nodes1, Nodes).
extract_nodes_edges([rel(Type, Pred, [A,Compound])|Rest], Nodes, [edge(A, Pred, Compound, Type)|Edges]) :-
    atom(A), compound(Compound), !,
    extract_nodes_edges(Rest, NodesRest, Edges),
    sort([A|NodesRest], Nodes1),
    add_compound_parts(Compound, Nodes1, Nodes).
extract_nodes_edges([_|Rest], Nodes, Edges) :-
    extract_nodes_edges(Rest, Nodes, Edges).

add_compound_parts(Compound, Nodes0, Nodes) :-
    compound(Compound), !,
    Compound =.. [_|Args],
    add_args_to_nodes(Args, Nodes0, Nodes).
add_compound_parts(_, Nodes, Nodes).

add_args_to_nodes([], Nodes, Nodes).
add_args_to_nodes([A|Rest], Nodes0, Nodes) :-
    atom(A), !,
    add_args_to_nodes(Rest, [A|Nodes0], Nodes).
add_args_to_nodes([_|Rest], Nodes0, Nodes) :-
    add_args_to_nodes(Rest, Nodes0, Nodes).


% ════════════════════════════════════════════════════════════════════
%  STRUCTURAL COMPATIBILITY
% ════════════════════════════════════════════════════════════════════
% The core insight: energy should flow preferentially through edges
% whose TYPE matches the QUESTION TYPE.
%
%   WHERE  ↔ location
%   WHEN   ↔ temporal
%   COLOR  ↔ attribute (color)
%   WHAT   ↔ main (object)
%   WHO    ↔ main (subject)

propagation_weight(where,   location,   0.95).
propagation_weight(where,   indirect,   0.6).
propagation_weight(where,   main,       0.2).
propagation_weight(where,   attribute,  0.1).
propagation_weight(where,   temporal,   0.05).
propagation_weight(where,   _,          0.05).

propagation_weight(who,     main,       0.9).
propagation_weight(who,     indirect,   0.4).
propagation_weight(who,     _,          0.1).

propagation_weight(what,    main,       0.9).
propagation_weight(what,    attribute,  0.5).
propagation_weight(what,    indirect,   0.3).
propagation_weight(what,    _,          0.1).

propagation_weight(when,    temporal,   0.95).
propagation_weight(when,    main,       0.2).
propagation_weight(when,    _,          0.05).

propagation_weight(color_query, attribute, 0.95).
propagation_weight(color_query, main,      0.2).
propagation_weight(color_query, _,         0.05).

propagation_weight(size_query,  attribute, 0.95).
propagation_weight(size_query,  main,      0.2).
propagation_weight(size_query,  _,         0.05).

propagation_weight(negation,    negation,  0.95).
propagation_weight(negation,    main,      0.3).
propagation_weight(negation,    _,         0.1).

propagation_weight(_,           main,      0.5).
propagation_weight(_,           attribute, 0.4).
propagation_weight(_,           _,         0.2).


% ════════════════════════════════════════════════════════════════════
%  ENERGY FIELD INITIALIZATION (query-subject-driven)
% ════════════════════════════════════════════════════════════════════
% Energy starts at the entity mentioned in the question (the subject),
% then flows through the graph via query-type-matching edges.
%
% For "donde vive juan": juan=1.0, others=0.1
%   → propagation through location edges → malaga gets energy
%
% For "que compro juan": juan=1.0, others=0.1
%   → propagation through main edges → coche gets energy

init_energy(graph(Nodes, Edges), QueryType, QueryTokens, Field) :-
    findall(N, (member(N, QueryTokens), member(N, Nodes), atom(N)), QueryEntities),
    ( QueryEntities = [] ->
        maplist(init_node_structural(QueryType, Edges), Nodes, Field)
    ;
        maplist(init_node_subject(QueryEntities), Nodes, Field0),
        ( QueryType = who ->
            findall(Source, (
                member(edge(Source, _, Target, main), Edges),
                member(Target, QueryEntities)
            ), Agents0),
            sort(Agents0, Agents),
            foldl(boost_node(0.7), Agents, Field0, Field)
        ;
            Field = Field0
        )
    ).

% When we know the query subject: 1.0 for subjects, 0.1 for others
init_node_subject(QueryEntities, Node, energy(Node, 1.0)) :-
    member(Node, QueryEntities), !.
init_node_subject(_, Node, energy(Node, 0.1)).

boost_node(Energy, Node, Field0, Field) :-
    ( member(energy(Node, EOld), Field0) ->
        NewE is max(Energy, EOld),
        select(energy(Node, EOld), Field0, Field1),
        Field = [energy(Node, NewE)|Field1]
    ;
        Field = [energy(Node, Energy)|Field0]
    ).

% Fallback: structural initialization (when no entity matches)
init_node_structural(QueryType, Edges, Node, energy(Node, Score)) :-
    findall(Sc, (
        member(edge(Node, _, _, Type), Edges),
        structural_role(QueryType, source, Type, Sc)
    ; member(edge(_, _, Node, Type), Edges),
      structural_role(QueryType, target, Type, Sc)
    ), Scores),
    ( Scores = [] -> Score = 0.1
    ; sum_list(Scores, Total), length(Scores, Len),
      Score is Total / Len
    ).

% structural_role(+QueryType, +NodeType, +EdgeType, -Score)
structural_role(what,  target, main,  0.9).
structural_role(what,  source, main,  0.3).
structural_role(what,  _,      attribute, 0.5).
structural_role(who,   source, main,  0.9).
structural_role(who,   target, main,  0.3).
structural_role(where, target, location, 0.95).
structural_role(where, source, main, 0.2).
structural_role(where, target, main, 0.2).
structural_role(where, target, indirect, 0.5).
structural_role(when,  target, temporal, 0.95).
structural_role(color_query, _, attribute, 0.9).
structural_role(size_query,  _, attribute, 0.9).
structural_role(negation, source, negation, 0.9).
structural_role(_, source, main, 0.5).
structural_role(_, target, main, 0.5).


% ════════════════════════════════════════════════════════════════════
%  ENERGY PROPAGATION (query-type-dependent)
% ════════════════════════════════════════════════════════════════════
% Energy flows through edges. The weight of each edge depends on
% how well its TYPE matches the QUERY TYPE.
%
%   E(target) = E(source) × propagation_weight(QueryType, EdgeType) × decay

propagate(_, Field, _, 0, Field) :- !.
propagate(Graph, Field0, QueryType, Hops, FinalField) :-
    Hops > 0,
    propagate_one(Graph, QueryType, Field0, Field1),
    Hops1 is Hops - 1,
    propagate(Graph, Field1, QueryType, Hops1, FinalField).

propagate_one(graph(_Nodes, Edges), QueryType, Field0, Field1) :-
    % Direct edges: Source has energy -> Target gets energy
    findall(energy(Target, NewE), (
        member(edge(Source, _R1, Target, Type), Edges),
        member(energy(Source, E0), Field0),
        E0 > 0.01,
        propagation_weight(QueryType, Type, W),
        decay_factor(D),
        NewE is E0 * W * D
    ), DirectContribs),
    % Compound-source edges (temporal/indirect): e.g. edge(comprar(mary,house), tiempo, yesterday, temporal)
    % Energy flows from compound parts (mary, house) to Target (yesterday)
    findall(energy(Target, NewE), (
        member(edge(Source, _R2, Target, Type), Edges),
        compound(Source),
        Source =.. [_|Args],
        member(Arg, Args), atom(Arg),
        member(energy(Arg, E0), Field0),
        E0 > 0.01,
        propagation_weight(QueryType, Type, W),
        decay_factor(D),
        NewE is E0 * W * D
    ), CompoundContribs),
    append(DirectContribs, CompoundContribs, Contributions),
    merge_energy(Contributions, Field0, Field1).

decay_factor(0.85).

% Merge: max energy per node
merge_energy([], Field, Field).
merge_energy([energy(N, E)|Rest], Field0, FieldFinal) :-
    ( member(energy(N, EOld), Field0) ->
        NewE is max(E, EOld),
        select(energy(N, EOld), Field0, Field1)
    ;
        NewE is E,
        Field1 = Field0
    ),
    ( member(energy(N, _), Field1) ->
        select(energy(N, _), Field1, Field2),
        Field3 = [energy(N, NewE)|Field2]
    ;
        Field3 = [energy(N, NewE)|Field1]
    ),
    merge_energy(Rest, Field3, FieldFinal).


% ════════════════════════════════════════════════════════════════════
%  TRAJECTORY EXTRACTION (BFS path-finding)
% ════════════════════════════════════════════════════════════════════
% Find path from query subject to appropriate destination node.
% WHERE -> location node, WHEN -> temporal node, COLOR -> attribute node,
% WHAT -> main-edge target, WHO -> main-edge source.

extract_trajectory(graph(_Nodes, Edges), Field, QueryType, QueryTokens, trajectory(Path, TotalEnergy)) :-
    find_subject(Field, Edges, QueryType, QueryTokens, Subject),
    find_destination(Edges, Field, QueryType, Subject, QueryTokens, Destination),
    find_path(Subject, Destination, Edges, Field, Path),
    path_energy(Path, Field, TotalEnergy).

% Query tense from auxiliaries (function words only, no content hardcoding)
% past: did/was/were | present: does/do/is/are
query_tense(QueryTokens, past) :-
    ( member(did, QueryTokens) ; member(was, QueryTokens) ; member(were, QueryTokens) ), !.
query_tense(QueryTokens, present) :-
    ( member(does, QueryTokens) ; member(do, QueryTokens) ;
      member(is, QueryTokens) ; member(are, QueryTokens) ), !.
query_tense(_, unknown).

% Does a main target have a temporal edge? (direct or via compound containing it)
has_temporal(Target, Edges) :-
    member(edge(Target, _, _, temporal), Edges), !.
has_temporal(Target, Edges) :-
    member(edge(Source, _, _, temporal), Edges),
    compound(Source), Source =.. [_|Args], member(Target, Args), !.

% Find the query subject
% Note: QueryTokens reserved for future tense-aware subject selection;
% currently WHAT uses agent + tense-aware object selection in find_best_object/5
find_subject(Field, Edges, QueryType, _QueryTokens, Subject) :-
    % For WHO: look for source of main edges (the agent)
    ( QueryType = who ->
        findall(N, (
            member(energy(N, E), Field), E > 0.5, atom(N), \+ compound(N),
            member(edge(N, _, _, main), Edges)
        ), Agents),
        Agents = [Subject|_]
    ;
    % For WHAT: return the agent (source), let find_best_object pick tense-appropriate target
    % This avoids prematurely picking first object when multiple exist (house vs car)
    ( QueryType = what ->
        find_main_agent(Field, Edges, Subject)
    ;
    % For WHERE/WHEN/COLOR: look for subject connected to the answer type
    find_main_agent(Field, Edges, Subject)
    )).

find_main_agent(Field, Edges, Subject) :-
    findall(E-N, (
        member(energy(N, E), Field), E > 0.5, atom(N), \+ compound(N),
        member(edge(N, _, _, main), Edges)
    ), Agents),
    ( Agents = [] ->
        findall(E-N, (
            member(energy(N, E), Field), atom(N), \+ compound(N)
        ), All),
        keysort(All, Sorted), reverse(Sorted, Rev),
        Rev = [_-Subject|_]
    ;
        keysort(Agents, Sorted), reverse(Sorted, Rev),
        Rev = [_-Subject|_]
    ).

% Find the appropriate destination node based on query type
find_destination(Edges, Field, QueryType, Subject, QueryTokens, Destination) :-
    ( QueryType = where ->
        find_best_location(Subject, Edges, Field, QueryTokens, Destination)
    ; QueryType = when ->
        find_best_temporal(Subject, Edges, Field, Destination)
    ; QueryType = color_query ->
        find_best_attribute(Subject, Edges, Field, Destination)
    ; QueryType = what ->
        find_best_object(Subject, Edges, Field, QueryTokens, Destination)
    ; QueryType = who ->
        Destination = Subject
    ;
        findall(E-N, (member(energy(N, E), Field), atom(N), \+ compound(N)), All),
        keysort(All, Sorted), reverse(Sorted, Rev), Rev = [_-Destination|_]
    ).

% Find location: prefer direct if subject-focused, via-main if object-focused
find_best_location(Subject, Edges, Field, QueryTokens, Dest) :-
    % Check if query implies action on object (compro, compro, etc.)
    ( (member(compro, QueryTokens); member(comprou, QueryTokens); member(bought, QueryTokens); member(buy, QueryTokens)) ->
        % Object-focused: prefer via-main (location of purchased object)
        findall(D, (
            member(edge(Subject, _, Mid, main), Edges),
            member(edge(Mid, _, D, location), Edges),
            member(energy(D, E), Field), E > 0.1
        ), ViaMainLocs),
        ( ViaMainLocs = [D|_] -> Dest = D ;
            findall(D, (
                member(edge(Subject, _, D, location), Edges),
                member(energy(D, E), Field), E > 0.1
            ), DirectLocs),
            ( DirectLocs = [D|_] -> Dest = D ; Dest = Subject )
        )
    ;
        % Subject-focused: prefer direct
        findall(D, (
            member(edge(Subject, _, D, location), Edges),
            member(energy(D, E), Field), E > 0.1
        ), DirectLocs),
        ( DirectLocs = [D|_] -> Dest = D ;
            findall(D, (
                member(edge(Subject, _, Mid, main), Edges),
                member(edge(Mid, _, D, location), Edges),
                member(energy(D, E), Field), E > 0.1
            ), ViaMainLocs),
            ( ViaMainLocs = [D|_] -> Dest = D ; Dest = Subject )
        )
    ).

find_best_temporal(Subject, Edges, Field, Dest) :-
    % Direct temporal edge from Subject
    ( member(edge(Subject, _, D, temporal), Edges), member(energy(D, E), Field), E > 0.3 ->
        Dest = D
    % Compound-source temporal: edge(comprar(Subject,Obj), tiempo, D, temporal)
    ; member(edge(Source, _, D, temporal), Edges), compound(Source),
      Source =.. [_|Args], member(Subject, Args),
      member(energy(D, E), Field), E > 0.3 ->
        Dest = D
    ;
        findall(D, (
            member(edge(_, _, D, temporal), Edges),
            member(energy(D, E), Field), E > 0.3
        ), All),
        ( All = [Dest|_] -> true ; Dest = Subject )
    ).

find_best_attribute(Subject, Edges, Field, Dest) :-
    ( member(edge(Subject, _, D, attribute), Edges), member(energy(D, E), Field), E > 0.3 ->
        Dest = D
    ;
    % For "color del coche": coche → attribute → rojo
        findall(D-E, (
            member(edge(Mid, _, D, attribute), Edges),
            member(edge(Subject, _, Mid, main), Edges),
            member(energy(D, E), Field), E > 0.3
        ), Attrs),
        ( Attrs = [D-_|_] -> Dest = D ;
            findall(D, (
                member(edge(_, _, D, attribute), Edges),
                member(energy(D, E), Field), E > 0.3
            ), All),
            ( All = [Dest|_] -> true ; Dest = Subject )
        )
    ).

% Target has an attribute descriptor (e.g. house->blue): it's a noun object, not a bare place
has_attribute(Target, Edges) :-
    member(edge(Target, _, _, attribute), Edges), !.

find_best_object(Subject, Edges, Field, QueryTokens, Dest) :-
    findall(D, (
        member(edge(Subject, _, D, main), Edges),
        atom(D), \+ compound(D)
    ), AllTargets0),
    sort(AllTargets0, AllTargets),
    query_tense(QueryTokens, Tense),
    ( AllTargets = [Single] ->
        Dest = Single
    ; AllTargets = [_|_] ->
        % Multiple candidates: tense + temporal first, then noun-with-attribute, then energy
        % past (did/was) -> prefers target WITH temporal (bought yesterday)
        % present (does/is) -> prefers target WITHOUT temporal (has now)
        ( Tense = past, member(Cand, AllTargets), has_temporal(Cand, Edges),
          member(energy(Cand, E), Field), E > 0.2 -> Dest = Cand
        ; Tense = present, member(Cand, AllTargets), \+ has_temporal(Cand, Edges),
          has_attribute(Cand, Edges),
          member(energy(Cand, E), Field), E > 0.2 -> Dest = Cand
        ; Tense = present, member(Cand, AllTargets), \+ has_temporal(Cand, Edges),
          member(energy(Cand, E), Field), E > 0.2 -> Dest = Cand
        % Fallback: prefer noun objects (with attributes) over bare places (berlin/paris)
        ; member(Cand, AllTargets), has_attribute(Cand, Edges),
          member(energy(Cand, E), Field), E > 0.2 -> Dest = Cand
        ; member(Cand, AllTargets), member(energy(Cand, E), Field), E > 0.3 -> Dest = Cand
        ; AllTargets = [Dest|_]
        )
    ;
        % No direct main targets (Subject is already the object): return Subject
        Dest = Subject
    ).

% Find shortest path between two nodes (wrapper)
find_path(Start, Goal, Edges, Field, Path) :-
    bfs_path(Start, Goal, Edges, Field, Path).

% BFS from Start to Goal with max path length
bfs_path(Start, Goal, _Edges, _Field, [Start, Goal]) :- Start = Goal, !.
bfs_path(Start, Goal, Edges, Field, Path) :-
    bfs([[Start]], Goal, Edges, Field, 10, RevPath),
    reverse(RevPath, Path).

% Base case: goal found at end of a path
bfs(QUEUE, Goal, _Edges, _Field, _MaxLen, Path) :-
    QUEUE = [Path|_],
    Path = [Goal|_], !.
% Expand first path in queue
% Traversal includes: direct edges (Current->Next) AND compound-source edges
% (e.g. edge(comprar(mary,house), tiempo, yesterday) traversable from mary or house)
bfs(QUEUE, Goal, Edges, Field, MaxLen, Result) :-
    QUEUE = [Path|Rest],
    Path = [Current|_],
    length(Path, Len), Len < MaxLen,
    findall([Next|Path], (
        ( member(edge(Current, _, Next, _), Edges)
        ; member(edge(Source, _, Next, _), Edges), compound(Source),
          Source =.. [_|Args], member(Current, Args)
        ),
        member(energy(Next, E), Field), E > 0.01,
        \+ member(Next, Path)
    ), NewPaths),
    append(Rest, NewPaths, NewQueue),
    bfs(NewQueue, Goal, Edges, Field, MaxLen, Result).

% Sum energy of all nodes in path
path_energy(Path, Field, Total) :-
    findall(E, (
        member(N, Path),
        member(energy(N, E), Field)
    ), Energies),
    sum_list(Energies, Total).


% ════════════════════════════════════════════════════════════════════
%  ANSWER QUERY — full pipeline
% ════════════════════════════════════════════════════════════════════

answer_query(Text, QueryText, Result, Details) :-
    % Split multi-sentence text and merge relations
    split_string(Text, ".", "", Parts),
    % Parse sentences with coreference resolution
    resolve_sentences(Parts, AllRelations),
    flatten(AllRelations, Relations),
    % Collect tokens from ALL sentences for article detection in dedup
    findall(Tokens, (
        member(Part, Parts),
        string_to_atom(Part, Atom),
        atom_string(Atom, Str),
        string_length(Str, Len), Len > 1,
        parser_v2:normalize_text(Str, Tokens)
    ), TokenLists),
    flatten(TokenLists, AllTokens),
    ( Relations = [] ->
        Result = unknown,
        Details = details(query_type=unknown, field=[], trajectory=[], energy=0)
    ;
        maplist(rel_to_compact, Relations, CompactRels),
        build_graph(CompactRels, AllTokens, Graph),
        % Extract referents from text for query resolution
        extract_referents(Relations, Subject, Object),
        % Resolve pronouns in query
        parser_v2:normalize_text(QueryText, QueryTokens0),
        resolve_query_pronouns(QueryTokens0, Subject, Object, QueryTokens),
        parser_v2:query_type(QueryType, QueryTokens),
        init_energy(Graph, QueryType, QueryTokens, Field0),
        propagate(Graph, Field0, QueryType, 3, FieldFinal),
        extract_trajectory(Graph, FieldFinal, QueryType, QueryTokens, Trajectory),
        Trajectory = trajectory(Path, Energy),
        ( Path = [] ->
            Result = unknown,
            Details = details(query_type=QueryType, field=FieldFinal, trajectory=[], energy=0)
        ;
            last(Path, Answer),
            Result = Answer,
            Details = details(query_type=QueryType, field=FieldFinal, trajectory=Path, energy=Energy)
        )
    ).

% ════════════════════════════════════════════════════════════════════
%  COREFERENCE RESOLUTION
% ════════════════════════════════════════════════════════════════════

% Parse sentences with coreference: first sentence establishes referents
resolve_sentences([], _, []).
resolve_sentences([Part|Rest], PrevSubject, [Rels|RelsRest]) :-
    string_to_atom(Part, Atom),
    atom_string(Atom, Str),
    string_length(Str, Len),
    Len > 1,
    !,
    parser_v2:normalize_text(Str, Tokens0),
    resolve_sentence_pronouns(Tokens0, PrevSubject, Tokens),
    tokens_to_string(Tokens, ResolvedStr),
    parser_v2:parse_sentence(ResolvedStr, Rels),
    ( member(relation(main, _, [Subj, _]), Rels) ->
        NewSubject = Subj
    ; NewSubject = PrevSubject
    ),
    resolve_sentences(Rest, NewSubject, RelsRest).
resolve_sentences([_|Rest], PrevSubject, RelsRest) :-
    resolve_sentences(Rest, PrevSubject, RelsRest).

resolve_sentences([], []).
resolve_sentences([Part|Rest], [Rels|RelsRest]) :-
    resolve_sentences([Part|Rest], unknown, [Rels|RelsRest]).

% Per-sentence dedup: handle bidirectional attribute edges
dedup_per_sentence(Relations, Tokens, Cleaned) :-
    extract_nodes_edges(Relations, _, Edges0),
    sort(Edges0, Edges),
    dedup_main_edges(Edges, Tokens, CleanedEdges),
    maplist(edge_to_rel, CleanedEdges, CleanedRels),
    sort(CleanedRels, Cleaned).

edge_to_rel(edge(A, Pred, B, Type), relation(Type, Pred, [A,B])).

% Convert list of atoms to space-separated string
tokens_to_string(Tokens, Str) :-
    atomic_list_concat(Tokens, ' ', Atom),
    atom_string(Atom, Str).

% Resolve pronouns in sentence tokens
resolve_sentence_pronouns([], _, []).
resolve_sentence_pronouns([she|T], Subject, [Subject|TResolved]) :-
    Subject \== unknown, !,
    resolve_sentence_pronouns(T, Subject, TResolved).
resolve_sentence_pronouns([he|T], Subject, [Subject|TResolved]) :-
    Subject \== unknown, !,
    resolve_sentence_pronouns(T, Subject, TResolved).
resolve_sentence_pronouns([it|T], Subject, [Subject|TResolved]) :-
    Subject \== unknown, !,
    resolve_sentence_pronouns(T, Subject, TResolved).
resolve_sentence_pronouns([H|T], Subject, [H|TResolved]) :-
    resolve_sentence_pronouns(T, Subject, TResolved).

% Extract referents from relations: first main edge subject and object
extract_referents(Relations, Subject, Object) :-
    % Find first main edge (may be relation/3 or rel/3)
    ( member(relation(main, _, [Subject, Object]), Relations) -> true
    ; member(rel(main, _, [Subject, Object]), Relations) -> true
    ), !.
extract_referents(_, unknown, unknown).

% Resolve pronouns in query tokens using referents from text
resolve_query_pronouns([], _, _, []).
resolve_query_pronouns([she|T], Subject, Object, [Subject|TResolved]) :-
    Subject \== unknown, !,
    resolve_query_pronouns(T, Subject, Object, TResolved).
resolve_query_pronouns([he|T], Subject, Object, [Subject|TResolved]) :-
    Subject \== unknown, !,
    resolve_query_pronouns(T, Subject, Object, TResolved).
resolve_query_pronouns([it|T], Subject, Object, [Object|TResolved]) :-
    Object \== unknown, !,
    resolve_query_pronouns(T, Subject, Object, TResolved).
resolve_query_pronouns([H|T], Subject, Object, [H|TResolved]) :-
    resolve_query_pronouns(T, Subject, Object, TResolved).

parse_part(Part, Relations) :-
    string_to_atom(Part, Atom),
    atom_string(Atom, Str),
    string_length(Str, Len),
    Len > 1,
    !,
    parser_v2:parse_sentence(Str, Relations).
parse_part(_, []).

parse_part_tokens(Part, Tokens) :-
    string_to_atom(Part, Atom),
    atom_string(Atom, Str),
    string_length(Str, Len),
    Len > 1,
    !,
    parser_v2:normalize_text(Str, Tokens).
parse_part_tokens(_, []).

rel_to_compact(relation(Type, Pred, Args), rel(Type, Pred, Args)).


% ════════════════════════════════════════════════════════════════════
%  DISPLAY
% ════════════════════════════════════════════════════════════════════

field_to_string(Field, Str) :-
    maplist(energy_to_string, Field, Strs),
    atomic_list_concat(Strs, '\n', Str).

energy_to_string(energy(Node, E), Str) :-
    format(atom(Str), '  ~w: ~4f', [Node, E]).
