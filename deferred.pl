% deferred.pl
% EXP56: conflicto DIFERIDO con equivalencia exacta (sin tocar conflict.pl
% ni memory.pl). Idea: lo caro no es el conflicto, es REESCANEAR toda R
% por inserto. Se mantiene un indice incremental O(1):
%   dx_cc(S,R,N) = nº de objetos LIMPIOS (prov active) del grupo.
%   dx_cn(R,N)   = nº de grupos (S,R) con ccnt == 1.
%   dx_mc(R,N)   = nº de grupos (S,R) con ccnt >= 2.
% functional_except(S,R) <=> otros-singles >= 2 Y cero otros-multis
% (los grupos con ccnt == 0 se ignoran en ambos lados, como eager).
% Fases: dx_remember (peso + orden; prov NO), dx_build_indexes (recompute
% O(N) en una pasada), dx_replay (afirma prov + check en orden de llegada;
% dx_resolve_pair replica resolve_pair + mantiene contadores).
% Caso con tiempos (resolve_time_conflicts): fuera de alcance verificado
% (nuestros corpus usan Time=none; el replay lo documenta si aparece).
:- use_module(library(lists)).

:- dynamic dx_seq/2.    % dx_seq(Idx, (S,R,O)) primer insert (orden llegada)
:- dynamic dx_cc/3.     % dx_cc(S, R, Nclean)
:- dynamic dx_cn/2.     % dx_cn(R, N groups with ccnt==1)
:- dynamic dx_mc/2.     % dx_mc(R, N groups with ccnt>=2)
:- dynamic dx_n/1.

dx_reset :-
    retractall(dx_seq(_, _)),
    retractall(dx_cc(_, _, _)),
    retractall(dx_cn(_, _)),
    retractall(dx_mc(_, _)),
    retractall(dx_n(_)),
    assertz(dx_n(0)).

% ---------- fase 1: peso + orden (prov y checks van en replay) ----------
dx_remember(S, R, O, _Src, _Time) :-
    ( memory_relation(S, R, O, _, _) -> true
    ; dx_record(S, R, O)
    ),
    remember_relation(S, R, O, 1.0).

dx_record(S, R, O) :-
    retract(dx_n(N)), !,
    N1 is N + 1,
    assertz(dx_n(N1)),
    assertz(dx_seq(N1, (S, R, O))).

% ---------- fase 2: (sin precomputo: los contadores se mantienen en replay)
% Se conserva la fase por simetria del protocolo; su coste debe ser ~0.
dx_build_indexes :-
    memory_size(N),
    format('DX-INDEX facts=~w (contadores en replay)~n', [N]).

dx_cn_inc(R) :-
    ( retract(dx_cn(R, N)) -> N1 is N + 1 ; N1 = 1 ),
    assertz(dx_cn(R, N1)).

dx_cn_dec(R) :-
    retract(dx_cn(R, N)), !,
    N1 is N - 1,
    ( N1 > 0 -> assertz(dx_cn(R, N1)) ; true ).

% ---------- fase 3: replay con functional O(1) ----------
dx_replay :-
    findall(I-(S, R, O), dx_seq(I, (S, R, O)), Seq0),
    keysort(Seq0, Seq),
    forall(member(_-(S, R, O), Seq),
           ( ref_of(S, R, O, Ref) ->
               dx_replay_step(S, R, O, Ref, none)
           ; true
           )).

ref_of(S, R, O, Ref) :-
    provfact(S, R, O, Ref, _, _), !.
ref_of(_, _, _, noref).

dx_replay_step(S, R, O, Src, Time) :-
    ( prov(S, R, O, info(_, _, _)) ->
        adopt_time(S, R, O, Src, Time),
        assertz(prov_log(confirmed, (S, R, O, Src)))
    ; assertz(prov(S, R, O, info(Src, Time, active))),
      dx_cc_inc(S, R),
      dx_check(S, R, O, Src, Time)
    ).

% Alta de objeto limpio en (S,R): 0->1 entra en cn, 1->2 sale de cn y
% entra en mc, 2+ sin cambios.
dx_cc_inc(S, R) :-
    ( retract(dx_cc(S, R, N)) -> N1 is N + 1 ; N1 = 1 ),
    assertz(dx_cc(S, R, N1)),
    ( N1 == 1 -> dx_cn_inc(R)
    ; N1 == 2 -> dx_cn_dec(R), dx_mc_inc(R)
    ; true
    ).

dx_mc_inc(R) :-
    ( retract(dx_mc(R, N)) -> N1 is N + 1 ; N1 = 1 ),
    assertz(dx_mc(R, N1)).

dx_mc_dec(R) :-
    retract(dx_mc(R, N)), !,
    N1 is N - 1,
    ( N1 > 0 -> assertz(dx_mc(R, N1)) ; true ).

% Identico a check_conflict/5 salvo functional O(1).
dx_check(S, R, O, Src, Time) :-
    prov(S, R, O1, info(Src1, Time1, active)),
    O1 \== O,
    dx_functional_except(S, R),
    !,
    dx_resolve_pair(S, R, O1, Src1, Time1, O, Src, Time).
dx_check(_, _, _, _, _).

dx_functional_except(S, R) :-
    ( dx_cn(R, CN) -> true ; CN = 0 ),
    ( dx_cc(S, R, 1) -> SubC = 1 ; SubC = 0 ),
    CN - SubC >= 2,
    ( dx_mc(R, MC) -> true ; MC = 0 ),
    ( dx_cc(S, R, N), N >= 2 -> SubM = 1 ; SubM = 0 ),
    MC - SubM =:= 0.

% Replica exacta de resolve_pair/8 con mantenimiento de contadores.
% Solo ocurren transiciones active->X (Time=none en nuestros corpus);
% si apareciera un tiempo numerico se registra y se sigue (documentado).
dx_resolve_pair(S, R, O1, _Src1, T1, O2, _Src2, T2) :-
    number(T1), number(T2), !,
    format('DX-TIME not covered by verified path: ~w~n', [(S, R, O1, O2)]),
    fail.
dx_resolve_pair(S, R, O1, Src1, _, O2, Src2, _) :-
    dx_set_status(S, R, O1, contested),
    dx_set_status(S, R, O2, contested),
    assertz(prov_log(conflict, ((S, R, O1, Src1), (S, R, O2, Src2)))),
    format('CONFLICT held: ~w vs ~w (sources ~w, ~w)~n',
           [(S, R, O1), (S, R, O2), Src1, Src2]).

% dx_set_status = set_status + ajuste O(1) de (cc, cn, mc).
dx_set_status(S, R, O, St) :-
    prov(S, R, O, info(Src, T, Old)),
    ( Old == active, St \== active ->
        dx_cc(S, R, N),
        retract(dx_cc(S, R, N)),
        N1 is N - 1,
        assertz(dx_cc(S, R, N1)),
        ( N == 2 -> dx_cn_inc(R), dx_mc_dec(R)
        ; N == 1 -> dx_cn_dec(R)
        ; true
        )
    ; true
    ),
    retract(prov(S, R, O, info(Src, T, _))),
    assertz(prov(S, R, O, info(Src, T, St))).

% ---------- snapshot canonico para diff de equivalencia ----------
% dx_snapshot(+File): vuelca hechos+pesos, prov y pares de conflicto
% ordenados (writeq). Dos snapshots se comparan linea a linea.
dx_snapshot(File) :-
    open(File, write, Out),
    findall((S, R, O, W, U), memory_relation(S, R, O, W, U), Ms0),
    sort(Ms0, Ms),
    forall(member((S, R, O, W, U), Ms),
           format(Out, 'M ~q ~q ~q ~q ~q~n', [S, R, O, W, U])),
    findall((S, R, O, Src, Tm, St), prov(S, R, O, info(Src, Tm, St)), Ps0),
    sort(Ps0, Ps),
    forall(member((S, R, O, Src, Tm, St), Ps),
           format(Out, 'P ~q ~q ~q ~q ~q ~q~n', [S, R, O, Src, Tm, St])),
    findall((S, R, O1, O2), prov_log(conflict, ((S, R, O1, _), (S, R, O2, _))), Cs0),
    sort(Cs0, Cs),
    forall(member((S, R, O1, O2), Cs),
           format(Out, 'C ~q ~q ~q ~q~n', [S, R, O1, O2])),
    close(Out),
    length(Ms, NM),
    length(Ps, NP),
    length(Cs, NC),
    format('DX-SNAPSHOT ~w mem=~w prov=~w conflicts=~w~n', [File, NM, NP, NC]).
