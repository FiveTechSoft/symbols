% experiment34.pl
% EXPERIMENT 34 - STRUCTURE/BEHAVIOR SEPARATION (principio arquitectonico)
% Misma evidencia, dos regimenes de firma:
%   CON reaches en la firma  -> los nuevos NO casan (reproduce el fallo)
%   SIN reaches en la firma  -> los nuevos casan EXACTO (identidad!)
% Regla: el SSE representa lo sabido ANTES de la inferencia a predecir.
% Skill reaches adherida al concepto; herencia x2; wex UNKNOWN.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic struct_class/3.
:- dynamic class_skill/4.
:- dynamic check_results/2.

experiment34 :-
    reset_experiment,
    build_world,
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    discover_composition(reaches, 3),
    induce_constrained(reaches, [visits, in]),
    contrast_regimes,
    attach_skill,
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
    retractall(struct_class(_, _, _)),
    retractall(class_skill(_, _, _, _)),
    retractall(check_results(_, _)).

% mismo mundo que EXP33 (visita != hogar)
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

% ---------- SSE con exclusion (lo inferido no describe) ----------
% sse_excluding(+E, +ExcludeRels, -Sig)
sse_excluding(E, Exclude, sig(O, I, ON, IN)) :-
    findall(X, ( memory_relation(E, R, X, _, _),
                 \+ member(R, Exclude)
               ),
            Outs),
    length(Outs, O),
    findall(X, ( memory_relation(X, R, E, _, _),
                 \+ member(R, Exclude)
               ),
            Ins),
    length(Ins, I),
    findall((A, B), ( member(N, Outs),
                      deg_excluding(N, Exclude, A, B)
                    ),
            ON0),
    sort(ON0, ON),
    findall((A, B), ( member(N, Ins),
                      deg_excluding(N, Exclude, A, B)
                    ),
            IN0),
    sort(IN0, IN).

deg_excluding(E, Exclude, O, I) :-
    findall(X, ( memory_relation(E, R, X, _, _),
                 \+ member(R, Exclude)
               ),
            L1),
    length(L1, O),
    findall(X, ( memory_relation(X, R, E, _, _),
                 \+ member(R, Exclude)
               ),
            L2),
    length(L2, I).

% derive_classes(+Exclude, -Tag): particion por igualdad exacta.
derive_classes(Exclude, Tag) :-
    findall(E, ( memory_relation(E, _, _, _, _) ;
                 memory_relation(_, _, E, _, _)
               ),
            E0),
    sort(E0, Es),
    forall(member(E, Es),
           ( sse_excluding(E, Exclude, Sig),
             ( struct_class(Tag, Sig, Ms), member(E, Ms) -> true
             ; ( retract(struct_class(Tag, Sig, Old)) ->
                     assertz(struct_class(Tag, Sig, [E|Old]))
                 ; assertz(struct_class(Tag, Sig, [E]))
                 )
             )
           )).

class_members(Tag, Sig, Sorted) :-
    struct_class(Tag, Sig, Ms),
    sort(Ms, Sorted).

contrast_regimes :-
    nl, writeln('===== REGIME CONTRAST (same evidence) ====='),
    derive_classes([reaches], with),
    derive_classes([], without),
    % CON reaches: los nuevos se separan (el fallo de EXP33, reproducido)
    check(( class_members(without, _, _),
            \+ ( class_members(without, _, Ms),
                 member(alpha, Ms),
                 member(zorin, Ms)
               )
          ),
          'WITH reaches: newcomers split (failure reproduced)'),
    % SIN reaches: identidad exacta con la clase
    check(( class_members(with, SigA, MA),
            sort(MA, [alpha, beta, carl, velara, zorin])
          ),
          'WITHOUT reaches: exact identity with class (5 members)'),
    format('class signature: ~w~n', [SigA]),
    check(signature_clean(SigA), 'signature has zero atoms (audit)').

signature_clean(Sig) :-
    \+ ( sub_term(T, Sig),
         atom(T)
       ).

% skill al concepto relacional con evidencia (como EXP33).
attach_skill :-
    constrained_rule(reaches, Path, Sig),
    concept_member(CA, alpha, _),
    \+ concept_member(CA, gamma, _),
    assertz(class_skill(CA, reaches, Path, Sig)),
    findall(M, concept_member(CA, M, _), Ms0),
    sort(Ms0, Ms),
    format('CLASS SKILL reaches on ~w~n', [Ms]).

class_predict(X, T, O) :-
    concept_member(C, X, _),
    class_skill(C, T, Path, Sig),
    full_bindings(X, O, Path, Full),
    eq_signature(Full, Sig).

run_inheritance_tests :-
    nl, writeln('===== INHERITANCE (via separated regime) ====='),
    check(class_predict(zorin, reaches, kn1),
          'zorin inherits reaches -> kn1'),
    check(class_predict(velara, reaches, kn2),
          'velara inherits reaches -> kn2'),
    check(\+ class_predict(wex, reaches, _),
          'wex -> UNKNOWN'),
    check(\+ class_predict(gamma, reaches, _),
          'gamma excluded'),
    check(\+ class_predict(zorin, reaches, ka1),
          'cross rejected'),
    check(\+ class_predict(velara, reaches, ka2),
          'cross rejected').

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

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
