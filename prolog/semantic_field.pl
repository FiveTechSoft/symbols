:- module(semantic_field,
    [ build_graph/2,
      init_energy/4,
      propagate/5,
      extract_trajectory/5,
      answer_query/4,
      field_to_string/2
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
    findall(energy(Target, NewE), (
        member(edge(Source, _Rel, Target, Type), Edges),
        member(energy(Source, E0), Field0),
        E0 > 0.01,
        propagation_weight(QueryType, Type, W),
        decay_factor(D),
        NewE is E0 * W * D
    ), Contributions),
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
    find_subject(Field, Edges, QueryType, Subject),
    find_destination(Edges, Field, QueryType, Subject, QueryTokens, Destination),
    find_path(Subject, Destination, Edges, Field, Path),
    path_energy(Path, Field, TotalEnergy).

% Find the query subject
find_subject(Field, Edges, QueryType, Subject) :-
    % For WHO: look for source of main edges (the agent)
    ( QueryType = who ->
        findall(N, (
            member(energy(N, E), Field), E > 0.5, atom(N), \+ compound(N),
            member(edge(N, _, _, main), Edges)
        ), Agents),
        Agents = [Subject|_]
    ;
    % For WHAT: look for object of main edges
    ( QueryType = what ->
        findall(T, (
            member(edge(_, _, T, main), Edges),
            member(energy(T, E), Field), E > 0.3, atom(T), \+ compound(T)
        ), Objects),
        ( Objects = [Subject|_] -> true ;
            find_main_agent(Field, Edges, Subject)
        )
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
        find_best_object(Subject, Edges, Field, Destination)
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
    ( member(edge(Subject, _, D, temporal), Edges), member(energy(D, E), Field), E > 0.3 ->
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

find_best_object(Subject, Edges, Field, Dest) :-
    findall(D, (
        member(edge(Subject, _, D, main), Edges),
        atom(D), \+ compound(D)
    ), AllTargets0),
    sort(AllTargets0, AllTargets),
    % Prefer: node that is TARGET of attribute from another target (it's the noun)
    % Exclude: node that is SOURCE of attribute to another target (it's the adjective)
    ( AllTargets = [D|_] ->
        ( member(Other, AllTargets), Other \= D,
          member(edge(Other, _, D, attribute), Edges) ->
            Dest = D
        ;
            member(energy(D, E), Field), E > 0.3, Dest = D
        )
    ;
        findall(D-E, (
            member(edge(Subject, _, D, main), Edges),
            member(energy(D, E), Field), E > 0.3, atom(D), \+ compound(D)
        ), Objects),
        ( Objects = [D-_|_] -> Dest = D ; Dest = Subject )
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
bfs(QUEUE, Goal, Edges, Field, MaxLen, Result) :-
    QUEUE = [Path|Rest],
    Path = [Current|_],
    length(Path, Len), Len < MaxLen,
    findall([Next|Path], (
        member(edge(Current, _, Next, _), Edges),
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
    maplist(parse_part, Parts, AllRelations),
    flatten(AllRelations, Relations),
    % Collect tokens from first sentence for article detection
    Parts = [FirstPart|_],
    ( parse_part_tokens(FirstPart, FirstTokens) ->
        SentenceTokens = FirstTokens
    ;
        SentenceTokens = []
    ),
    ( Relations = [] ->
        Result = unknown,
        Details = details(query_type=unknown, field=[], trajectory=[], energy=0)
    ;
        maplist(rel_to_compact, Relations, CompactRels),
        build_graph(CompactRels, SentenceTokens, Graph),
        parser_v2:normalize_text(QueryText, QueryTokens),
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
