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
aux_skip(through).

% --- extension EXP22: travels/arrives (tokens nuevos, aditiva) ---
travels_verb(travel). travels_verb(travels).
travels_verb(travelled). travels_verb(traveled). travels_verb(travelling).
arrives_verb(arrive). arrives_verb(arrives).
arrives_verb(arrived). arrives_verb(arriving).

% --- extension EXP28: reads/borrows/owns/likes (tokens nuevos, aditiva) ---
reads_verb(read). reads_verb(reads).
borrows_verb(borrow). borrows_verb(borrows). borrows_verb(borrowed).
owns_verb(own). owns_verb(owns). owns_verb(owned).
likes_verb(like). likes_verb(likes). likes_verb(liked).

% --- extension EXP29/Corpus 3: 15 verbos nuevos (tokens nuevos, aditiva) ---
provides_verb(provide). provides_verb(provides). provides_verb(provided).
works_verb(work). works_verb(works). works_verb(worked).
supplies_verb(supply). supplies_verb(supplies). supplies_verb(supplied).
buys_verb(buy). buys_verb(buys). buys_verb(bought).
imports_verb(import). imports_verb(imports). imports_verb(imported).
cooks_verb(cook). cooks_verb(cooks). cooks_verb(cooked).
needs_verb(need). needs_verb(needs). needs_verb(needed).
uses_verb(use). uses_verb(uses). uses_verb(used).
praises_verb(praise). praises_verb(praises). praises_verb(praised).
honors_verb(honor). honors_verb(honors). honors_verb(honored).
exalts_verb(exalt). exalts_verb(exalts). exalts_verb(exalted).
admires_verb(admire). admires_verb(admires). admires_verb(admired).
plants_verb(plant). plants_verb(plants). plants_verb(planted).
yields_verb(yield). yields_verb(yields). yields_verb(yielded).
harvests_verb(harvest). harvests_verb(harvests). harvests_verb(harvested).

% deteccion verbal local: extiende sin tocar english_graph
my_detect_verb(Tokens, travels) :-
    member(T, Tokens), travels_verb(T), !.
my_detect_verb(Tokens, arrives) :-
    member(T, Tokens), arrives_verb(T), !.
my_detect_verb(Tokens, reads) :-
    member(T, Tokens), reads_verb(T), !.
my_detect_verb(Tokens, borrows) :-
    member(T, Tokens), borrows_verb(T), !.
my_detect_verb(Tokens, owns) :-
    member(T, Tokens), owns_verb(T), !.
my_detect_verb(Tokens, likes) :-
    member(T, Tokens), likes_verb(T), !.
my_detect_verb(Tokens, provides) :-
    member(T, Tokens), provides_verb(T), !.
my_detect_verb(Tokens, works) :-
    member(T, Tokens), works_verb(T), !.
my_detect_verb(Tokens, supplies) :-
    member(T, Tokens), supplies_verb(T), !.
my_detect_verb(Tokens, buys) :-
    member(T, Tokens), buys_verb(T), !.
my_detect_verb(Tokens, imports) :-
    member(T, Tokens), imports_verb(T), !.
my_detect_verb(Tokens, cooks) :-
    member(T, Tokens), cooks_verb(T), !.
my_detect_verb(Tokens, needs) :-
    member(T, Tokens), needs_verb(T), !.
my_detect_verb(Tokens, uses) :-
    member(T, Tokens), uses_verb(T), !.
my_detect_verb(Tokens, praises) :-
    member(T, Tokens), praises_verb(T), !.
my_detect_verb(Tokens, honors) :-
    member(T, Tokens), honors_verb(T), !.
my_detect_verb(Tokens, exalts) :-
    member(T, Tokens), exalts_verb(T), !.
my_detect_verb(Tokens, admires) :-
    member(T, Tokens), admires_verb(T), !.
my_detect_verb(Tokens, plants) :-
    member(T, Tokens), plants_verb(T), !.
my_detect_verb(Tokens, yields) :-
    member(T, Tokens), yields_verb(T), !.
my_detect_verb(Tokens, harvests) :-
    member(T, Tokens), harvests_verb(T), !.
my_detect_verb(Tokens, eats) :-
    member(T, Tokens), eats_verb(T), !.
my_detect_verb(Tokens, lives_in) :-
    append(_, [A, in|_], Tokens),
    member(A, [live, lives, lived]), !.
my_detect_verb(Tokens, V) :-
    detect_verb(Tokens, V), !.
% P2: fallback morfológico — detecta CUALQUIER verbo por sufijos.
% No necesita listas: -ed → stem, -ing → stem, -s → stem.
% Si el stem tiene ≥3 chars, lo acepamos como relación novel.
my_detect_verb(Tokens, Stem) :-
    member(T, Tokens),
    verb_stem(T, Stem),
    atom_length(Stem, SL), SL >= 3,
    \+ known_verb_form(T),
    !.

% ── Detección morfológica de verbos (P2) ─────────────────────────────

% verb_stem(+Word, -Stem) — extrae el stem verbal por sufijos
verb_stem(Word, Stem) :-
    atom_chars(Word, Chars),
    ( append(Base, [e,d], Chars) ->            % walked → walk
        atom_chars(Stem, Base)
    ; append(Base, [i,n,g], Chars) ->           % walking → walk
        atom_chars(Stem, Base)
    ; append(Base, [s], Chars),                 % walks → walk
      Base \== [],
      atom_chars(Stem, Base),
      \+ member(Stem, [this, thus, yes])        % evitar falsos positivos
    ).

% known_verb_form/1 — formas verbales conocidas (para no duplicar)
known_verb_form(T) :- visits_verb(T).
known_verb_form(T) :- reaches_verb(T).
known_verb_form(T) :- eats_verb(T).
known_verb_form(T) :- travels_verb(T).
known_verb_form(T) :- arrives_verb(T).
known_verb_form(T) :- reads_verb(T).
known_verb_form(T) :- borrows_verb(T).
known_verb_form(T) :- owns_verb(T).
known_verb_form(T) :- likes_verb(T).
known_verb_form(T) :- provides_verb(T).
known_verb_form(T) :- works_verb(T).
known_verb_form(T) :- supplies_verb(T).
known_verb_form(T) :- buys_verb(T).
known_verb_form(T) :- imports_verb(T).
known_verb_form(T) :- cooks_verb(T).
known_verb_form(T) :- needs_verb(T).
known_verb_form(T) :- uses_verb(T).
known_verb_form(T) :- praises_verb(T).
known_verb_form(T) :- honors_verb(T).
known_verb_form(T) :- exalts_verb(T).
known_verb_form(T) :- admires_verb(T).
known_verb_form(T) :- plants_verb(T).
known_verb_form(T) :- yields_verb(T).
known_verb_form(T) :- harvests_verb(T).

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
verb_form_word(T) :-
    travels_verb(T), !.
verb_form_word(T) :-
    arrives_verb(T), !.
verb_form_word(T) :-
    reads_verb(T), !.
verb_form_word(T) :-
    borrows_verb(T), !.
verb_form_word(T) :-
    owns_verb(T), !.
verb_form_word(T) :-
    likes_verb(T), !.
verb_form_word(T) :-
    member(V, [provides_verb, works_verb, supplies_verb, buys_verb,
               imports_verb, cooks_verb, needs_verb, uses_verb,
               praises_verb, honors_verb, exalts_verb, admires_verb,
               plants_verb, yields_verb, harvests_verb]),
    call(V, T), !.
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

% symbolize_timed: como symbolize_open pero extrae el primer año (Time)
% y lo quita de los tokens (los adjuntos temporales no son argumentos).
% Time = year | none.
symbolize_timed(Sentence, T, Kind, Time) :-
    tokenize_en(Sentence, T0),
    map_pronouns_pure(T0, T1, Maps),
    include(year_token, T1, Years),
    exclude(year_token, T1, Tokens),
    ( Years = [Y|_] -> atom_number(Y, Time) ; Time = none ),
    parse_mapped(Tokens, Maps, T, Kind).

year_token(T) :-
    atom(T),
    atom_length(T, 4),
    atom_number(T, N),
    N >= 1000, N =< 2100.

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
parse_mapped(Tokens, Maps, (S, travels, O), usage) :-
    my_detect_verb(Tokens, travels),
    verb_pos(Tokens, travels, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_visits, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, arrives, O), usage) :-
    my_detect_verb(Tokens, arrives),
    verb_pos(Tokens, arrives, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_in, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, reads, O), usage) :-
    my_detect_verb(Tokens, reads),
    verb_pos(Tokens, reads, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, borrows, O), usage) :-
    my_detect_verb(Tokens, borrows),
    verb_pos(Tokens, borrows, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, owns, O), usage) :-
    my_detect_verb(Tokens, owns),
    verb_pos(Tokens, owns, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, likes, O), usage) :-
    my_detect_verb(Tokens, likes),
    verb_pos(Tokens, likes, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, provides, O), usage) :-
    my_detect_verb(Tokens, provides),
    verb_pos(Tokens, provides, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, works, O), usage) :-
    my_detect_verb(Tokens, works),
    verb_pos(Tokens, works, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, supplies, O), usage) :-
    my_detect_verb(Tokens, supplies),
    verb_pos(Tokens, supplies, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, buys, O), usage) :-
    my_detect_verb(Tokens, buys),
    verb_pos(Tokens, buys, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, imports, O), usage) :-
    my_detect_verb(Tokens, imports),
    verb_pos(Tokens, imports, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, cooks, O), usage) :-
    my_detect_verb(Tokens, cooks),
    verb_pos(Tokens, cooks, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, needs, O), usage) :-
    my_detect_verb(Tokens, needs),
    verb_pos(Tokens, needs, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, uses, O), usage) :-
    my_detect_verb(Tokens, uses),
    verb_pos(Tokens, uses, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, praises, O), usage) :-
    my_detect_verb(Tokens, praises),
    verb_pos(Tokens, praises, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, honors, O), usage) :-
    my_detect_verb(Tokens, honors),
    verb_pos(Tokens, honors, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, exalts, O), usage) :-
    my_detect_verb(Tokens, exalts),
    verb_pos(Tokens, exalts, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, admires, O), usage) :-
    my_detect_verb(Tokens, admires),
    verb_pos(Tokens, admires, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, plants, O), usage) :-
    my_detect_verb(Tokens, plants),
    verb_pos(Tokens, plants, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, yields, O), usage) :-
    my_detect_verb(Tokens, yields),
    verb_pos(Tokens, yields, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).
parse_mapped(Tokens, Maps, (S, harvests, O), usage) :-
    my_detect_verb(Tokens, harvests),
    verb_pos(Tokens, harvests, I),
    pre_fillers(Tokens, I, subj_visits, [S]),
    post_fillers(Tokens, I, obj_any, [O]),
    assert_mentions(Maps).

verb_pos(Tokens, visits, I) :-
    nth0(I, Tokens, W), visits_verb(W), !.
verb_pos(Tokens, reaches, I) :-
    nth0(I, Tokens, W), reaches_verb(W), !.
verb_pos(Tokens, eats, I) :-
    nth0(I, Tokens, W), eats_verb(W), !.
verb_pos(Tokens, travels, I) :-
    nth0(I, Tokens, W), travels_verb(W), !.
verb_pos(Tokens, arrives, I) :-
    nth0(I, Tokens, W), arrives_verb(W), !.
verb_pos(Tokens, reads, I) :-
    nth0(I, Tokens, W), reads_verb(W), !.
verb_pos(Tokens, borrows, I) :-
    nth0(I, Tokens, W), borrows_verb(W), !.
verb_pos(Tokens, owns, I) :-
    nth0(I, Tokens, W), owns_verb(W), !.
verb_pos(Tokens, likes, I) :-
    nth0(I, Tokens, W), likes_verb(W), !.
verb_pos(Tokens, provides, I) :-
    nth0(I, Tokens, W), provides_verb(W), !.
verb_pos(Tokens, works, I) :-
    nth0(I, Tokens, W), works_verb(W), !.
verb_pos(Tokens, supplies, I) :-
    nth0(I, Tokens, W), supplies_verb(W), !.
verb_pos(Tokens, buys, I) :-
    nth0(I, Tokens, W), buys_verb(W), !.
verb_pos(Tokens, imports, I) :-
    nth0(I, Tokens, W), imports_verb(W), !.
verb_pos(Tokens, cooks, I) :-
    nth0(I, Tokens, W), cooks_verb(W), !.
verb_pos(Tokens, needs, I) :-
    nth0(I, Tokens, W), needs_verb(W), !.
verb_pos(Tokens, uses, I) :-
    nth0(I, Tokens, W), uses_verb(W), !.
verb_pos(Tokens, praises, I) :-
    nth0(I, Tokens, W), praises_verb(W), !.
verb_pos(Tokens, honors, I) :-
    nth0(I, Tokens, W), honors_verb(W), !.
verb_pos(Tokens, exalts, I) :-
    nth0(I, Tokens, W), exalts_verb(W), !.
verb_pos(Tokens, admires, I) :-
    nth0(I, Tokens, W), admires_verb(W), !.
verb_pos(Tokens, plants, I) :-
    nth0(I, Tokens, W), plants_verb(W), !.
verb_pos(Tokens, yields, I) :-
    nth0(I, Tokens, W), yields_verb(W), !.
verb_pos(Tokens, harvests, I) :-
    nth0(I, Tokens, W), harvests_verb(W), !.
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
role_filler(obj_any, T) :- city(T), !.
role_filler(obj_any, T) :- country(T), !.
role_filler(obj_any, T) :- food(T), !.
role_filler(obj_any, T) :-
    content_word(T), \+ person(T), \+ typename(T).

% all unknown words of accepted triples (recall audit)
unknown_words(Triples, Unknowns) :-
    findall(U, ( member(T, Triples),
                 unknown_in_triple(T, Us),
                 member(U, Us)
               ),
            U0),
    sort(U0, Unknowns).
