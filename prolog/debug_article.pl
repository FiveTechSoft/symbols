:- use_module(semantic_field).
:- use_module(parser_v2).
:- use_module(library(lists)).

:- initialization((
    % Test: "John bought a red car in Madrid"
    Tokens = [john,bought,a,red,car,in,madrid],
    MainTargets = [car, red],
    write('Tokens: '), write(Tokens), nl,
    write('MainTargets: '), write(MainTargets), nl,
    nl,
    % Manual article detection
    ( member(Article, [a, an, the]), member(Article, Tokens) ->
        nth0(ArtIdx, Tokens, Article),
        format('Found article "~w" at index ~w~n', [Article, ArtIdx]),
        findall(C-Pos, (
            nth0(Pos, Tokens, C),
            Pos > ArtIdx,
            member(C, MainTargets)
        ), AfterArticle),
        format('After article: ~w~n', [AfterArticle]),
        keysort(AfterArticle, Sorted),
        format('Sorted: ~w~n', [Sorted]),
        last(Sorted, Noun-_),
        format('Noun (last): ~w~n', [Noun])
    ;
        write('No English article found'), nl
    ),
    halt
)).
