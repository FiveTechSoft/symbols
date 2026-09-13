% conversation.pl
% EXP45 - FIRST SYMBOLIC CHAT (etapa 1: conversacion minima)
% ask/2 pregunta->respuesta con prueba; say/1 verbaliza; why/0 explica
% la ultima respuesta (o su ausencia: UNKNOWN con razon, no alucinacion).
% ~12 formas: si/no, quien/que/donde, why. La respuesta se genera desde
% el proof, nunca por continuacion estadistica. Sin tocar el motor.
:- consult('memory.pl').
:- consult('composition.pl').
:- consult('natural_parse.pl').
:- consult('corpus_natural2/gold.pl').
:- consult('corpus_natural2/expected.pl').
:- consult('corpus_natural2/distractor_natural2.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
:- dynamic composed_rule/3.
:- dynamic distinct_rule/3.
:- dynamic constrained_rule/3.
:- dynamic found_rule/3.
:- dynamic last_proof/1.
:- dynamic check_results/2.

% ---------- carga del mundo (ingesta + descubrimiento, como EXP43) ----------
load_demo :-
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
    retractall(last_proof(_)),
    reset_natural,
    open('corpus_natural2/corpus.txt', read, S, [encoding(utf8)]),
    ingest_lines(S),
    close(S),
    discover_concepts,
    discover_concept_relations,
    discover_composition(reaches, 3),
    ( composed_rule(reaches, [visits, in], F1) ->
        assertz(found_rule(reaches, [visits, in], F1))
    ; true ),
    discover_composition(based, 3),
    ( composed_rule(based, [works_at, located, in], F2) ->
        assertz(found_rule(based, [works_at, located, in], F2))
    ; true ).

ingest_lines(S) :-
    get_char(S, C),
    ( C == end_of_file -> true
    ; read_line_rest(S, C, Chars),
      string_chars(Line, Chars),
      ( Line == "" -> true
      ; ( symbolize_natural(Line, (Sub, Rel, Obj)) ->
            remember_relation(Sub, Rel, Obj, 1.0)
        ; true )
      ),
      ingest_lines(S)
    ).

read_line_rest(S, C, [C|Cs]) :-
    C \== end_of_file, C \== '\n', !,
    get_char(S, C2),
    read_line_rest(S, C2, Cs).
read_line_rest(_, _, []).

% ---------- conceptos (maquinaria local estándar) ----------
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

% ---------- preguntas (~12 formas) ----------
% ask(+QuestionString, -Answer). Answer: yes(Proof) | no | unknown |
% some(Xs, How) | none | proof(Proof) | noproof(Reason).
ask(Q, A) :-
    tokenize_nat(Q, Tokens),
    ( chat_parse(Tokens, A) ->
        ( remember_proof(A) -> true ; true )
    ; A = unknown ).

remember_proof(yes(Proof)) :-
    retractall(last_proof(_)), assertz(last_proof(Proof)), !.
remember_proof(some(_, _, Proof)) :-
    retractall(last_proof(_)), assertz(last_proof(Proof)), !.
remember_proof(_) :-
    retractall(last_proof(_)), assertz(last_proof(none)).

person_known(X) :-
    memory_relation(X, _, _, _, _), !.
person_known(X) :-
    memory_relation(_, _, X, _, _), !.

chat_parse([does, P, reach, K], yes(Proof)) :-
    found_rule(reaches, [visits, in], _),
    memory_relation(P, visits, C, _, _),
    memory_relation(C, in, K, _, _), !,
    Proof = [rule(reaches, [visits, in]), (P, visits, C), (C, in, K)].
chat_parse([does, P, reach, K], unknown) :-
    \+ person_known(P), !.
chat_parse([does, _, reach, _], no).
chat_parse([does, P, visit, C], yes([(P, visits, C)])) :-
    memory_relation(P, visits, C, _, _), !.
chat_parse([does, P, visit, C], unknown) :-
    \+ person_known(P), !.
chat_parse([does, _, visit, _], no).
chat_parse([does, P, live, in, C], yes([(P, lives_in, C)])) :-
    memory_relation(P, lives_in, C, _, _), !.
chat_parse([does, P, live, in, _], unknown) :-
    \+ person_known(P), !.
chat_parse([does, _, live, in, _], no).
chat_parse([is, P, based, in, K], yes(Proof)) :-
    found_rule(based, [works_at, located, in], _),
    memory_relation(P, works_at, G, _, _),
    memory_relation(G, located, C, _, _),
    memory_relation(C, in, K, _, _), !,
    Proof = [rule(based, [works_at, located, in]),
             (P, works_at, G), (G, located, C), (C, in, K)].
chat_parse([is, P, based, in, _], unknown) :-
    \+ person_known(P), !.
chat_parse([is, _, based, in, _], no).
chat_parse([who, visits, C], some(Ps, retrieved, [(visits, C)])) :-
    findall(P, memory_relation(P, visits, C, _, _), P0),
    sort(P0, Ps), Ps \== [], !.
chat_parse([who, visits, _], none).
chat_parse([who, reaches, K], some(Ps, reasoned, Proof)) :-
    found_rule(reaches, [visits, in], _),
    findall(P, (memory_relation(P, visits, C, _, _),
                memory_relation(C, in, K, _, _)), P0),
    sort(P0, Ps), Ps \== [], !,
    Proof = [rule(reaches, [visits, in]), reaches(K)].
chat_parse([who, reaches, _], none).
chat_parse([where, does, P, reach], some(Ks, reasoned, Proof)) :-
    found_rule(reaches, [visits, in], _),
    person_known(P),
    findall(K, (memory_relation(P, visits, C, _, _),
                memory_relation(C, in, K, _, _)), K0),
    sort(K0, Ks), Ks \== [], !,
    Proof = [rule(reaches, [visits, in]), reacher(P)].
chat_parse([where, does, P, reach], unknown) :-
    \+ person_known(P), !.
chat_parse([where, does, _, reach], none).
chat_parse([where, does, P, live], some([C], retrieved, [(P, lives_in, C)])) :-
    memory_relation(P, lives_in, C, _, _), !.
chat_parse([where, does, P, live], unknown) :-
    \+ person_known(P), !.
chat_parse([where, does, _, live], none).
chat_parse([what, does, P, own], some(Os, retrieved, [(P, owns, _)])) :-
    findall(O, memory_relation(P, owns, O, _, _), O0),
    sort(O0, Os), Os \== [], !.
chat_parse([what, does, P, own], unknown) :-
    \+ person_known(P), !.
chat_parse([what, does, _, own], none).
chat_parse([why], proof(Proof)) :-
    last_proof(Proof), Proof \== none, !.
chat_parse([why], noproof('No valid proof exists.')).

% ---------- verbalizacion desde el proof ----------
say(yes(Proof)) :-
    writeln('Yes.'),
    say_proof(Proof).
say(no) :-
    writeln('No.').
say(unknown) :-
    writeln('I don\'t know.').
say(some(Xs, _, _)) :-
    atomic_list_concat(Xs, ', ', A),
    format('~w.~n', [A]).
say(none) :-
    writeln('Nobody.').
say(proof(Proof)) :-
    writeln('Because:'),
    say_proof(Proof).
say(noproof(Reason)) :-
    writeln('UNKNOWN.'),
    format('Reason: ~w~n', [Reason]).

say_proof([rule(R, Path)|Steps]) :- !,
    forall(member(St, Steps), say_fact(St)),
    format('Rule: ~w :- ~w.~n', [R, Path]),
    writeln('Confidence: 1.00').
say_proof([Single]) :- !,
    say_fact(Single),
    writeln('Confidence: 1.00').
say_proof([]) :-
    writeln('Confidence: 1.00').
% fallback generico (EXP47): pruebas anidadas/compuestas se recorren
% paso a paso; los pasos conocidos se verbalizan, el resto se muestra.
say_proof(Proof) :-
    is_list(Proof), !,
    forall(member(St, Proof), say_step(St)),
    writeln('Confidence: 1.00').

say_step(rule(R, Path)) :- !,
    format('Rule: ~w :- ~w.~n', [R, Path]).
say_step(map(Pairs)) :- !,
    format('Transfer: ~w.~n', [Pairs]).
say_step(reuse(C, S)) :- !,
    format('Reuse concept ~w at ~w.~n', [C, S]).
say_step(member(S, C)) :- !,
    format('Member ~w of ~w.~n', [S, C]).
say_step((S, R, O)) :- !,
    format('~w ~w ~w.~n', [S, R, O]).
say_step(Other) :-
    format('~w.~n', [Other]).

say_fact((S, R, O)) :- !,
    format('~w ~w ~w.~n', [S, R, O]).
say_fact(Other) :-
    format('~w.~n', [Other]).

% ---------- dialogo de prueba (14 intercambios) ----------
conversation_demo :-
    load_demo,
    nl, writeln('===== DIALOGUE (scripted, expected answers) ====='),
    t("Does alba reach norway?", yes(_)),
    t("Why?", proof(_)),
    t("Does alba reach ireland?", no),
    t("Who visits oslo?", some(_, _, _)),
    t("Where does alba reach?", some(_, _, _)),
    t("Is carla based in switzerland?", yes(_)),
    t("Why?", proof(_)),
    t("Does zorin visit madrid?", unknown),
    t("Why?", noproof(_)),
    t("What does alba own?", some(_, _, _)),
    t_has("Who visits oslo?", alba),
    t_has("Where does alba reach?", norway),
    t_has("Who reaches italy?", carla),
    t("Does quinn reach ecuador?", no),
    t("Who reaches norway?", some(_, _, _)),
    t("Where does zorin reach?", unknown),
    report_checks.

t(Q, Expected) :-
    ask(Q, A),
    format('> ~w~n', [Q]),
    say(A),
    ( match_expected(A, Expected) ->
        check(true, Q)
    ; format('MISMATCH got ~w want ~w~n', [A, Expected]),
      check(false, Q)).

match_expected(yes(_), yes(_)).
match_expected(no, no).
match_expected(unknown, unknown).
match_expected(some(_, _, _), some(_, _, _)).
match_expected(none, none).
match_expected(proof(_), proof(_)).
match_expected(noproof(_), noproof(_)).

% t_has: ademas del tipo, exige un contenido concreto en la respuesta.
t_has(Q, Elem) :-
    ask(Q, some(Xs, _, _)),
    format('> ~w~n', [Q]),
    say(some(Xs, _, _)),
    ( member(Elem, Xs) ->
        check(true, Q)
    ; format('MISSING ~w in ~w~n', [Elem, Xs]),
      check(false, Q)).

% ---------- checks ----------
check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps), length(Ps, NP),
    findall(1, check_results(_, fail), Fs), length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
