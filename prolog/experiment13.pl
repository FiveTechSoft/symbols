% experiment13.pl
% EXPERIMENT 13 - LANGUAGE -> SYMBOLIC GRAPH
% Mismo motor que EXP1-12; solo cambia la ENTRADA (frases -> grafo).
% Capas: perfect (hechos directos) | clean | variation | noise.
% Held-out: compra de {juan,pablo,diego} omitida en TODAS las capas.
:- consult('memory.pl').
:- consult('language_graph.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment13 :-
    reset_experiment,
    gold_facts(Gold),
    heldout(HPos, HNeg),
    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 13 - LANGUAGE TO GRAPH'),
    writeln('=============================================='),
    run_layer(perfect, Gold, HPos, HNeg),
    layer_sentences(clean, Gold, CleanS),
    run_layer(clean, CleanS, HPos, HNeg),
    layer_sentences(variation, Gold, VarS),
    run_layer(variation, VarS, HPos, HNeg),
    layer_sentences(noise, Gold, NoiseS),
    run_layer(noise, NoiseS, HPos, HNeg),
    run_adversarial,
    nl, writeln('LAYER COMPARISON: see per-layer F1 above.').

% Fuera de lexico / ambiguo: el simbolizador debe RECHAZAR, no alucinar.
adversarial_sentences([
    "juan alquila pan",
    "maria presta leche en sevilla",
    "pedro compra pan y queso",
    "ana come manzana y pera",
    "el pan lo vende juan y maria",
    "ayer llovera mucho en malaga"
]).

run_adversarial :-
    nl, writeln('===== ADVERSARIAL (must reject all) ====='),
    adversarial_sentences(Ss),
    findall(S, ( member(S, Ss),
                 symbolize_sentence(S, T),
                 format('EMITTED (bad): ~w -> ~w~n', [S, T])
               ),
            Bad),
    length(Bad, NB),
    length(Ss, NS),
    Rej is NS - NB,
    format('rejected ~w/~w (no hallucinated triples)~n', [Rej, NS]).

reset_experiment :-
    clear_memory,
    retractall(concept(_, _, _)),
    retractall(concept_member(_, _, _)),
    retractall(concept_relation(_, _, _, _)),
    retractall(learned_rule(_, _, _, _, _)).

% ---------- datos gold (patron EXP1, held-out omitido) ----------
person_idx(juan, 1). person_idx(maria, 2). person_idx(pedro, 3).
person_idx(ana, 4). person_idx(luis, 5). person_idx(carmen, 6).
person_idx(pablo, 7). person_idx(lucia, 8). person_idx(miguel, 9).
person_idx(elena, 10). person_idx(david, 11). person_idx(sara, 12).
person_idx(jorge, 13). person_idx(laura, 14). person_idx(diego, 15).
person_idx(ines, 16). person_idx(raul, 17). person_idx(nadia, 18).
person_idx(ivan, 19). person_idx(vera, 20).

food_idx(pan, 1). food_idx(leche, 2). food_idx(queso, 3).
food_idx(manzana, 4). food_idx(arroz, 5). food_idx(pollo, 6).
food_idx(pescado, 7). food_idx(huevo, 8). food_idx(tomate, 9).
food_idx(cebolla, 10). food_idx(ajo, 11). food_idx(pimiento, 12).
food_idx(platano, 13). food_idx(naranja, 14). food_idx(uva, 15).
food_idx(pera, 16). food_idx(fresa, 17). food_idx(limon, 18).
food_idx(melon, 19). food_idx(sandia, 20).

city_idx(malaga, 1). city_idx(sevilla, 2). city_idx(granada, 3).
city_idx(cordoba, 4). city_idx(almeria, 5).

food_n(N, F) :- food_idx(F, N).
city_n(N, C) :- city_idx(C, N).
food_at(I, Off, F) :- J is ((I - 1 + Off) mod 20) + 1, food_n(J, F).
city_at(I, C) :- J is ((I - 1) mod 5) + 1, city_n(J, C).

heldout_person(1). heldout_person(7). heldout_person(13).

% vende uniforme en 4 personas (firma estable, umbral 0.80 lo tolera)
vende_fact(pedro, arroz). vende_fact(jorge, pera).
vende_fact(laura, leche). vende_fact(nadia, melon).

gold_facts(Gold) :-
    findall((P, compra, F),
            ( person_idx(P, I),
              \+ heldout_person(I),
              ( food_at(I, 1, F) ; food_at(I, 2, F) )
            ),
            Buys),
    findall((P, come, F),
            ( person_idx(P, I), food_at(I, 3, F) ),
            Eats),
    findall((P, vive_en, C),
            ( person_idx(P, I), city_at(I, C) ),
            Lives),
    findall((P, vende, F), vende_fact(P, F), Sells),
    append([Buys, Eats, Lives, Sells], All),
    sort(All, Gold).

heldout(HPos, HNeg) :-
    findall((P, compra, F),
            ( heldout_person(I), person_idx(P, I),
              ( food_at(I, 1, F) ; food_at(I, 2, F) )
            ),
            HPos0),
    sort(HPos0, HPos),
    findall((P, vive_en, F),
            ( heldout_person(I), person_idx(P, I),
              ( food_at(I, 1, F) ; food_at(I, 2, F) )
            ),
            HNeg0),
    sort(HNeg0, HNeg).

% ---------- generacion de frases por capa ----------
% plantillas limpias
tpl(compra, "~w compra ~w.").
tpl(come, "~w come ~w.").
tpl(vive_en, "~w vive en ~w.").
tpl(vende, "~w vende ~w.").

% variantes (misma tripleta, superficie distinta)
var_tpl(compra, 0, "~w compra ~w.").
var_tpl(compra, 1, "El ~w lo compra ~w.").   % OVS: objeto primero
var_tpl(compra, 2, "~w está comprando ~w.").
var_tpl(compra, 3, "~w compró ~w.").
var_tpl(compra, 4, "El ~w es comprado por ~w.").
var_tpl(come, 0, "~w come ~w.").
var_tpl(come, 1, "El ~w lo come ~w.").
var_tpl(come, 2, "~w está comiendo ~w.").
var_tpl(come, 3, "~w comió ~w.").
var_tpl(vive_en, 0, "~w vive en ~w.").
var_tpl(vive_en, 1, "~w reside en ~w.").
var_tpl(vive_en, 2, "~w vivió en ~w.").
var_tpl(vive_en, 3, "Donde vive ~w es ~w.").
var_tpl(vende, 0, "~w vende ~w.").
var_tpl(vende, 1, "~w vendió ~w.").

% ruido: tiempo + manera + PP locativo (el tipo filtra lo irrelevante)
noise_pref(["ayer ", "hoy ", ""]).
noise_adv([" rápidamente", " tranquilamente", ""]).
noise_loc([" en malaga", " en sevilla", ""]).

layer_sentences(clean, Gold, Ss) :-
    findall(S, ( member((X, V, Y), Gold),
                 tpl(V, T), format(string(S), T, [X, Y])
               ),
            Ss).
layer_sentences(variation, Gold, Ss) :-
    findall(S, ( member((X, V, Y), Gold),
                 fact_variant_index((X, V, Y), K),
                 var_tpl_count(V, N), K2 is K mod N,
                 var_tpl(V, K2, T),
                 var_args(V, K2, X, Y, A, B),
                 format(string(S), T, [A, B])
               ),
            Ss).
layer_sentences(noise, Gold, Ss) :-
    findall(S, ( member((X, V, Y), Gold),
                 fact_variant_index((X, V, Y), K),
                 noise_pref(Ps), nth0(K2, Ps, P), K2 is K mod 3,
                 noise_adv(As), nth0(K3, As, A), K3 is (K + 1) mod 3,
                 noise_loc(Ls), nth0(K4, Ls, L0), K4 is (K + 2) mod 3,
                 % vive ya lleva su ciudad como argumento: no apilar loc
                 % (generaria "en X en Y", ambiguo hasta para humanos)
                 ( V == vive_en -> L = "" ; L = L0 ),
                 noise_base(V, X, Y, Base),
                 string_concat(P, Base, P2),
                 string_concat(P2, A, P3),
                 string_concat(P3, L, P4),
                 string_concat(P4, ".", S)
               ),
            Ss).

% indice determinista por hecho (round-robin de plantillas)
fact_variant_index(Fact, K) :-
    gold_facts(Gold),
    nth0(K, Gold, Fact).
var_tpl_count(compra, 5). var_tpl_count(come, 4).
var_tpl_count(vive_en, 4). var_tpl_count(vende, 2).

% OVS/pasiva invierten el orden de los argumentos en la plantilla
var_args(_, K, X, Y, X, Y) :- K =:= 0, !.
var_args(compra, 1, X, Y, Y, X) :- !.  % El Y lo compra X
var_args(compra, 4, X, Y, Y, X) :- !.  % El Y es comprado por X
var_args(come, 1, X, Y, Y, X) :- !.
var_args(vive_en, 3, X, Y, X, Y) :- !. % Donde vive X es Y
var_args(_, _, X, Y, X, Y).

% base en pasado para la capa de ruido
noise_base(compra, X, Y, B) :-
    format(string(B), "~w compró ~w", [X, Y]).
noise_base(come, X, Y, B) :-
    format(string(B), "~w comió ~w", [X, Y]).
noise_base(vive_en, X, Y, B) :-
    format(string(B), "~w vivió en ~w", [X, Y]).
noise_base(vende, X, Y, B) :-
    format(string(B), "~w vendió ~w", [X, Y]).

% ---------- ejecucion por capa ----------
run_layer(perfect, Gold, HPos, HNeg) :-
    reset_experiment,
    forall(member((S, V, O), Gold),
           remember_relation(S, V, O, 1.0)),
    evaluate_layer(perfect, Gold, Gold, 1.0, 1.0, HPos, HNeg).
run_layer(Layer, Sentences, HPos, HNeg) :-
    Layer \== perfect,
    reset_experiment,
    gold_facts(Gold),
    symbolize_corpus(Sentences, Gold, Extracted, P, R),
    forall(member((S, V, O), Extracted),
           remember_relation(S, V, O, 1.0)),
    evaluate_layer(Layer, Gold, Extracted, P, R, HPos, HNeg).

evaluate_layer(Layer, Gold, Extracted, P, R, HPos, HNeg) :-
    length(Gold, NG),
    length(Extracted, NE),
    format('~n===== LAYER ~w =====~n', [Layer]),
    format('gold=~w extracted=~w extraction P=~4f R=~4f~n',
           [NG, NE, P, R]),
    discover_concepts,
    discover_concept_relations,
    discover_rules,
    show_concept_summary,
    run_heldout(HPos, HNeg, TP, FN, FP, TN),
    DenP is TP + FP,
    ( DenP =:= 0 -> Prec = 0.0 ; Prec is TP / DenP ),
    Rec is TP / 6,
    ( Prec + Rec =:= 0 -> F1 = 0.0
    ; F1 is 2 * Prec * Rec / (Prec + Rec)
    ),
    format('heldout TP=~w FN=~w FP=~w TN=~w F1=~4f~n',
           [TP, FN, FP, TN, F1]).

show_concept_summary :-
    findall(C, concept(C, _, _), Cs),
    length(Cs, N),
    findall(Rl, learned_rule(Rl, _, _, _, _), Rs),
    length(Rs, NR),
    format('concepts=~w rules=~w~n', [N, NR]).

run_heldout(HPos, HNeg, TP, FN, FP, TN) :-
    findall(1, (member((S, V, O), HPos), infer(S, V, O, _)), TPL),
    length(TPL, TP),
    findall(1, (member((S, V, O), HPos), \+ infer(S, V, O, _)), FNL),
    length(FNL, FN),
    findall(1, (member((S, V, O), HNeg), infer(S, V, O, _)), FPL),
    length(FPL, FP),
    findall(1, (member((S, V, O), HNeg), \+ infer(S, V, O, _)), TNL),
    length(TNL, TN).

% --- descubrimiento estilo EXP1 (umbral 0.80, idempotente) ---

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

show_concepts :-
    nl, writeln('===== DISCOVERED CONCEPTS ====='),
    forall(concept(C, Sig, _),
           ( format('~w  ~w~n', [C, Sig]),
             forall(concept_member(C, E, Sc),
                    format('   ~w ~2f~n', [E, Sc])),
             nl
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

discover_rules :-
    forall(concept_relation(SC, R, OC, Sc),
           assertz(learned_rule(rule(SC, R, OC), SC, R, OC, Sc))).

infer(S, R, O, Sc) :-
    learned_rule(_, SC, R, OC, RS),
    concept_member(SC, S, SS),
    concept_member(OC, O, OS),
    Sc is RS * SS * OS.
