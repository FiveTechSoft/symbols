% experiment_world_kb.pl — EXP-SYMBOLIC-REPLAY phase 2 (KB materialization)
% Materializes the lessons from the real experiment history into the SAME
% KB format the engine already ingests: memfact/5 (bb_load/bookbrain_v0).
% No private memfact/3, no separate KB: the lessons become queryable
% knowledge of the common KB, tagged so they can be cleaned selectively.
%
% Integration (user-corrected design):
%   experiment_world.pl -> experiment_world_kb.pl -> common KB
%
% Run: swipl -f experiment_world_kb.pl -g "run_all" -t halt

:- module(experiment_world_kb, [
    materializar_lecciones/0,
    limpiar_lecciones/0,
    leccion_memfact_count/1,
    que_aprendimos_de/2,
    lecciones_por_area/2,
    lecciones_por_confianza/2,
    run_all/0
]).

:- use_module(experiment_world).

% ============================================================
% 1. LESSONS (evidence, not opinions)
%
% leccion(ID, Text, SourceExperiment, Area, Confidence)
% Text is a factual statement of what was measured/observed.
% Confidence in [0,1]: 1.0 = verified by execution, 0.8 = measured
% but with caveats, 0.5 = measurement-only probe.
% ============================================================

leccion(l_r12c_recall,
    'parser surgical fixes moved recall 0 in real text corpora',
    r12c, parser, 1.0).
leccion(l_r12c_precision,
    'r12c improved huge precision from 98.47 to 98.85 percent',
    r12c, parser, 1.0).
leccion(l_jonas_coverage,
    'attention failures were coverage gaps not mechanism failures',
    jonas, atencion, 1.0).
leccion(l_multifact_delta0,
    'attention showed zero delta versus direct access on clean synthetic kb',
    multifact, atencion, 1.0).
leccion(l_kjv_delta0,
    'attention showed zero delta on real noisy kb once the direct path covered what-V-E',
    kjv, atencion, 1.0).
leccion(l_attention_closed,
    'in the five evaluated scenarios no recall delta was observed for symbolic attention',
    attention_closed, atencion, 1.0).
leccion(l_r8_coref,
    'pronoun resolution by proximity or quote scope produced false positives and stays unknown',
    attention_closed, coreferencia, 1.0).
leccion(l_env_signature,
    'delta measurements require isolated environments verified by signature probe',
    r12c, metodologia, 1.0).
leccion(l_kb_quality,
    'attention bottleneck was kb quality not the attention mechanism',
    multifact, atencion, 1.0).

% ============================================================
% 2. SYMBOL REGISTRY (selective cleaning without touching
%    unrelated memfacts of the common KB)
% ============================================================

:- dynamic leccion_symbol/1.

leccion_prefix(leccion_).

new_leccion_symbol(S) :-
    \+ leccion_symbol(S), !,
    assertz(leccion_symbol(S)).
new_leccion_symbol(_).

all_leccion_symbols(Ss) :-
    findall(S, leccion_symbol(S), Ss).

is_leccion_atom(A) :-
    atom(A),
    leccion_prefix(P),
    atom_concat(P, _, A).

% ============================================================
% 3. MATERIALIZATION into the common KB format (memfact/5)
%
% For each lesson:
%   memfact(leccion_<ID>, dice,   <Text>,      Conf, 1)
%   memfact(leccion_<ID>, deriva_de, <Source>, Conf, 1)
%   memfact(leccion_<ID>, aplica_a,  <Area>,   Conf, 1)
% Dedup: existing triple with same (S,V,O) is left untouched.
% ============================================================

materializar_lecciones :-
    forall(leccion(ID, Text, Source, Area, Conf),
           (   atom_concat(leccion_, ID, Sym),
               new_leccion_symbol(Sym),
               put_lesson_fact(Sym, dice, Text, Conf),
               put_lesson_fact(Sym, deriva_de, Source, Conf),
               put_lesson_fact(Sym, aplica_a, Area, Conf)
           )).

put_lesson_fact(S, V, O, Conf) :-
    (   memory_relation(S, V, O, _, _) -> true
    ;   assertz(memory_relation(S, V, O, Conf, 1))
    ).

% Selective cleaning: only the lesson symbols we registered.
% Does NOT touch knowledge facts of the common KB.
limpiar_lecciones :-
    all_leccion_symbols(Ss),
    forall(member(S, Ss),
           (   retractall(memory_relation(S, _, _, _, _)),
               retractall(leccion_symbol(S))
           )).

leccion_memfact_count(N) :-
    findall(S, (leccion_symbol(S), memory_relation(S, _, _, _, _)), Ss),
    length(Ss, N).

% ============================================================
% 4. QUERIES
% ============================================================

% All lessons derived from a given experiment (0..N results).
que_aprendimos_de(Source, L) :-
    leccion_symbol(L),
    memory_relation(L, deriva_de, Source, _, _).

% All lessons for an area.
lecciones_por_area(Area, L) :-
    leccion_symbol(L),
    memory_relation(L, aplica_a, Area, _, _).

% Lessons with confidence >= Min.
lecciones_por_confianza(Min, L) :-
    Min >= 0, Min =< 1,
    leccion_symbol(L),
    memory_relation(L, _, _, Conf, _),
    Conf >= Min.

% Lessons about the parser hierarchy (area query + evidence text).
lecciones_sobre_jerarquia(Area, L-Text) :-
    lecciones_por_area(Area, L),
    memory_relation(L, dice, Text, _, _).

% ============================================================
% 5. TEST RUNNER (exit-code based, no emoji in output)
% ============================================================

run_all :-
    write('=== experiment_world_kb tests ==='), nl,
    t_run(materializar_dedup, T1),
    t_run(consulta_por_experimento, T2),
    t_run(consulta_por_area, T3),
    t_run(consulta_por_confianza, T4),
    t_run(limpieza_selectiva, T5),
    t_run(no_colision_kb, T6),
    Tests = [T1,T2,T3,T4,T5,T6],
    forall(member(T, Tests), report_test(T)),
    (   member(pass, Tests)
    ->  true
    ;   true
    ),
    (   \+ member(fail, Tests)
    ->  write('ALL PASS'), nl, halt(0)
    ;   write('SOME FAIL'), nl, halt(1)
    ).

t_run(Goal, pass) :-
    catch(once(Goal), Err, (write('error: '), write(Err), nl, fail)), !.
t_run(_, fail).

report_test(pass)  :- write('PASS'), nl.
report_test(fail)  :- write('FAIL'), nl.
report_test(pass)  :- write('PASS'), nl.
report_test(fail)  :- write('FAIL'), nl.

materializar_dedup :-
    limpiar_lecciones,
    materializar_lecciones,
    materializar_lecciones,          % idempotent: second pass no dup
    leccion_memfact_count(27),       % 9 lessons x 3 facts
    findall(S, (leccion_symbol(S), memory_relation(S, dice, _, _, _)), Ss),
    length(Ss, 9).

consulta_por_experimento :-
    materializar_lecciones,
    findall(L, que_aprendimos_de(attention_closed, L), Ls),
    sort(Ls, LsS),
    LsS = [leccion_l_attention_closed, leccion_l_r8_coref],
    \+ que_aprendimos_de(exp3, _).

consulta_por_area :-
    materializar_lecciones,
    findall(L, lecciones_por_area(atencion, L), Ls),
    sort(Ls, LsS),
    length(LsS, 5),                   % jonas/multifact/kjv/attention_closed/kb_quality
    findall(L2, lecciones_por_area(parser, L2), Ps),
    length(Ps, 2).

consulta_por_confianza :-
    materializar_lecciones,
    findall(L, lecciones_por_confianza(1.0, L), Ls),
    length(Ls, N),
    N >= 9,
    \+ (   leccion_symbol(L3),
           memory_relation(L3, _, _, C, _),
           C < 0.5
       ).

limpieza_selectiva :-
    materializar_lecciones,
    memory_size(Before),
    limpiar_lecciones,
    memory_size(After),
    After =:= Before - 27,
    leccion_memfact_count(0),
    \+ (leccion_symbol(_)).

no_colision_kb :-
    limpiar_lecciones,
    materializar_lecciones,
    % knowledge facts coexist with lessons
    memory_relation(certain, man, bethlehemjudah, _, _),
    memory_relation(lord, grant, find, _, _),
    % a knowledge subject must not be visible as a lesson
    \+ (   memory_relation(S, _, _, _, _),
           is_leccion_atom(S),
           \+ leccion_symbol(S)
       ).

% Seed the common KB with two real knowledge facts to prove coexistence.
:- dynamic memory_relation/5.
memory_relation(certain, man, bethlehemjudah, 1.0, 1).
memory_relation(lord, grant, find, 1.0, 1).

memory_size(N) :-
    findall(_, memory_relation(_,_,_,_,_), Ls),
    length(Ls, N).