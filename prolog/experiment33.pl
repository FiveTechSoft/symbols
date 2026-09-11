% experiment33.pl
% EXPERIMENT 33 - BEHAVIORAL CLASS TRANSFER
% Clases por firma estructural SSE (sin etiquetas, sin tipos).
% CLASS_A = {alpha,beta,carl} (+ recien llegados zorin,velara).
% CLASS_B = {gamma,delta} (sin skill componible).
% La skill reaches se adhiere a CLASS_A y se hereda; wex -> UNKNOWN.
% Simbolico directo (aisla el claim; el lenguaje ya esta probado).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic struct_class/2.
:- dynamic class_skill/4.
:- dynamic check_results/2.

experiment33 :-
    reset_experiment,
    build_world,
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, [visits, in]),
    derive_struct_classes,
    attach_class_skills,
    run_partition_tests,
    run_inheritance_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(struct_class(_, _)),
    retractall(class_skill(_, _, _, _)),
    retractall(check_results(_, _)).

% A: 3 observados + 2 recien llegados (reaches oculto en estos).
% Ciudad VISITADA (con in-link) != ciudad HOGAR (sin in-link): si no,
% [lives_in,in] empataria con [visits,in] y el sistema rehusaria bien.
build_world :-
    forall(member((X, C, F, H, K),
                  [(alpha, ca1, fa1, ha1, ka1),
                   (beta, ca2, fa2, ha2, ka2),
                   (carl, ca3, fa3, ha3, ka3),
                   (zorin, cn1, fn1, hn1, kn1),
                   (velara, cn2, fn2, hn2, kn2)]),
           ( remember_relation(X, visits, C, 1.0),
             remember_relation(X, eats, F, 1.0),
             remember_relation(X, lives_in, H, 1.0),
             remember_relation(C, in, K, 1.0),
             ( (X == zorin ; X == velara) -> true
             ; remember_relation(X, reaches, K, 1.0)
             )
           )),
    forall(member((X, D),
                  [(gamma, db1), (delta, db2)]),
           ( remember_relation(X, reads, D, 1.0),
             remember_relation(X, likes, D, 1.0)
           )),
    remember_relation(wex, visits, cn3, 1.0),
    memory_size(N),
    format('world facts: ~w~n', [N]).

% ---------- SSE (grados + vecindad; solo enteros/listas) ----------
object_signature(E, sig(O, I, ON, IN)) :-
    findall(X, memory_relation(E, _, X, _, _), Outs),
    length(Outs, O),
    findall(X, memory_relation(_, _, E, _, _), Ins),
    length(Ins, I),
    findall((A, B), ( member(N, Outs), deg(N, A, B) ), ON0),
    sort(ON0, ON),
    findall((A, B), ( member(N, Ins), deg(N, A, B) ), IN0),
    sort(IN0, IN).

deg(E, O, I) :-
    findall(X, memory_relation(E, _, X, _, _), L1),
    length(L1, O),
    findall(X, memory_relation(_, _, E, _, _), L2),
    length(L2, I).

derive_struct_classes :-
    retractall(struct_class(_, _)),
    findall(E, ( memory_relation(E, _, _, _, _) ;
                 memory_relation(_, _, E, _, _)
               ),
            E0),
    sort(E0, Es),
    forall(member(E, Es),
           ( object_signature(E, Sig),
             ( struct_class(Sig, Ms), member(E, Ms) -> true
             ; ( retract(struct_class(Sig, Old)) ->
                     assertz(struct_class(Sig, [E|Old]))
                 ; assertz(struct_class(Sig, [E]))
                 )
             )
           )).

% la skill se adhiere al CONCEPTO con evidencia reaches (pertenencia por
% umbral). El SSE exacto es identidad (EXP17), no pertenencia: los
% observados llevan 'reaches' en su firma y los nuevos no.
attach_class_skills :-
    constrained_rule(reaches, Path, Sig),
    concept_member(CA, alpha, _),
    \+ concept_member(CA, gamma, _),
    assertz(class_skill(CA, reaches, Path, Sig)),
    findall(M, concept_member(CA, M, _), Ms0),
    sort(Ms0, Ms),
    format('CLASS SKILL reaches on concept ~w = ~w~n', [CA, Ms]).

% prediccion por clase: pertenencia (umbral) + estructura (camino+firma).
class_predict(X, T, O) :-
    concept_member(C, X, _),
    class_skill(C, T, Path, Sig),
    full_bindings(X, O, Path, Full),
    eq_signature(Full, Sig).

% explicacion estructural: que comparten X e Y (incluye conclusion).
shared_evidence(X, Y, Common) :-
    entity_relations(X, RX),
    entity_relations(Y, RY),
    intersection(RX, RY, Common).

entity_relations(E, Rels) :-
    findall(R, memory_relation(E, R, _, _, _), S0),
    findall(R, memory_relation(_, R, E, _, _), O0),
    append(S0, O0, All),
    sort(All, Rels).

run_partition_tests :-
    nl, writeln('===== PARTITION (membership by threshold) ====='),
    check_concept_members([alpha, beta, carl, zorin, velara], 'CLASS_A grown'),
    check_concept_members([gamma, delta], 'CLASS_B stable'),
    check(concept_singleton(wex), 'wex isolated (no class inflated)'),
    check(( shared_evidence(zorin, alpha, Common),
            member(visits, Common),
            member(eats, Common),
            member(lives_in, Common)
          ),
          'shared structural evidence (no labels needed)').

check_concept_members(Es, Label) :-
    sort(Es, Ess),
    ( concept(C, _, _),
      findall(M, concept_member(C, M, _), Ms0),
      sort(Ms0, Ess) ->
        format('PASS ~w = ~w~n', [Label, Ess]),
        assertz(check_results(Label, pass))
    ; format('FAIL ~w (expected ~w)~n', [Label, Ess]),
      assertz(check_results(Label, fail))
    ).

concept_singleton(E) :-
    concept(C, _, _),
    findall(M, concept_member(C, M, _), [E]).

run_inheritance_tests :-
    nl, writeln('===== INHERITANCE ====='),
    check(class_predict(zorin, reaches, kn1),
          'zorin inherits reaches -> kn1 (never observed)'),
    check(class_predict(velara, reaches, kn2),
          'velara inherits reaches -> kn2 (never observed)'),
    check(identity_unknown(wex),
          'wex -> UNKNOWN (no class, no skill)'),
    check(\+ class_predict(gamma, reaches, _),
          'gamma (CLASS_B) gets no reaches'),
    check(\+ class_predict(zorin, reaches, ka1),
          'cross zorin->ka1 rejected'),
    check(\+ class_predict(velara, reaches, ka2),
          'cross velara->ka2 rejected').

identity_unknown(X) :-
    \+ class_predict(X, reaches, _).

% --- checks & discovery (threshold 0.80, idempotent) ---

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

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

discover_concept_relations :-
    forall(memory_relation(S, R, O, W, _),
           discover_relation(S, R, O, W)).

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

discover_rules :-
    forall(concept_relation(SC, R, OC, Sc),
           assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))).

show_concepts_brief :-
    findall(C, concept(C, _, _), Cs),
    length(Cs, N),
    format('relational concepts=~w~n', [N]).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
