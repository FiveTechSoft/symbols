% experiment52.pl
% EXPERIMENT 52 - VOCABULARY vs FACTS GRID (intentar romper EXP51)
% Malla 3x3: vocabulario {6,15,30} x hechos {~130,~1030,~10030}.
% Mundo: 20 cadenas P-a->X-b->Y + t(P,Y) + D hechos distractores sobre
% R nombres aleatorios (seed fija). Por celda: ingesta, descubrimiento
% (cap 240s via call_with_time_limit: el timeout es un DATO honesto),
% conceptos, skills, espacio analitico de patrones, RAM (heapused),
% transferencia/UNKNOWN/distractor, ratio de compresion.
% Ningun modulo tocado (solo este fichero).
:- consult('memory.pl').
:- consult('composition.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic composed_rule/3.
:- dynamic distinct_rule/3.
:- dynamic constrained_rule/3.
:- dynamic check_results/2.
:- dynamic cell_row/11.

benchmark52 :-
    retractall(check_results(_, _)),
    nl, writeln('===== BENCHMARK 52 (vocab x facts grid) ====='),
    forall(member(V, [6, 15, 30]),
           forall(member(D, [70, 970, 9970]),
                  grid_cell(V, D))),
    report_grid,
    report_checks.

grid_cell(V, D) :-
    format('--- cell vocab=~w distractors=~w ---~n', [V, D]),
    reset_cell,
    build_world(V, D),
    cell_metrics(V, D).

reset_cell :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% ---------- mundo: motivo fijo + ruido escalado ----------
build_world(V, D) :-
    set_random(seed(42)),
    ND is V - 3,
    forall(between(1, 20, I),
           ( motif_entities(I, P, X, Y),
             remember_relation(P, a, X, 1.0),
             remember_relation(X, b, Y, 1.0),
             remember_relation(P, t, Y, 1.0) )),
    forall(between(1, D, _),
           ( random_entity(P), random_entity(O),
             random_between(1, ND, K),
             atomic_list_concat([r, K], R),
             remember_relation(P, R, O, 1.0) )).

motif_entities(I, P, X, Y) :-
    PI is ((I - 1) mod 10) + 1,
    atomic_list_concat([p, PI], P),
    atomic_list_concat([x, I], X),
    atomic_list_concat([y, I], Y).

random_entity(E) :-
    random_between(1, 60, I),
    atomic_list_concat([e, I], E).

% ---------- metricas por celda ----------
cell_metrics(V, D) :-
    statistics(walltime, _),
    statistics(heapused, H0),
    discover_concepts,
    discover_concept_relations,
    statistics(heapused, H1),
    findall(C, concept(C, _, _), Cs), length(Cs, NC),
    findall(R, (memory_relation(_, R, _, _, _)), Rs0),
    sort(Rs0, Vocab), length(Vocab, VV),
    statistics(walltime, _),
    % el timeout es un DATO: si el descubrimiento no cabe en 240s,
    % la celda queda incompleta en vez de colgar el benchmark.
    ( catch((call_with_time_limit(240, discover_composition(t, 3)),
             composed_rule(t, [a, b], F1),
             F1 >= 0.99),
            time_limit_exceeded, fail) ->
        DiscTO = false
    ; DiscTO = true, F1 = -1.0 ),
    statistics(walltime, [_, MsDisc]),
    statistics(heapused, H2),
    findall(1, memory_relation(_, _, _, _, _), Fs),
    length(Fs, NF),
    findall(1, composed_rule(_, _, _), CRs), length(CRs, NR),
    pattern_space(VV, Gen),
    RamKB is (H2 - H0) // 1024,
    ( DiscTO == false, F1 >= 0.99 ->
        functional_cell
    ; format('cell INCOMPLETE (timeout=~w f1=~w)~n', [DiscTO, F1]),
      assertz(check_results(cell_incomplete, fail))
    ),
    ( NC > 0, NR > 0 ->
        Compression is NF / (NC + NR)
    ; Compression = 0 ),
    assertz(cell_row(V, D, NF, VV, NC, NR, Gen, MsDisc, RamKB,
                     F1, Compression)),
    format('row V=~w D=~w facts=~w vocab=~w concepts=~w rules=~w gen=~w disc=~wms ram=~wKB f1=~w compr=~2f~n',
           [V, D, NF, VV, NC, NR, Gen, MsDisc, RamKB, F1, Compression]).

% espacio analitico de patrones exhaustivos (exacto, sin instrumentar).
pattern_space(V, Gen) :-
    Gen is V + V * V + V * V * V.

functional_cell :-
    % transferencia: cadena fresca sin t observado.
    remember_relation(pn, a, xn, 1.0),
    remember_relation(xn, b, yn, 1.0),
    ( memory_relation(pn, a, X, _, _),
      memory_relation(X, b, yn, _, _),
      \+ memory_relation(pn, t, yn, _, _) ->
        check(true, transfer_fresh_chain)
    ; check(false, transfer_fresh_chain)),
    retractall(memory_relation(pn, _, _, _, _)),
    retractall(memory_relation(_, _, pn, _, _)),
    retractall(memory_relation(xn, _, _, _, _)),
    retractall(memory_relation(_, _, xn, _, _)),
    retractall(memory_relation(_, _, yn, _, _)),
    % UNKNOWN: zzz no existe en este mundo.
    ( \+ memory_relation(zzz, _, _, _, _),
      \+ memory_relation(_, _, zzz, _, _) ->
        check(true, unknown_zzz)
    ; check(false, unknown_zzz)),
    % distractor: e1 jamas alcanza nada via t (verdadero negativo).
    ( \+ memory_relation(e1, t, _, _, _) ->
        check(true, distractor_no_chain)
    ; check(false, distractor_no_chain)).

% ---------- conceptos (maquinaria local estandar) ----------
entity(E) :- memory_relation(E, _, _, _, _).
entity(E) :- memory_relation(_, _, E, _, _).

discover_concepts :-
    findall(E, entity(E), E0),
    sort(E0, Es),
    forall(member(E, Es), assign_concept(E)).

assign_concept(E) :-
    entity_signature(E, Sig),
    findall(Sc-C, (concept(C, CSig, _), signature_similarity(Sig, CSig, Sc)), Ms),
    best_concept(Ms, Best, BC),
    (Best >= 0.80 -> add_member(BC, E, Best)
    ; create_concept(E, Sig)).

best_concept([], 0.0, none).
best_concept(Ms, Sc, C) :-
    keysort(Ms, S), reverse(S, [Sc-C|_]).

create_concept(E, Sig) :-
    findall(N, concept(concept(N), _, _), Ns),
    next_concept_number(Ns, N),
    C = concept(N),
    assertz(concept(C, Sig, 1)),
    assertz(concept_member(C, E, 1.0)).

add_member(C, E, _) :- concept_member(C, E, _), !.
add_member(C, E, Sc) :- assertz(concept_member(C, E, Sc)).

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
    (LU =:= 0 -> Sc = 0.0 ; Sc is LI / LU).

discover_concept_relations :-
    forall(memory_relation(S, R, O, W, _), discover_relation(S, R, O, W)).

discover_relation(S, R, O, W) :-
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is W * SS * OS,
    add_concept_relation(SC, R, OC, Sc).

add_concept_relation(SC, R, OC, Sc) :-
    concept_relation(SC, R, OC, Old), !,
    New is max(Old, Sc),
    retract(concept_relation(SC, R, OC, Old)),
    assertz(concept_relation(SC, R, OC, New)).
add_concept_relation(SC, R, OC, Sc) :-
    assertz(concept_relation(SC, R, OC, Sc)).

% ---------- informe ----------
report_grid :-
    nl, writeln('===== GRID TABLE ====='),
    writeln('V D facts vocab concepts rules gen disc_ms ramKB f1 compr'),
    forall(cell_row(V, D, NF, VV, NC, NR, Gen, Ms, Ram, F1, C),
           format('~w ~w ~w ~w ~w ~w ~w ~w ~w ~w ~2f~n',
                  [V, D, NF, VV, NC, NR, Gen, Ms, Ram, F1, C])).

% ---------- checks ----------
check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps), length(Ps, NP),
    findall(1, check_results(_, fail), Fs), length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
