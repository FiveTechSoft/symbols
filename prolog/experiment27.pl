% experiment27.pl
% EXPERIMENT 27 - EMERGENT CLASSES + SKILL INHERITANCE
% Cero clases declaradas. CLASS_A = {o1,o2,o3} por firma {visits,eats};
% CLASS_B = {o4,o5} por {reads,likes}. Recien llegados obj6/obj8 (A) y
% obj7 (B) se clasifican por estructura; la skill reaches (solo de A)
% se hereda a obj6/obj8 y NO a obj7. Sin lenguaje (aisla el claim).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic check_results/2.
:- dynamic class_skill/4.
% class_skill(ClassConcept, Target, Path, Sig)

experiment27 :-
    reset_experiment,
    build_world,
    discover_concepts,
    show_concepts_brief,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, [visits, in]),
    register_class_skills,
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
    retractall(class_skill(_, _, _, _)),
    retractall(check_results(_, _)).

% CLASS_A: o1..o3 (reaches observado). Recien llegados: obj6, obj8.
% CLASS_B: o4,o5 (reads/likes). Recien llegado: obj7.
build_world :-
    forall(member((X, C, F, K),
                  [(o1, c1, f1, k1), (o2, c2, f2, k2), (o3, c3, f3, k3),
                   (obj6, c6, f6, k6), (obj8, c8, f8, k8)]),
           ( remember_relation(X, visits, C, 1.0),
             remember_relation(X, eats, F, 1.0),
             remember_relation(C, in, K, 1.0),
             ( (X == obj6 ; X == obj8) -> true
             ; remember_relation(X, reaches, K, 1.0)
             )
           )),
    forall(member((X, D),
                  [(o4, d1), (o5, d2), (obj7, d3)]),
           ( remember_relation(X, reads, D, 1.0),
             remember_relation(X, likes, D, 1.0)
           )),
    memory_size(N),
    format('world facts: ~w~n', [N]).

% la skill se adhiere a la CLASE con evidencia (no a la otra).
register_class_skills :-
    constrained_rule(reaches, Path, Sig),
    findall(C, ( concept_member(C, o1, _),
                 concept_member(C, o4, _)
               ),
            Both),
    Both == [],
    concept_member(CA, o1, _),
    assertz(class_skill(CA, reaches, Path, Sig)),
    format('CLASS SKILL reaches attached to ~w~n', [CA]),
    concept_member(CB, o4, _),
    CA \== CB,
    format('CLASS_B lives in ~w (no reaches skill)~n', [CB]).

% prediccion por clase: pertenencia + estructura.
class_predict(X, T, O) :-
    concept_member(C, X, _),
    class_skill(C, T, Path, Sig),
    full_bindings(X, O, Path, Full),
    eq_signature(Full, Sig).

run_partition_tests :-
    nl, writeln('===== PARTITION (no declared classes) ====='),
    check_members([o1, o2, o3, obj6, obj8], 'CLASS_A Emergent'),
    check_members([o4, o5, obj7], 'CLASS_B Emergent'),
    check_members([c1, c2, c3, c6, c8], 'cities'),
    check_members([f1, f2, f3, f6, f8], 'foods'),
    check_members([d1, d2, d3], 'book-things').

check_members(Es, Label) :-
    findall(C, ( member(E, Es),
                 concept_member(C, E, _)
               ),
            Cs0),
    sort(Cs0, [C]),
    findall(M, concept_member(C, M, _), Ms0),
    sort(Ms0, Ms),
    sort(Es, EsS),
    check(Ms == EsS, Label).

run_inheritance_tests :-
    nl, writeln('===== INHERITANCE ====='),
    check(class_predict(obj6, reaches, k6),
          'obj6 inherits reaches -> k6 (never observed)'),
    check(class_predict(obj8, reaches, k8),
          'obj8 inherits reaches -> k8 (never observed)'),
    check(\+ class_predict(obj7, reaches, _),
          'obj7 (CLASS_B) gets no reaches'),
    check(\+ class_predict(o4, reaches, _),
          'o4 (CLASS_B member) gets no reaches'),
    check(\+ class_predict(obj6, reaches, k2),
          'cross obj6->k2 rejected'),
    check(\+ class_predict(o1, reaches, k2),
          'cross o1->k2 rejected'),
    check(\+ memory_relation(_, is_a, _, _, _),
          'zero type facts anywhere (pure emergence)').

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
    format('concepts=~w~n', [N]),
    forall(concept(C, Sig, _),
           ( findall(M, concept_member(C, M, _), Ms0),
             sort(Ms0, Ms),
             format('  ~w ~w ~w~n', [C, Sig, Ms])
           )).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
