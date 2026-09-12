% exp56.pl — EXP56: conflicto diferido vs eager + equivalencia exacta.
% Uso: swipl -s exp56.pl -g exp56 -t halt
% Prefijos anidados de alice_memory.pl (orden de fichero = llegada):
% N = [500,1000,1500,2000,2900]. Por N: eager (remember_tracked) vs
% deferred (dx_remember + build + replay), con snapshot canonico.
% La comparacion de equivalencia la hace un diff independiente (python).
:- consult('corpus.pl').
:- consult('deferred.pl').

:- use_module(library(lists)).

exp56 :-
    consult('alice_memory.pl'),
    findall((S, R, O), memfact(S, R, O, _, _), All),
    length(All, Total),
    format('E56 total=~w~n', [Total]),
    forall(member(N, [500, 1000, 1500, 2000, 2900]),
           ( prefix(All, N, Pre),
             eager_run(Pre, N),
             deferred_run(Pre, N)
           )),
    writeln('E56-DONE').

prefix(All, N, Pre) :-
    length(Pre, N),
    append(Pre, _, All).

e56_reset :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    dx_reset.

% ref_of/4 vive en deferred.pl (misma definicion).

eager_run(Pre, N) :-
    e56_reset,
    get_time(T0),
    forall(member((S, R, O), Pre),
           ( ref_of(S, R, O, Ref),
             remember_tracked(S, R, O, Ref, none)
           )),
    get_time(T1),
    Ms is round((T1 - T0) * 1000),
    memory_size(NF),
    format(atom(F), 'e56_eager_~w.txt', [N]),
    dx_snapshot(F),
    format('E56-EAGER n=~w memfacts=~w ms=~w~n', [N, NF, Ms]).

deferred_run(Pre, N) :-
    e56_reset,
    get_time(T0),
    forall(member((S, R, O), Pre),
           ( ref_of(S, R, O, Ref),
             dx_remember(S, R, O, Ref, none)
           )),
    get_time(T1),
    MsIns is round((T1 - T0) * 1000),
    get_time(T2),
    dx_build_indexes,
    get_time(T3),
    MsIdx is round((T3 - T2) * 1000),
    get_time(T4),
    dx_replay,
    get_time(T5),
    MsRep is round((T5 - T4) * 1000),
    MsTot is MsIns + MsIdx + MsRep,
    memory_size(NF),
    format(atom(F), 'e56_deferred_~w.txt', [N]),
    dx_snapshot(F),
    format('E56-DEFERRED n=~w memfacts=~w insert=~w index=~w replay=~w total=~w~n',
           [N, NF, MsIns, MsIdx, MsRep, MsTot]).
