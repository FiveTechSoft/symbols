% identity.pl
% Identidad de entidades: SAME / DIFFERENT / UNKNOWN / CONTRADICTION.
% - SAME solo por declaracion same_as (clausura simetrica+transitiva).
%   Misma estructura NUNCA implica misma entidad (EXP17.5).
% - DIFFERENT: perfiles canonicos distintos (mundo con descripcion completa).
% - UNKNOWN: perfiles identicos sin declaracion. UNKNOWN \== DIFFERENT.
% - CONTRADICTION: entidad fusionada con 2 objetos en relacion funcional.
% - Funcionalidad DESCUBIERTA (nunca programada): todos los sujetos con
%   exactamente 1 objeto distinto, con >= 2 sujetos observados.
:- use_module(library(lists)).

:- dynamic same_as_link/2.

remember_identity(X, Y) :-
    same_as_link(X, Y), !.
remember_identity(X, Y) :-
    same_as_link(Y, X), !.
remember_identity(X, Y) :-
    assertz(same_as_link(X, Y)).

same_edge(A, B) :- same_as_link(A, B).
same_edge(A, B) :- same_as_link(B, A).

same_closed(A, B) :-
    same_reach(A, B, [A]).

same_reach(A, A, _).
same_reach(A, B, Visited) :-
    same_edge(A, M),
    \+ member(M, Visited),
    same_reach(M, B, [M|Visited]).

% canonical: representante minimo de la clase (incluye al propio E).
canonical(E, Rep) :-
    findall(M, ( same_closed(E, M) ; E = M ), M0),
    sort(M0, [Rep|_]).

% merged_profile: hechos con extremos canonizados.
merged_profile(E, Profile) :-
    canonical(E, _),
    findall(P, merged_fact(E, P), F0),
    sort(F0, Profile).

merged_fact(E, out(R, O2)) :-
    memory_relation(S, R, O, _, _),
    canonical(S, RS),
    canonical(E, RE),
    RS == RE,
    canonical(O, O2).
merged_fact(E, in(S2, R)) :-
    memory_relation(S, R, O, _, _),
    canonical(O, RO),
    canonical(E, RE),
    RO == RE,
    canonical(S, S2).

% functionality descubierta por relacion
functionality_score(R, Score, NSubjects) :-
    findall(S, memory_relation(S, R, _, _, _), S0),
    sort(S0, Ss),
    length(Ss, NSubjects),
    ( NSubjects =:= 0 -> Score = 0.0
    ; findall(S, ( member(S, Ss),
                   findall(O, memory_relation(S, R, O, _, _), O0),
                   sort(O0, [_])
                 ),
              Single),
      length(Single, NS),
      Score is NS / NSubjects
    ).

functional_candidate(R) :-
    functionality_score(R, Sc, N),
    N >= 2,
    Sc >= 0.999.

% contradicted(+Rep, -R, -Objs): R ligada ANTES de puntuar (si no,
% functionality_score con R libre mezcla relaciones distintas).
contradicted_entity(Rep, R, Objs) :-
    canonical(Rep, Rep),
    all_relations(Rs),
    member(R, Rs),
    functional_candidate(R),
    merged_profile(Rep, P),
    findall(O, member(out(R, O), P), O0),
    sort(O0, Objs),
    length(Objs, N),
    N >= 2.

all_relations(Rs) :-
    findall(R, memory_relation(_, R, _, _, _), R0),
    sort(R0, Rs).

contradicted(E) :-
    canonical(E, Rep),
    contradicted_entity(Rep, _, _).

% identity_status(+A, +B, -Status)
identity_status(A, B, contradiction) :-
    ( contradicted(A) ; contradicted(B) ), !.
identity_status(A, B, same) :-
    canonical(A, RA),
    canonical(B, RB),
    RA == RB, !.
identity_status(A, B, different) :-
    merged_profile(A, PA),
    merged_profile(B, PB),
    PA \== PB, !.
identity_status(_, _, unknown).

show_functionality :-
    nl, writeln('--- discovered relation functionality ---'),
    findall(R, memory_relation(_, R, _, _, _), R0),
    sort(R0, Rs),
    forall(member(R, Rs),
           ( functionality_score(R, Sc, N),
             ( functional_candidate(R) -> F = functional
             ; F = non_functional
             ),
             format('~w: score=~2f subjects=~w (~w)~n', [R, Sc, N, F])
           )).
