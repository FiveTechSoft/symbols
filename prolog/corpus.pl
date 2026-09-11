% corpus.pl
% Sistema v0.1 de aprendizaje por corpus: INGESTA separada de APRENDIZAJE.
% API: learn_sentence/1,2  learn_file/1  learn_corpus/1
%      learn_cycle/1 (targets explicitos; auto-targets = futuro)
%      ask/2  why/2  save_knowledge/1  load_knowledge/1  knowledge_stats/1
% Experiencia = grafo + provenance (conflict.pl). Conocimiento = reglas,
% skills, meta (predicados separados). Sin clear_memory entre documentos.
:- consult('memory.pl').
:- consult('open_vocab.pl').
:- consult('coreference.pl').
:- consult('conflict.pl').
:- consult('composition.pl').
:- consult('multivariable.pl').
:- consult('reuse.pl').
:- consult('continuous.pl').
:- consult('guided_search.pl').
:- consult('question_parser.pl').

:- use_module(library(lists)).
:- use_module(library(readutil)).

:- dynamic sentence_counter/1.
:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.
% skill/meta consultados solo si existen (corpus v0.1 no los induce,
% pero save/load/stats deben funcionar igual).
:- dynamic skill/3.
:- dynamic meta_rule/4.

% ---------- INGESTA (sin aprender reglas) ----------
learn_sentence(Text) :-
    learn_sentence(Text, _).

learn_sentence(Text, Stored) :-
    next_sentence_id(Src),
    ( symbolize_open(Text, (A, V, O), usage) ->
        remember_tracked(A, V, O, Src, none),
        Stored = stored(A, V, O, Src)
    ; symbolize_open(Text, (A, V, O), typed) ->
        remember_tracked(A, V, O, Src, none),
        Stored = stored(A, V, O, Src)
    ; symbolize_open(Text, (X, same_as, Y), identity) ->
        remember_identity(X, Y),
        Stored = stored_identity(X, Y, Src)
    ; Stored = rejected(Text)
    ).

next_sentence_id(Src) :-
    ( retract(sentence_counter(N)) -> true ; N = 0 ),
    N1 is N + 1,
    assertz(sentence_counter(N1)),
    atomic_list_concat([sentence_, N1], Src).

learn_file(File) :-
    open(File, read, S, [encoding(utf8)]),
    read_lines_loop(S),
    close(S).

read_lines_loop(S) :-
    read_line_to_string(S, Line),
    ( Line == end_of_file -> true
    ; ( Line == "" -> true
      ; learn_sentence(Line, Stored),
        ( Stored = rejected(_) ->
            format('REJECTED: ~w~n', [Line])
        ; true
        )
      ),
      read_lines_loop(S)
    ).

learn_corpus(Dir) :-
    directory_files(Dir, Files0),
    include(txt_file, Files0, Txts),
    sort(Txts, TxtsS),
    forall(member(F, TxtsS),
           ( atomic_list_concat([Dir, '/', F], P),
             format('--- ingesting ~w ---~n', [P]),
             learn_file(P)
           )).

txt_file(F) :-
    atom_string(A, F),
    sub_atom(A, _, 4, 0, '.txt').

% ---------- CICLO DE APRENDIZAJE (sobre experiencia acumulada) ----------
learn_cycle(Targets) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup,
    retry_unresolved,
    forall(member(T, Targets),
           ( discover_composition_scoped(T, 3) ->
               ( induce_constrained(T, _Path) ->
                   format('learned ~w~n', [T])
               ; format('no constrained rule for ~w~n', [T])
               )
           ; format('no composition for ~w~n', [T])
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

discover_rules_dedup :-
    forall(concept_relation(SC, R, OC, Sc),
           assert_learned_rule_dedup(rule(SC, R, OC), SC, R, OC, Sc)).

% discover_all: conceptos+relaciones+reglas (idempotente, sin targets).
discover_all :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup.

% learn_cycle_guided(+Targets, -Rows): guided per target + metricas.
% Rows = [row(Target, Path, F1, Generated, Evaluated, Ms)].
learn_cycle_guided(Targets, Rows) :-
    discover_concepts,
    discover_concept_relations,
    discover_rules_dedup,
    retry_unresolved,
    findall(Row, ( member(T, Targets),
                   guided_target(T, Row)
                 ),
            Rows).

guided_target(T, row(T, Path, F1, Gen, Ev, Ms)) :-
    run_discovery(guided, T, 3, Stats),
    Stats = stats(Path, F1, _Sup, Gen, Ev, _Pr, Ms),
    retractall(composed_rule(T, _, _)),
    assertz(composed_rule(T, Path, F1)),
    induce_constrained(T, Path).

% Exhaustive candidate COUNT (aritmetico, sin puntuar): cota superior
% honesta de lo que costaria no guiar.
exhaustive_count(Target, MaxLen, Count) :-
    findall(R, ( memory_relation(_, R, _, _, _), R \== Target ), Rs0),
    sort(Rs0, Vocab),
    length(Vocab, V),
    count_patterns(V, MaxLen, Count).

count_patterns(V, MaxLen, Count) :-
    findall(C, ( between(1, MaxLen, L),
                 C is V ^ L
               ),
            Cs),
    sum_ints(Cs, Count).

sum_ints([], 0).
sum_ints([H|T], S) :-
    sum_ints(T, S0),
    S is S0 + H.

% ---------- PREGUNTAS ----------
ask(Q, A) :-
    parse_question(Q, QP),
    answer_query(QP, A).

why(Q, E) :-
    parse_question(Q, QP),
    QP = q_why(X, R, Y),
    answer_query(QP, explanation(X, R, Y, E)).

% ---------- PERSISTENCIA (reglas + skills + meta) ----------
save_knowledge(File) :-
    open(File, write, Out),
    forall(constrained_rule(T, P, S),
           format(Out, '~q.~n', [rule(conclusion(T), body(P), sig(S))])),
    forall(meta_rule(M, L, S, Ts),
           format(Out, '~q.~n', [meta(len(L), sig(S), members(Ts),
                                       id(M))])),
    forall(skill(U, P, S),
           format(Out, '~q.~n', [skill(unit(U), path(P), sig(S))])),
    close(Out),
    format('knowledge saved to ~w~n', [File]).

load_knowledge(File) :-
    consult(File),
    forall(( rule(conclusion(T), body(P), sig(S)) ),
           ( retractall(constrained_rule(T, _, _)),
             assertz(constrained_rule(T, P, S))
           )),
    forall(( meta(len(L), sig(S), members(Ts), id(M)) ),
           ( retractall(meta_rule(M, _, _, _)),
             assertz(meta_rule(M, L, S, Ts))
           )),
    forall(( skill(unit(U), path(P), sig(S)) ),
           ( retractall(skill(U, _, _)),
             assertz(skill(U, P, S))
           )),
    format('knowledge loaded from ~w~n', [File]).

% ---------- METRICAS (antes/despues) ----------
knowledge_stats(stats(Facts, Symbols, Concepts, Crels, Rules, Skills,
                      Unknowns, Contested, Conflicts)) :-
    memory_size(Facts),
    findall(E, ( memory_relation(E, _, _, _, _) ;
                 memory_relation(_, _, E, _, _)
               ),
            E0),
    sort(E0, Es),
    length(Es, Symbols),
    findall(1, concept(_, _, _), Cs),
    length(Cs, Concepts),
    findall(1, concept_relation(_, _, _, _), CRs),
    length(CRs, Crels),
    findall(1, ( constrained_rule(_, _, _) ;
                 learned_rule(_, _, _, _, _)
               ),
            Rs),
    length(Rs, Rules),
    findall(1, skill(_, _, _), Ss),
    length(Ss, Skills),
    findall(E, ( member(E, Es), unknown_word(E) ), Us),
    length(Us, Unknowns),
    findall(1, prov(_, _, _, info(_, _, contested)), Ct),
    length(Ct, Contested),
    findall(1, prov_log(conflict, _), Cf),
    length(Cf, Conflicts).

unknown_word(E) :-
    \+ person(E),
    \+ city(E),
    \+ country(E),
    \+ food(E).

show_stats(Label, stats(F, S, C, CR, R, Sk, U, Ct, Cf)) :-
    format('~w: facts=~w symbols=~w concepts=~w crels=~w rules=~w skills=~w unknowns=~w contested=~w conflicts=~w~n',
           [Label, F, S, C, CR, R, Sk, U, Ct, Cf]).

% --- discovery (threshold 0.70, idempotent) ---

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
