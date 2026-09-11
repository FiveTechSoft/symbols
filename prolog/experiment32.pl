% experiment32.pl
% EXPERIMENT 32 - OBJECT SIGNATURES (embeddings simbolicos sin etiquetas)
% Firma puramente estructural (grados + vecindad, estilo WL-1): ningun
% nombre de relacion u objeto aparece en ella (auditado).
% 3 vocabularios DISJUNTOS (A/B/C) con misma topologia de comportamiento:
% persona: 3 salidas (visits/lives_in/owns) a [objeto(0,1), lugar(0,2)x2].
% Fases: A aprende clases; B y C deben caer en ELLAS (sin crear nuevas),
% mas anomalia aislada. Similitud con EXPLICACION estructural.
:- consult('memory.pl').

:- use_module(library(lists)).

:- dynamic struct_class/2.
:- dynamic check_results/2.

experiment32 :-
    reset_experiment,
    phase_a,
    phase_b,
    phase_c,
    similarity_demo,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(struct_class(_, _)),
    retractall(check_results(_, _)).

% ---------- firma estructural (solo enteros y listas) ----------
% sig(OutDeg, InDeg, OutNeighDegsSorted, InNeighDegsSorted)
object_signature(E, sig(O, I, ON, IN)) :-
    findall(X, memory_relation(E, _, X, _, _), Outs),
    length(Outs, O),
    findall(X, memory_relation(X, _, E, _, _), Ins),
    findall(X, ( memory_relation(X, _, E, _, _) ), Ins2),
    length(Ins, I),
    findall((A, B), ( member(N, Outs),
                      deg(N, A, B)
                    ),
            ON0),
    sort(ON0, ON),
    findall((A, B), ( member(N, Ins2),
                      deg(N, A, B)
                    ),
            IN0),
    sort(IN0, IN).

deg(E, O, I) :-
    findall(X, memory_relation(E, _, X, _, _), L1),
    length(L1, O),
    findall(X, memory_relation(X, _, E, _, _), L2),
    length(L2, I).

% la firma no contiene atomos (auditoria: solo estructura)
signature_clean(Sig) :-
    \+ ( sub_term(T, Sig),
         atom(T)
       ).

% ---------- clases por igualdad exacta de firma ----------
derive_classes :-
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

% ---------- fases ----------
phase_a :-
    nl, writeln('===== PHASE A (vocab A) ====='),
    forall(member((S, V, O),
                  [(alpha, visits, madrid), (alpha, lives_in, madrid),
                   (alpha, owns, book), (beta, visits, paris),
                   (beta, lives_in, paris), (beta, owns, pen)]),
           remember_relation(S, V, O, 1.0)),
    derive_classes,
    show_classes,
    check_class([alpha, beta], 'PERSON-A learned'),
    check_class([madrid, paris], 'PLACE-A learned'),
    check(class_count(3), 'exactly 3 structural classes').

phase_b :-
    nl, writeln('===== PHASE B (vocab B, disjoint) ====='),
    forall(member((S, V, O),
                  [(zorin, visits, sevilla), (zorin, lives_in, sevilla),
                   (zorin, owns, tablet), (velara, visits, granada),
                   (velara, lives_in, granada), (velara, owns, stylus),
                   (wex, owns, quark2)]),
           remember_relation(S, V, O, 1.0)),
    derive_classes,
    show_classes,
    check_class([alpha, beta, zorin, velara], 'B persons JOIN (no new class)'),
    check(class_count(5), '5 classes (wex+quark2 singletons, rest hold)').

phase_c :-
    nl, writeln('===== PHASE C (vocab C, disjoint) ====='),
    forall(member((S, V, O),
                  [(kappa, visits, bilbao), (kappa, lives_in, bilbao),
                   (kappa, owns, device), (luma, visits, valencia),
                   (luma, lives_in, valencia), (luma, owns, gadget)]),
           remember_relation(S, V, O, 1.0)),
    derive_classes,
    show_classes,
    check_class([alpha, beta, zorin, velara, kappa, luma],
                'C persons JOIN (vocab C absorbed)').

check_class(Expected, Label) :-
    sort(Expected, Es),
    ( struct_class(_, Ms),
      sort(Ms, Es) ->
        format('PASS ~w = ~w~n', [Label, Es]),
        assertz(check_results(Label, pass))
    ; format('FAIL ~w (expected ~w)~n', [Label, Es]),
      assertz(check_results(Label, fail))
    ).

class_count(N) :-
    findall(C, struct_class(C, _), Cs),
    length(Cs, N).

show_classes :-
    forall(struct_class(Sig, Ms),
           ( sort(Ms, S),
             format('  ~w : ~w~n', [Sig, S])
           )).

% ---------- similitud explicada ----------
similar_explained(A, B, Explanation) :-
    object_signature(A, sig(OA, IA, ONA, INA)),
    object_signature(B, sig(OB, IB, ONB, INB)),
    OA == OB, IA == IB, ONA == ONB, INA == INB,
    signature_clean(sig(OA, IA, ONA, INA)),
    Explanation = [out_degree(OA), in_degree(IA),
                   out_neighbours(ONA), in_neighbours(INA)].

similarity_demo :-
    nl, writeln('===== EXPLAINED SIMILARITY ====='),
    check(( similar_explained(alpha, zorin, E1),
            E1 \== []
          ),
          'alpha ~= zorin with non-empty structural explanation'),
    check(( similar_explained(alpha, kappa, _),
            \+ similar_explained(alpha, wex, _)
          ),
          'kappa similar, wex (anomaly) not similar'),
    ( similar_explained(alpha, zorin, E) ->
        format('why similar: ~w~n', [E])
    ; true
    ).

% --- checks ---

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
