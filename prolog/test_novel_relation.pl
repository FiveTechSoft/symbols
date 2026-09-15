% test_novel_relation.pl — EXP-22 Novel Relation Learning (baseline fc64115 FROZEN).
% Measures 4 learning metrics. Metric 5 (baseline preservation) is verified
% separately by re-running test_trajectory (25/25), composition_test (7/7)
% and holdout_1000 (1000/1000): this file MUST NOT modify the baseline.
% Usage: swipl -s test_novel_relation.pl -g run_exp22 -t halt
:- use_module(novel_relation).
:- use_module(semantic_field).

:- dynamic tpass/1.
:- dynamic tfail/2.

% check(+Label, +Goal) : pass/fail accounting without touching the baseline.
check(Label, Goal) :-
    ( catch(call(Goal), _, fail) ->
        assertz(tpass(Label)),
        format('  OK ~w~n', [Label])
    ; assertz(tfail(Label, got_no_proof)),
        format('  FAIL ~w~n', [Label])
    ),
    !.
check_eq(Label, Got, Expected) :-
    ( Got == Expected ->
        assertz(tpass(Label)),
        format('  OK ~w => ~w~n', [Label, Got])
    ; assertz(tfail(Label, Got)),
        format('  FAIL ~w => ~w (expected ~w)~n', [Label, Got, Expected])
    ),
    !.

run_exp22 :-
    retractall(tpass(_)), retractall(tfail(_, _)),
    novel_relation:reset_learning,

    format('=== PHASE 1+2: experience + induction (repair x3) ===~n', []),
    learn_from_sentences(
        ["Elena repaired a bicycle.",
         "Carlos repaired a motorcycle.",
         "David repaired a watch."],
        Induced1),
    % Metric 1: relation induced (lexical identifier from observation).
    check(m1_relation_induced,
          ( member(N, Induced1), novel_relation:novel_relation(N, 2) )),
    % Metric 2: roles induced (arg1=agent, arg2=object).
    ( Induced1 = [RName|_] -> true ; RName = none ),
    check(m2_agent_role, novel_relation:agent(RName, 1)),
    check(m2_object_role, novel_relation:object(RName, 2)),
    check(m2_fact_stored,
          novel_relation:learned_fact(RName, [elena, bicycle])),
    % No repair-specific rule may exist in the module (restriction check):
    % the module source must not mention the induced name literally.
    check(m0_no_hardcoding, \+ novel_relation_source_mentions(RName)),

    format('=== PHASE 3: semantic separation (have vs repair) ===~n', []),
    novel_relation:reset_facts,
    apply_sentences(
        ["Elena has a bicycle.",
         "Elena repaired a motorcycle."],
        _Stored3),
    answer_novel("Elena has a bicycle. Elena repaired a motorcycle.",
                 "What does Elena have?", AHave),
    check_eq(m3a_have_kept, AHave, bicycle),
    answer_novel("Elena has a bicycle. Elena repaired a motorcycle.",
                 "What did Elena repair?", ARep),
    check_eq(m3b_repair_distinct, ARep, motorcycle),
    verify_novel(RName, [elena, bicycle], V1),
    check_eq(m3c_repair_bicycle_no_evidence, V1, no_evidence),
    verify_novel(RName, [elena, motorcycle], V2),
    check_eq(m3d_repair_motorcycle_yes, V2, yes),

    format('=== PHASE 4a: transfer, known verb, no re-induction ===~n', []),
    findall(N, novel_relation:novel_relation(N, _), RegBefore),
    apply_sentences(["Laura repaired a camera."], Stored4a),
    check(m4a_fact_produced,
          novel_relation:learned_fact(RName, [laura, camera])),
    findall(N, novel_relation:novel_relation(N, _), RegAfter),
    check(m4a_no_reinduction, RegBefore == RegAfter),
    format('  (stored: ~w)~n', [Stored4a]),

    format('=== PHASE 4b: second novel verb stays distinct ===~n', []),
    learn_from_sentences(
        ["Peter rescued a dog.",
         "Anna rescued a cat."],
        Induced4b),
    check(m4b_second_relation,
          ( member(N2, Induced4b), N2 \== RName,
            novel_relation:novel_relation(N2, 2) )),
    Induced4b = [R2|_],
    ( Induced4b = [R2|_] -> true ; R2 = none ),
    answer_novel("Peter rescued a dog. Anna rescued a cat.",
                 "What did Peter rescue?", ARes),
    check_eq(m4b_rescue_answer, ARes, dog),
    % rescue facts must not leak into repair and vice versa.
    check(m4b_no_cross_contamination,
          ( \+ novel_relation:learned_fact(RName, [peter, dog]),
            \+ novel_relation:learned_fact(R2, [elena, bicycle]) )),

    report_exp22.

% Restriction check: the module source must not contain a code occurrence of
% the induced relation name as a functor, i.e. Name+"(" outside comments.
% (The name may only ever appear as runtime DATA, never in a clause.)
novel_relation_source_mentions(Name) :-
    module_property(novel_relation, file(Path)),
    open(Path, read, S),
    get_char(S, C0),
    collect_chars(C0, S, Chars),
    close(S),
    atom_chars(Src, Chars),
    strip_prolog_comments(Src, Code),
    atom_concat(Name, "(", Pat),
    sub_atom(Code, _, _, _, Pat), !.

collect_chars(end_of_file, _, []) :- !.
collect_chars(C, S, [C|R]) :-
    get_char(S, C2),
    collect_chars(C2, S, R).

strip_prolog_comments(Src, Code) :-
    atom_chars(Src, Cs),
    strip_cs(Cs, Out),
    atom_chars(Code, Out).

strip_cs([], []) :- !.
strip_cs(['%'|T], R) :- !,
    skip_to_nl(T, T2),
    strip_cs(T2, R).
strip_cs([H|T], [H|R]) :-
    strip_cs(T, R).

skip_to_nl([], []) :- !.
skip_to_nl(['\n'|T], ['\n'|T]) :- !.
skip_to_nl([_|T], R) :-
    skip_to_nl(T, R).

report_exp22 :-
    nl,
    findall(L, tpass(L), Ps), length(Ps, NP),
    findall(L, tfail(L, _), Fs), length(Fs, NF),
    Total is NP + NF,
    format('EXP-22 SCORE: ~w/~w~n', [NP, Total]),
    ( tpass(m1_relation_induced) -> L1 = yes ; L1 = no ),
    ( tpass(m3b_repair_distinct) -> L3 = yes ; L3 = no ),
    ( tpass(m4a_fact_produced) -> L4a = yes ; L4a = no ),
    ( tpass(m4b_second_relation) -> L4b = yes ; L4b = no ),
    format('  m1 induction:~w m3 separation:~w m4a transfer:~w m4b distinct:~w~n',
           [L1, L3, L4a, L4b]),
    ( NF =:= 0 -> format('*** ALL LEARNING CHECKS PASSED ***~n', [])
    ; format('Failed: ~w~n', [Fs])
    ).

:- initialization(run_exp22).
