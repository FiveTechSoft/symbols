% experiment53.pl
% EXPERIMENT 53 - MANY SKILLS (memoria procedural a escala)
% K skills independientes (K=3/10/30/100), cada una con vocabulario
% propio (a_k/b_k/t_k) y entidades frescas. Descubrimiento GUIADO por
% skill (run_discovery: Generados/Evaluados/Podados/Ms reales).
% WIPE de experiencia (facts=0, skills intactas) + dominio fresco con
% solo premisas: cada t_k se predice por su skill. Coste de razonar
% vs K y vs vocabulario. Ningun modulo tocado (solo este fichero).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('guided_search.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic sk53/3.
:- dynamic check_results/2.
:- dynamic skill_row/8.

benchmark53 :-
    nl, writeln('===== BENCHMARK 53 (many skills) ====='),
    forall(member(K, [3, 10, 30, 100]),
           skill_level(K)),
    report_skills,
    report_checks.

skill_level(K) :-
    format('--- level K=~w ---~n', [K]),
    reset_level,
    build_skills(K),
    statistics(walltime, _),
    discover_concepts,
    discover_concept_relations,
    statistics(walltime, [_, MsAbs]),
    get_time(Td0),
    discover_all_guided(K, _, TotalGen, TotalEv),
    get_time(Td1),
    DiscMsF is (Td1 - Td0) * 1000,
    facts_now(Facts),
    vocab_now(Vocab),
    ( all_discovered(K) ->
        check(true, K-skills-discovered)
    ; check(false, K-skills-discovered)),
    wipe_experience(K),
    build_fresh(K),
    get_time(Tq0),
    transfer_all(K, TP, TT),
    get_time(Tq1),
    MsQF is (Tq1 - Tq0) * 1000,
    ( TP =:= TT ->
        check(true, K-transfer-all)
    ; format('FAIL transfer ~w/~w at K=~w~n', [TP, TT, K]),
      check(false, K-transfer-all)),
    ( \+ memory_relation(zzz, _, _, _, _) ->
        check(true, K-unknown)
    ; check(false, K-unknown)),
    ( \+ memory_relation(q1, t_1, q9, _, _) ->
        check(true, K-distractor)
    ; check(false, K-distractor)),
    assertz(skill_row(K, Facts, Vocab, DiscMsF, TotalGen, TotalEv,
                      MsQF, TT)),
    format('row K=~w facts=~w vocab=~w disc=~2fms gen=~w eval=~w q=~2fms nq=~w~n',
           [K, Facts, Vocab, DiscMsF, TotalGen, TotalEv, MsQF, TT]).

reset_level :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(sk53(_, _, _)).

% ---------- K skills con vocabulario propio ----------
build_skills(K) :-
    forall(between(1, K, J),
           ( skill_rels(J, A, B, T),
             forall(between(1, 5, I),
                    ( skill_entities(J, I, P, X, Y),
                      remember_relation(P, A, X, 1.0),
                      remember_relation(X, B, Y, 1.0),
                      remember_relation(P, T, Y, 1.0) )) )).

skill_rels(J, A, B, T) :-
    atomic_list_concat([a_, J], A),
    atomic_list_concat([b_, J], B),
    atomic_list_concat([t_, J], T).

skill_entities(J, I, P, X, Y) :-
    atomic_list_concat([p, J, I], '_', P),
    atomic_list_concat([x, J, I], '_', X),
    atomic_list_concat([y, J, I], '_', Y).

% ---------- descubrimiento guiado por skill ----------
discover_all_guided(K, DiscMs, TotalGen, TotalEv) :-
    findall((G, E, M),
            ( between(1, K, J),
              skill_rels(J, _, _, T),
              run_discovery(guided, T, 3, St),
              St = stats(Path, F1, _, G, E, _, M),
              assertz(sk53(T, Path, F1)) ),
            Rows),
    findall(G, member((G, _, _), Rows), Gs), sum_list(Gs, TotalGen),
    findall(E, member((_, E, _), Rows), Es), sum_list(Es, TotalEv),
    findall(M, member((_, _, M), Rows), Ms), sum_list(Ms, _DiscMs).

all_discovered(K) :-
    forall(( between(1, K, J), skill_rels(J, A, B, T) ),
           ( sk53(T, [A, B], F1), F1 >= 0.99 )).

% ---------- wipe: hechos=0, skills intactas ----------
wipe_experience(K) :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    findall(1, memory_relation(_, _, _, _, _), Fs),
    length(Fs, 0),
    findall(T, sk53(T, _, _), Ss),
    length(Ss, K),
    check(true, K-wipe-facts0-skills-kept).

% ---------- dominio fresco: solo premisas, entidades nuevas ----------
build_fresh(K) :-
    forall(between(1, K, J),
           ( skill_rels(J, A, B, _),
             fresh_entities(J, P, X, Y),
             remember_relation(P, A, X, 1.0),
             remember_relation(X, B, Y, 1.0) )).

fresh_entities(J, P, X, Y) :-
    atomic_list_concat([q, J], '_', P),
    atomic_list_concat([u, J], '_', X),
    atomic_list_concat([v, J], '_', Y).

transfer_all(K, TP, TT) :-
    findall(J, ( between(1, K, J),
                 skill_rels(J, A, B, T),
                 fresh_entities(J, P, X, Y),
                 sk53(T, [A, B], _),
                 memory_relation(P, A, X, _, _),
                 memory_relation(X, B, Y, _, _),
                 \+ memory_relation(P, T, Y, _, _),
                 remember_relation(P, T, Y, 1.0) ),
            Js),
    length(Js, TP),
    TT = K.

% ---------- metricas ----------
facts_now(N) :-
    findall(1, memory_relation(_, _, _, _, _), Fs),
    length(Fs, N).

vocab_now(N) :-
    findall(R, memory_relation(_, R, _, _, _), Rs0),
    sort(Rs0, Rs),
    length(Rs, N).

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
report_skills :-
    nl, writeln('===== SKILLS TABLE ====='),
    writeln('K facts vocab disc_ms gen eval q_ms nq'),
    forall(skill_row(K, F, V, D, G, E, Q, N),
           format('~w ~w ~w ~w ~w ~w ~w ~w~n', [K, F, V, D, G, E, Q, N])).

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
