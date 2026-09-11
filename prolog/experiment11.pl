% experiment11.pl
% EXPERIMENT 11 - META RULES (second-order abstraction)
% Familias con vocabularios disjuntos e identica topologia:
%   Alpha s1,s2,s3=>t1  sig [1,2,3,4]    Beta u1,u2,u3=>t2  sig [1,2,3,4]
%   Gamma v1,v2,v3=>t3  sig [1,2,2,3] (near-miss + 4 decoys sin loop)
%   Delta w1,w2,w3=>t4  UN observado + 2 ocultos (concreto rehusa F1=0.5)
% Meta agrupa t1,t2 (forma comun), excluye t3, y licencia transferencia a t4.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('meta_pattern.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment11 :-
    reset_experiment,
    create_training_data,
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 11 - META RULES'),
    writeln('=============================================='),
    nl,
    memory_size(Size),
    format('Training facts: ~w~n~n', [Size]),
    discover_concepts,
    show_concepts,
    discover_concept_relations,
    show_concept_relations,
    discover_rules,
    show_rules,
    discover_f(t1, 3),
    induce_constrained(t1, [s1, s2, s3]),
    discover_f(t2, 3),
    induce_constrained(t2, [u1, u2, u3]),
    discover_f(t3, 3),
    induce_constrained(t3, [v1, v2, v3]),
    ( discover_f(t4, 3) ->
        format('CONTROL FAILED: t4 concrete induction should refuse~n', [])
    ; format('CONTROL OK: t4 concrete induction refused (support=1)~n', [])
    ),
    discover_meta,
    show_meta_rules,
    ( meta_transfer(t4, 3) ->
        format('TRANSFER OK~n', [])
    ; format('TRANSFER FAILED~n', []),
      fail
    ),
    show_transferred,
    show_constrained_rules,
    run_tests.

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
    retractall(transferred_rule(_, _, _, _)).

% --- datos: vocabularios disjuntos, topologias iguales/distintas ---
chain_a(ax1, as1, at1, ac1).
chain_a(ax2, as2, at2, ac2).
chain_a(ax3, as3, at3, ac3).
chain_a(ax4, as4, at4, ac4).
hidden_a(ax4).

chain_b(bx1, bu1, bt1, bc1).
chain_b(bx2, bu2, bt2, bc2).
chain_b(bx3, bu3, bt3, bc3).
chain_b(bx4, bu4, bt4, bc4).
hidden_b(bx4).

chain_g(gx1, ga1, gz1).
chain_g(gx2, ga2, gz2).
chain_g(gx3, ga3, gz3).
hidden_g(gx3).

decoy_g(kx1, ka1, kz1).
decoy_g(kx2, ka2, kz2).
decoy_g(kx3, ka3, kz3).
decoy_g(kx4, ka4, kz4).

chain_d(dx1, da1, db1, de1).
chain_d(dx2, da2, db2, de2).
chain_d(dx3, da3, db3, de3).
observed_d(dx1).

create_training_data :-
    forall(chain_a(X, A, B, C),
           ( remember_relation(X, s1, A, 1.0),
             remember_relation(A, s2, B, 1.0),
             remember_relation(B, s3, C, 1.0),
             ( hidden_a(X) -> true
             ; remember_relation(X, t1, C, 1.0)
             )
           )),
    forall(chain_b(X, A, B, C),
           ( remember_relation(X, u1, A, 1.0),
             remember_relation(A, u2, B, 1.0),
             remember_relation(B, u3, C, 1.0),
             ( hidden_b(X) -> true
             ; remember_relation(X, t2, C, 1.0)
             )
           )),
    forall(chain_g(X, A, Z),
           ( remember_relation(X, v1, A, 1.0),
             remember_relation(A, v2, A, 1.0),
             remember_relation(A, v3, Z, 1.0),
             ( hidden_g(X) -> true
             ; remember_relation(X, t3, Z, 1.0)
             )
           )),
    forall(decoy_g(X, A, Z),
           ( remember_relation(X, v1, A, 1.0),
             remember_relation(A, v3, Z, 1.0)
           )),
    forall(chain_d(X, A, B, C),
           ( remember_relation(X, w1, A, 1.0),
             remember_relation(A, w2, B, 1.0),
             remember_relation(B, w3, C, 1.0),
             ( observed_d(X) ->
               remember_relation(X, t4, C, 1.0)
             ; true
             )
           )).

hidden_pair(X, C, t1) :- hidden_a(X), chain_a(X, _, _, C).
hidden_pair(X, C, t2) :- hidden_b(X), chain_b(X, _, _, C).
hidden_pair(X, Z, t3) :- hidden_g(X), chain_g(X, _, Z).
hidden_pair(X, C, t4) :-
    chain_d(X, _, _, C), \+ observed_d(X).

distractor(X, Y, t1) :-
    member((X, Y), [(ax1, ac2), (ax2, ac1)]).
distractor(X, Y, t2) :-
    member((X, Y), [(bx1, bc2), (bx2, bc1)]).
distractor(X, Y, t3) :-
    member((X, Y), [(gx1, gz2), (kx1, gz1)]).
distractor(X, Y, t4) :-
    member((X, Y), [(dx1, de2), (dx2, de1), (dx1, bc1), (ax1, de1)]).

% --- busqueda con alcance conceptual (completa para len<=3) ---
% Vocabulario = relaciones incidentes a ≤2 saltos de los miembros.
% Aristas de otras familias nunca tocan estos nodos (contribucion 0
% demostrable), asi que el recorte no pierde ningun patron util.
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
    length(Vocab, V),
    format('Filtered vocab for ~w: ~w (~w rels)~n', [Target, Vocab, V]),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    forall(member(F1-P-S, Ranked),
           format('  Path=~w F1=~4f support=~w~n', [P, F1, S])),
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

% --- conceptos (umbral 0.70) ---

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

show_concepts :-
    nl, writeln('===== DISCOVERED CONCEPTS ====='),
    forall(concept(C, Sig, _),
           ( format('~w  signature=~w~n', [C, Sig]),
             findall(E, concept_member(C, E, _), Ms),
             length(Ms, N),
             format('   members=~w~n   ~w~n~n', [N, Ms])
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

show_concept_relations :-
    nl, writeln('===== CONCEPT RELATIONS ====='),
    forall(concept_relation(SC, R, OC, SCc),
           format('~w --~w--> ~w  score=~2f~n', [SC, R, OC, SCc])).

discover_rules :-
    forall(concept_relation(SC, R, OC, Sc),
           assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))).

show_rules :-
    nl, writeln('===== LEARNED RULES ====='),
    forall(learned_rule(Rule, _, _, _, Sc),
           format('~w confidence=~2f~n', [Rule, Sc])).

infer(S, R, O, Sc) :-
    learned_rule(_, SC, R, OC, RS),
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is RS * SS * OS.

% --- tests ---

run_tests :-
    nl, writeln('===== HIDDEN POSITIVE (L1 + transfer) ====='),
    forall(hidden_pair(A, B, T),
           ( (infer(A, T, B, S1) ->
                 format('BASELINE PASS  ~w --~w--> ~w  ~2f~n', [A, T, B, S1])
             ; format('BASELINE FAIL  ~w --~w--> ~w~n', [A, T, B])
             ),
             ( reuse_predict(A, T, B) ->
                 format('META     PASS  ~w --~w--> ~w~n', [A, T, B])
             ; format('META     FAIL  ~w --~w--> ~w~n', [A, T, B])
             )
           )),
    nl, writeln('===== DISTRACTORS (must be rejected) ====='),
    forall(distractor(A, B, T),
           ( (infer(A, T, B, S2) ->
                 format('BASELINE ERROR (overgeneralizes) ~w --~w--> ~w  ~2f~n',
                        [A, T, B, S2])
             ; format('BASELINE reject ~w --~w--> ~w~n', [A, T, B])
             ),
             ( reuse_predict(A, T, B) ->
                 format('META     ERROR ~w --~w--> ~w~n', [A, T, B])
             ; format('META     reject ~w --~w--> ~w~n', [A, T, B])
             )
           )),
    nl, writeln('===== METRICS: meta method ====='),
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
    R is TP / 5,
    ( P + R =:= 0 -> F1 = 0.0 ; F1 is 2 * P * R / (P + R) ),
    Acc is (TP + TN) / 15,
    format('Precision=~4f Recall=~4f F1=~4f Accuracy=~4f~n',
           [P, R, F1, Acc]).
