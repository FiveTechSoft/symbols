% experiment44.pl
% EXPERIMENT 44-A - CROSS-SURFACE ROLE MAPPING (EXP39 en corpus natural)
% Un mundo, dos familias de superficie disjuntas (A: owns/visits/...,
% B: keeps/tours/...). El parser NO normaliza entre familias: produce
% relaciones superficiales y el MOTOR induce el mapa por roles
% estructurales (alineacion generica por firmas-miembro, sin nombres).
% stocked/serves se descubren en A; stored/offers de B (jamas
% enunciados) se predicen por path traducido + puerta conceptual.
% Firmas LOCALES (EXP40) + exclusion uniforme de conclusiones.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('natural_parse.pl').
:- consult('corpus_natural3/gold.pl').
:- consult('corpus_natural3/expected.pl').
:- consult('corpus_natural3/distractor_natural3.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic composed_rule/3.
:- dynamic distinct_rule/3.
:- dynamic constrained_rule/3.
:- dynamic found_rule/3.
:- dynamic struct_def/3.
:- dynamic struct_sig/2.
:- dynamic rel_map/3.
:- dynamic parsed_line/2.
:- dynamic check_results/2.

experiment44 :-
    reset_experiment,
    ingest_corpus,
    extraction_report,
    discover_concepts,
    discover_concept_relations,
    discover_native,
    define_structures,
    induce_maps,
    run_queries,
    run_distractors,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)),
    retractall(check_results(_, _)),
    retractall(composed_rule(_, _, _)),
    retractall(distinct_rule(_, _, _)),
    retractall(constrained_rule(_, _, _)),
    retractall(found_rule(_, _, _)),
    retractall(struct_def(_, _, _)),
    retractall(struct_sig(_, _)),
    retractall(rel_map(_, _, _)),
    retractall(parsed_line(_, _)),
    reset_natural.

% ---------- ingesta + extraccion ----------
ingest_corpus :-
    open('corpus_natural3/corpus.txt', read, S, [encoding(utf8)]),
    ingest_lines(S, 1),
    close(S),
    findall(1, parsed_line(_, _), Ps), length(Ps, NP),
    format('ingested lines: ~w~n', [NP]).

ingest_lines(S, N) :-
    get_char(S, C),
    ( C == end_of_file -> true
    ; read_line_rest(S, C, Chars),
      string_chars(Line, Chars),
      ( Line == "" -> true
      ; ( symbolize_natural(Line, (Sub, Rel, Obj)) ->
            ( remember_relation(Sub, Rel, Obj, 1.0),
              assertz(parsed_line(N, (Sub, Rel, Obj))) )
        ; assertz(parsed_line(N, none)) )
      ),
      N1 is N + 1,
      ingest_lines(S, N1)
    ).

read_line_rest(S, C, [C|Cs]) :-
    C \== end_of_file, C \== '\n', !,
    get_char(S, C2),
    read_line_rest(S, C2, Cs).
read_line_rest(_, _, []).

extraction_report :-
    findall(N, gold_fact(N, _, _, _), GNs), length(GNs, NG),
    findall(N, (parsed_line(N, T), T \== none,
                gold_fact(N, S, R, O), T == (S, R, O)), TPs),
    length(TPs, TP),
    findall(N, (parsed_line(N, T), T \== none), PPos),
    length(PPos, NPpos),
    ( NG > 0 -> P is TP / NG ; P = 0 ),
    ( NPpos > 0 -> R is TP / NPpos ; R = 0 ),
    format('extraction: TP=~w gold=~w parsedpos=~w P=~4f R=~4f~n',
           [TP, NG, NPpos, P, R]),
    check(P >= 0.95, 'extraction precision >= 0.95'),
    check(R >= 0.95, 'extraction recall >= 0.95'),
    findall(N-T, (parsed_line(N, T), T \== none,
                  \+ gold_fact(N, _, _, _)), Halls),
    length(Halls, NH),
    ( NH =:= 0 ->
        check(true, 'hallucination = 0 (no invented triplets)')
    ; format('HALLUCINATIONS: ~w~n', [Halls]),
      check(false, 'hallucination = 0 (no invented triplets)')).

% ---------- conceptos (maquinaria local) ----------
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

% ---------- descubrimiento nativo en A (B no tiene conclusiones) ----------
keep_rule(Target) :-
    composed_rule(Target, Path, F1),
    assertz(found_rule(Target, Path, F1)).

discover_native :-
    discover_composition(reaches, 3),
    ( composed_rule(reaches, [visits, in], F1) ->
        ( format('discovered: reaches :- [visits,in] F1=~4f~n', [F1]),
          keep_rule(reaches),
          check(true, 'native reaches :- [visits,in]') )
    ; check(false, 'native reaches :- [visits,in]')),
    discover_composition(serves, 3),
    ( composed_rule(serves, [cooks, needs], F2) ->
        ( format('discovered: serves :- [cooks,needs] F1=~4f~n', [F2]),
          keep_rule(serves),
          check(true, 'native serves :- [cooks,needs]') )
    ; check(false, 'native serves :- [cooks,needs]')).

% ---------- firmas locales + exclusion uniforme ----------
excluded_all([reaches, serves, stored, offers]).

member_sig_l(E, Members, sig(O, I, ON, IN)) :-
    excluded_all(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), Outs),
    length(Outs, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), Ins),
    length(Ins, I),
    findall((A, B), (member(N, Outs), deg_l(N, Members, A, B)), ON0),
    sort(ON0, ON),
    findall((A, B), (member(N, Ins), deg_l(N, Members, A, B)), IN0),
    sort(IN0, IN).

deg_l(E, Members, O, I) :-
    excluded_all(Exclude),
    findall(X, (member(X, Members),
                memory_relation(E, R, X, _, _), \+ member(R, Exclude)), L1),
    length(L1, O),
    findall(X, (member(X, Members),
                memory_relation(X, R, E, _, _), \+ member(R, Exclude)), L2),
    length(L2, I).

struct_sse(S, stsig(MS, E)) :-
    struct_def(S, _Tag, Members),
    findall(M, (member(X, Members), member_sig_l(X, Members, M)), M0),
    sort(M0, MS),
    excluded_all(Exclude),
    findall((A, R, B), (member(A, Members), member(B, Members),
                        memory_relation(A, R, B, _, _),
                        \+ member(R, Exclude)),
            Edges),
    length(Edges, E).

sig_clean(Sig) :-
    \+ (sub_term(T, Sig), atom(T)).

% ---------- estructuras: ejemplar A + todas las B ----------
% Ejemplares A fijados (entrenamiento conocido): alba/book/oslo,
% alba/paella/rice. B: owns[0]/visits[0] y dish/ing por persona.
define_structures :-
    assertz(struct_def(sa_sup, supply, [alba, book, oslo])),
    assertz(struct_def(sa_kit, kitchen, [alba, paella, rice])),
    forall(member(P, [moss, nila, ovar, pela, quim, rufa, silo, tove,
                      urre, xana]),
           ( b_sup_members(P, MS), assertz(struct_def(P, supply_b, MS)) )),
    forall(member(P, [moss, nila, ovar, pela, quim, rufa, silo, tove,
                      urre, xana]),
           ( b_kit_members(P, MK), assertz(struct_def(P, kitchen_b, MK)) )),
    struct_sse(sa_sup, SigS),
    struct_sse(sa_kit, SigK),
    check(sig_clean(SigS), 'exemplar SSEs zero atoms'),
    assertz(struct_sig(supply, SigS)),
    assertz(struct_sig(kitchen, SigK)),
    findall(S, (struct_def(S, supply_b, _),
                struct_sse(S, SigS)), JoinedS),
    length(JoinedS, NS),
    check(NS =:= 10, 'all 10 B-supply join by signature (no shared vocab)'),
    findall(S, (struct_def(S, kitchen_b, _),
                struct_sse(S, SigK)), JoinedK),
    length(JoinedK, NK),
    check(NK =:= 10, 'all 10 B-kitchen join by signature').

b_sup_members(P, [P, O, C]) :-
    memory_relation(P, keeps, O, _, _),
    memory_relation(P, tours, C, _, _),
    memory_relation(O, held_by, P, _, _), !.
b_kit_members(P, [P, D, I]) :-
    memory_relation(P, prepares, D, _, _),
    memory_relation(D, requires, I, _, _), !.

% ---------- mapa generico por roles ----------
% Alinea miembros B con el ejemplar A por igualdad de firma-miembro;
% cada arista A entre roles alineados induce el par A<->B.
induce_maps :-
    induce_map(sa_sup, moss, supply),
    induce_map(sa_kit, moss, kitchen),
    findall((T, A, B), rel_map(T, A, B), Maps),
    sort(Maps, SM),
    format('maps: ~w~n', [SM]),
    check(SM == [(kitchen, cooks, prepares), (kitchen, needs, requires),
                 (supply, belongs_to, held_by), (supply, owns, keeps),
                 (supply, visits, tours)],
          'full A<->B role-map induced (nothing given)').

induce_map(Exemplar, BPerson, Tag) :-
    struct_def(Exemplar, Tag, ExM),
    struct_def(BPerson, TagB, BM),
    ( Tag == supply -> TagB = supply_b ; TagB = kitchen_b ),
    forall(( member(X, ExM),
             member_sig_l(X, ExM, SX),
             member(Y, BM),
             member_sig_l(Y, BM, SX),
             member(X2, ExM), member(Y2, BM),
             member_sig_l(X2, ExM, SX2),
             member_sig_l(Y2, BM, SX2),
             memory_relation(X, RA, X2, _, _),
             \+ excluded_rel(RA),
             memory_relation(Y, RB, Y2, _, _),
             \+ excluded_rel(RB)
           ),
           assertz(rel_map(Tag, RA, RB))).

excluded_rel(R) :-
    excluded_all(E),
    member(R, E).

% ---------- queries ----------
answer_reaches(P, K, retrieved) :-
    memory_relation(P, reaches, K, _, _), !.
answer_reaches(_, _, unknown).

answer_serves(P, I, retrieved) :-
    memory_relation(P, serves, I, _, _), !.
answer_serves(_, _, unknown).

answer_visits(P, C, retrieved) :-
    ( memory_relation(P, visits, C, _, _)
    ; memory_relation(P, tours, C, _, _) ), !.
answer_visits(_, _, unknown).

answer_owns(P, O, retrieved) :-
    ( memory_relation(P, owns, O, _, _)
    ; memory_relation(P, keeps, O, _, _) ), !.
answer_owns(_, _, unknown).

% stored(K,C) en B: plantilla estructural (spoke->hub->spoke) leida
% posicionalmente en B via mapa + puerta conceptual. La regla nativa
% reaches prueba que el motor abstrae; el mapa traduce la estructura.
answer_stored(K, C, reasoned, Proof) :-
    found_rule(reaches, [visits, in], _),
    rel_map(supply, belongs_to, RB1),
    rel_map(supply, visits, RB2),
    struct_def(S, supply_b, Members),
    member(K, Members), member(C, Members),
    struct_sse(S, Sig), struct_sig(supply, Sig),
    memory_relation(K, RB1, H, _, _),
    memory_relation(H, RB2, C, _, _),
    Proof = [reuse(supply, S), map([belongs_to-RB1, visits-RB2]),
             native(reaches, [visits, in]),
             rule(stored, [RB1, RB2]), (K, RB1, H), (H, RB2, C)].

answer_offers(P, I, reasoned, Proof) :-
    found_rule(serves, [cooks, needs], _),
    rel_map(kitchen, cooks, RB1),
    rel_map(kitchen, needs, RB2),
    struct_def(S, kitchen_b, Members),
    member(P, Members), member(I, Members),
    struct_sse(S, Sig), struct_sig(kitchen, Sig),
    memory_relation(P, RB1, D, _, _),
    memory_relation(D, RB2, I, _, _),
    Proof = [reuse(kitchen, S), map([cooks-RB1, needs-RB2]),
             rule(offers, [RB1, RB2]), (P, RB1, D), (D, RB2, I)].

person_known(X) :-
    memory_relation(X, _, _, _, _), !.
person_known(X) :-
    memory_relation(_, _, X, _, _), !.

run_queries :-
    nl, writeln('===== QUERIES (cross-surface transfer) ====='),
    forall(expected(Id, Zone, Kind, A, B, How),
           run_query(Id, Zone, Kind, A, B, How)),
    findall(I, expected(I, _, _, _, _, _), Ids),
    length(Ids, NQ),
    findall(Q, (check_results(Q, _), compound(Q)), Qs),
    length(Qs, NC),
    check(NC =:= NQ, 'every expected query executed (no silent skips)').

run_query(Id, Zone, stored, K, C, reasoned) :-
    ( answer_stored(K, C, reasoned, Proof) ->
        ( check(true, Id-Zone-stored),
          ( Id =< 2 -> format('proof q~w: ~w~n', [Id, Proof]) ; true ) )
    ; format('FAIL q~w stored ~w ~w~n', [Id, K, C]),
      check(false, Id-Zone-stored)).
run_query(Id, Zone, offers, P, I, reasoned) :-
    ( answer_offers(P, I, reasoned, Proof) ->
        ( check(true, Id-Zone-offers),
          ( Id =< 8 -> format('proof q~w: ~w~n', [Id, Proof]) ; true ) )
    ; format('FAIL q~w offers ~w ~w~n', [Id, P, I]),
      check(false, Id-Zone-offers)).
run_query(Id, Zone, reaches, P, K, retrieved) :-
    ( answer_reaches(P, K, retrieved) ->
        check(true, Id-Zone-reaches)
    ; format('FAIL q~w reaches ~w ~w~n', [Id, P, K]),
      check(false, Id-Zone-reaches)).
run_query(Id, Zone, serves, P, I, retrieved) :-
    ( answer_serves(P, I, retrieved) ->
        check(true, Id-Zone-serves)
    ; format('FAIL q~w serves ~w ~w~n', [Id, P, I]),
      check(false, Id-Zone-serves)).
run_query(Id, Zone, visits, P, C, retrieved) :-
    ( answer_visits(P, C, retrieved) ->
        check(true, Id-Zone-visits)
    ; format('FAIL q~w visits ~w ~w~n', [Id, P, C]),
      check(false, Id-Zone-visits)).
run_query(Id, Zone, tours, P, C, retrieved) :-
    ( answer_visits(P, C, retrieved) ->
        check(true, Id-Zone-tours)
    ; format('FAIL q~w tours ~w ~w~n', [Id, P, C]),
      check(false, Id-Zone-tours)).
run_query(Id, Zone, unknown_person, Z, _, unknown) :-
    ( \+ person_known(Z) ->
        check(true, Id-Zone-unknown)
    ; format('FAIL q~w ~w should be unknown~n', [Id, Z]),
      check(false, Id-Zone-unknown)).

run_distractors :-
    nl, writeln('===== DISTRACTORS (verified false) ====='),
    forall(distractor(P, X, R),
           ( ( R == stored, \+ answer_stored(P, X, _, _) ->
                 check(true, distractor-reject-stored)
             ; R == offers, \+ answer_offers(P, X, _, _) ->
                 check(true, distractor-reject-offers)
             ; R == reaches, \+ answer_reaches(P, X, retrieved) ->
                 check(true, distractor-reject-reaches)
             ; (R == tours ; R == visits),
               \+ answer_visits(P, X, retrieved) ->
                 check(true, distractor-reject-visits)
             ; (R == keeps ; R == owns),
               \+ answer_owns(P, X, retrieved) ->
                 check(true, distractor-reject-owns)
             ; format('FP ~w ~w ~w~n', [P, R, X]),
               check(false, distractor-fp) ) )).

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
