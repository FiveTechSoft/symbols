:- module(parser_v2,
    [ parse_sentence/2,
      parse_and_attend/4,
      attention/4,
      top_k/3,
      normalize_text/2,
      sentence_relations/2,
      pattern/2,
      query_type/2,
      query_weight/3,
      head_weight/2,
      score_relation/3,
      compatibility/3,
      query_bonus/3
    ]).

:- use_module(library(lists)).

:- dynamic known_symbol/1.
:- dynamic known_relation/3.

known_symbol(juan).   known_symbol(maria).
known_symbol(pedro).  known_symbol(ana).
known_symbol(coche).  known_symbol(casa).
known_symbol(libro).  known_symbol(empresa).
known_symbol(manzana). known_symbol(bicicleta).
known_symbol(madrid).  known_symbol(barcelona).
known_symbol(malaga).  known_symbol(marbella).

known_relation(juan, vive_en, madrid).
known_relation(maria, vive_en, barcelona).


% ════════════════════════════════════════════════════════════════════
%  TOKENIZER
% ════════════════════════════════════════════════════════════════════

parse_sentence(Text, Relations) :-
    normalize_text(Text, Normalized),
    sentence_relations(Normalized, Relations).

normalize_text(Text, Normalized) :-
    string_lower(Text, Lower),
    split_string(Lower, " ,.!?;:\t\n", " ,.!?;:\t\n", Strings),
    maplist(atom_string, Tokens, Strings),
    normalize_accents(Tokens, Normalized).

normalize_accents([], []).
normalize_accents([H|T], [N|R]) :-
    normalize_atom(H, N),
    normalize_accents(T, R).

normalize_atom(Atom, Normalized) :-
    atom_string(Atom, Str),
    normalize_space(string(Norm), Str),
    ( number_string(Num, Norm) -> Normalized = Num
    ; atom_string(Normalized, Norm)
    ).


% ════════════════════════════════════════════════════════════════════
%  LEXICONS
% ════════════════════════════════════════════════════════════════════

subject(juan).   subject(maria).   subject(pedro).  subject(ana).

object(coche).   object(casa).     object(libro).   object(empresa).
object(manzana). object(bicicleta).

article(el). article(la). article(un). article(una).
article(los). article(las). article(unos). article(unas).

verb_family(compro, comprar).   verb_family(compra, comprar).
verb_family(compran, comprar).
verb_family(vive, vivir).       verb_family(viven, vivir).
verb_family(vivia, vivir).
verb_family(tiene, tener).      verb_family(tienen, tener).
verb_family(tenia, tener).
verb_family(visitó, visitar).   verb_family(visitamos, visitar).
verb_family(visito, visitar).
verb_family(llevo, llevar).     verb_family(llevar, llevar).
verb_family(lleva, llevar).
verb_family(trabaja, trabajar).  verb_family(trabajan, trabajar).
verb_family(trabajo, trabajar).
verb_family(esta, estar).        verb_family(estado, estar).

adjective_color(rojo).   adjective_color(roja).
adjective_color(azul).   adjective_color(verde).
adjective_color(blanco). adjective_color(blanca).
adjective_color(negro).  adjective_color(negra).
adjective_size(grande).  adjective_size(pequeno).
adjective_size(largo).   adjective_size(larga).

location(madrid).  location(barcelona). location(malaga).  location(marbella).

temporal(ayer).  temporal(hoy).  temporal(manana).
temporal(antes). temporal(despues). temporal(2025). temporal(2020).

negation(no).

conjunction(y). conjunction(pero).

pronoun(lo). pronoun(la). pronoun(le). pronoun(se).

preposition(para). preposition(desde). preposition(en). preposition(a).


% ════════════════════════════════════════════════════════════════════
%  PARSER — pattern matching → relation(Type, Predicate, Args)
% ════════════════════════════════════════════════════════════════════

sentence_relations(Tokens, Relations) :-
    all_clause_relations(Tokens, FullRels),
    FullRels \== [], !,
    sort(FullRels, Relations).
sentence_relations(Tokens, Relations) :-
    split_conjunctions(Tokens, Clauses),
    findall(R, (
        member(Clause, Clauses),
        all_clause_relations(Clause, Rels),
        member(R, Rels)
    ), AllRels),
    sort(AllRels, Relations), !.

sentence_relations(_, []).


split_conjunctions(Tokens, [Before, After]) :-
    append(Before, [Conj|After0], Tokens),
    conjunction(Conj),
    Before \== [],
    After0 \== [],
    After = After0, !.
split_conjunctions(Tokens, [Tokens]).


all_clause_relations(Tokens, Relations) :-
    findall(R, pattern(Tokens, R), Relations), !.

pattern(Tokens, Relation) :- p1(Tokens, Relation).
pattern(Tokens, Relation) :- p2(Tokens, Relation).
pattern(Tokens, Relation) :- p3(Tokens, Relation).
pattern(Tokens, Relation) :- p4(Tokens, Relation).
pattern(Tokens, Relation) :- p5(Tokens, Relation).
pattern(Tokens, Relation) :- p6(Tokens, Relation).
pattern(Tokens, Relation) :- p7(Tokens, Relation).
pattern(Tokens, Relation) :- p8(Tokens, Relation).
pattern(Tokens, Relation) :- p9(Tokens, Relation).
pattern(Tokens, Relation) :- p10(Tokens, Relation).
pattern(Tokens, Relation) :- p11(Tokens, Relation).
pattern(Tokens, Relation) :- p12(Tokens, Relation).
pattern(Tokens, Relation) :- p13(Tokens, Relation).
pattern(Tokens, Relation) :- p14(Tokens, Relation).


% ── P1: S V Art O Adj en Place Time ────────────────────────────────
p1([S,V,Art,O,Adj,en,Place,Time], relation(main, comprar, [S,O])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1([S,V,Art,O,Adj,en,Place,Time], relation(attribute, color, [O,Adj])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1([S,V,Art,O,Adj,en,Place,Time], relation(location, ubicado_en, [O,Place])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1([S,V,Art,O,Adj,en,Place,Time], relation(temporal, tiempo, [comprar(S,O),Time])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).


% ── P2: S V en Place ───────────────────────────────────────────────
p2([S,V,en,Place], relation(main, vivir, [S,Place])) :-
    subject(S), verb_family(V,vivir), location(Place).


% ── P3: S V Art O Adj en Place ─────────────────────────────────────
p3([S,V,Art,O,Adj,en,Place], relation(main, tener, [S,O])) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).
p3([S,V,Art,O,Adj,en,Place], relation(attribute, tamano, [O,Adj])) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).
p3([S,V,Art,O,Adj,en,Place], relation(location, ubicado_en, [O,Place])) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).


% ── P4: S V Art O Adj para Person ──────────────────────────────────
p4([S,V,Art,O,Adj,para,Person], relation(main, comprar, [S,O])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).
p4([S,V,Art,O,Adj,para,Person], relation(attribute, color, [O,Adj])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).
p4([S,V,Art,O,Adj,para,Person], relation(indirect, para, [comprar(S,O),Person])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).


% ── P5: S no V en Place ────────────────────────────────────────────
p5([S,no,V,en,Place], relation(negation, negado, [vivir(S,Place)])) :-
    subject(S), negation(no), verb_family(V,vivir), location(Place).


% ── P6: S V O en Time ──────────────────────────────────────────────
p6([S,V,O,en,Time], relation(main, visitar, [S,O])) :-
    subject(S), verb_family(V,visitar), temporal(Time),
    ( object(O) ; location(O) ).
p6([S,V,O,en,Time], relation(temporal, tiempo, [visitar(S,O),Time])) :-
    subject(S), verb_family(V,visitar), temporal(Time),
    ( object(O) ; location(O) ).


% ── P7: S V Art O y lo V a Place ────────────────────────────────────
p7([S,V1,Art,O,Conj,Pron,V2,a,Place], relation(main, comprar, [S,O])) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).
p7([S,V1,Art,O,Conj,Pron,V2,a,Place], relation(main, llevar, [S,O])) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).
p7([S,V1,Art,O,Conj,Pron,V2,a,Place], relation(location, llevar_destino, [O,Place])) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).


% ── P8: S V Art O. Art O V en Place (coreference) ──────────────────
p8([S,V1,Art,O,Art2,O2,V2,en,Place], relation(main, tener, [S,O])) :-
    subject(S), verb_family(V1,tener), article(Art), object(O),
    article(Art2), object(O2), O == O2, verb_family(V2,estar), location(Place).
p8([S,V1,Art,O,Art2,O2,V2,en,Place], relation(location, ubicado_en, [O,Place])) :-
    subject(S), verb_family(V1,tener), article(Art), object(O),
    article(Art2), object(O2), O == O2, verb_family(V2,estar), location(Place).


% ── P9: S V en Place desde Time ────────────────────────────────────
p9([S,V,en,Place,desde,Time], relation(main, trabajar, [S,Place])) :-
    subject(S), verb_family(V,trabajar), location(Place), temporal(Time).
p9([S,V,en,Place,desde,Time], relation(temporal, desde, [trabajar(S,Place),Time])) :-
    subject(S), verb_family(V,trabajar), location(Place), temporal(Time).


% ── P10: S V Art O Adj (no location) ───────────────────────────────
p10([S,V,Art,O,Adj], relation(main, comprar, [S,O])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O), adjective_color(Adj).
p10([S,V,Art,O,Adj], relation(attribute, color, [O,Adj])) :-
    subject(S), verb_family(V,comprar), article(Art), object(O), adjective_color(Adj).


% ── P11: S V Art O (simple) ────────────────────────────────────────
p11([S,V,Art,O], relation(main, tener, [S,O])) :-
    subject(S), verb_family(V,tener), article(Art), object(O).


% ── P12: S V O (no article, simple) ────────────────────────────────
p12([S,V,O], relation(main, visitar, [S,O])) :-
    subject(S), verb_family(V,visitar), object(O).


% ── P13: S V O a Place ─────────────────────────────────────────────
p13([S,V,O,a,Place], relation(main, llevar, [S,O])) :-
    subject(S), verb_family(V,llevar), object(O), location(Place).
p13([S,V,O,a,Place], relation(location, llevar_destino, [O,Place])) :-
    subject(S), verb_family(V,llevar), object(O), location(Place).


% ── P14: S V en Place (estar, etc.) ────────────────────────────────
p14([S,V,en,Place], relation(location, ubicado_en, [S,Place])) :-
    object(S), verb_family(V,estar), location(Place).


% ════════════════════════════════════════════════════════════════════
%  QUERIES — tipos de pregunta
% ════════════════════════════════════════════════════════════════════

:- discontiguous query_weight/3.
:- discontiguous query_weight_extra/3.

query_type(color_query, Tokens) :- ( member(color, Tokens) ; member(rojo, Tokens) ; member(roja, Tokens) ; member(azul, Tokens) ; member(negro, Tokens) ; member(negra, Tokens) ), !.
query_type(size_query, Tokens) :- ( member(grande, Tokens) ; member(pequeno, Tokens) ; member(size, Tokens) ; member(tipo, Tokens) ), !.
query_type(what,    Tokens) :- member(que, Tokens), !.
query_type(where,   Tokens) :- ( member(donde, Tokens) ; member(ciudad, Tokens) ), !.
query_type(when,    Tokens) :- ( member(cuando, Tokens) ; member(desde, Tokens) ; member(ano, Tokens) ), !.
query_type(who,     Tokens) :- ( member(quien, Tokens) ; member(para, Tokens) ), !.
query_type(how,    Tokens) :- member(como, Tokens), !.
query_type(negation, Tokens) :- member(no, Tokens), !.
query_type(unknown, _).


% ════════════════════════════════════════════════════════════════════
%  HEAD WEIGHTS
% ════════════════════════════════════════════════════════════════════

head_weight(relation,  0.30).
head_weight(entity,    0.20).
head_weight(position,  0.10).
head_weight(discourse, 0.10).
head_weight(temporal,  0.15).
head_weight(novelty,   0.15).


% ════════════════════════════════════════════════════════════════════
%  QUERY-DEPENDENT WEIGHTS
% ════════════════════════════════════════════════════════════════════

% query_weight(QueryType, Head, Weight) — override for specific queries
% Default: use head_weight/2

query_weight(what, relation,  0.35).
query_weight(what, entity,    0.25).
query_weight(what, position,  0.05).
query_weight(what, discourse, 0.15).
query_weight(what, temporal,  0.10).
query_weight(what, novelty,   0.10).

query_weight(where, relation,  0.20).
query_weight(where, entity,    0.15).
query_weight(where, position,  0.05).
query_weight(where, discourse, 0.10).
query_weight(where, temporal,  0.10).
query_weight(where, novelty,   0.10).
% boost location
query_weight_extra(where, location, 0.30).
query_weight_extra(where, main, 0.10).

query_weight(when, relation,  0.20).
query_weight(when, entity,    0.10).
query_weight(when, position,  0.05).
query_weight(when, discourse, 0.10).
query_weight(when, temporal,  0.35).
query_weight(when, novelty,   0.10).

query_weight(who, relation,  0.25).
query_weight(who, entity,    0.35).
query_weight(who, position,  0.05).
query_weight(who, discourse, 0.15).
query_weight(who, temporal,  0.10).
query_weight(who, novelty,   0.10).

query_weight(negation, relation,  0.25).
query_weight(negation, entity,    0.15).
query_weight(negation, position,  0.10).
query_weight(negation, discourse, 0.15).
query_weight(negation, temporal,  0.10).
query_weight(negation, novelty,   0.10).
% negation gets a flat boost
query_weight_extra(negation, negation, 0.30).

query_weight(color_query, relation,  0.20).
query_weight(color_query, entity,    0.15).
query_weight(color_query, position,  0.10).
query_weight(color_query, discourse, 0.15).
query_weight(color_query, temporal,  0.05).
query_weight(color_query, novelty,   0.10).

query_weight(size_query, relation,  0.20).
query_weight(size_query, entity,    0.15).
query_weight(size_query, position,  0.10).
query_weight(size_query, discourse, 0.15).
query_weight(size_query, temporal,  0.05).
query_weight(size_query, novelty,   0.10).


% ════════════════════════════════════════════════════════════════════
%  SYMBOLIC ATTENTION — query-dependent
% ════════════════════════════════════════════════════════════════════

parse_and_attend(Text, QueryText, Selected, All) :-
    parse_sentence(Text, Relations),
    normalize_text(QueryText, QueryTokens),
    query_type(QueryType, QueryTokens),
    attention(Relations, QueryType, Scored),
    top_k(Scored, 5, Selected),
    All = Scored.

attention(Relations, QueryType, Scored) :-
    maplist(score_relation(QueryType), Relations, Raw),
    predsort(compare_score, Raw, Scored).


score_relation(QueryType, Relation, scored(Relation, Score, Heads)) :-
    relation_head(Relation, R),
    entity_head(Relation, E),
    position_head(Relation, P),
    discourse_head(Relation, D),
    temporal_head(Relation, T),
    novelty_head(Relation, N),
    type_head(Relation, Typ),
    get_weight(QueryType, relation,  R, WR),
    get_weight(QueryType, entity,    E, WE),
    get_weight(QueryType, position,  P, WP),
    get_weight(QueryType, discourse, D, WD),
    get_weight(QueryType, temporal,  T, WT),
    get_weight(QueryType, novelty,   N, WN),
    BaseScore is WR + WE + WP + WD + WT + WN,
    compatibility(QueryType, Typ,Compat),
    ( query_bonus(QueryType, Typ, Bonus) -> true ; Bonus = 0.0 ),
    Score is BaseScore + Compat + Bonus,
    Heads = heads(relation-R, entity-E, position-P, discourse-D, temporal-T, novelty-N, type-Typ, compat-Compat, bonus-Bonus).


get_weight(QueryType, Head, Value, Weight) :-
    ( query_weight(QueryType, Head, Base) -> true ; head_weight(Head, Base) ),
    Weight is Base * Value.


% ── COMPATIBILITY: how well does this relation type match this query? ──

% compatibility(QueryType, RelationType, Score)
% Range: 0.0 (incompatible) → 2.0 (perfect match with strong boost)

compatibility(where,     location,    2.0).
compatibility(where,     main,        0.6).
compatibility(where,     temporal,    0.3).
compatibility(where,     attribute,   0.2).
compatibility(where,     negation,    0.4).
compatibility(where,     indirect,    0.2).

compatibility(when,      temporal,    2.0).
compatibility(when,      main,        0.5).
compatibility(when,      location,    0.3).
compatibility(when,      attribute,   0.2).
compatibility(when,      negation,    0.3).
compatibility(when,      indirect,    0.2).

compatibility(what,      main,        1.5).
compatibility(what,      attribute,   0.8).
compatibility(what,      location,    0.5).
compatibility(what,      temporal,    0.4).
compatibility(what,      indirect,    0.6).
compatibility(what,      negation,    0.3).

compatibility(who,       main,        0.8).
compatibility(who,       indirect,    2.0).
compatibility(who,       attribute,   0.5).
compatibility(who,       location,    0.3).
compatibility(who,       temporal,    0.2).
compatibility(who,       negation,    0.2).

compatibility(negation,  negation,    2.0).
compatibility(negation,  main,        0.5).
compatibility(negation,  location,    0.6).
compatibility(negation,  temporal,    0.3).
compatibility(negation,  attribute,   0.2).
compatibility(negation,  indirect,    0.2).

compatibility(color_query, attribute, 2.0).
compatibility(color_query, main,      0.3).
compatibility(color_query, location,  0.1).
compatibility(color_query, temporal,  0.0).
compatibility(color_query, negation,  0.0).
compatibility(color_query, indirect,  0.1).

compatibility(size_query, attribute, 2.0).
compatibility(size_query, main,      0.4).
compatibility(size_query, location,  0.2).
compatibility(size_query, temporal,  0.0).
compatibility(size_query, negation,  0.0).
compatibility(size_query, indirect,  0.1).

compatibility(unknown,   main,        0.7).
compatibility(unknown,   _,           0.5).

compatibility(color_query, attribute, 1.0).
compatibility(color_query, main,      0.4).
compatibility(color_query, location,  0.2).
compatibility(color_query, temporal,  0.1).
compatibility(color_query, negation,  0.1).
compatibility(color_query, indirect,  0.1).

compatibility(size_query, attribute, 1.0).
compatibility(size_query, main,      0.5).
compatibility(size_query, location,  0.3).
compatibility(size_query, temporal,  0.1).
compatibility(size_query, negation,  0.1).
compatibility(size_query, indirect,  0.1).


% ── QUERY BONUS: explicit boost for specific combinations ───────────

% query_bonus(QueryType, RelationType, Bonus)
% Used for edge cases where compatibility alone isn't enough

% "para quien" → indirect gets a strong boost
query_bonus(who, indirect, 0.25).

% "donde" + negation → still relevant but shouldn't beat location
query_bonus(where, negation, -0.10).

% "cuando" + main verb → temporal context boost
query_bonus(when, main, 0.10).


% ── HEAD 0: TYPE ──────────────────────────────────────────────────
type_head(relation(main, _, _), main) :- !.
type_head(relation(attribute, _, _), attribute) :- !.
type_head(relation(location, _, _), location) :- !.
type_head(relation(temporal, _, _), temporal) :- !.
type_head(relation(indirect, _, _), indirect) :- !.
type_head(relation(negation, _, _), negation) :- !.
type_head(_, unknown).

% ── HEAD 1: RELATION ───────────────────────────────────────────────
relation_head(relation(main, _, _), 1.0) :- !.
relation_head(relation(negation, _, _), 0.95) :- !.
relation_head(relation(temporal, _, _), 0.8) :- !.
relation_head(relation(location, _, _), 0.7) :- !.
relation_head(relation(attribute, _, _), 0.7) :- !.
relation_head(relation(indirect, _, _), 0.6) :- !.
relation_head(_, 0.5).

% ── HEAD 2: ENTITY ─────────────────────────────────────────────────
entity_head(relation(_, _, Args), Score) :-
    maplist(entity_known, Args, Scores),
    sum_list(Scores, Total),
    length(Scores, Len),
    Score is Total / Len.

entity_known(X, 1.0) :- atom(X), known_symbol(X), !.
entity_known(X, 1.0) :- compound(X), arg(1, X, A), known_symbol(A), !.
entity_known(X, 0.5) :- atom(X), !.
entity_known(_, 0.3).

% ── HEAD 3: POSITION ──────────────────────────────────────────────
position_head(relation(main, _, _), 1.0) :- !.
position_head(relation(negation, _, _), 0.9) :- !.
position_head(relation(attribute, _, _), 0.8) :- !.
position_head(relation(location, _, _), 0.7) :- !.
position_head(relation(temporal, _, _), 0.6) :- !.
position_head(relation(indirect, _, _), 0.5) :- !.
position_head(_, 0.4).

% ── HEAD 4: DISCOURSE ─────────────────────────────────────────────
discourse_head(relation(main, comprar, _), 1.0) :- !.
discourse_head(relation(main, tener, _), 1.0) :- !.
discourse_head(relation(main, visitar, _), 1.0) :- !.
discourse_head(relation(main, llevar, _), 1.0) :- !.
discourse_head(relation(main, trabajar, _), 1.0) :- !.
discourse_head(relation(main, vivir, _), 0.9) :- !.
discourse_head(relation(negation, _, _), 0.9) :- !.
discourse_head(relation(temporal, _, _), 0.8) :- !.
discourse_head(relation(attribute, _, _), 0.7) :- !.
discourse_head(relation(location, _, _), 0.7) :- !.
discourse_head(_, 0.5).

% ── HEAD 5: TEMPORAL ──────────────────────────────────────────────
temporal_head(relation(temporal, _, _), 1.0) :- !.
temporal_head(relation(main, _, _), 0.4) :- !.
temporal_head(relation(negation, _, _), 0.6) :- !.
temporal_head(_, 0.2).

% ── HEAD 6: NOVELTY ───────────────────────────────────────────────
novelty_head(relation(main, R, Args), 0.2) :-
    Args = [S,O], known_relation(S, R, O), !.
novelty_head(_, 1.0).


% ════════════════════════════════════════════════════════════════════
%  SORT / TOP-K
% ════════════════════════════════════════════════════════════════════

compare_score(Order, A, B) :-
    A = scored(_, ScoreA, _),
    B = scored(_, ScoreB, _),
    compare(Order, ScoreB, ScoreA).

top_k(Items, K, Result) :- take_k(Items, K, Result).

take_k(_, 0, []) :- !.
take_k([], _, []).
take_k([H|T], K, [H|R]) :- K > 0, K1 is K - 1, take_k(T, K1, R).
