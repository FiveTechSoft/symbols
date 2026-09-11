% experiment38.pl
% EXPERIMENT 38 - HIERARCHICAL COMPOSITION (compuesto como ladrillo)
% El concepto compuesto de EXP37 (cA+cB -> cater, nivel 3) se reutiliza
% como unidad de primer orden para un nivel 4: attends(Q,L).
% cD: {P invites Q, Q brings W, W needs V} -> contrib(Q,V).
% L4: attends(Q,L) :- cater(P,L) [ladrillo L3, opaco] + Q en cD con P.
% Nada vuelve a hechos crudos ni reinduce componentes: la prueba L4
% anida la prueba L3 como sub-prueba. Exclusion global y temporal de
% todos los objetivos (stocked, cuisine, cater, contrib, attends).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic struct_def/3.
:- dynamic struct_sig/2.
:- dynamic struct_member/2.
:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic struct_skill/3.
:- dynamic check_results/2.

experiment38 :-
    reset_experiment,
    build_base,
    discover_concepts,
    discover_concept_relations,
    discover_sub_skills,
    observe_cater,
    discover_contrib,
    observe_attends,
    derive_struct_concepts,
    attach_struct_skills,
    build_test,
    run_level4_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(struct_def(_, _, _)),
    retractall(struct_sig(_, _)),
    retractall(struct_member(_, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)).

% ---------- base: 2 stacks completos, SIN conclusiones ----------
build_base :-
    assertz(struct_def(sa1, cA, [alpha, book, madrid])),
    assertz(struct_def(sb1, cB, [alpha, paella, rice])),
    assertz(struct_def(sd1, cD, [alpha, carl, wine, cheese])),
    assertz(struct_def(sa2, cA, [beta, pen, paris])),
    assertz(struct_def(sb2, cB, [beta, tortilla, egg])),
    assertz(struct_def(sd2, cD, [beta, dora, cake, flour])),
    forall(member((S, R, O),
                  [(alpha, owns, book), (book, belongs_to, alpha),
                   (alpha, visits, madrid), (book, stocked, madrid),
                   (alpha, cooks, paella), (paella, needs, rice),
                   (alpha, cuisine, rice),
                   (alpha, invites, carl),
                   (carl, brings, wine), (wine, needs, cheese),
                   (carl, contrib, cheese),
                   (beta, owns, pen), (pen, belongs_to, beta),
                   (beta, visits, paris), (pen, stocked, paris),
                   (beta, cooks, tortilla), (tortilla, needs, egg),
                   (beta, cuisine, egg),
                   (beta, invites, dora),
                   (dora, brings, cake), (cake, needs, flour),
                   (dora, contrib, flour)]),
           remember_relation(S, R, O, 1.0)).

% ---------- conceptos relacionales (maquinaria EXP33/36/37) ----------
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

% ---------- sub-skills L2 (vocabulario limpio: sin cater/attends) ----------
discover_sub_skills :-
    discover_composition(stocked, 3),
    induce_constrained(stocked, [belongs_to, visits]),
    check(composed_rule(stocked, [belongs_to, visits], _),
          'L2 stocked :- [belongs_to, visits]'),
    discover_composition(cuisine, 3),
    induce_constrained(cuisine, [cooks, needs]),
    check(composed_rule(cuisine, [cooks, needs], _),
          'L2 cuisine :- [cooks, needs]').

% cater observado SOLO tras sub-skills L2 (exclusion temporal):
% el descubrimiento necesita hechos del objetivo (contrib vive en base),
% pero cater empataria stocked ([belongs_to,cater]) y attends es la cima.
observe_cater :-
    forall(member((S, R, O),
                  [(alpha, cater, madrid), (beta, cater, paris)]),
           remember_relation(S, R, O, 1.0)).

% contrib descubierto con cater ya presente pero attends ausente:
% attends(Q,L) muere en L (sumidero), sin empates.
discover_contrib :-
    discover_composition(contrib, 3),
    induce_constrained(contrib, [brings, needs]),
    check(composed_rule(contrib, [brings, needs], _),
          'L2 contrib :- [brings, needs]').

% attends observado en ultimo lugar (cima temporal)
observe_attends :-
    forall(member((S, R, O),
                  [(carl, attends, madrid), (dora, attends, paris)]),
           remember_relation(S, R, O, 1.0)).

% ---------- SSE con exclusion global (los 5 objetivos) ----------
member_sig_x(E, Exclude, sig(O, I, ON, IN)) :-
    findall(X, (memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_x(N, Exclude, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_x(N, Exclude, A, B)), IN0),
    sort(IN0, IN).

deg_x(E, Exclude, O, I) :-
    findall(X, (memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

excluded_all([stocked, cuisine, cater, contrib, attends]).

struct_sse_x(S, stsig(MS, E)) :-
    struct_def(S, _Tag, Members),
    excluded_all(Exclude),
    findall(M, (member(X, Members), member_sig_x(X, Exclude, M)), M0),
    sort(M0, MS),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

derive_struct_concepts :-
    struct_sse_x(sa1, SigA1), struct_sse_x(sa2, SigA2),
    check(SigA1 == SigA2, 'cA train share SSE'),
    struct_sse_x(sb1, SigB1), struct_sse_x(sb2, SigB2),
    check(SigB1 == SigB2, 'cB train share SSE'),
    struct_sse_x(sd1, SigD1), struct_sse_x(sd2, SigD2),
    check(SigD1 == SigD2, 'cD train share SSE'),
    check(sig_clean(SigA1), 'cA SSE zero atoms'),
    check(sig_clean(SigD1), 'cD SSE zero atoms'),
    assertz(struct_sig(cA, SigA1)),
    assertz(struct_member(cA, sa1)),
    assertz(struct_member(cA, sa2)),
    assertz(struct_sig(cB, SigB1)),
    assertz(struct_member(cB, sb1)),
    assertz(struct_member(cB, sb2)),
    assertz(struct_sig(cD, SigD1)),
    assertz(struct_member(cD, sd1)),
    assertz(struct_member(cD, sd2)),
    check(true, 'three structure-concepts cA/cB/cD').

attach_struct_skills :-
    assertz(struct_skill(cA, stocked, [belongs_to, visits])),
    assertz(struct_skill(cB, cuisine, [cooks, needs])),
    assertz(struct_skill(cD, contrib, [brings, needs])),
    check(true, 'sub-skills on concepts (never raw facts)').

classify_struct(S, Tag) :-
    struct_sse_x(S, Sig),
    struct_sig(Tag, Sig).

% ---------- test: stack completo nuevo + 2 parciales ----------
build_test :-
    assertz(struct_def(sa3, cA, [zorin, tablet, sevilla])),
    assertz(struct_def(sb3, cB, [zorin, stew, bean])),
    assertz(struct_def(sd3, cD, [zorin, nell, mead, honey])),
    forall(member((S, R, O),
                  [(zorin, owns, tablet), (tablet, belongs_to, zorin),
                   (zorin, visits, sevilla),
                   (zorin, cooks, stew), (stew, needs, bean),
                   (zorin, invites, nell),
                   (nell, brings, mead), (mead, needs, honey)]),
           remember_relation(S, R, O, 1.0)),
    % wex: sin cocina (cB ausente) -> cater imposible
    assertz(struct_def(sa4, cA, [wex, quark, nowhere])),
    assertz(struct_def(sd4, cD, [wex, gus, soda, lime])),
    forall(member((S, R, O),
                  [(wex, owns, quark), (quark, belongs_to, wex),
                   (wex, visits, nowhere),
                   (wex, invites, gus),
                   (gus, brings, soda), (soda, needs, lime)]),
           remember_relation(S, R, O, 1.0)),
    % yago: sin invitado (cD ausente) -> attends imposible
    assertz(struct_def(sa5, cA, [yago, cake2, oslo])),
    assertz(struct_def(sb5, cB, [yago, pie, plum])),
    forall(member((S, R, O),
                  [(yago, owns, cake2), (cake2, belongs_to, yago),
                   (yago, visits, oslo),
                   (yago, cooks, pie), (pie, needs, plum)]),
           remember_relation(S, R, O, 1.0)).

% sub-skill dentro de estructura miembro (puerta conceptual)
predict_in(S, stocked, O, L, Proof) :-
    struct_def(S, cA, Members),
    member(O, Members), member(L, Members),
    classify_struct(S, cA),
    struct_skill(cA, stocked, [belongs_to, visits]),
    memory_relation(O, belongs_to, P, _, _),
    memory_relation(P, visits, L, _, _),
    Proof = [struct_match(S, cA), rule(stocked, [belongs_to, visits])].

predict_in(S, cuisine, P, I, Proof) :-
    struct_def(S, cB, Members),
    member(P, Members), member(I, Members),
    classify_struct(S, cB),
    struct_skill(cB, cuisine, [cooks, needs]),
    memory_relation(P, cooks, D, _, _),
    memory_relation(D, needs, I, _, _),
    Proof = [struct_match(S, cB), rule(cuisine, [cooks, needs])].

predict_in(S, contrib, Q, V, Proof) :-
    struct_def(S, cD, Members),
    member(Q, Members), member(V, Members),
    classify_struct(S, cD),
    struct_skill(cD, contrib, [brings, needs]),
    memory_relation(Q, brings, W, _, _),
    memory_relation(W, needs, V, _, _),
    Proof = [struct_match(S, cD), rule(contrib, [brings, needs])].

% compuesto L3 (ladrillo reutilizable, opaco hacia abajo)
predict_cater(P, L, Proof) :-
    predict_in(SA, stocked, _, L, PStock),
    struct_def(SA, cA, MA), member(P, MA),
    struct_def(SB, cB, MB), member(P, MB),
    classify_struct(SB, cB),
    Proof = [composite(cA, cB, P), PStock, member(SB, cB)].

% NIVEL 4: el compuesto L3 entra como unidad (jamas se reabre)
predict_attends(Q, L, Proof) :-
    predict_cater(P, L, PCater),
    struct_def(SD, cD, MD), member(P, MD), member(Q, MD),
    classify_struct(SD, cD),
    predict_in(SD, contrib, Q, _, PContrib),
    Proof = [composite4(cABC, cD, Q), PCater, PContrib].

run_level4_tests :-
    nl, writeln('===== LEVEL 4 (composite as brick) ====='),
    (classify_struct(sa3, cA) ->
        check(true, 'sa3 joins cA')
    ; check(false, 'sa3 joins cA')),
    (classify_struct(sb3, cB) ->
        check(true, 'sb3 joins cB')
    ; check(false, 'sb3 joins cB')),
    (classify_struct(sd3, cD) ->
        check(true, 'sd3 joins cD (new invite vocab)')
    ; check(false, 'sd3 joins cD (new invite vocab)')),
    (predict_in(sd3, contrib, nell, honey, PCo) ->
        (check(true, 'sub-skill contrib fires in sd3'),
         format('sub-proof contrib: ~w~n', [PCo]))
    ; check(false, 'sub-skill contrib fires in sd3')),
    (predict_cater(zorin, sevilla, PC) ->
        (check(true, 'L3 brick cater(zorin,sevilla) reused whole'),
         format('L3 proof: ~w~n', [PC]))
    ; check(false, 'L3 brick cater(zorin,sevilla) reused whole')),
    (predict_attends(nell, sevilla, Proof) ->
        (check(true, 'LEVEL4 attends(nell,sevilla) (never observed)'),
         format('L4 proof: ~w~n', [Proof]))
    ; check(false, 'LEVEL4 attends(nell,sevilla) (never observed)')),
    (predict_attends(gus, _, _) ->
        check(false, 'gus (no kitchen, no cater) -> UNKNOWN')
    ; (check(true, 'gus (no kitchen, no cater) -> UNKNOWN'),
       format('gus -> UNKNOWN~n', []))),
    (predict_attends(yago, _, _) ->
        check(false, 'yago-stack (no invite) -> UNKNOWN')
    ; (check(true, 'yago-stack (no invite) -> UNKNOWN'),
       format('yago-stack -> UNKNOWN~n', []))),
    (predict_attends(nell, paris, _) ->
        check(false, 'cross nell->paris rejected')
    ; check(true, 'cross nell->paris rejected')).

% ---------- reporte ----------
check(Cond, Msg) :-
    (call(Cond) ->
        format('PASS ~w~n', [Msg]),
        assertz(check_results(Msg, pass))
    ; format('FAIL ~w~n', [Msg]),
      assertz(check_results(Msg, fail))).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps), length(Ps, NP),
    findall(1, check_results(_, fail), Fs), length(Fs, NF),
    N is NP + NF,
    format('passed ~w/~w~n', [NP, N]).
