:- module(parser_attention,
    [ parse_sentence/2,
      parse_and_attend/3,
      attention/3,
      top_k/3,
      normalize_text/2,
      sentence_relations/2,
      all_clause_relations/2,
      split_conjunctions/2,
      pattern/2,
      p6_visitar_en_time/2,
      p7_comprar_lo_llevar_a/2,
      p8_two_sentences/2,
      p9_trabajar_en_desde/2
    ]).

:- use_module(library(lists)).

:- dynamic known_symbol/1.
:- dynamic known_relation/3.
:- dynamic pronoun_ref/2.

known_symbol(juan).
known_symbol(maria).
known_symbol(pedro).
known_symbol(ana).
known_symbol(coche).
known_symbol(casa).
known_symbol(libro).
known_symbol(empresa).
known_symbol(manzana).
known_symbol(bicicleta).
known_symbol(madrid).
known_symbol(barcelona).
known_symbol(malaga).
known_symbol(marbella).

known_relation(juan, vive_en, madrid).
known_relation(maria, vive_en, barcelona).


% ════════════════════════════════════════════════════════════════════
%  TOKENIZER
% ════════════════════════════════════════════════════════════════════

parse_sentence(Text, Relations) :-
    normalize_text(Text, Normalized),
    sentence_relations(Normalized, Relations).

parse_and_attend(Text, Selected, All) :-
    parse_sentence(Text, Relations),
    attention(Relations, Text, Scored),
    top_k(Scored, 5, Selected),
    All = Scored.

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

% Verb families: each verb maps to its infinitive
verb_family(compro, comprar).   verb_family(compra, comprar).
verb_family(compran, comprar).  verb_family(compro, comprar).
verb_family(vive, vivir).       verb_family(viven, vivir).
verb_family(vivia, vivir).
verb_family(tiene, tener).      verb_family(tienen, tener).
verb_family(tenia, tener).
verb_family(visitó, visitar).   verb_family(visitó, visitar).
verb_family(visitamos, visitar). verb_family(visito, visitar).
verb_family(llevo, llevar).     verb_family(llevar, llevar).
verb_family(lleva, llevar).
verb_family(trabaja, trabajar).  verb_family(trabajan, trabajar).
verb_family(trabajo, trabajar).
verb_family(esta, estar).        verb_family(esta, estar).
verb_family(estado, estar).

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

pronoun(lo). pronoun(la). pronoun(le). pronoun(se). pronoun(lo).
pronoun(Lo) :- atom_string(Lo, "lo").
pronoun(La) :- atom_string(La, "la").

preposition(para). preposition(desde). preposition(en). preposition(a).


% ════════════════════════════════════════════════════════════════════
%  PARSER — pattern matching on token lists
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


% ── split_conjunctions/2 ────────────────────────────────────────────

split_conjunctions(Tokens, [Before, After]) :-
    append(Before, [Conj|After0], Tokens),
    conjunction(Conj),
    Before \== [],
    After0 \== [],
    After = After0, !.
split_conjunctions(Tokens, [Tokens]).


% ── clause_relations/2 — dispatch to pattern matchers ───────────────

clause_relations(Tokens, Relation) :-
    pattern(Tokens, Relation).

% Collect all relations from a single clause
all_clause_relations(Tokens, Relations) :-
    findall(R, pattern(Tokens, R), Relations), !.

pattern(Tokens, Relation) :- p1_comprar_adj_en_time(Tokens, Relation).
pattern(Tokens, Relation) :- p2_vivir_en(Tokens, Relation).
pattern(Tokens, Relation) :- p3_tener_adj_en(Tokens, Relation).
pattern(Tokens, Relation) :- p4_comprar_adj_para(Tokens, Relation).
pattern(Tokens, Relation) :- p5_no_vivir_en(Tokens, Relation).
pattern(Tokens, Relation) :- p6_visitar_en_time(Tokens, Relation).
pattern(Tokens, Relation) :- p7_comprar_lo_llevar_a(Tokens, Relation).
pattern(Tokens, Relation) :- p8_two_sentences(Tokens, Relation).
pattern(Tokens, Relation) :- p9_trabajar_en_desde(Tokens, Relation).
pattern(Tokens, Relation) :- p10_comprar_simple(Tokens, Relation).
pattern(Tokens, Relation) :- p11_tener_simple(Tokens, Relation).
pattern(Tokens, Relation) :- p12_visitar_simple(Tokens, Relation).
pattern(Tokens, Relation) :- p13_llevar_a(Tokens, Relation).
pattern(Tokens, Relation) :- p14_estar_en(Tokens, Relation).


% ── P1: S V Art O Adj en Place Time ────────────────────────────────
% "Juan compro un coche rojo en madrid ayer"

p1_comprar_adj_en_time([S,V,Art,O,Adj,en,Place,Time], relation(comprar,S,O,source(main))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1_comprar_adj_en_time([S,V,Art,O,Adj,en,Place,Time], relation(color,O,Adj,source(attribute))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1_comprar_adj_en_time([S,V,Art,O,Adj,en,Place,Time], relation(ubicado_en,O,Place,source(location))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).
p1_comprar_adj_en_time([S,V,Art,O,Adj,en,Place,Time], relation(tiempo,comprar(S,O),Time,source(temporal))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), location(Place), temporal(Time).


% ── P2: S V en Place ───────────────────────────────────────────────
% "Juan vive en barcelona"

p2_vivir_en([S,V,en,Place], relation(vivir,S,Place,source(main))) :-
    subject(S), verb_family(V,vivir), location(Place).


% ── P3: S V Art O Adj en Place ─────────────────────────────────────
% "Pedro tiene una casa grande en malaga"

p3_tener_adj_en([S,V,Art,O,Adj,en,Place], relation(tener,S,O,source(main))) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).
p3_tener_adj_en([S,V,Art,O,Adj,en,Place], relation(tamano,O,Adj,source(attribute))) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).
p3_tener_adj_en([S,V,Art,O,Adj,en,Place], relation(ubicado_en,O,Place,source(location))) :-
    subject(S), verb_family(V,tener), article(Art), object(O),
    adjective_size(Adj), location(Place).


% ── P4: S V Art O Adj para Person ──────────────────────────────────
% "Ana compro un libro azul para maria"

p4_comprar_adj_para([S,V,Art,O,Adj,para,Person], relation(comprar,S,O,source(main))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).
p4_comprar_adj_para([S,V,Art,O,Adj,para,Person], relation(color,O,Adj,source(attribute))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).
p4_comprar_adj_para([S,V,Art,O,Adj,para,Person], relation(para,comprar(S,O),Person,source(indirect))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O),
    adjective_color(Adj), subject(Person).


% ── P5: S no V en Place ────────────────────────────────────────────
% "Juan no vive en madrid"

p5_no_vivir_en([S,no,V,en,Place], relation(negado,vivir(S,Place),source(negation))) :-
    subject(S), negation(no), verb_family(V,vivir), location(Place).


% ── P6: S V O en Time ──────────────────────────────────────────────
% "Maria visitó madrid en 2025"

p6_visitar_en_time([S,V,O,en,Time], relation(visitar,S,O,source(main))) :-
    subject(S), verb_family(V,visitar), temporal(Time),
    ( object(O) ; location(O) ).
p6_visitar_en_time([S,V,O,en,Time], relation(tiempo,visitar(S,O),Time,source(temporal))) :-
    subject(S), verb_family(V,visitar), temporal(Time),
    ( object(O) ; location(O) ).


% ── P7: S V Art O y lo V a Place ────────────────────────────────────
% "Pedro compro un coche y lo llevo a barcelona"

p7_comprar_lo_llevar_a([S,V1,Art,O,Conj,Pron,V2,a,Place],
                        relation(comprar,S,O,source(main))) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).
p7_comprar_lo_llevar_a([S,V1,Art,O,Conj,Pron,V2,a,Place],
                        relation(llevar,S,O,source(main))) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).
p7_comprar_lo_llevar_a([S,V1,Art,O,Conj,Pron,V2,a,Place],
                        relation(llevar_destino,O,Place,source(location))) :-
    subject(S), verb_family(V1,comprar), article(Art), object(O),
    conjunction(Conj), pronoun(Pron), verb_family(V2,llevar), location(Place).


% ── P8: S V Art O. O V en Place (coreference) ──────────────────────
% "Ana tiene una casa. La casa esta en Marbella."
% Tokens: [ana,tiene,una,casa,la,casa,esta,en,marbella]

p8_two_sentences([S,V1,Art,O,Art2,O2,V2,en,Place], relation(tener,S,O,source(main))) :-
    subject(S), verb_family(V1,tener), article(Art), object(O),
    article(Art2), object(O2), O == O2, verb_family(V2,estar), location(Place).
p8_two_sentences([S,V1,Art,O,Art2,O2,V2,en,Place], relation(ubicado_en,O,Place,source(location))) :-
    subject(S), verb_family(V1,tener), article(Art), object(O),
    article(Art2), object(O2), O == O2, verb_family(V2,estar), location(Place).

p8_two_sentences(Tokens, Relation) :-
    append(First, ['.'|Second], Tokens),
    First \== [],
    Second \== [],
    clause_relations(First, Relation).
p8_two_sentences(Tokens, Relation) :-
    append(First, ['.'|Second], Tokens),
    First \== [],
    Second \== [],
    resolve_coreference(Second, Resolved),
    clause_relations(Resolved, Relation).

resolve_coreference([la, O, V, en, Place], [O, V, en, Place]) :-
    object(O), !.
resolve_coreference([el, O, V, en, Place], [O, V, en, Place]) :-
    object(O), !.
resolve_coreference(X, X).


% ── P9: S V en Place desde Time ────────────────────────────────────
% "Juan trabaja en madrid desde 2020"

p9_trabajar_en_desde([S,V,en,Place,desde,Time], relation(trabajar,S,Place,source(main))) :-
    subject(S), verb_family(V,trabajar), location(Place), temporal(Time).
p9_trabajar_en_desde([S,V,en,Place,desde,Time], relation(desde,trabajar(S,Place),Time,source(temporal))) :-
    subject(S), verb_family(V,trabajar), location(Place), temporal(Time).


% ── P10: S V Art O Adj (no location) ───────────────────────────────
% "Maria compro un coche negro"

p10_comprar_simple([S,V,Art,O,Adj], relation(comprar,S,O,source(main))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O), adjective_color(Adj).
p10_comprar_simple([S,V,Art,O,Adj], relation(color,O,Adj,source(attribute))) :-
    subject(S), verb_family(V,comprar), article(Art), object(O), adjective_color(Adj).


% ── P11: S V Art O (simple) ────────────────────────────────────────
% "Pedro tiene una empresa"

p11_tener_simple([S,V,Art,O], relation(tener,S,O,source(main))) :-
    subject(S), verb_family(V,tener), article(Art), object(O).


% ── P12: S V O (no article, simple) ────────────────────────────────
% "Maria visito madrid" (without time)

p12_visitar_simple([S,V,O], relation(visitar,S,O,source(main))) :-
    subject(S), verb_family(V,visitar), object(O).


% ── P13: S V O a Place ─────────────────────────────────────────────
% Generic "llevar a"

p13_llevar_a([S,V,O,a,Place], relation(llevar,S,O,source(main))) :-
    subject(S), verb_family(V,llevar), object(O), location(Place).
p13_llevar_a([S,V,O,a,Place], relation(llevar_destino,O,Place,source(location))) :-
    subject(S), verb_family(V,llevar), object(O), location(Place).


% ── P14: S V en Place (estar, etc.) ────────────────────────────────
% "La casa esta en marbella"

p14_estar_en([S,V,en,Place], relation(ubicado_en,S,Place,source(location))) :-
    object(S), verb_family(V,estar), location(Place).


% ════════════════════════════════════════════════════════════════════
%  SYMBOLIC ATTENTION
% ════════════════════════════════════════════════════════════════════

attention(Relations, _Text, Scored) :-
    maplist(score_relation, Relations, Raw),
    predsort(compare_score, Raw, Scored).


score_relation(Relation, scored(Relation, Score, Heads)) :-
    relation_head(Relation, R),
    entity_head(Relation, E),
    position_head(Relation, P),
    discourse_head(Relation, D),
    temporal_head(Relation, T),
    novelty_head(Relation, N),
    Score is 0.30*R + 0.20*E + 0.10*P + 0.10*D + 0.15*T + 0.15*N,
    Heads = heads(relation-R, entity-E, position-P, discourse-D, temporal-T, novelty-N).


% HEAD 1: RELATION — main verb gets highest
relation_head(relation(comprar, _, _, source(main)), 1.0) :- !.
relation_head(relation(tener, _, _, source(main)), 1.0) :- !.
relation_head(relation(visitar, _, _, source(main)), 1.0) :- !.
relation_head(relation(llevar, _, _, source(main)), 1.0) :- !.
relation_head(relation(trabajar, _, _, source(main)), 1.0) :- !.
relation_head(relation(vivir, _, _, source(main)), 0.9) :- !.
relation_head(relation(negado, _, _, _), 0.9) :- !.
relation_head(relation(_, _, _, _), 0.7).

% HEAD 2: ENTITY — known symbols
entity_head(relation(_, A, B, _), Score) :-
    entity_known(A, SA), entity_known(B, SB),
    Score is (SA + SB) / 2.
entity_known(X, 1.0) :- known_symbol(X), !.
entity_known(X, 0.5) :- atom(X), !.
entity_known(_, 0.0).

% HEAD 3: POSITION — source type
position_head(relation(_, _, _, source(main)), 1.0) :- !.
position_head(relation(_, _, _, source(attribute)), 0.8) :- !.
position_head(relation(_, _, _, source(location)), 0.7) :- !.
position_head(relation(_, _, _, source(temporal)), 0.6) :- !.
position_head(relation(_, _, _, source(indirect)), 0.6) :- !.
position_head(relation(_, _, _, source(negation)), 0.9) :- !.

% HEAD 4: DISCOURSE — main action is most relevant
discourse_head(relation(comprar, _, _, source(main)), 1.0) :- !.
discourse_head(relation(tener, _, _, source(main)), 1.0) :- !.
discourse_head(relation(visitar, _, _, source(main)), 1.0) :- !.
discourse_head(relation(llevar, _, _, source(main)), 1.0) :- !.
discourse_head(relation(trabajar, _, _, source(main)), 1.0) :- !.
discourse_head(relation(vivir, _, _, source(main)), 0.9) :- !.
discourse_head(relation(color, _, _, _), 0.8) :- !.
discourse_head(relation(ubicado_en, _, _, _), 0.8) :- !.
discourse_head(relation(tamano, _, _, _), 0.8) :- !.
discourse_head(relation(tiempo, _, _, _), 0.9) :- !.
discourse_head(relation(negado, _, _, _), 0.9) :- !.
discourse_head(_, 0.5).

% HEAD 5: TEMPORAL
temporal_head(relation(tiempo, _, _, _), 1.0) :- !.
temporal_head(relation(desde, _, _, _), 0.9) :- !.
temporal_head(relation(negado, _, _, _), 0.8) :- !.
temporal_head(_, 0.3).

% HEAD 6: NOVELTY
novelty_head(relation(R,S,O,_), 0.2) :- known_relation(S,R,O), !.
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
