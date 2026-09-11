% experiment48.pl
% EXPERIMENT 48 - BIDIRECTIONAL DIALOGUE (el modelo pregunta)
% Tras "Pierre tours Paris", el modelo detecta una estructura supply
% PARCIAL (falta el spoke-O) y SONDEA el rol ausente hipotetizando en
% vocabulario A: "What does Pierre own?". La respuesta llega en
% superficie B ("He keeps a tablet") y se acepta por encaje
% estructural; segunda sonda ("Does the tablet belong to Pierre?")
% acepta "held by"; se induce el mapa y stored queda disponible.
% Iniciativa + hipotesis + aceptacion estructural + alineamiento.
:- consult('conversation.pl').
:- consult('experiment46.pl').

:- use_module(library(lists)).

:- dynamic struct_def/3.
:- dynamic struct_sig/2.
:- dynamic rel_map/3.

dialogue48 :-
    load_demo,
    nl, writeln('===== DIALOGUE 48 (model asks) ====='),
    t("Does alba reach norway?", yes(_)),
    t_tell("Pierre tours Paris.", learned((pierre, tours, paris))),
    probe_missing(pierre, Q1),
    check(Q1 == 'What does pierre own?', 'probe 1 targets missing O-spoke'),
    format('Model: ~w~n', [Q1]),
    t_tell("He keeps a tablet.", learned((pierre, keeps, tablet))),
    probe_missing(pierre, Q2),
    check(Q2 == 'Does tablet belong to pierre?',
          'probe 2 targets missing O-backlink'),
    format('Model: ~w~n', [Q2]),
    t_tell("The tablet is held by Pierre.",
           learned((tablet, held_by, pierre))),
    learn_map48,
    t_stored48(tablet, paris, yes),
    t("Why?", proof(_)),
    t_stored48(tablet, london, unknown),
    t("Why?", noproof(_)),
    t("Does alba reach norway?", yes(_)),
    report_checks.

% ---------- sonda por rol ausente (hipotesis en vocabulario A) ----------
% spoke-O ausente: hay salida tours/visits pero ninguna keeps/owns.
probe_missing(P, Q) :-
    ( memory_relation(P, tours, _, _, _)
    ; memory_relation(P, visits, _, _, _) ),
    \+ ( memory_relation(P, keeps, _, _, _)
       ; memory_relation(P, owns, _, _, _) ), !,
    atomic_list_concat(['What does ', P, ' own?'], Q).
% backlink-O ausente: el spoke no enlaza de vuelta hacia P
% (local al par P/O: el objeto puede tener aristas en el corpus).
probe_missing(P, Q) :-
    ( memory_relation(P, keeps, O, _, _)
    ; memory_relation(P, owns, O, _, _) ),
    \+ memory_relation(O, _, P, _, _), !,
    atomic_list_concat(['Does ', O, ' belong to ', P, '?'], Q).
probe_missing(_, complete).

% ---------- firmas locales + exclusion (EXP40/44/47) ----------
excluded48([reaches, based, stored]).

member_sig_48(E, Members, sig(O, I, ON, IN)) :-
    excluded48(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_48(N, Members, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_48(N, Members, A, B)), IN0),
    sort(IN0, IN).

deg_48(E, Members, O, I) :-
    excluded48(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

struct_sse_m48(Members, stsig(MS, E)) :-
    findall(M, (member(X, Members), member_sig_48(X, Members, M)), M0),
    sort(M0, MS),
    excluded48(Exclude),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

sig_clean_48(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

learn_map48 :-
    retractall(struct_def(_, _, _)),
    retractall(struct_sig(_, _)),
    retractall(rel_map(_, _, _)),
    assertz(struct_def(sa48, supply, [alba, book, oslo])),
    struct_sse_m48([alba, book, oslo], SigA),
    struct_sse_m48([pierre, tablet, paris], SigB),
    check(sig_clean_48(SigA), 'exemplar SSE zero atoms'),
    ( SigA == SigB ->
        check(true, 'completed structure recognized')
    ; format('GOT ~w WANT ~w~n', [SigB, SigA]),
      check(false, 'completed structure recognized')),
    assertz(struct_sig(supply, SigA)),
    induce_map_48([alba, book, oslo], [pierre, tablet, paris]),
    findall((A, B), rel_map(supply, A, B), Maps),
    sort(Maps, SM),
    format('probed map: ~w~n', [SM]),
    check(SM == [(belongs_to, held_by), (owns, keeps), (visits, tours)],
          'map induced after probing (nothing given)').

induce_map_48(ExM, BM) :-
    forall(( member(X, ExM),
             member_sig_48(X, ExM, SX),
             member(Y, BM),
             member_sig_48(Y, BM, SX),
             member(X2, ExM), member(Y2, BM),
             member_sig_48(X2, ExM, SX2),
             member_sig_48(Y2, BM, SX2),
             memory_relation(X, RA, X2, _, _),
             \+ excluded48_rel(RA),
             memory_relation(Y, RB, Y2, _, _),
             \+ excluded48_rel(RB)
           ),
           assertz(rel_map(supply, RA, RB))).

excluded48_rel(R) :-
    excluded48(E),
    member(R, E).

% ---------- stored por transferencia ----------
ask_stored48(K, C, yes(Proof)) :-
    rel_map(supply, belongs_to, RB1),
    rel_map(supply, visits, RB2),
    memory_relation(K, RB1, P, _, _),
    memory_relation(P, RB2, C, _, _),
    struct_sse_m48([P, K, C], Sig), struct_sig(supply, Sig),
    Proof = [reuse(supply, [P, K, C]), map([belongs_to-RB1, visits-RB2]),
             rule(stored, [RB1, RB2]), (K, RB1, P), (P, RB2, C)].
ask_stored48(_, _, unknown).

t_stored48(K, C, yes) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored48(K, C, A),
    say(A),
    remember_proof(A),
    ( A = yes(_) ->
        check(true, stored-yes)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored-yes)).
t_stored48(K, C, unknown) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored48(K, C, A),
    say(A),
    remember_proof(A),
    ( A = unknown ->
        check(true, stored-unknown)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored-unknown)).

% t/2, t_tell/2, remember_proof/1, say/1, check/2, report_checks/0
% heredados de conversation.pl / experiment46.pl.
