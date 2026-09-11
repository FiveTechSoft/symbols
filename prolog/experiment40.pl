% experiment40.pl
% EXPERIMENT 40 - IMMUTABLE ABSTRACT KNOWLEDGE (doble transferencia)
% A -> abstraccion C (cA + stocked). WIPE A. B (vocabulario nuevo)
% reconoce C. WIPE B. D (tercer vocabulario) reconoce C y compone E
% (C + cF -> banquet) SIN modificar C: igualdad estructural de la
% abstraccion antes y despues de E (snapshot == snapshot).
% A: owns/belongs_to/visits. B: keeps/held_by/tours. D: hauls/carried_by/
% lodges + fixes/mends-needs. Tareas ocultas: stored (B), banquet (D).
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
% abstraccion persistente C (sobrevive a los wipes)
:- dynamic saved_sig/2.
:- dynamic saved_skill/3.
:- dynamic saved_roles/2.
:- dynamic rel_map/3.
:- dynamic c_snapshot/2.
:- dynamic check_results/2.

experiment40 :-
    reset_experiment,
    phase_a_learn,
    phase_a_persist,
    phase_wipe_a,
    phase_b_recognize,
    phase_wipe_b,
    phase_d_compose,
    run_immutability_tests,
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
    retractall(rel_map(_, _, _)),
    retractall(c_snapshot(_, _)).

% ================= FASE A =================
phase_a_learn :-
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
          'A: stocked :- [belongs_to, visits]').

% ---------- conceptos relacionales (maquinaria EXP33/36-39) ----------
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

% ---------- SSE LOCAL a la estructura (solo aristas internas) ----------
% P participa en varias estructuras; su firma global las mezcla. La firma
% que define el concepto usa solo aristas con ambos extremos en Members:
% la unidad de generalizacion es la estructura, no el objeto.
member_sig_l(E, Members, Task, sig(O, I, ON, IN)) :-
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), R \== Task), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), R \== Task), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_l(N, Members, Task, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_l(N, Members, Task, A, B)), IN0),
    sort(IN0, IN).

deg_l(E, Members, Task, O, I) :-
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), R \== Task), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), R \== Task), L2),
    length(L2, I).

struct_sse_t(S, Task, stsig(MS, E)) :-
    struct_def(S, Members),
    findall(M, (member(X, Members), member_sig_l(X, Members, Task, M)), M0),
    sort(M0, MS),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _), R \== Task),
            Edges),
    length(Edges, E).

sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

% ================= PERSISTENCIA + WIPE A =================
phase_a_persist :-
    struct_sse_t(sa1, stocked, Sig),
    struct_sse_t(sa2, stocked, Sig),
    check(sig_clean(Sig), 'A: abstraction zero atoms'),
    assertz(saved_sig(cA, Sig)),
    assertz(saved_skill(cA, stocked, [belongs_to, visits])),
    Sig = stsig(MS, _),
    assertz(saved_roles(cA, MS)),
    snapshot_c(post_a),
    check(true, 'C persisted + snapshot post_a').

snapshot_c(Tag) :-
    saved_sig(cA, Sig),
    saved_skill(cA, T, Path),
    saved_roles(cA, Roles),
    assertz(c_snapshot(Tag, c(Sig, skill(T, Path), Roles))).

phase_wipe_a :-
    wipe_facts,
    findall((S, R, O), memory_relation(S, R, O, _, _), Fs),
    length(Fs, 0),
    check(true, 'WIPE A: facts = 0'),
    check(c_snapshot(post_a, _), 'C survives wipe A').

wipe_facts :-
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
    retractall(rel_map(_, _, _)).

% ================= FASE B (segundo vocabulario) =================
phase_b_recognize :-
    assertz(struct_def(sb1, [zorin, tablet, sevilla])),
    forall(member((S, R, O),
                  [(zorin, keeps, tablet), (tablet, held_by, zorin),
                   (zorin, tours, sevilla)]),
           remember_relation(S, R, O, 1.0)),
    assertz(struct_def(sb2, [wex, quark, nowhere])),
    forall(member((S, R, O),
                  [(wex, keeps, quark), (quark, held_by, wex)]),
           remember_relation(S, R, O, 1.0)),
    struct_sse_t(sb1, stored, SigB),
    check(sig_clean(SigB), 'B: signature zero atoms'),
    (saved_sig(cA, SigB) ->
        check(true, 'B recognizes C (2nd vocab, A gone)')
    ; check(false, 'B recognizes C (2nd vocab, A gone)')),
    induce_map_b(sb1, stored),
    (predict_task_b(tablet, sevilla, stored, P) ->
        (check(true, 'B: stored(tablet,sevilla) via C'),
         format('B proof: ~w~n', [P]))
    ; check(false, 'B: stored(tablet,sevilla) via C')),
    (predict_task_b(quark, _, stored, _) ->
        check(false, 'B: wex -> UNKNOWN')
    ; check(true, 'B: wex -> UNKNOWN')).

% mapa B por roles: hub(2,1), spoke-O(1,1), spoke-L(0,1)
induce_map_b(S, Task) :-
    retractall(rel_map(b, _, _)),
    saved_skill(cA, _, [RA1, RA2]),
    struct_def(S, Members),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(1, 1, _, _))), Os),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(0, 1, _, _))), Ls),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(2, 1, _, _))), Hs),
    Os = [ONode], Ls = [LNode], Hs = [HNode],
    memory_relation(ONode, RB1, HNode, _, _),
    memory_relation(HNode, RB2, LNode, _, _),
    assertz(rel_map(b, RA1, RB1)),
    assertz(rel_map(b, RA2, RB2)),
    format('B map: ~w<->~w, ~w<->~w~n', [RA1, RB1, RA2, RB2]).

predict_task_b(K, C, Task, Proof) :-
    saved_sig(cA, SigSaved),
    struct_def(S, Members),
    member(K, Members), member(C, Members),
    struct_sse_t(S, Task, SigSaved),
    rel_map(b, belongs_to, RB1),
    rel_map(b, visits, RB2),
    memory_relation(K, RB1, H, _, _),
    memory_relation(H, RB2, C, _, _),
    Proof = [reuse(cA, S), map([belongs_to-RB1, visits-RB2]),
             rule(Task, [RB1, RB2])].

phase_wipe_b :-
    snapshot_c(pre_wipe_b),
    wipe_facts,
    findall((S, R, O), memory_relation(S, R, O, _, _), Fs),
    length(Fs, 0),
    check(true, 'WIPE B: facts = 0'),
    snapshot_c(post_wipe_b),
    check((c_snapshot(pre_wipe_b, S1), c_snapshot(post_wipe_b, S2), S1 == S2),
          'C identical across wipe B (1st immutability)').

% ================= FASE D (tercer vocabulario + E) =================
% cA-estructura: hauls/carried_by/lodges. cF: fixes/mends-needs.
% E: banquet(P,M) = C + cF compartiendo P. C no se toca.
phase_d_compose :-
    assertz(struct_def(sd1, [pico, sledge, aconcagua])),
    assertz(struct_def(sf1, [pico, rope, fibre])),
    assertz(struct_def(sd2, [finn, cart, olympus])),
    assertz(struct_def(sf2, [finn, axe, steel])),
    forall(member((S, R, O),
                  [(pico, hauls, sledge), (sledge, carried_by, pico),
                   (pico, lodges, aconcagua),
                   (pico, fixes, rope), (rope, mends, fibre),
                   (pico, banquet, aconcagua),
                   (finn, hauls, cart), (cart, carried_by, finn),
                   (finn, lodges, olympus),
                   (finn, fixes, axe), (axe, mends, steel),
                   (finn, banquet, olympus)]),
           remember_relation(S, R, O, 1.0)),
    struct_sse_t(sd1, banquet, SigD),
    check(sig_clean(SigD), 'D: signature zero atoms'),
    (saved_sig(cA, SigD) ->
        check(true, 'D recognizes C (3rd vocab, A+B gone)')
    ; check(false, 'D recognizes C (3rd vocab, A+B gone)')),
    struct_sse_t(sf1, banquet, SigF1),
    struct_sse_t(sf2, banquet, SigF2),
    check(SigF1 == SigF2, 'cF train pair shares SSE (new concept)'),
    assertz(struct_sig(cF, SigF1)),
    induce_map_d(sd1, banquet),
    assertz(struct_def(sd3, [odin, sled, kilimanjaro])),
    assertz(struct_def(sf3, [odin, lamp, oil])),
    forall(member((S, R, O),
                  [(odin, hauls, sled), (sled, carried_by, odin),
                   (odin, lodges, kilimanjaro),
                   (odin, fixes, lamp), (lamp, mends, oil)]),
           remember_relation(S, R, O, 1.0)),
    assertz(struct_def(sd4, [hal, pod, nowhere2])),
    forall(member((S, R, O),
                  [(hal, hauls, pod), (pod, carried_by, hal),
                   (hal, lodges, nowhere2)]),
           remember_relation(S, R, O, 1.0)).

induce_map_d(S, Task) :-
    retractall(rel_map(d, _, _)),
    saved_skill(cA, _, [RA1, RA2]),
    struct_def(S, Members),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(1, 1, _, _))), Os),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(0, 1, _, _))), Ls),
    findall(X, (member(X, Members),
                member_sig_l(X, Members, Task, sig(2, 1, _, _))), Hs),
    Os = [ONode], Ls = [LNode], Hs = [HNode],
    memory_relation(ONode, RB1, HNode, _, _),
    memory_relation(HNode, RB2, LNode, _, _),
    assertz(rel_map(d, RA1, RB1)),
    assertz(rel_map(d, RA2, RB2)),
    format('D map: ~w<->~w, ~w<->~w~n', [RA1, RB1, RA2, RB2]).

% E: banquet(P,M) sobre C (traducido) + cF (miembro), sin tocar C.
predict_banquet(P, M, Proof) :-
    saved_sig(cA, SigSaved),
    struct_def(S, Members), member(P, Members),
    member(J, Members),
    struct_sse_t(S, banquet, SigSaved),
    rel_map(d, belongs_to, RB1),
    rel_map(d, visits, RB2),
    memory_relation(J, RB1, P, _, _),
    memory_relation(P, RB2, M, _, _),
    struct_def(SF, MembersF), member(P, MembersF),
    struct_sig(cF, SigF),
    struct_sse_t(SF, banquet, SigF),
    Proof = [compositeE(cA, cF, P), reuseC(S, [RB1, RB2]), member(SF, cF)].

run_immutability_tests :-
    nl, writeln('===== IMMUTABLE C + NEW E ====='),
    (predict_banquet(pico, aconcagua, PP) ->
        (check(true, 'E fires on D-train via concepts'),
         format('E train proof: ~w~n', [PP]))
    ; check(false, 'E fires on D-train via concepts')),
    (predict_banquet(odin, kilimanjaro, PE) ->
        (check(true, 'E: banquet(odin,kilimanjaro) (never observed)'),
         format('E proof: ~w~n', [PE]))
    ; check(false, 'E: banquet(odin,kilimanjaro) (never observed)')),
    (predict_banquet(hal, _, _) ->
        check(false, 'hal (C only, no cF) -> UNKNOWN')
    ; (check(true, 'hal (C only, no cF) -> UNKNOWN'),
       format('hal -> UNKNOWN~n', []))),
    (predict_banquet(odin, olympus, _) ->
        check(false, 'cross odin->olympus rejected')
    ; check(true, 'cross odin->olympus rejected')),
    snapshot_c(post_e),
    check((c_snapshot(post_a, S1), c_snapshot(post_e, S2), S1 == S2),
          'C BEFORE E == C AFTER E (2nd immutability)'),
    findall((S, R, O), memory_relation(S, R, O, _, _), Fs),
    \+ member((_, owns, _), Fs),
    \+ member((_, belongs_to, _), Fs),
    \+ member((_, visits, _), Fs),
    \+ member((_, keeps, _), Fs),
    \+ member((_, held_by, _), Fs),
    \+ member((_, tours, _), Fs),
    check(true, 'zero A/B relations at E query time (audit)').

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
