% experiment16.pl
% EXPERIMENT 16 - EMERGENT CONCEPTS FROM OPEN VOCABULARY
% Cero hechos de tipo en todo el experimento (ni is_a ni type/1).
% Entidades conocidas y desconocidas se agrupan SOLO por firma relacional.
% Held-out: "kadir lives in barcelona" omitida (kadir sigue PERSON @0.833).
% Anomalias: wex (solo eats), velara (solo objeto de visits).
:- consult('memory.pl').
:- consult('open_vocab.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment16 :-
    reset_experiment,
    feed_language,
    zero_types_check,
    discover_concepts,
    show_concepts,
    run_partition_tests,
    run_heldout_check,
    what_is(zorin),
    what_is(velara),
    what_is(wex).

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)).

% corpus: 14 frases (kadir-lives omitida = held-out; barcelona tiene a
% mia como segundo residente para no colapsar con velara)
sentences([
    "leo visits madrid",
    "leo eats bread",
    "leo lives in madrid",
    "mia visits paris",
    "mia eats cheese",
    "mia lives in paris",
    "mia lives in barcelona",
    "zorin visits madrid",
    "zorin eats bread",
    "zorin lives in madrid",
    "kadir visits barcelona",
    "kadir eats cheese",
    "leo visits velara",
    "wex eats bread"
]).

feed_language :-
    nl, writeln('===== LANGUAGE IN (14 sentences, 0 type facts) ====='),
    sentences(Ss),
    findall(T, ( member(S, Ss),
                 symbolize_open(S, T, usage)
               ),
            Ts),
    length(Ss, NS),
    length(Ts, NT),
    format('parsed ~w/~w~n', [NT, NS]),
    NT =:= NS,
    forall(member((S, V, O), Ts),
           remember_relation(S, V, O, 1.0)).

zero_types_check :-
    findall(1, memory_relation(_, is_a, _, _, _), L),
    length(L, 0),
    format('type facts in graph: 0 OK~n', []).

% ---------- partition esperada (umbral 0.80) ----------
% PERSON {leo,mia,zorin,kadir} CITY {madrid,barcelona,paris}
% FOOD {bread,cheese} ANOM {wex} {velara}
run_partition_tests :-
    nl, writeln('===== PARTITION TESTS ====='),
    check_members([leo, mia, zorin, kadir], "PERSON-like"),
    check_members([madrid, barcelona, paris], "CITY-like"),
    check_members([bread, cheese], "FOOD-like"),
    check_singleton(wex),
    check_singleton(velara),
    ( memory_relation(_, is_a, _, _, _) ->
        format('FAIL: type leak~n', []), fail
    ; format('no types anywhere OK~n', [])
    ).

check_members(Es, Label) :-
    findall(C, ( member(E, Es),
                 concept_member(C, E, _)
               ),
            Cs0),
    sort(Cs0, [C]),
    findall(M, concept_member(C, M, _), Ms0),
    sort(Ms0, Ms),
    sort(Es, EsS),
    ( Ms == EsS ->
        format('PASS ~w ~w = ~w~n', [Label, C, Ms])
    ; format('FAIL ~w: ~w has ~w, expected ~w~n', [Label, C, Ms, EsS]),
      fail
    ).

check_singleton(E) :-
    concept_member(C, E, _),
    findall(M, concept_member(C, M, _), [E]),
    format('PASS singleton ~w in ~w~n', [E, C]).

run_heldout_check :-
    nl, writeln('===== HELD-OUT (kadir lives strongly omitted) ====='),
    concept_member(C, kadir, _),
    concept_member(C, leo, _),
    format('kadir still with leo in ~w OK~n', [C]).

% ---------- "que es X?" por evidencia relacional ----------
what_is(X) :-
    nl, format('===== WHAT IS ~w? =====~n', [X]),
    concept_member(C, X, _),
    concept(C, Sig, _),
    findall(M, concept_member(C, M, _), Ms0),
    sort(Ms0, Ms),
    format('~w pertenece a ~w por firma ~w~n', [X, C, Sig]),
    format('co-miembros: ~w~n', [Ms]),
    forall(memory_relation(X, R, O, _, _),
           format('evidencia: ~w --~w--> ~w~n', [X, R, O])),
    forall(memory_relation(S, R, X, _, _),
           format('evidencia: ~w --~w--> ~w~n', [S, R, X])),
    findall(1, memory_relation(X, is_a, _, _, _), L),
    length(L, 0),
    format('tipos asertados: ninguno (0 is_a)~n', []).

% --- descubrimiento (umbral 0.80, idempotente) ---

entity(E) :- memory_relation(E, _, _, _, _).
entity(E) :- memory_relation(_, _, E, _, _).

discover_concepts :-
    findall(E, entity(E), E0),
    sort(E0, Es),
    forall(member(E, Es), assign_concept(E)).

assign_concept(E) :-
    entity_signature(E, Sig),
    findall(Sc-C,
            (concept(C, CSig, _), signature_similarity(Sig, CSig, Sc)),
            Ms),
    best_concept(Ms, Best, BC),
    ( Best >= 0.80 -> add_member(BC, E, Best)
    ; create_concept(E, Sig)
    ).

best_concept([], 0.0, none).
best_concept(Ms, Sc, C) :-
    keysort(Ms, S), reverse(S, [Sc-C|_]).

create_concept(E, Sig) :-
    findall(N, concept(concept(N), _, _), Ns),
    next_concept_number(Ns, N),
    C = concept(N),
    assertz(concept(C, Sig, 1)),
    assertz(concept_member(C, E, 1.0)).

add_member(C, E, _) :-
    concept_member(C, E, _), !.
add_member(C, E, Sc) :-
    assertz(concept_member(C, E, Sc)).

next_concept_number([], 1).
next_concept_number(Ns, N) :- max_list(Ns, M), N is M + 1.

entity_signature(E, signature(S, O)) :-
    findall(R, memory_relation(E, R, _, _, _), S0),
    findall(R, memory_relation(_, R, E, _, _), O0),
    sort(S0, S), sort(O0, O).

signature_similarity(signature(S1, O1), signature(S2, O2), Sc) :-
    jaccard(S1, S2, A), jaccard(O1, O2, B),
    Sc is (A + B) / 2.

jaccard([], [], 1.0) :- !.
jaccard(A, B, Sc) :-
    append(A, B, C), sort(C, U),
    intersection(A, B, I),
    length(U, LU), length(I, LI),
    ( LU =:= 0 -> Sc = 0.0 ; Sc is LI / LU ).

show_concepts :-
    nl, writeln('===== DISCOVERED CONCEPTS ====='),
    forall(concept(C, Sig, _),
           ( format('~w  ~w~n', [C, Sig]),
             forall(concept_member(C, E, Sc),
                    format('   ~w ~2f~n', [E, Sc])),
             nl
           )).
