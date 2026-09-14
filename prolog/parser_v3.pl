:- module(parser_v3,
    [ to_roles/2
    ]).

:- use_module(library(lists)).
:- use_module(parser_v2).

% ════════════════════════════════════════════════════════════════════
%  PARSER V3: Semantic Role Extraction
% ════════════════════════════════════════════════════════════════════
% Converts flat parser_v2 relations into event-centric role format.
%
% Input:
%   relation(main, comprar, [mary, blue])   ← adj-as-object (remove)
%   relation(main, comprar, [mary, house])  ← true object (keep)
%   relation(attribute, attribute, [blue, house])
%
% Output:
%   event(comprar, e0)
%   agent(e0, mary)
%   object(e0, house)
%   event_attribute(house, color, blue)
%
% Rule: keep object if it is TARGET of attribute edge FROM another main target.
% (The noun is what the adjective describes.)

to_roles(Relations, Roles) :-
    findall(r(T,P,A), member(relation(T,P,A), Relations), TypedRels),
    findall(r(main,P,A), member(r(main,P,A), TypedRels), MainRels),
    findall(r(attribute,P,A), member(r(attribute,P,A), TypedRels), AttrRels),

    % Main targets (objects of main relations)
    findall(O, member(r(main, _, [_,O]), MainRels), Objs0),
    sort(Objs0, AllObjects),

    % Adjective sources: nodes that have attr edges TO other main targets
    % BUT are NOT targets of attr edges FROM other main targets
    findall(S, (
        member(r(attribute, _, [S, T]), AttrRels),
        atom(S), atom(T),
        member(T, AllObjects),
        S \= T,
        \+ (member(r(attribute, _, [S2, S]), AttrRels),
            atom(S2), member(S2, AllObjects), S2 \= S)
    ), AdjSources0),
    sort(AdjSources0, AdjSources),

    % Keep main relations where object is NOT a pure adjective source
    findall(r(main, P, [S,O]), (
        member(r(main, P, [S,O]), MainRels),
        atom(O),
        \+ member(O, AdjSources)
    ), FilteredMain),

    assign_events(FilteredMain, 0, EventPairs),
    build_roles(EventPairs, AttrRels, [], Roles).

assign_events([], _, []).
assign_events([r(main, Pred, [S,O])|Rest], N, [event(Pred, E)-r(main,Pred,[S,O])|Pairs]) :-
    atom_concat(e, N, E),
    N1 is N + 1,
    assign_events(Rest, N1, Pairs).

build_roles([], _, Acc, Roles) :- reverse(Acc, Roles).
build_roles([event(Pred, E)-r(main,Pred,[Agent,Obj])|Rest], AttrRels, Acc, Roles) :-
    build_roles(Rest, AttrRels,
                [event(Pred, E), agent(E, Agent), object(E, Obj)|Acc], Roles).
