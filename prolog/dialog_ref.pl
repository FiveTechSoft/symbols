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

% tiers: mismo rol primero (sujeto pronominal ~ sujetos recientes),
% luego el otro; recencia dentro de cada tier; 1 superviviente o nada.
dialog_resolve(P, C) :-
    dialog_stack(subj, Ss),
    dialog_stack(obj, Os),
    ( tier_single(P, Ss, C) -> true
    ; tier_single(P, Os, C)
    ).

tier_single(P, Es, C) :-
    findall(C, ( member(C, Es), \+ dialog_pronoun(C) ), Cs0),
    sort(Cs0, Cs),
    Cs = [C],
    C \== P.
