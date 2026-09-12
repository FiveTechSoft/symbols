% dialog_ref.pl — referencia pronominal en dialogo (D1).
% Pila de entidades mencionadas (sujetos y objetos por separado,
% recencia) + sustitucion de pronombres en preguntas. Pronombres =
% clase cerrada EN (sintaxis). Sin hechos de genero en el mapa, he/she
% no se distinguen: decide tiers (sujetos antes que objetos, el slot
% pronominal busca su rol) + recencia. 1 superviviente -> sustituye;
% 0 o 2+ -> deja el token (UNKNOWN honesto, nunca azar).
% Sin veto funcional: necesitaria hechos del pronombre (EXP18 los
% ingiere) y el chat sustituye antes de almacenar por diseno.
% Sin mutacion del mapa: solo la pila dialog_stack/1 cambia.
:- use_module(library(lists)).
:- use_module(library(pairs)).

:- dynamic dialog_stack/2.
% dialog_stack(Role, Entities): Role = subj | obj, mas reciente primero.

dialog_reset :-
    retractall(dialog_stack(_, _)),
    assertz(dialog_stack(subj, [])),
    assertz(dialog_stack(obj, [])).

% dialog_note(+Entities, +Role): empuja atomos (sin vars, sin dup
% consecutivos), tope 20.
dialog_note(Es, Role) :-
    dialog_stack(Role, Old),
    foldl(note_one, Es, Old, New0),
    length(New0, L),
    ( L > 20 -> length(New, 20), append(New, _, New0) ; New = New0 ),
    retractall(dialog_stack(Role, _)),
    assertz(dialog_stack(Role, New)).

note_one(E, Old, New) :-
    atom(E), !,
    ( Old = [E|_] -> New = Old ; New = [E|Old] ).
note_one(_, Old, Old).

dialog_pronoun(W) :-
    member(W, [he, him, his, she, her, hers, it, its,
               they, them, their]).

% Tier por caso morfologico (sintaxis, como la lista): sujeto para
% he/she/they, objeto para him/her/them, ambos para el resto.
dialog_tier(P, subj) :- member(P, [he, she, they]), !.
dialog_tier(P, obj) :- member(P, [him, her, them]), !.
dialog_tier(_, both).

dialog_has_pronoun(Toks) :-
    member(W, Toks),
    dialog_pronoun(W), !.

% dialog_substitute(+Toks, -Out, -Changed): reemplaza pronombres por la
% entidad resuelta; Changed = yes si cambio algo.
dialog_substitute(Toks, Out, Changed) :-
    maplist(sub_one, Toks, Pairs),
    pairs_keys_values(Pairs, Out, Flags),
    ( member(yes, Flags) -> Changed = yes ; Changed = no ).

sub_one(W, C-yes) :-
    dialog_pronoun(W),
    dialog_resolve(W, C), !.
sub_one(W, W-no).

% tiers: el caso del pronombre elige tier (sujeto u objeto); resto
% (his/hers/its/their/it) prueba sujetos y luego objetos. 1
% superviviente o nada (UNKNOWN honesto, nunca azar).
dialog_resolve(P, C) :-
    dialog_tier(P, subj), !,
    dialog_stack(subj, Ss),
    tier_single(P, Ss, C).
dialog_resolve(P, C) :-
    dialog_tier(P, obj), !,
    dialog_stack(obj, Os),
    tier_single(P, Os, C).
dialog_resolve(P, C) :-
    dialog_stack(subj, Ss),
    dialog_stack(obj, Os),
    ( tier_single(P, Ss, C) -> true
    ; tier_single(P, Os, C)
    ).

% dialog_candidates(+P, -Cs): lista ordenada para aclarar (D3), en el
% tier de su caso. Sin veto: la pregunta, no la afirmacion, decide.
dialog_candidates(P, Cs) :-
    dialog_tier(P, subj), !,
    dialog_stack(subj, Ss),
    tier_list(Ss, Cs).
dialog_candidates(P, Cs) :-
    dialog_tier(P, obj), !,
    dialog_stack(obj, Os),
    tier_list(Os, Cs).
dialog_candidates(_P, Cs) :-
    dialog_stack(subj, Ss),
    dialog_stack(obj, Os),
    append(Ss, Os, All),
    foldl(note_ord, All, [], Ordered),
    findall(C, (member(C, Ordered), \+ dialog_pronoun(C)), Cs).

tier_list(Es, Cs) :-
    foldl(note_ord, Es, [], Ordered),
    findall(C, (member(C, Ordered), \+ dialog_pronoun(C)), Cs).

note_ord(E, Old, New) :-
    ( member(E, Old) -> New = Old ; append(Old, [E], New) ).

% dialog_ambiguity(+Toks, -P, -Cs): primer pronombre con 2+ candidatos.
dialog_ambiguity(Toks, P, Cs) :-
    member(P, Toks),
    dialog_pronoun(P),
    dialog_candidates(P, Cs),
    length(Cs, N),
    N >= 2, !.

tier_single(P, Es, C) :-
    findall(C, ( member(C, Es), \+ dialog_pronoun(C) ), Cs0),
    sort(Cs0, Cs),
    Cs = [C],
    C \== P.
