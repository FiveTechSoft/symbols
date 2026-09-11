% experiment47.pl
% EXPERIMENT 47 - TRANSFER DURING CONVERSATION (EXP44 + EXP46)
% En pleno dialogo aparece una ontologia nueva (tours/resides_in/keeps,
% jamas vistas): el sistema induce el mapa por roles contra un ejemplar
% A, predice stored con prueba de transferencia y queda disponible de
% inmediato para el siguiente turno. London -> UNKNOWN con razon.
% Ningun modulo tocado (solo este fichero + patron resides_in aditivo).
:- consult('conversation.pl').
:- consult('experiment46.pl').

:- use_module(library(lists)).

:- dynamic struct_def/3.
:- dynamic struct_sig/2.
:- dynamic rel_map/3.

dialogue47 :-
    load_demo,
    nl, writeln('===== DIALOGUE 47 (transfer inside conversation) ====='),
    t("Does alba reach norway?", yes(_)),
    t_tell("Pierre keeps a tablet.", learned((pierre, keeps, tablet))),
    t_tell("The tablet is held by Pierre.",
           learned((tablet, held_by, pierre))),
    t_tell("Pierre tours Paris.", learned((pierre, tours, paris))),
    learn_map47,
    t_stored(tablet, paris, yes),
    t("Why?", proof(_)),
    t_tell("Pierre tours Rome.", learned((pierre, tours, rome))),
    t_stored(tablet, rome, yes),
    t_stored(tablet, london, unknown),
    t("Why?", noproof(_)),
    t_tell("Pierre resides in Paris.",
           learned((pierre, resides_in, paris))),
    t("Does alba reach norway?", yes(_)),
    report_checks.

% ---------- firmas locales + exclusion (EXP40/EXP44) ----------
excluded47([reaches, based, stored]).

member_sig_47(E, Members, sig(O, I, ON, IN)) :-
    excluded47(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_47(N, Members, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_47(N, Members, A, B)), IN0),
    sort(IN0, IN).

deg_47(E, Members, O, I) :-
    excluded47(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

struct_sse_47(S, stsig(MS, E)) :-
    struct_def(S, _Tag, Members),
    struct_sse_m(Members, stsig(MS, E)).

% SSE sobre lista explicita: la estructura se descubre alrededor de la
% pregunta (vale para paris, rome o cualquier C contado despues).
struct_sse_m(Members, stsig(MS, E)) :-
    findall(M, (member(X, Members), member_sig_47(X, Members, M)), M0),
    sort(M0, MS),
    excluded47(Exclude),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

sig_clean_47(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

% ---------- mapa inducido en conversacion ----------
% Ejemplar A: alba/book/oslo (entrenamiento conocido). B: lo contado.
learn_map47 :-
    retractall(struct_def(_, _, _)),
    retractall(struct_sig(_, _)),
    retractall(rel_map(_, _, _)),
    assertz(struct_def(sa47, supply, [alba, book, oslo])),
    assertz(struct_def(sb47, supply_b, [pierre, tablet, paris])),
    struct_sse_47(sa47, SigA),
    struct_sse_47(sb47, SigB),
    check(sig_clean_47(SigA), 'exemplar SSE zero atoms'),
    ( SigA == SigB ->
        check(true, 'B told-structure recognized (no shared vocab)')
    ; format('GOT ~w WANT ~w~n', [SigB, SigA]),
      check(false, 'B told-structure recognized (no shared vocab)')),
    assertz(struct_sig(supply, SigA)),
    induce_map_47(sa47, sb47, supply),
    findall((A, B), rel_map(supply, A, B), Maps),
    sort(Maps, SM),
    format('dialogue map: ~w~n', [SM]),
    check(SM == [(belongs_to, held_by), (owns, keeps), (visits, tours)],
          'map induced mid-dialogue (nothing given)').

induce_map_47(Exemplar, BStruct, Tag) :-
    struct_def(Exemplar, Tag, ExM),
    struct_def(BStruct, _, BM),
    forall(( member(X, ExM),
             member_sig_47(X, ExM, SX),
             member(Y, BM),
             member_sig_47(Y, BM, SX),
             member(X2, ExM), member(Y2, BM),
             member_sig_47(X2, ExM, SX2),
             member_sig_47(Y2, BM, SX2),
             memory_relation(X, RA, X2, _, _),
             \+ excluded47_rel(RA),
             memory_relation(Y, RB, Y2, _, _),
             \+ excluded47_rel(RB)
           ),
           assertz(rel_map(Tag, RA, RB))).

excluded47_rel(R) :-
    excluded47(E),
    member(R, E).

% ---------- stored por transferencia, disponible de inmediato ----------
ask_stored(K, C, yes(Proof)) :-
    rel_map(supply, belongs_to, RB1),
    rel_map(supply, visits, RB2),
    memory_relation(K, RB1, P, _, _),
    memory_relation(P, RB2, C, _, _),
    struct_sse_m([P, K, C], Sig), struct_sig(supply, Sig),
    Proof = [reuse(supply, [P, K, C]), map([belongs_to-RB1, visits-RB2]),
             rule(stored, [RB1, RB2]), (K, RB1, P), (P, RB2, C)].
ask_stored(_, _, unknown).

t_stored(K, C, yes) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored(K, C, A),
    say(A),
    remember_proof(A),
    ( A = yes(_) ->
        check(true, stored-yes)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored-yes)).
t_stored(K, C, unknown) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored(K, C, A),
    say(A),
    remember_proof(A),
    ( A = unknown ->
        check(true, stored-unknown)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored-unknown)).

% t/2, t_tell/2, remember_proof/1, say/1, check/2, report_checks/0
% heredados de conversation.pl / experiment46.pl.
