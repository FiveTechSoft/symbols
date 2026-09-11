% conflict.pl
% Creencia con estado: cada hecho lleva provenance (fuente, tiempo, estado).
% Estados: active | contested | superseded. Aprender tambien es revisar:
% confirmacion (refuerzo), contradiccion (contested, sin elegir),
% tiempo (el ultimo gana, supersede), fuentes en conflicto (pending).
% Funcionalidad LOCAL: solo otros sujetos con hechos limpios (>= 2) deciden.
:- use_module(library(lists)).

:- dynamic prov/4.
% prov(S, R, O, info(Src, Time, Status))
:- dynamic prov_log/2.
% prov_log(event, detail)

% remember_tracked(+S, +R, +O, +Src, +Time)
remember_tracked(S, R, O, Src, Time) :-
    remember_relation(S, R, O, 1.0),
    ( prov(S, R, O, info(_, _, _)) ->
        adopt_time(S, R, O, Src, Time),
        assertz(prov_log(confirmed, (S, R, O, Src)))
    ; assertz(prov(S, R, O, info(Src, Time, active))),
      check_conflict(S, R, O, Src, Time)
    ).

adopt_time(_S, _R, _O, _Src, none) :- !.
adopt_time(S, R, O, Src, T) :-
    retract(prov(S, R, O, info(OldSrc, _, St))),
    assertz(prov(S, R, O, info(OldSrc, T, St))),
    assertz(prov_log(time_evidence, ((S, R, O), Src, T))).

% functional_except: todos los OTROS sujetos limpios con 1 objeto (>= 2).
functional_except(S, R) :-
    findall(X, ( memory_relation(X, R, _, _, _),
                 X \== S,
                 clean_active(X, R, _)
               ),
            X0),
    sort(X0, Xs),
    length(Xs, N),
    N >= 2,
    forall(member(X, Xs),
           ( findall(O, clean_active(X, R, O), O0),
             sort(O0, [_])
           )).

clean_active(S, R, O) :-
    memory_relation(S, R, O, _, _),
    prov(S, R, O, info(_, _, active)).

check_conflict(S, R, O, Src, Time) :-
    prov(S, R, O1, info(Src1, Time1, active)),
    O1 \== O,
    functional_except(S, R),
    !,
    resolve_pair(S, R, O1, Src1, Time1, O, Src, Time).
check_conflict(_, _, _, _, _).

% resolve_pair: tiempo decide; sin tiempo, contested (no se elige).
resolve_pair(S, R, O1, _Src1, T1, O2, _Src2, T2) :-
    number(T1), number(T2), T2 > T1, !,
    set_status(S, R, O1, superseded),
    assertz(prov_log(superseded, ((S, R, O1), by((S, R, O2))))),
    format('TIME: ~w kept, ~w superseded~n', [(S, R, O2), (S, R, O1)]).
resolve_pair(S, R, O1, _Src1, T1, O2, _Src2, T2) :-
    number(T1), number(T2), T1 > T2, !,
    set_status(S, R, O2, superseded),
    assertz(prov_log(superseded, ((S, R, O2), by((S, R, O1))))),
    format('TIME: late old news ~w superseded on arrival~n', [(S, R, O2)]).
resolve_pair(S, R, O1, Src1, _, O2, Src2, _) :-
    set_status(S, R, O1, contested),
    set_status(S, R, O2, contested),
    assertz(prov_log(conflict, ((S, R, O1, Src1), (S, R, O2, Src2)))),
    format('CONFLICT held: ~w vs ~w (sources ~w, ~w)~n',
           [(S, R, O1), (S, R, O2), Src1, Src2]).

set_status(S, R, O, St) :-
    retract(prov(S, R, O, info(Src, T, _))),
    assertz(prov(S, R, O, info(Src, T, St))).

% resolve_time_conflicts: pares contested con tiempos comparables.
resolve_time_conflicts :-
    prov(S, R, O1, info(_, T1, contested)),
    prov(S, R, O2, info(_, T2, contested)),
    O1 @< O2,
    number(T1), number(T2),
    ( T1 \== T2 ->
        ( T2 > T1 ->
            set_status(S, R, O1, superseded),
            set_status(S, R, O2, active),
            assertz(prov_log(superseded, ((S, R, O1), by((S, R, O2)))))
        ; set_status(S, R, O2, superseded),
          set_status(S, R, O1, active),
          assertz(prov_log(superseded, ((S, R, O2), by((S, R, O1)))))
        ),
        assertz(prov_log(resolved_by_time, ((S, R, O1, T1),
                                            (S, R, O2, T2)))),
        format('RESOLVED: latest wins for ~w --~w--> (~w vs ~w)~n',
               [S, R, O1, O2]),
        fail
    ; true
    ),
    !.
resolve_time_conflicts.

% current_belief: solo hechos activos.
current_belief(S, R, O) :-
    prov(S, R, O, info(_, _, active)).

% why_belief(+S, +R, +O, -Why): provenance + historial de superacion.
why_belief(S, R, O, [source(Src), time(T), status(active),
                     time_evidence(TEv), superseded(Hist)]) :-
    prov(S, R, O, info(Src, T, active)),
    findall((S, R, OO),
            prov_log(superseded, ((S, R, OO), by((S, R, O)))),
            Hist),
    findall(((S, R, O), S2, T2),
            prov_log(time_evidence, ((S, R, O), S2, T2)),
            TEv).
