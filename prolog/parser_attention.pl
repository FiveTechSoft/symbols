:- module(parser_attention,
    [ parse_sentence/2,
      parse_and_attend/3,
      attention/3,
      top_k/3
    ]).

:- use_module(library(lists)).

:- dynamic known_symbol/1.
:- dynamic known_relation/3.

known_symbol(juan).
known_symbol(maria).
known_symbol(pedro).
known_symbol(coche).
known_symbol(casa).
known_symbol(madrid).
known_symbol(barcelona).

known_relation(juan, vive_en, madrid).
known_relation(maria, vive_en, barcelona).


parse_sentence(Text, Relations) :-
    tokenize(Text, Tokens),
    normalize_tokens(Tokens, Normalized),
    sentence_relations(Normalized, Relations).


parse_and_attend(Text, Selected, All) :-
    parse_sentence(Text, Relations),
    attention(Relations, Text, Scored),
    top_k(Scored, 5, Selected),
    All = Scored.


tokenize(Text, Tokens) :-
    string_lower(Text, Lower),
    split_string(
        Lower,
        " ,.!?;:\t\n",
        " ,.!?;:\t\n",
        Strings
    ),
    maplist(atom_string, Tokens, Strings).


normalize_tokens([], []).

normalize_tokens([H|T], [N|R]) :-
    normalize_atom(H, N),
    normalize_tokens(T, R).


normalize_atom(Atom, Normalized) :-
    atom_string(Atom, Str),
    normalize_space(string(Norm), Str),
    atom_string(Normalized, Norm).


/*
   ============================================================
   PARSER
   ============================================================
*/


sentence_relations(Tokens, Relations) :-
    findall(
        Relation,
        relation_from_sentence(Tokens, Relation),
        Raw
    ),
    sort(Raw, Relations).


/*
   Juan compro un coche rojo en madrid ayer
*/
relation_from_sentence(
    [S,V,Art,O,Adj,en,Place,Time],
    relation(comprar,S,O,source(main))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    adjective_color(Adj),
    location(Place),
    temporal(Time).


relation_from_sentence(
    [S,V,Art,O,Adj,en,Place,Time],
    relation(color,O,Adj,source(attribute))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    adjective_color(Adj),
    location(Place),
    temporal(Time).


relation_from_sentence(
    [S,V,Art,O,Adj,en,Place,Time],
    relation(ubicado_en,O,Place,source(location))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    adjective_color(Adj),
    location(Place),
    temporal(Time).


relation_from_sentence(
    [S,V,Art,O,Adj,en,Place,Time],
    relation(tiempo,comprar(S,O),Time,source(temporal))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    adjective_color(Adj),
    location(Place),
    temporal(Time).


/*
   Version sin color.
*/
relation_from_sentence(
    [S,V,Art,O,en,Place,Time],
    relation(comprar,S,O,source(main))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    location(Place),
    temporal(Time).


relation_from_sentence(
    [S,V,Art,O,en,Place,Time],
    relation(ubicado_en,O,Place,source(location))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    location(Place),
    temporal(Time).


relation_from_sentence(
    [S,V,Art,O,en,Place,Time],
    relation(tiempo,comprar(S,O),Time,source(temporal))) :-
    subject(S),
    verb_comprar(V),
    article(Art),
    object(O),
    location(Place),
    temporal(Time).


subject(juan).
subject(maria).
subject(pedro).
subject(ana).

object(coche).
object(casa).
object(libro).
object(comida).
object(empresa).

article(el).
article(la).
article(un).
article(una).
article(unos).
article(unas).

verb_comprar(compro).
verb_comprar(compra).
verb_comprar(compran).

adjective_color(rojo).
adjective_color(roja).
adjective_color(azul).
adjective_color(verde).
adjective_color(blanco).
adjective_color(blanca).
adjective_color(negro).
adjective_color(negra).

location(madrid).
location(barcelona).
location(malaga).
location(marbella).

temporal(ayer).
temporal(hoy).
temporal(manana).
temporal(antes).
temporal(despues).


/*
   ============================================================
   SYMBOLIC ATTENTION
   ============================================================
*/


attention(Relations, _Text, Scored) :-
    maplist(score_relation, Relations, Raw),
    predsort(compare_score, Raw, Scored).


score_relation(
    Relation,
    scored(Relation, Score, Heads)
) :-
    relation_head(Relation, R),
    entity_head(Relation, E),
    position_head(Relation, P),
    discourse_head(Relation, D),
    temporal_head(Relation, T),
    novelty_head(Relation, N),

    Score is
          0.30 * R
        + 0.20 * E
        + 0.10 * P
        + 0.10 * D
        + 0.15 * T
        + 0.15 * N,

    Heads = heads(
        relation-R,
        entity-E,
        position-P,
        discourse-D,
        temporal-T,
        novelty-N
    ).


/*
   HEAD 1: RELATION
*/
relation_head(
    relation(comprar, _, _, source(main)),
    1.0
) :- !.

relation_head(
    relation(_, _, _, _),
    0.7
).


/*
   HEAD 2: ENTITY
*/
entity_head(
    relation(_, A, B, _),
    Score
) :-
    entity_known(A, SA),
    entity_known(B, SB),
    Score is (SA + SB) / 2.


entity_known(X, 1.0) :-
    known_symbol(X),
    !.

entity_known(X, 0.5) :-
    atom(X),
    !.

entity_known(_, 0.0).


/*
   HEAD 3: POSITION
*/
position_head(
    relation(_, _, _, source(main)),
    1.0
) :- !.

position_head(
    relation(_, _, _, source(attribute)),
    0.8
) :- !.

position_head(
    relation(_, _, _, source(location)),
    0.7
) :- !.

position_head(
    relation(_, _, _, source(temporal)),
    0.6
).


/*
   HEAD 4: DISCOURSE
*/
discourse_head(
    relation(comprar, _, _, source(main)),
    1.0
) :- !.

discourse_head(
    relation(color, _, _, source(attribute)),
    0.8
) :- !.

discourse_head(
    relation(ubicado_en, _, _, source(location)),
    0.8
) :- !.

discourse_head(
    relation(tiempo, _, _, source(temporal)),
    0.9
).


/*
   HEAD 5: TEMPORAL
*/
temporal_head(
    relation(tiempo, _, _, source(temporal)),
    1.0
) :- !.

temporal_head(
    relation(_, _, _, _),
    0.3
).


/*
   HEAD 6: NOVELTY
*/
novelty_head(
    relation(R,S,O,_),
    0.2
) :-
    known_relation(S,R,O),
    !.

novelty_head(
    relation(_, _, _, _),
    1.0
).


/*
   ============================================================
   SORT / TOP-K
   ============================================================
*/

compare_score(Order, A, B) :-
    A = scored(_, ScoreA, _),
    B = scored(_, ScoreB, _),
    compare(Order, ScoreB, ScoreA).


top_k(Items, K, Result) :-
    take_k(Items, K, Result).


take_k(_, 0, []) :-
    !.

take_k([], _, []).

take_k([H|T], K, [H|R]) :-
    K > 0,
    K1 is K - 1,
    take_k(T, K1, R).
