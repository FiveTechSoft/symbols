% experiment12.pl
% EXPERIMENT 12 - RULE INSTANTIATION
% Fase A: 2 familias (a*, c*) => meta len3 sig1234 (z1, z2).
% Fase B: vocabulario NUEVO disjunto (e*: z3 con 1 ejemplo; g*: z4 SIN
%   ningun ejemplo). Chequeo de novedad programatico.
% Fase C: instanciar z3 via meta (exito justificado); intentar z4
%   (rehuso justificado: no_conclusion_evidence). INVENTION=0.
% Fase D: exportar a longterm.pl, borrar todo, recargar, retest;
%   borrar la meta, retest (independencia).
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('meta_pattern.pl').
:- consult('rule_instantiation.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment12 :-
    reset_experiment,
    phase_a,
    phase_b,
    phase_c,
    phase_d.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(meta_rule(_, _, _, _)),
    retractall(transferred_rule(_, _, _, _)),
    retractall(instantiation_record(_, _, _, _, _, _)).

% ---------- Fase A: formar la meta (sin ver vocabulario nuevo) ----------
chain_f1(fa1, fb1, fc1, fd1).
chain_f1(fa2, fb2, fc2, fd2).
chain_f1(fa3, fb3, fc3, fd3).
hidden_f1(fa3).

chain_f2(ga1, gb1, gc1, gd1).
chain_f2(ga2, gb2, gc2, gd2).
chain_f2(ga3, gb3, gc3, gd3).
hidden_f2(ga3).

phase_a :-
    nl, writeln('===== PHASE A: meta formation ====='),
    forall(chain_f1(X, A, B, C),
           ( remember_relation(X, a1, A, 1.0),
             remember_relation(A, a2, B, 1.0),
             remember_relation(B, a3, C, 1.0),
             ( hidden_f1(X) -> true
             ; remember_relation(X, z1, C, 1.0)
             )
           )),
    forall(chain_f2(X, A, B, C),
           ( remember_relation(X, c1, A, 1.0),
             remember_relation(A, c2, B, 1.0),
             remember_relation(B, c3, C, 1.0),
             ( hidden_f2(X) -> true
             ; remember_relation(X, z2, C, 1.0)
             )
           )),
    refresh_concepts,
    discover_f(z1, 3),
    induce_constrained(z1, [a1, a2, a3]),
    discover_f(z2, 3),
    induce_constrained(z2, [c1, c2, c3]),
    discover_meta,
    show_meta_rules,
    memory_size(M),
    format('Phase A memory: ~w facts~n', [M]).

% ---------- Fase B: vocabulario nuevo disjunto ----------
chain_new(na1, nb1, nc1, nd1).
chain_new(na2, nb2, nc2, nd2).
chain_new(na3, nb3, nc3, nd3).
observed_new(na1).

chain_zero(za1, zb1, zd1).
chain_zero(za2, zb2, zd2).

phase_b :-
    nl, writeln('===== PHASE B: new disjoint vocabulary ====='),
    forall(chain_new(X, A, B, C),
           ( remember_relation(X, e1, A, 1.0),
             remember_relation(A, e2, B, 1.0),
             remember_relation(B, e3, C, 1.0),
             ( observed_new(X) ->
               remember_relation(X, z3, C, 1.0)
             ; true
             )
           )),
    forall(chain_zero(X, A, C),
           ( remember_relation(X, g1, A, 1.0),
             remember_relation(A, g3, C, 1.0)
           )),
    novelty_check,
    refresh_concepts.

% Ningun simbolo nuevo existia en fase A: disjuncion de vocabularios.
novelty_check :-
    findall(R, ( memory_relation(_, R, _, _, _),
                 member(R, [e1, e2, e3, z3, g1, g3])
               ),
            NewRels),
    sort(NewRels, NR),
    findall(R, ( memory_relation(_, R, _, _, _),
                 member(R, [a1, a2, a3, z1, c1, c2, c3, z2])
               ),
            OldRels),
    sort(OldRels, OR),
    intersection(NR, OR, []),
    format('Novelty OK: new=~w old-disjoint~n', [NR]),
    forall(chain_new(X, _, _, _),
           ( \+ chain_f1(X, _, _, _),
             \+ chain_f2(X, _, _, _)
           )),
    format('Novelty OK: entities disjoint~n', []).

% ---------- Fase C: instanciar con justificacion / rehusar ----------
phase_c :-
    nl, writeln('===== PHASE C: instantiation ====='),
    ( discover_f(z3, 3) ->
        format('NOTE: z3 concrete induction unexpectedly succeeded~n', [])
    ; format('z3 concrete refused (support=1) -> meta path~n', [])
    ),
    ( instantiate_rule(z3, 3) -> true
    ; format('INSTANTIATE FAILED (should succeed)~n', []), fail
    ),
    ( instantiate_rule(z4, 3) ->
        format('TRAP: z4 instantiated without evidence!~n', []), fail
    ; format('z4 correctly refused (no_conclusion_evidence)~n', [])
    ),
    show_instantiations,
    audit_invention(Inv),
    format('INVENTION = ~w~n', [Inv]),
    ( Inv == [] -> format('INVENTION=0 OK~n', [])
    ; format('INVENTION VIOLATION~n', []), fail
    ),
    format('JUSTIFIED_INSTANTIATION = 2/2 (accept z3 + refuse z4)~n', []).

% ---------- Fase D: persistencia e independencia ----------
hidden_pair(X, C, T) :-
    member((X, C, T), [(fa3, fd3, z1), (ga3, gd3, z2),
                       (na2, nd2, z3), (na3, nd3, z3)]).

distractor(X, Y, T) :-
    member((X, Y, T), [(fa1, fd2, z1), (ga1, gd2, z2),
                       (na1, nd2, z3), (na2, nd1, z3),
                       (za1, zd1, z3), (za1, zd2, z3)]).

phase_d :-
    nl, writeln('===== PHASE D: tests + persistence + independence ====='),
    run_suite_tests('before export'),
    export_rules('longterm.pl'),
    wipe_all_rules,
    format('wiped all rules (simulated session end)~n', []),
    import_rules('longterm.pl'),
    run_suite_tests('after reload'),
    retractall(meta_rule(_, _, _, _)),
    format('meta deleted (independence check)~n', []),
    run_suite_tests('without meta'),
    show_instantiations.

run_suite_tests(Label) :-
    format('--- suite ~w ---~n', [Label]),
    findall(1, (hidden_pair(A, B, T), reuse_predict(A, T, B)), TPL),
    length(TPL, TP),
    findall(1, (hidden_pair(A, B, T), \+ reuse_predict(A, T, B)), FNL),
    length(FNL, FN),
    findall(1, (distractor(A, B, T), reuse_predict(A, T, B)), FPL),
    length(FPL, FP),
    findall(1, (distractor(A, B, T), \+ reuse_predict(A, T, B)), TNL),
    length(TNL, TN),
    format('TP=~w FN=~w FP=~w TN=~w~n', [TP, FN, FP, TN]),
    DenP is TP + FP,
    ( DenP =:= 0 -> P = 0.0 ; P is TP / DenP ),
    R is TP / 4,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 10,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).

% --- concepts refresh (idempotente) + busqueda con alcance ---

refresh_concepts :-
    discover_concepts,
    discover_concept_relations,
    forall(concept_relation(SC, R, OC, Sc),
           ( learned_rule(rule(SC, R, OC), _, _, _, _) -> true
           ; assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))
           )).

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

discover_f(Target, MaxLen) :-
    retractall(composed_rule(Target, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    append(SS, OS, N0),
    incident(N0, R1, N1),
    incident(N1, R2, _),
    append(R1, R2, Rall0),
    sort(Rall0, Rall),
    delete(Rall, Target, Vocab),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(composed_rule(Target, BestPath, BestF1)),
    format('Composed: ~w(S,O) :- ~w  (F1=~4f support=~w)~n',
           [Target, BestPath, BestF1, BestSup]).

incident(Nodes, Rels, Neigh) :-
    findall(R, ( memory_relation(S, R, O, _, _),
                 ( member(S, Nodes) ; member(O, Nodes) )
               ),
            Rels0),
    sort(Rels0, Rels),
    findall(N, ( memory_relation(S, _, O, _, _),
                 ( member(S, Nodes) ; member(O, Nodes) ),
                 ( N = S ; N = O )
               ),
            Neigh0),
    sort(Neigh0, Neigh).

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
    ( Best >= 0.70 -> add_member(BC, E, Best)
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
