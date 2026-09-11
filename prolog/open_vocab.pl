% open_vocab.pl
% Open-vocabulary symbolizer: unknown content words become UNKNOWN symbols.
% NEVER invents types: usage sentences yield relational triples only;
% is_a facts come ONLY from explicit "X is a <type>" sentences.
% Unknown verbs / ambiguous slots -> REJECT (no triple).
%
% REGLA DE HIERRO: nunca definir clausulas de predicados de otro fichero
% (SWI-Prolog borra el original). Solo delegacion local + predicados nuevos.
%
% Pronombres: se mapean UNA vez por frase (sin reintentos que muevan el
% contador) y los mentions solo se afirman si el parse tiene exito.
:- consult('english_graph.pl').
:- use_module(library(lists)).

typename(person). typename(city). typename(country). typename(food).

% --- extension EXP16: foods + eats/lives_in ---
food(bread). food(cheese). food(rice). food(apple).
food(fish). food(meat). food(soup). food(salad).

eats_verb(eat). eats_verb(eats). eats_verb(ate). eats_verb(eating).

% auxiliares de span excluidos de slots (is/was/lies ya lo estan en origen)
aux_skip(live). aux_skip(lives). aux_skip(lived).

% deteccion verbal local: extiende sin tocar english_graph
my_detect_verb(Tokens, eats) :-
    member(T, Tokens), eats_verb(T), !.
my_detect_verb(Tokens, lives_in) :-
    append(_, [A, in|_], Tokens),
    member(A, [live, lives, lived]), !.
my_detect_verb(Tokens, V) :-
    detect_verb(Tokens, V).

% content word: not skip, not aux, not verb form, not typename
content_word(T) :-
    \+ skip_word(T),
    \+ aux_skip(T),
    \+ verb_form_word(T),
    \+ typename(T).

verb_form_word(T) :-
    visits_verb(T), !.
verb_form_word(T) :-
    reaches_verb(T), !.
verb_form_word(T) :-
    eats_verb(T), !.
verb_form_word(in).

% known entity of any type
known_entity(T) :- person(T), !.
known_entity(T) :- city(T), !.
known_entity(T) :- country(T), !.
known_entity(T) :- food(T), !.

unknown_in_triple((S, _, O), U) :-
    findall(X, ( member(X, [S, O]),
                 content_word(X),
                 \+ known_entity(X)
               ),
            U).

% --- extension EXP18: pronombres (cada mencion -> nodo fresco) ---
% Recurso lexico declarado: genero de nombres propios conocidos.
pronoun(he, masc, sing). pronoun(him, masc, sing).
pronoun(she, fem, sing). pronoun(her, fem, sing).
gender(juan, masc). gender(maria, fem). gender(pedro, masc).

:- dynamic pronoun_mention/3.
:- dynamic pronoun_counter/1.

reset_pronouns :-
    retractall(pronoun_mention(_, _, _)),
    retractall(pronoun_counter(_)).

next_mention_num(K1) :-
    ( retract(pronoun_counter(K)) -> K1 is K + 1
    ; K1 = 1
    ),
    assertz(pronoun_counter(K1)).

map_pronouns_pure([], [], []).
map_pronouns_pure([T|Ts], [M|Ms], [(M, G, N)|Maps]) :-
    pronoun(T, G, N), !,
    next_mention_num(K1),
    atomic_list_concat([T, '_', K1], M),
    map_pronouns_pure(Ts, Ms, Maps).
map_pronouns_pure([T|Ts], [T|Ms], Maps) :-
    map_pronouns_pure(Ts, Ms, Maps).

assert_mentions(Maps) :-
    forall(member((M, G, N), Maps),
           ( pronoun_mention(M, _, _) -> true
           ; assertz(pronoun_mention(M, G, N))
           )).

% ===== entrada unica: tokeniza, mapea (1 vez), parsea =====
symbolize_open(Sentence, T, Kind) :-
    tokenize_en(Sentence, T0),
    map_pronouns_pure(T0, Tokens, Maps),
    parse_mapped(Tokens, Maps, T, Kind).

% is_a pattern: "X is a <type>"
parse_mapped([X, is, a, T], Maps, (X, is_a, T), typed) :-
    typename(T),
    content_word(X), !,
    assert_mentions(Maps).

% identity declaration: "X is Y" (Y not a typename; stored separately).
parse_mapped([X, is, Y], _, (X, same_as, Y), identity) :-
    content_word(X),
    content_word(Y),
    \+ typename(Y), !.

% relational: known verb + POSITIONAL slots (SVO order disambiguates
% unknowns: subject precedes the verb, object follows it).
% Known-type mismatches still rejected (a known city cannot be a subject).
parse_mapped(Tokens, Maps, (S, visits, O), usage) :-
    my_detect_verb(Tokens, visits),
    verb_pos(Tokens, visits, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_visits, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, in, O), usage) :-
    my_detect_verb(Tokens, in),
    verb_pos(Tokens, in, I),
    pre_fillers(Tokens, I, subj_in, [S]),
    post_fillers(Tokens, I, obj_in, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, reaches, O), usage) :-
    my_detect_verb(Tokens, reaches),
    verb_pos(Tokens, reaches, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_in, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, eats, O), usage) :-
    my_detect_verb(Tokens, eats),
    verb_pos(Tokens, eats, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_eats, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, lives_in, O), usage) :-
    my_detect_verb(Tokens, lives_in),
    verb_pos(Tokens, lives_in, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_visits, [O]),
    assert_mentions(Maps).

verb_pos(Tokens, visits, I) :-
    nth0(I, Tokens, W), visits_verb(W), !.
verb_pos(Tokens, reaches, I) :-
    nth0(I, Tokens, W), reaches_verb(W), !.
verb_pos(Tokens, eats, I) :-
    nth0(I, Tokens, W), eats_verb(W), !.
verb_pos(Tokens, lives_in, I) :-
    nth0(I, Tokens, in),
    nth0(J, Tokens, A), J < I, member(A, [live, lives, lived]), !.
verb_pos(Tokens, in, I) :-
    nth0(I, Tokens, in),
    nth0(J, Tokens, A), J < I, member(A, [is, was, lies]), !.

pre_fillers(Tokens, I, Role, Fillers) :-
    findall(T, ( nth0(J, Tokens, T), J < I, role_filler(Role, T) ),
            Fillers).

post_fillers(Tokens, I, Role, Fillers) :-
    findall(T, ( nth0(J, Tokens, T), J > I, role_filler(Role, T) ),
            Fillers).

role_filler(subj_visits, T) :- person(T), !.
role_filler(subj_visits, T) :-
    content_word(T), \+ city(T), \+ country(T), \+ typename(T).
role_filler(subj_in, T) :- city(T), !.
role_filler(subj_in, T) :-
    content_word(T), \+ person(T), \+ country(T), \+ typename(T).
role_filler(obj_visits, T) :- city(T), !.
role_filler(obj_visits, T) :-
    content_word(T), \+ person(T), \+ country(T), \+ typename(T).
role_filler(obj_in, T) :- country(T), !.
role_filler(obj_in, T) :-
    content_word(T), \+ person(T), \+ city(T), \+ typename(T).
role_filler(obj_eats, T) :- food(T), !.
role_filler(obj_eats, T) :-
    content_word(T), \+ person(T), \+ city(T),
    \+ country(T), \+ typename(T).

% all unknown words of accepted triples (recall audit)
unknown_words(Triples, Unknowns) :-
    findall(U, ( member(T, Triples),
                 unknown_in_triple(T, Us),
                 member(U, Us)
               ),
            U0),
    sort(U0, Unknowns).
