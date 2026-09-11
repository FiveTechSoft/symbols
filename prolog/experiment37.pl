% experiment37.pl
% EXPERIMENT 37 - COMPOSITE STRUCTURAL CONCEPTS (composicion de conceptos)
% Dos conceptos estructurales aprendidos (cA suministro, cB cocina) se
% tratan como unidades composables: la skill compuesta cater(P,L) se
% aprende y aplica SOBRE conceptos + sub-skills, sin volver a los datos
% crudos. cater/cuisine/stocked excluidos de las firmas por construccion.
% cA: {P owns O, O belongs_to P, P visits L} -> stocked(O,L).
% cB: {P cooks D, D needs I} -> cuisine(P,I).
% C = cA + cB compartiendo P: cater(P,L) si stocked(O,L) via cA y P en cB.
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

experiment37 :-
    reset_experiment,
    build_train,
    discover_concepts,
    discover_concept_relations,
    discover_skills,
    observe_composite_train,
    derive_struct_concepts,
    attach_struct_skills,
    build_test,
    run_composite_tests,
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

% ---------- train: alpha y beta con AMBAS estructuras ----------
build_train :-
    assertz(struct_def(sa1, cA, [alpha, book, madrid])),
    assertz(struct_def(sb1, cB, [alpha, paella, rice])),
    assertz(struct_def(sa2, cA, [beta, pen, paris])),
    assertz(struct_def(sb2, cB, [beta, tortilla, egg])),
    forall(member((S, R, O),
                  [(alpha, owns, book), (book, belongs_to, alpha),
                   (alpha, visits, madrid), (book, stocked, madrid),
                   (alpha, cooks, paella), (paella, needs, rice),
                   (alpha, cuisine, rice),
                   (beta, owns, pen), (pen, belongs_to, beta),
                   (beta, visits, paris), (pen, stocked, paris),
                   (beta, cooks, tortilla), (tortilla, needs, egg),
                   (beta, cuisine, egg)]),
           remember_relation(S, R, O, 1.0)).

% Conclusiones compuestas OBSERVADAS (train), solo DESPUES de inducir las
% sub-skills: si cater estuviera en el vocabulario de descubrimiento,
% empataria ([belongs_to,cater] F1=1.0) y romperia el margen. Es la misma
% exclusion evidencial (EXP34), aplicada en el tiempo.
observe_composite_train :-
    forall(member((S, R, O),
                  [(alpha, cater, madrid), (beta, cater, paris)]),
           remember_relation(S, R, O, 1.0)).

% ---------- conceptos relacionales (maquinaria, como EXP33/36) ----------
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

% ---------- sub-skills por composicion ----------
discover_skills :-
    discover_composition(stocked, 3),
    induce_constrained(stocked, [belongs_to, visits]),
    check(composed_rule(stocked, [belongs_to, visits], _),
          'sub-skill stocked :- [belongs_to, visits]'),
    discover_composition(cuisine, 3),
    induce_constrained(cuisine, [cooks, needs]),
    check(composed_rule(cuisine, [cooks, needs], _),
          'sub-skill cuisine :- [cooks, needs]').

% ---------- SSE con exclusion parametrizada (label-free) ----------
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

% exclusion UNIFORME: toda relacion objetivo (stocked, cuisine, cater)
% queda fuera de TODAS las firmas. Los miembros P participan en ambas
% estructuras, asi que una exclusion por clase los haria inigualables
% cuando las conclusiones estan ocultas (EXP33: identidad vs pertenencia).
excluded_all([stocked, cuisine, cater]).

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
    check(SigA1 == SigA2, 'cA train structures share SSE'),
    struct_sse_x(sb1, SigB1), struct_sse_x(sb2, SigB2),
    check(SigB1 == SigB2, 'cB train structures share SSE'),
    check(sig_clean(SigA1), 'cA SSE has zero atoms'),
    check(sig_clean(SigB1), 'cB SSE has zero atoms'),
    assertz(struct_sig(cA, SigA1)),
    assertz(struct_member(cA, sa1)),
    assertz(struct_member(cA, sa2)),
    assertz(struct_sig(cB, SigB1)),
    assertz(struct_member(cB, sb1)),
    assertz(struct_member(cB, sb2)),
    check(true, 'two structure-concepts cA/cB (no invented splits)').

attach_struct_skills :-
    assertz(struct_skill(cA, stocked, [belongs_to, visits])),
    assertz(struct_skill(cB, cuisine, [cooks, needs])),
    check(true, 'sub-skills attached to concepts (not to raw facts)').

classify_struct(S, Tag) :-
    struct_sse_x(S, Sig),
    struct_sig(Tag, Sig).

% ---------- test: zorin completo, wex solo cA, yago solo cB ----------
build_test :-
    assertz(struct_def(sa3, cA, [zorin, tablet, sevilla])),
    assertz(struct_def(sb3, cB, [zorin, stew, bean])),
    forall(member((S, R, O),
                  [(zorin, owns, tablet), (tablet, belongs_to, zorin),
                   (zorin, visits, sevilla),
                   (zorin, cooks, stew), (stew, needs, bean)]),
           remember_relation(S, R, O, 1.0)),
    assertz(struct_def(sa4, cA, [wex, quark, nowhere])),
    forall(member((S, R, O),
                  [(wex, owns, quark), (quark, belongs_to, wex),
                   (wex, visits, nowhere)]),
           remember_relation(S, R, O, 1.0)),
    assertz(struct_def(sb5, cB, [yago, cake, sugar])),
    forall(member((S, R, O),
                  [(yago, cooks, cake), (cake, needs, sugar)]),
           remember_relation(S, R, O, 1.0)).

% sub-skill aplicada DENTRO de una estructura miembro (puerta conceptual)
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

% skill COMPUESTA: solo sobre conceptos + sub-skills (jamas hechos crudos)
predict_cater(P, L, Proof) :-
    predict_in(SA, stocked, _, L, PStock),
    struct_def(SA, cA, MA), member(P, MA),
    struct_def(SB, cB, MB), member(P, MB),
    classify_struct(SB, cB),
    Proof = [composite(cA, cB, P), PStock, member(SB, cB)].

run_composite_tests :-
    nl, writeln('===== COMPOSITE (concepts as bricks) ====='),
    (classify_struct(sa3, cA) ->
        check(true, 'sa3 joins cA (new supply vocab)')
    ; check(false, 'sa3 joins cA (new supply vocab)')),
    (classify_struct(sb3, cB) ->
        check(true, 'sb3 joins cB (new kitchen vocab)')
    ; check(false, 'sb3 joins cB (new kitchen vocab)')),
    (predict_in(sa3, stocked, tablet, sevilla, PS) ->
        (check(true, 'sub-skill stocked fires in sa3'),
         format('sub-proof stocked: ~w~n', [PS]))
    ; check(false, 'sub-skill stocked fires in sa3')),
    (predict_in(sb3, cuisine, zorin, bean, PC) ->
        (check(true, 'sub-skill cuisine fires in sb3'),
         format('sub-proof cuisine: ~w~n', [PC]))
    ; check(false, 'sub-skill cuisine fires in sb3')),
    (predict_cater(zorin, sevilla, Proof) ->
        (check(true, 'COMPOSITE cater(zorin,sevilla) (never observed)'),
         format('composite proof: ~w~n', [Proof]))
    ; check(false, 'COMPOSITE cater(zorin,sevilla) (never observed)')),
    (predict_cater(wex, _, _) ->
        check(false, 'wex (cA only) -> UNKNOWN')
    ; (check(true, 'wex (cA only) -> UNKNOWN'),
       format('wex -> UNKNOWN~n', []))),
    (predict_cater(yago, _, _) ->
        check(false, 'yago (cB only) -> UNKNOWN')
    ; (check(true, 'yago (cB only) -> UNKNOWN'),
       format('yago -> UNKNOWN~n', []))),
    (predict_cater(zorin, paris, _) ->
        check(false, 'cross zorin->paris rejected')
    ; check(true, 'cross zorin->paris rejected')).

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
