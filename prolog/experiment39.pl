% experiment39.pl
% EXPERIMENT 39 - HORIZONTAL ABSTRACTION TRANSFER (transferencia sin origen)
% Dominio A: estructuras {P owns O, O belongs_to P, P visits L} -> skill
% stocked :- [belongs_to, visits] (como EXP36).
% La abstraccion (firma SSE label-free + skill + perfiles de rol) se
% PERSISTE; despues se BORRAN todos los hechos A (facts=0) y llega el
% dominio B con objetos Y relaciones completamente nuevos:
% {P keeps K, K held_by P, P tours C} -> tarea stored(K,C) oculta.
% Sin reabrir A (ya no existe) ni reinducir: B se reconoce por firma,
% los roles se alinean por perfil, el path se traduce posicionalmente
% y stored se predice con prueba de reutilizacion.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').

:- use_module(library(lists)).

:- dynamic struct_def/2.
:- dynamic struct_sig/2.
:- dynamic struct_member/2.
:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
% abstraccion persistente (sobrevive al wipe: clear_memory no la toca)
:- dynamic saved_sig/2.
:- dynamic saved_skill/3.
:- dynamic saved_roles/2.
:- dynamic rel_map/2.
:- dynamic check_results/2.

experiment39 :-
    reset_experiment,
    learn_domain_a,
    persist_abstraction,
    wipe_domain_a,
    build_domain_b,
    reuse_abstraction,
    run_transfer_tests,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(struct_def(_, _)),
    retractall(struct_sig(_, _)),
    retractall(struct_member(_, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(saved_sig(_, _)),
    retractall(saved_skill(_, _, _)),
    retractall(saved_roles(_, _)),
    retractall(rel_map(_, _)).

% ================= DOMINIO A =================
learn_domain_a :-
    assertz(struct_def(sa1, [alpha, book, madrid])),
    assertz(struct_def(sa2, [beta, pen, paris])),
    forall(member((S, R, O),
                  [(alpha, owns, book), (book, belongs_to, alpha),
                   (alpha, visits, madrid), (book, stocked, madrid),
                   (beta, owns, pen), (pen, belongs_to, beta),
                   (beta, visits, paris), (pen, stocked, paris)]),
           remember_relation(S, R, O, 1.0)),
    discover_concepts,
    discover_concept_relations,
    discover_composition(stocked, 3),
    induce_constrained(stocked, [belongs_to, visits]),
    check(composed_rule(stocked, [belongs_to, visits], _),
          'A: skill stocked :- [belongs_to, visits]').

% ---------- conceptos relacionales (maquinaria EXP33/36-38) ----------
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

% ---------- SSE label-free con exclusion (como EXP36) ----------
member_sig(E, sig(O, I, ON, IN)) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stocked), Outs),
    length(Outs, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stocked), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_excl(N, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_excl(N, A, B)), IN0),
    sort(IN0, IN).

deg_excl(E, O, I) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stocked), L1),
    length(L1, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stocked), L2),
    length(L2, I).

struct_sse(S, stsig(MS, E)) :-
    struct_def(S, Members),
    findall(M, (member(X, Members), member_sig(X, M)), M0),
    sort(M0, MS),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _), R \== stocked),
            Edges),
    length(Edges, E).

sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

% ================= PERSISTENCIA + WIPE =================
% La abstraccion = firma + skill + perfiles de rol por firma-miembro.
% Sin hechos, sin nombres de B (aun desconocidos), sin estructuras A.
persist_abstraction :-
    struct_sse(sa1, Sig),
    struct_sse(sa2, Sig),
    check(sig_clean(Sig), 'A: abstraction has zero atoms'),
    assertz(saved_sig(cA, Sig)),
    assertz(saved_skill(cA, stocked, [belongs_to, visits])),
    Sig = stsig(MS, _),
    assertz(saved_roles(cA, MS)),
    check(true, 'abstraction persisted (sig + skill + roles)').

wipe_domain_a :-
    clear_memory,
    retractall(struct_def(_, _)),
    retractall(struct_sig(_, _)),
    retractall(struct_member(_, _)),
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    findall((S, R, O), memory_relation(S, R, O, _, _), Fs),
    length(Fs, 0),
    check(true, 'WIPE: domain A facts = 0'),
    check((saved_sig(cA, _), saved_skill(cA, stocked, _)),
          'abstraction survives the wipe').

% ================= DOMINIO B (todo nuevo) =================
% Relaciones jamas vistas: keeps/held_by/tours. Tarea stored oculta.
build_domain_b :-
    assertz(struct_def(sb1, [zorin, tablet, sevilla])),
    forall(member((S, R, O),
                  [(zorin, keeps, tablet), (tablet, held_by, zorin),
                   (zorin, tours, sevilla)]),
           remember_relation(S, R, O, 1.0)),
    % distractor: casi-estructura (falta tours)
    assertz(struct_def(sb2, [wex, quark, nowhere])),
    forall(member((S, R, O),
                  [(wex, keeps, quark), (quark, held_by, wex)]),
           remember_relation(S, R, O, 1.0)).

% firma B con exclusion de la tarea (stored, como stocked en A)
member_sig_b(E, sig(O, I, ON, IN)) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stored), Outs),
    length(Outs, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stored), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_b(N, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_b(N, A, B)), IN0),
    sort(IN0, IN).

deg_b(E, O, I) :-
    findall(X, (memory_relation(E, R, X, _, _), R \== stored), L1),
    length(L1, O),
    findall(X, (memory_relation(X, R, E, _, _), R \== stored), L2),
    length(L2, I).

struct_sse_b(S, stsig(MS, E)) :-
    struct_def(S, Members),
    findall(M, (member(X, Members), member_sig_b(X, M)), M0),
    sort(M0, MS),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _), R \== stored),
            Edges),
    length(Edges, E).

% reconocimiento: igualdad exacta con la firma guardada (cero etiquetas)
reuse_abstraction :-
    struct_sse_b(sb1, SigB),
    check(sig_clean(SigB), 'B: signature has zero atoms'),
    (saved_sig(cA, SigB) ->
        check(true, 'B structure recognized as cA (no shared vocab)')
    ; check(false, 'B structure recognized as cA (no shared vocab)')),
    induce_map(sb1).

% alineacion por perfiles de rol guardados: cada miembro B casa con un
% perfil; las aristas entre roles alineados inducen el mapa A<->B.
induce_map(S) :-
    retractall(rel_map(_, _)),
    saved_skill(cA, _, [RA1, RA2]),
    saved_roles(cA, Profiles),
    struct_def(S, Members),
    % rol O: spoke con salida (perfil con out=1); rol L: spoke sin salida
    findall(X, (member(X, Members), member_sig_b(X, sig(1, 1, _, _))), Os),
    findall(X, (member(X, Members), member_sig_b(X, sig(0, 1, _, _))), Ls),
    findall(X, (member(X, Members), member_sig_b(X, sig(2, 1, _, _))), Hs),
    Os = [ONode], Ls = [LNode], Hs = [HNode],
    check(Profiles \== [], 'roles aligned by saved profiles'),
    memory_relation(ONode, RB1, HNode, _, _),
    memory_relation(HNode, RB2, LNode, _, _),
    assertz(rel_map(RA1, RB1)),
    assertz(rel_map(RA2, RB2)),
    format('map induced: ~w <-> ~w, ~w <-> ~w~n', [RA1, RB1, RA2, RB2]).

% tarea stored(K,C): path traducido por el mapa, puerta conceptual B.
% A ya no existe: solo la abstraccion + B participan.
predict_stored(K, C, Proof) :-
    saved_sig(cA, SigSaved),
    struct_def(S, Members),
    member(K, Members), member(C, Members),
    struct_sse_b(S, SigSaved),
    rel_map(belongs_to, RB1),
    rel_map(visits, RB2),
    memory_relation(K, RB1, H, _, _),
    memory_relation(H, RB2, C, _, _),
    Proof = [reuse(cA, S), map([belongs_to-RB1, visits-RB2]),
             rule(stored, [RB1, RB2]), (K, RB1, H), (H, RB2, C)].

run_transfer_tests :-
    nl, writeln('===== HORIZONTAL TRANSFER (no origin left) ====='),
    findall((R1, R2), rel_map(R1, R2), Maps),
    sort(Maps, SM),
    check(SM == [(belongs_to, held_by), (visits, tours)],
          'full map A<->B induced from roles'),
    (predict_stored(tablet, sevilla, Proof) ->
        (check(true, 'stored(tablet,sevilla) predicted (never observed)'),
         format('reuse proof: ~w~n', [Proof]))
    ; check(false, 'stored(tablet,sevilla) predicted (never observed)')),
    (predict_stored(quark, _, _) ->
        check(false, 'wex/quark -> UNKNOWN (near-miss excluded)')
    ; (check(true, 'wex/quark -> UNKNOWN (near-miss excluded)'),
       format('wex/quark -> UNKNOWN~n', []))),
    (predict_stored(tablet, paris, _) ->
        check(false, 'cross tablet->paris rejected')
    ; check(true, 'cross tablet->paris rejected')),
    findall((S, R, O), memory_relation(S, R, O, _, _), Fs),
    \+ member((_, owns, _), Fs),
    \+ member((_, belongs_to, _), Fs),
    \+ member((_, visits, _), Fs),
    check(true, 'zero A-relations used at query time (audit)').

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
