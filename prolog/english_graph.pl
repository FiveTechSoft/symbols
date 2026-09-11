% english_graph.pl
% SYMBOLIZER (English): sentence -> (S, V, O). Same discipline as the
% Spanish one: closed typed lexicon, strict slot counts per relation.
% visits: 1 person + 1 city. in: 1 city + 1 country. reaches: 1 person + 1 country.
:- use_module(library(lists)).

person(lina). person(mario). person(sofia). person(anna).
person(paul). person(elena). person(ruth). person(ivan).

city(roma). city(paris). city(madrid). city(oslo).
city(quito). city(lima). city(dublin). city(bern).

country(italy). country(france). country(spain). country(norway).
country(ecuador). country(peru). country(ireland). country(switzerland).

% verb spans -> canonical relation
visits_verb(visits). visits_verb(visit). visits_verb(visited).
visits_verb(visiting).
reaches_verb(reaches). reaches_verb(reach). reaches_verb(reached).
reaches_verb(reaching).

skip_word(the). skip_word(a). skip_word(an). skip_word(by).
skip_word(has). skip_word(have). skip_word(had). skip_word(already).
skip_word(just). skip_word(yesterday). skip_word(today).
skip_word(quickly). skip_word(eagerly). skip_word(is). skip_word(was).
skip_word(lies). skip_word(in).

tokenize_en(Sentence, Tokens) :-
    string_lower(Sentence, Lower),
    string_chars(Lower, Chars),
    exclude(punct, Chars, Clean),
    string_chars(CleanStr, Clean),
    split_string(CleanStr, " ", " ", Parts),
    exclude(empty_string, Parts, NonEmpty),
    maplist(atom_string, Tokens, NonEmpty).

punct('.'). punct(','). punct(';'). punct(':').
punct('?'). punct('!').
empty_string("").

% verb detection on raw tokens (before skip-filtering)
detect_verb(Tokens, visits) :-
    member(T, Tokens), visits_verb(T), !.
detect_verb(Tokens, reaches) :-
    member(T, Tokens), reaches_verb(T), !.
detect_verb(Tokens, in) :-
    append(_, [A, in|_], Tokens),
    member(A, [is, was, lies]), !.

symbolize_en(Sentence, (S, visits, O)) :-
    tokenize_en(Sentence, Tokens),
    detect_verb(Tokens, visits),
    include(person, Tokens, [S]),
    include(city, Tokens, [O]),
    include(country, Tokens, []).
symbolize_en(Sentence, (S, in, O)) :-
    tokenize_en(Sentence, Tokens),
    detect_verb(Tokens, in),
    include(person, Tokens, []),
    include(city, Tokens, [S]),
    include(country, Tokens, [O]).
symbolize_en(Sentence, (S, reaches, O)) :-
    tokenize_en(Sentence, Tokens),
    detect_verb(Tokens, reaches),
    include(person, Tokens, [S]),
    include(city, Tokens, []),
    include(country, Tokens, [O]).

% (person/city/country se usan directamente como hechos)
