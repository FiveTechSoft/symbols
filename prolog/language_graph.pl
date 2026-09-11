% language_graph.pl
% SYMBOLIZER: texto espanol -> tripleta (S, V, O). Lexico cerrado con tipos
% (nombres propios, sustantivos, formas verbales). Sin gramatica general:
% clasifica tokens por lexico y exige unicidad (1 persona, 1 verbo,
% 1 objeto del tipo que el verbo selecciona). El ruido se ignora por tipo.
:- use_module(library(lists)).

% --- lexico: personas ---
person(juan). person(maria). person(pedro). person(ana). person(luis).
person(carmen). person(pablo). person(lucia). person(miguel). person(elena).
person(david). person(sara). person(jorge). person(laura). person(diego).
person(ines). person(raul). person(nadia). person(ivan). person(vera).

% --- lexico: comidas (tipo food) ---
food(pan). food(leche). food(queso). food(manzana). food(arroz).
food(pollo). food(pescado). food(huevo). food(tomate). food(cebolla).
food(ajo). food(pimiento). food(platano). food(naranja). food(uva).
food(pera). food(fresa). food(limon). food(melon). food(sandia).

% --- lexico: ciudades (tipo city) ---
city(malaga). city(sevilla). city(granada). city(cordoba). city(almeria).

% --- formas verbales -> verbo canonico ---
verb_form(compra, compra). verb_form(comprando, compra).
verb_form(compró, compra). verb_form(comprado, compra).
verb_form(come, come). verb_form(comiendo, come).
verb_form(comió, come). verb_form(comido, come).
verb_form(vive, vive_en). verb_form(reside, vive_en).
verb_form(vivió, vive_en). verb_form(viviendo, vive_en).
verb_form(vende, vende). verb_form(vendió, vende).
verb_form(vendiendo, vende).

% objeto requerido por verbo
verb_object_type(compra, food). verb_object_type(come, food).
verb_object_type(vende, food). verb_object_type(vive_en, city).

% --- palabras a ignorar (auxiliares, cliticos, adjuntos) ---
skip_word(el). skip_word(la). skip_word(los). skip_word(las).
skip_word(lo). skip_word(un). skip_word(una). skip_word(de).
skip_word(del). skip_word(al). skip_word(en). skip_word(por).
skip_word(donde). skip_word(está). skip_word(es). skip_word(se).
skip_word(ha). skip_word(ayer). skip_word(hoy). skip_word(rápidamente).
skip_word(lentamente). skip_word(tranquilamente). skip_word(con).
skip_word(calma).

% --- tokenizacion: minusculas, sin puntuacion, split por espacios ---
tokenize_text(Sentence, Tokens) :-
    string_lower(Sentence, Lower),
    string_chars(Lower, Chars),
    exclude(punct, Chars, Clean),
    string_chars(CleanStr, Clean),
    split_string(CleanStr, " ", " ", Parts),
    exclude(empty_string, Parts, NonEmpty),
    maplist(atom_string, Tokens, NonEmpty).

punct('.'). punct(','). punct(';'). punct(':').
punct('¿'). punct('?'). punct('¡'). punct('!').
empty_string("").

% --- clasificacion de un token ---
class_of(Tok, person) :- person(Tok), !.
class_of(Tok, food) :- food(Tok), !.
class_of(Tok, city) :- city(Tok), !.
class_of(Tok, verb(V)) :- verb_form(Tok, V), !.
class_of(Tok, skip) :- skip_word(Tok), !.
class_of(_, unknown).

% --- simbolizacion: exige exactamente 1 persona, 1 verbo, 1 objeto tipado ---
symbolize_sentence(Sentence, (S, V, O)) :-
    tokenize_text(Sentence, Tokens),
    include(is_person, Tokens, Ps),
    Ps = [S],
    findall(V, ( member(T, Tokens), verb_form(T, V) ), Vs0),
    sort(Vs0, [V]),
    verb_object_type(V, Type),
    include(is_type(Type), Tokens, Os),
    Os = [O].

is_person(T) :- person(T).
is_type(person, T) :- person(T).
is_type(food, T) :- food(T).
is_type(city, T) :- city(T).

% --- corpus: metricas de extraccion contra gold ---
% symbolize_corpus(+Sentences, +Gold, -Extracted, -P, -R)
symbolize_corpus(Sentences, Gold, Extracted, P, R) :-
    findall(T, ( member(S, Sentences),
                 symbolize_sentence(S, T)
               ),
            Extracted0),
    sort(Extracted0, Extracted),
    sort(Gold, G),
    intersection(Extracted, G, TP),
    length(TP, NTP),
    length(Extracted, NE),
    length(G, NG),
    ( NE =:= 0 -> P = 0.0 ; P is NTP / NE ),
    ( NG =:= 0 -> R = 0.0 ; R is NTP / NG ).
