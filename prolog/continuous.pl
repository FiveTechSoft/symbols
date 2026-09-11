% continuous.pl
% Aprendizaje continuo: protocolo incremental + medicion.
% - Nunca retracta conocimiento viejo (sin clear_memory entre fases).
% - Reglas nuevas solo se anaden si no existen (dedup).
% - Composicion con alcance por Target (no borra otros Targets).
% - Snapshots/deltas para LOCALITY, suites para RETENTION/ACQUISITION.
:- use_module(library(lists)).

% kb_counts(-[Mem, Concepts, CRel, Rules, Comp])
kb_counts([Mem, C, CR, R, Comp]) :-
    memory_size(Mem),
    findall(1, concept(_, _, _), LC), length(LC, C),
    findall(1, concept_relation(_, _, _, _), LCR), length(LCR, CR),
    findall(1, learned_rule(_, _, _, _, _), LR), length(LR, R),
    findall(1, composed_rule(_, _, _), LComp), length(LComp, Comp).

snapshot(snap(Counts, Rules, CompRules)) :-
    kb_counts(Counts),
    findall(Rule, learned_rule(Rule, _, _, _, _), R0),
    sort(R0, Rules),
    findall(T-P, composed_rule(T, P, _), C0),
    sort(C0, CompRules).

print_counts(Label, [Mem, C, CR, R, Comp]) :-
    format('~w: mem=~w concepts=~w crel=~w rules=~w composed=~w~n',
           [Label, Mem, C, CR, R, Comp]).

delta_counts([A, B, Cc, D, E], [A2, B2, C2, D2, E2],
             [DA, DB, DC, DD, DE]) :-
    DA is A2 - A, DB is B2 - B, DC is C2 - Cc,
    DD is D2 - D, DE is E2 - E.

% assert_learned_rule_dedup: solo reglas nuevas (idempotente).
assert_learned_rule_dedup(Rule, _, _, _, _) :-
    learned_rule(Rule, _, _, _, _), !.
assert_learned_rule_dedup(Rule, SC, R, OC, Sc) :-
    assertz(learned_rule(Rule, SC, R, OC, Sc)).

% discover_composition_scoped: como discover_composition pero solo
% retracta/induces el Target dado (los demas sobreviven).
discover_composition_scoped(Target, MaxLen) :-
    retractall(composed_rule(Target, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(R, (memory_relation(_, R, _, _, _), R \== Target), Rs0),
    sort(Rs0, Vocab),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(F1-Path-Sup,
            ( member(Len, Lens),
              pattern(Len, Vocab, Path),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(composed_rule(Target, BestPath, BestF1)),
    format('Scoped composed: ~w(S,O) :- ~w (F1=~4f support=~w)~n',
           [Target, BestPath, BestF1, BestSup]).

% rule_persisted(+Target, +Path): la regla vieja sigue intacta.
rule_persisted(Target, Path) :-
    composed_rule(Target, Path, _).

% run_suite(+Suite, -Passed, -Total): expect_true/false con composed_predict.
run_suite(Suite, Passed, Total) :-
    findall(R, (member(E, Suite), check_expect(E, R)), Rs),
    include(==(pass), Rs, Ps),
    length(Ps, Passed),
    length(Rs, Total).

check_expect(expect_true(S, T, O), pass) :-
    composed_predict(S, T, O), !.
check_expect(expect_true(S, T, O), fail) :-
    format('SUITE FAIL (expected true): ~w --~w--> ~w~n', [S, T, O]).
check_expect(expect_false(S, T, O), pass) :-
    \+ composed_predict(S, T, O), !.
check_expect(expect_false(S, T, O), fail) :-
    format('SUITE FAIL (expected false): ~w --~w--> ~w~n', [S, T, O]).
