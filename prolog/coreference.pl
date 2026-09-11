% coreference.pl
% Abduccion de identidad linguistica (EXP7 + EXP17).
% Cada pronombre es un nodo provisional propio; la resolucion evalua
% candidatos con traza auditable: veto de genero, veto funcional
% (contradiccion hipotetica), y jamas elige al azar:
%   1 superviviente -> SAME + reconstruccion del grafo
%   0 -> UNKNOWN (refiere a entidad no mencionada; se conserva el nodo)
%   2+ -> UNKNOWN (ambiguo; se conserva el nodo)
% Los pronombres nunca son candidatos entre si (sin encadenamiento).
:- consult('identity.pl').
:- use_module(library(lists)).

:- dynamic resolution_verdict/3.
% resolution_verdict(Mention, same(Survivor) | unknown, Reason)

% person_role: sujetos de verbos de persona (disciplina de candidatos).
person_role(E) :-
    memory_relation(E, R, _, _, _),
    member(R, [visits, eats, lives_in, reaches]).

% gender_compatible(+Mention, +Candidate)
gender_compatible(M, _) :-
    pronoun_mention(M, _, _),
    \+ ( pronoun_mention(M, masc, _) ; pronoun_mention(M, fem, _) ).
gender_compatible(M, C) :-
    pronoun_mention(M, masc, _),
    \+ gender(C, fem).
gender_compatible(M, C) :-
    pronoun_mention(M, fem, _),
    \+ gender(C, masc).

% candidates(+Mention, -Candidates): entidades rol-persona, no pronombres,
% compatibles en genero. Sin trucos de recencia ni de orden.
candidates(M, Cs) :-
    findall(C, ( person_role(C),
                 C \== M,
                 \+ pronoun_mention(C, _, _),
                 gender_compatible(M, C)
               ),
            C0),
    sort(C0, Cs).

% hypothetical contradiction: perfil de P mapeado a C viola funcionalidad.
% R se enumera ANTES de puntuar (ver bug EXP17: R libre mezcla todo).
% La funcionalidad se mide SIN pronombres: el dato unico del pronombre
% no puede establecer funcionalidad ni envenenar el test (circularidad).
hypo_contradicts(P, C, R) :-
    all_relations(Rs),
    member(R, Rs),
    coref_functional_candidate(R),
    findall(O, hypo_out(P, C, R, O), O0),
    sort(O0, Objs),
    length(Objs, N),
    N >= 2.

hypo_out(P, C, R, O2) :-
    memory_relation(S, R, O, _, _),
    map_node(P, C, S, S2),
    map_node(P, C, O, O2),
    ( S2 == C ; O2 == C ),
    \+ ( S2 == P ; O2 == P ).

map_node(P, C, P, C) :- !.
map_node(P, C, X, X) :-
    X \== P, X \== C, !.
map_node(_, _, X, X).

% gender-vetoed candidates (for audit trace; filtered before survivors).
gender_vetoed(P, Vs) :-
    findall(C, ( person_role(C),
                 C \== P,
                 \+ pronoun_mention(C, _, _),
                 \+ gender_compatible(P, C)
               ),
            V0),
    sort(V0, Vs).

% coref_functional_candidate: como functional_candidate pero ignorando
% nodos pronominales (evidencia de terceros, no del propio hipotetico).
coref_functional_candidate(R) :-
    findall(S, ( memory_relation(S, R, _, _, _),
                 \+ pronoun_mention(S, _, _)
               ),
            S0),
    sort(S0, Ss),
    length(Ss, N),
    N >= 2,
    forall(member(S, Ss),
           ( findall(O, memory_relation(S, R, O, _, _), O0),
             sort(O0, [_])
           )).
survivors(P, Survs, Vetos) :-
    candidates(P, Cs),
    findall(C, ( member(C, Cs),
                 \+ hypo_contradicts(P, C, _)
               ),
            Survs),
    findall(vetoed(C, functional(R)),
            ( member(C, Cs),
              hypo_contradicts(P, C, R)
            ),
            Vetos).

resolved(P) :-
    same_closed(P, C),
    C \== P, !.

% resolve_mention(+P): decide y actua.
resolve_mention(P) :-
    resolved(P), !.
resolve_mention(P) :-
    retractall(resolution_verdict(P, _, _)),
    survivors(P, Survs, Vetos),
    ( Survs = [C] ->
        remember_identity(P, C),
        reconstruct(C, P, N),
        assertz(resolution_verdict(P, same(C), [single_survivor|Vetos])),
        format('COREF ~w == ~w (rewrote ~w facts)~n', [P, C, N])
    ; Survs = [] ->
        gender_vetoed(P, GV),
        assertz(resolution_verdict(P, unknown, [no_survivor,
                                                gender_vetoed(GV)|Vetos])),
        format('COREF ~w UNKNOWN (no survivor)~n', [P])
    ; assertz(resolution_verdict(P, unknown, [ambiguous(Survs)|Vetos])),
      format('COREF ~w UNKNOWN (ambiguous ~w, never random)~n', [P, Survs])
    ).

% reconstruct: reescribe hechos del pronombre al canonico (con localidad).
reconstruct(C, P, N) :-
    findall((S, V, O), ( memory_relation(S, V, O, _, _),
                         ( S == P ; O == P )
                       ),
            Fs),
    reconstruct_facts(C, P, Fs, 0, N).

reconstruct_facts(_, _, [], N, N).
reconstruct_facts(C, P, [(S, V, O)|Fs], A, N) :-
    map_node(P, C, S, S2),
    map_node(P, C, O, O2),
    ( memory_relation(S2, V, O2, _, _) -> A1 = A
    ; assertz(memory_relation(S2, V, O2, 1.0, 1)), A1 is A + 1
    ),
    ( (S2, V, O2) == (S, V, O) -> A2 = A1
    ; retract(memory_relation(S, V, O, _, _)), A2 is A1 + 1
    ),
    reconstruct_facts(C, P, Fs, A2, N).

% retry_unresolved: la evidencia nueva puede resolver lo pendiente.
retry_unresolved :-
    findall(P, ( pronoun_mention(P, _, _),
                 \+ resolved(P)
               ),
            Ps),
    forall(member(P, Ps), resolve_mention(P)).

% verdict(+P, -V, -Reason)
verdict(P, V, R) :-
    resolution_verdict(P, V, R), !.
verdict(_, unresolved, []).

% pronoun node consumed: no quedan hechos con el (resueltas OK).
node_consumed(P) :-
    \+ memory_relation(P, _, _, _, _),
    \+ memory_relation(_, _, P, _, _).
