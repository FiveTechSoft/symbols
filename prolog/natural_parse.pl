% natural_parse.pl
% Parser natural SIN lexicon de entidades: jamas lista personas, ciudades,
% objetos ni paises. Solo clase cerrada funcional (adverbios, articulos,
% pronombres por rol) + patrones verbo+estructura. La morfologia verbal
% se normaliza a relaciones canonicas (conjunto cerrado declarado).
% Pronombres: el sujeto pronominal es el ultimo sujeto; el objeto
% pronominal, el ultimo objeto (el corpus lo garantiza por construccion;
% el gold verifica cada resolucion). Sin tipos, sin plantillas de frase.
:- use_module(library(lists)).

:- dynamic last_subject/1.
:- dynamic last_object/1.

reset_natural :-
    retractall(last_subject(_)),
    retractall(last_object(_)).

tokenize_nat(Sentence, Tokens) :-
    string_lower(Sentence, Lower),
    string_chars(Lower, Chars),
    exclude(nat_punct, Chars, Clean),
    string_chars(CleanStr, Clean),
    split_string(CleanStr, " ", " ", Parts),
    exclude(nat_empty, Parts, NonEmpty),
    maplist(atom_string, Tokens, NonEmpty).

nat_punct('.'). nat_punct(','). nat_punct(';'). nat_punct(':').
nat_punct('?'). nat_punct('!').
nat_empty("").

% --- closed functional class (no entities below this line) ---
nat_adv(yesterday). nat_adv(today). nat_adv(tomorrow).
nat_adv(last). nat_adv(week). nat_adv(year). nat_adv(summer).
nat_adv(frequently). nat_adv(every). nat_adv(later).
nat_adv(strongly). nat_adv(quickly). nat_adv(brightly).
nat_adv(calmly). nat_adv(softly). nat_adv(loudly). nat_adv(late).
nat_adv(early). nat_adv(outside). nat_adv(beautifully). nat_adv(gradually).
nat_adv(never). nat_adv(today).
nat_art(a). nat_art(an). nat_art(the).
nat_subj_pro(he). nat_subj_pro(she).
nat_obj_pro(him). nat_obj_pro(her). nat_obj_pro(it).

strip_noise(Tokens, Core) :-
    exclude(nat_filler, Tokens, Core).

nat_filler(T) :- nat_adv(T), !.
nat_filler(T) :- nat_art(T), !.

% --- pronoun resolution by role recency (corpus-guaranteed) ---
resolve_subj(P, P) :- \+ nat_subj_pro(P), !.
resolve_subj(P, R) :- nat_subj_pro(P), last_subject(R), !.

resolve_obj(O, O) :- \+ nat_obj_pro(O), !.
resolve_obj(O, R) :- nat_obj_pro(O), last_object(R), !.

note_mentions(S, O) :-
    retractall(last_subject(_)), assertz(last_subject(S)),
    retractall(last_object(_)), assertz(last_object(O)).

% --- verb+structure patterns -> canonical relations ---
% [P lives|live in C]
parse_pat([P0, lives, in, C0], (P, lives_in, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, live, in, C0], (P, lives_in, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
% [P works|work in C]
parse_pat([P0, works, in, C0], (P, works_in, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, work, in, C0], (P, works_in, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
% [P visited|visits|visit C] + [P returned|arrived ... to|in C]
parse_pat([P0, visited, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, visits, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, visit, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, returned, to, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, returns, to, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, arrived, in, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, arrives, in, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, arrived, C0], (P, visits, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
% [P owns|has O]
parse_pat([P0, owns, O0], (P, owns, O)) :-
    resolve_subj(P0, P), resolve_obj(O0, O).
parse_pat([P0, has, O0], (P, owns, O)) :-
    resolve_subj(P0, P), resolve_obj(O0, O).
parse_pat([P0, have, O0], (P, owns, O)) :-
    resolve_subj(P0, P), resolve_obj(O0, O).
% [P reaches|reached K] (conclusiones observadas del objetivo latente)
parse_pat([P0, reaches, K0], (P, reaches, K)) :-
    resolve_subj(P0, P), resolve_obj(K0, K).
parse_pat([P0, reached, K0], (P, reaches, K)) :-
    resolve_subj(P0, P), resolve_obj(K0, K).
parse_pat([P0, reach, K0], (P, reaches, K)) :-
    resolve_subj(P0, P), resolve_obj(K0, K).
% [P works|worked at G] (corpus v0.2; distinto de works_in)
parse_pat([P0, works, at, G0], (P, works_at, G)) :-
    resolve_subj(P0, P), resolve_obj(G0, G).
parse_pat([P0, worked, at, G0], (P, works_at, G)) :-
    resolve_subj(P0, P), resolve_obj(G0, G).
% [G is|was located in C]
parse_pat([G0, is, located, in, C0], (G, located, C)) :-
    resolve_subj(G0, G), resolve_obj(C0, C).
parse_pat([G0, was, located, in, C0], (G, located, C)) :-
    resolve_subj(G0, G), resolve_obj(C0, C).
% [P is|was based in K] (conclusion observada del 2o objetivo latente)
parse_pat([P0, is, based, in, K0], (P, based, K)) :-
    resolve_subj(P0, P), resolve_obj(K0, K).
parse_pat([P0, was, based, in, K0], (P, based, K)) :-
    resolve_subj(P0, P), resolve_obj(K0, K).
% --- familia B, corpus v0.3 (EXP44-A): superficie disjunta, SIN
% normalizar contra la familia A. El mapa lo induce el motor por roles.
% [P keeps|kept O]
parse_pat([P0, keeps, O0], (P, keeps, O)) :-
    resolve_subj(P0, P), resolve_obj(O0, O).
parse_pat([P0, kept, O0], (P, keeps, O)) :-
    resolve_subj(P0, P), resolve_obj(O0, O).
% [O is|was held by P]
parse_pat([O0, is, held, by, P0], (O, held_by, P)) :-
    resolve_obj(O0, O), resolve_subj(P0, P).
parse_pat([O0, was, held, by, P0], (O, held_by, P)) :-
    resolve_obj(O0, O), resolve_subj(P0, P).
% [P tours|toured C] + [P came back to C]
parse_pat([P0, tours, C0], (P, tours, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, toured, C0], (P, tours, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
parse_pat([P0, came, back, to, C0], (P, tours, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
% [O is|was stored in C] (conclusion B, jamas observada: solo queries)
parse_pat([O0, is, stored, in, C0], (O, stored, C)) :-
    resolve_obj(O0, O), resolve_subj(C0, C).
parse_pat([O0, was, stored, in, C0], (O, stored, C)) :-
    resolve_obj(O0, O), resolve_subj(C0, C).
% [P prepares|prepared D]
parse_pat([P0, prepares, D0], (P, prepares, D)) :-
    resolve_subj(P0, P), resolve_obj(D0, D).
parse_pat([P0, prepared, D0], (P, prepares, D)) :-
    resolve_subj(P0, P), resolve_obj(D0, D).
% [D requires|required I]
parse_pat([D0, requires, I0], (D, requires, I)) :-
    resolve_subj(D0, D), resolve_obj(I0, I).
parse_pat([D0, required, I0], (D, requires, I)) :-
    resolve_subj(D0, D), resolve_obj(I0, I).
% [P serves|offers I] (conclusiones observadas en A / ocultas en B)
parse_pat([P0, serves, I0], (P, serves, I)) :-
    resolve_subj(P0, P), resolve_obj(I0, I).
parse_pat([P0, offers, I0], (P, offers, I)) :-
    resolve_subj(P0, P), resolve_obj(I0, I).
% [P cooks|cooked D]
parse_pat([P0, cooks, D0], (P, cooks, D)) :-
    resolve_subj(P0, P), resolve_obj(D0, D).
parse_pat([P0, cooked, D0], (P, cooks, D)) :-
    resolve_subj(P0, P), resolve_obj(D0, D).
% [D needs|needed I]
parse_pat([D0, needs, I0], (D, needs, I)) :-
    resolve_subj(D0, D), resolve_obj(I0, I).
parse_pat([D0, needed, I0], (D, needs, I)) :-
    resolve_subj(D0, D), resolve_obj(I0, I).
% [P resides in C] (EXP47: superficie B, SIN normalizar a lives_in;
% el mapa lo induce el motor por roles estructurales)
parse_pat([P0, resides, in, C0], (P, resides_in, C)) :-
    resolve_subj(P0, P), resolve_obj(C0, C).
% [O belongs to P]
parse_pat([O0, belongs, to, P0], (O, belongs_to, P)) :-
    resolve_obj(O0, O), resolve_subj(P0, P).
% [C is|was|lies in K]
parse_pat([C0, is, in, K0], (C, in, K)) :-
    resolve_subj(C0, C), resolve_obj(K0, K).
parse_pat([C0, was, in, K0], (C, in, K)) :-
    resolve_subj(C0, C), resolve_obj(K0, K).
parse_pat([C0, lies, in, K0], (C, in, K)) :-
    resolve_subj(C0, C), resolve_obj(K0, K).

% wrapper: menciona una sola vez, solo si el patron completo unifico
% (sin efectos laterales en ramas fallidas). En belongs_to la persona
% va segunda: se registra como sujeto para la resolucion por rol.
parse_nat(Core, Triple) :-
    parse_pat(Core, Triple),
    pat_person(Triple, P),
    triple_object(Triple, O),
    note_mentions(P, O).

triple_object((_, _, O), O).

pat_person((_, belongs_to, P), P) :- !.
pat_person((S, _, _), S).

% symbolize_natural(+Sentence, -Triple): falla si no hay patron (ruido).
symbolize_natural(Sentence, Triple) :-
    tokenize_nat(Sentence, Tokens),
    strip_noise(Tokens, Core),
    parse_nat(Core, Triple).
