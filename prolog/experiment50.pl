% experiment50.pl
% EXPERIMENT 50 - CUMULATIVE BENCHMARK (una memoria, sin reinicio)
% 50-100 turnos equivalentes en 5 episodios sobre UNA memoria que solo
% crece: baseline corpus -> ontologia supply-A -> supply-B (mapa
% estable) -> reversal (rechazo) -> interferencia (nada se olvida).
% Mide por episodio: hechos, checks, ms. Sin umbrales de rendimiento:
% benchmark honesto (registra, no exige). Ningun modulo tocado.
:- consult('conversation.pl').
:- consult('experiment46.pl').

:- use_module(library(lists)).

:- dynamic ep_row/4.
:- dynamic struct_sig50/2.
:- dynamic rel_map50/2.

benchmark50 :-
    statistics(walltime, _),
    load_demo,
    statistics(walltime, [_, MsLoad]),
    facts_now(FLoad),
    assertz(ep_row(load_corpus_519, FLoad, 0, MsLoad)),
    format('row load_corpus_519 facts=~w newchecks=~w ms=~w~n',
           [FLoad, 0, MsLoad]),
    nl, writeln('===== BENCHMARK 50 (cumulative, no reset) ====='),
    episode(e0_baseline, ep0),
    episode(e1_supply_a, ep1),
    episode(e2_supply_b, ep2),
    episode(e3_reversal, ep3),
    episode(e4_no_forgetting, ep4),
    report_benchmark,
    report_checks.

episode(Name, Goal) :-
    facts_now(F0),
    statistics(walltime, _),
    checks_now(C0),
    ( call(Goal) -> true ; format('EPISODE FAIL ~w~n', [Name]) ),
    statistics(walltime, [_, Ms]),
    facts_now(F1),
    checks_now(C1),
    Pass is C1 - C0,
    assertz(ep_row(Name, F1, Pass, Ms)),
    format('row ~w facts=~w newchecks=~w ms=~w~n', [Name, F1, Pass, Ms]),
    check(F0 =< F1, monotonic_memory).

facts_now(N) :-
    findall(1, memory_relation(_, _, _, _, _), Fs),
    length(Fs, N).

checks_now(N) :-
    findall(1, check_results(_, pass), Ps),
    length(Ps, N).

% ---------- E0: baseline del corpus ----------
ep0 :-
    t("Does alba reach norway?", yes(_)),
    t("Is carla based in switzerland?", yes(_)),
    t("Does quinn reach ecuador?", no),
    t("Does zorin visit madrid?", unknown).

% ---------- E1: ontologia supply-A contada ----------
ep1 :-
    t_tell("Pierre keeps a tablet.", learned((pierre, keeps, tablet))),
    t_tell("The tablet is held by Pierre.",
           learned((tablet, held_by, pierre))),
    t_tell("Pierre tours Paris.", learned((pierre, tours, paris))),
    learn_map50([pierre, tablet, paris]),
    t_stored50(tablet, paris, yes),
    t("Why?", proof(_)).

% ---------- E2: segunda ontologia supply (mapa estable) ----------
ep2 :-
    t_tell("Mira keeps a mirror.", learned((mira, keeps, mirror))),
    t_tell("The mirror is held by Mira.",
           learned((mirror, held_by, mira))),
    t_tell("Mira tours Rome.", learned((mira, tours, rome))),
    learn_map50([mira, mirror, rome]),
    t_stored50(mirror, rome, yes).

% ---------- E3: reversal (rechazo, sin mapa) ----------
ep3 :-
    t_tell("Roma allures Nina.", learned((roma, allures, nina))),
    check(role_reversal50(allures), 'reversal detected (city subject)'),
    check(\+ rel_map50(_, allures), 'rejected pair never mapped'),
    t_stored50(nina, _, unknown),
    t("Why?", noproof(_)).

role_reversal50(RNew) :-
    memory_relation(S, RNew, O, _, _),
    memory_relation(_, visits, S, _, _),
    \+ memory_relation(_, visits, O, _, _), !.

% ---------- E4: nada se olvida ----------
ep4 :-
    t("Does alba reach norway?", yes(_)),
    t("Is carla based in switzerland?", yes(_)),
    t_stored50(tablet, paris, yes),
    t_stored50(mirror, rome, yes),
    t("Does quinn reach ecuador?", no).

% ---------- mapa (EXP47, autocontenido) ----------
excluded50([reaches, based, stored]).

member_sig_50(E, Members, sig(O, I, ON, IN)) :-
    excluded50(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_50(N, Members, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_50(N, Members, A, B)), IN0),
    sort(IN0, IN).

deg_50(E, Members, O, I) :-
    excluded50(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

struct_sse_m50(Members, stsig(MS, E)) :-
    findall(M, (member(X, Members), member_sig_50(X, Members, M)), M0),
    sort(M0, MS),
    excluded50(Exclude),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

% ejemplar A fijo: alba/book/oslo (entrenamiento conocido v2).
learn_map50(BMembers) :-
    struct_sse_m50([alba, book, oslo], SigA),
    struct_sse_m50(BMembers, SigB),
    ( SigA == SigB ->
        check(true, map_recognized)
    ; format('GOT ~w WANT ~w~n', [SigB, SigA]),
      check(false, map_recognized)),
    induce_map_50([alba, book, oslo], BMembers),
    findall((A, B), rel_map50(A, B), Maps),
    sort(Maps, SM),
    check(SM == [(belongs_to, held_by), (owns, keeps), (visits, tours)],
          map_stable).

induce_map_50(ExM, BM) :-
    retractall(rel_map50(_, _)),
    forall(( member(X, ExM),
             member_sig_50(X, ExM, SX),
             member(Y, BM),
             member_sig_50(Y, BM, SX),
             member(X2, ExM), member(Y2, BM),
             member_sig_50(X2, ExM, SX2),
             member_sig_50(Y2, BM, SX2),
             memory_relation(X, RA, X2, _, _),
             \+ excluded50_rel(RA),
             memory_relation(Y, RB, Y2, _, _),
             \+ excluded50_rel(RB)
           ),
           assertz(rel_map50(RA, RB))).

excluded50_rel(R) :-
    excluded50(E),
    member(R, E).

% ---------- stored por transferencia ----------
ask_stored50(K, C, yes(Proof)) :-
    rel_map50(belongs_to, RB1),
    rel_map50(visits, RB2),
    memory_relation(K, RB1, P, _, _),
    memory_relation(P, RB2, C, _, _),
    struct_sse_m50([P, K, C], Sig),
    struct_sse_m50([alba, book, oslo], Sig),
    Proof = [reuse(supply, [P, K, C]), map([belongs_to-RB1, visits-RB2]),
             rule(stored, [RB1, RB2]), (K, RB1, P), (P, RB2, C)].
ask_stored50(_, _, unknown).

t_stored50(K, C, yes) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored50(K, C, A),
    say(A),
    remember_proof(A),
    ( A = yes(_) ->
        check(true, stored50_yes)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored50_yes)).
t_stored50(K, C, unknown) :-
    format('> stored ~w ~w?~n', [K, C]),
    ask_stored50(K, C, A),
    say(A),
    remember_proof(A),
    ( A = unknown ->
        check(true, stored50_unknown)
    ; format('MISMATCH stored ~w ~w got ~w~n', [K, C, A]),
      check(false, stored50_unknown)).

% ---------- informe ----------
report_benchmark :-
    nl, writeln('===== COST TABLE ====='),
    forall(ep_row(N, F, P, Ms),
           format('~w: facts=~w checks=~w ms=~w~n', [N, F, P, Ms])).

% t/2, t_tell/2, remember_proof/1, say/1, check/2, report_checks/0
% heredados de conversation.pl / experiment46.pl.
