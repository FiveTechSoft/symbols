:- consult('memory.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment3 :-
    reset_experiment,

    create_training_data,

    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 3 - STRUCTURAL VARIATION'),
    writeln('=============================================='),
    nl,

    memory_size(Size),
    format('Training facts: ~w~n~n', [Size]),

    discover_concepts,
    show_concepts,

    discover_concept_relations,
    show_concept_relations,

    discover_rules,
    show_rules,

    run_positive_tests,
    run_negative_tests,

    show_metrics.


reset_experiment :-
    clear_memory,
    retractall(concept(_,_,_)),
    retractall(concept_member(_,_,_)),
    retractall(concept_relation(_,_,_,_)),
    retractall(learned_rule(_,_,_,_,_)).


create_training_data :-
    create_subjects,
    create_objects,
    create_noise.


create_subjects :-
    forall(
        between(1,30,N),
        create_subject(N)
    ).


create_subject(N) :-
    symbol(N,A),

    object_index(N,1,B),
    object_index(N,2,C),
    object_index(N,3,D),

    remember_relation(
        A,
        r17,
        B,
        1.0
    ),

    remember_relation(
        A,
        r17,
        C,
        1.0
    ),

    remember_relation(
        A,
        r83,
        D,
        1.0
    ),

    optional_relation(
        N,
        A
    ).


optional_relation(N,A) :-
    0 is N mod 3,
    !,

    object_index(N,4,D),

    remember_relation(
        A,
        r61,
        D,
        1.0
    ).

optional_relation(_,_) .


create_objects :-
    forall(
        between(1,30,N),
        create_object(N)
    ).


create_object(N) :-
    symbol(N,A),

    object_index(N,1,B),

    remember_relation(
        B,
        r42,
        A,
        1.0
    ).


create_noise :-
    symbol(1,A),
    symbol(10,B),
    symbol(20,C),

    remember_relation(
        A,
        r91,
        B,
        0.20
    ),

    remember_relation(
        C,
        r91,
        B,
        0.20
    ).


symbol(N,Symbol) :-
    atom_concat(q,N,Symbol).


object_index(N,Offset,Index) :-
    Index is ((N - 1 + Offset) mod 30) + 1,

    atom_concat(x,Index,Symbol),

    Symbol = Symbol.


object_symbol(N,Symbol) :-
    atom_concat(x,N,Symbol).


entity(Entity) :-
    memory_relation(
        Entity,
        _,
        _,
        _,
        _
    ).

entity(Entity) :-
    memory_relation(
        _,
        _,
        Entity,
        _,
        _
    ).


discover_concepts :-
    findall(
        Entity,
        entity(Entity),
        Entities0
    ),

    sort(
        Entities0,
        Entities
    ),

    forall(
        member(Entity,Entities),
        assign_concept(Entity)
    ).


assign_concept(Entity) :-
    entity_signature(
        Entity,
        Signature
    ),

    findall(
        Score-Concept,
        (
            concept(
                Concept,
                ConceptSignature,
                _
            ),

            signature_similarity(
                Signature,
                ConceptSignature,
                Score
            )
        ),
        Matches
    ),

    best_concept(
        Matches,
        BestScore,
        BestConcept
    ),

    (
        BestScore >= 0.60
        ->
        add_member(
            BestConcept,
            Entity,
            BestScore
        )
        ;
        create_concept(
            Entity,
            Signature
        )
    ).


best_concept(
    [],
    0.0,
    none
).

best_concept(
    Matches,
    Score,
    Concept
) :-
    keysort(
        Matches,
        Sorted
    ),

    reverse(
        Sorted,
        [Score-Concept|_]
    ).


create_concept(
    Entity,
    Signature
) :-

    findall(
        N,
        concept(
            concept(N),
            _,
            _
        ),
        Numbers
    ),

    next_concept_number(
        Numbers,
        N
    ),

    Concept = concept(N),

    assertz(
        concept(
            Concept,
            Signature,
            1
        )
    ),

    assertz(
        concept_member(
            Concept,
            Entity,
            1.0
        )
    ).


add_member(
    Concept,
    Entity,
    Score
) :-

    \+ concept_member(
        Concept,
        Entity,
        _
    ),

    assertz(
        concept_member(
            Concept,
            Entity,
            Score
        )
    ).


next_concept_number(
    [],
    1
).

next_concept_number(
    Numbers,
    N
) :-
    max_list(
        Numbers,
        Max
    ),

    N is Max + 1.


entity_signature(
    Entity,
    signature(
        SubjectRelations,
        ObjectRelations
    )
) :-

    findall(
        Relation,
        memory_relation(
            Entity,
            Relation,
            _,
            _,
            _
        ),
        SubjectRelations0
    ),

    findall(
        Relation,
        memory_relation(
            _,
            Relation,
            Entity,
            _,
            _
        ),
        ObjectRelations0
    ),

    sort(
        SubjectRelations0,
        SubjectRelations
    ),

    sort(
        ObjectRelations0,
        ObjectRelations
    ).


signature_similarity(
    signature(S1,O1),
    signature(S2,O2),
    Score
) :-

    jaccard(
        S1,
        S2,
        SubjectScore
    ),

    jaccard(
        O1,
        O2,
        ObjectScore
    ),

    Score is
        (SubjectScore + ObjectScore) / 2.


jaccard([],[],1.0) :-
    !.

jaccard(A,B,Score) :-

    append(
        A,
        B,
        Combined
    ),

    sort(
        Combined,
        Union
    ),

    intersection(
        A,
        B,
        Common
    ),

    length(
        Union,
        UnionSize
    ),

    length(
        Common,
        CommonSize
    ),

    (
        UnionSize =:= 0
        ->
        Score = 0.0
        ;
        Score is
            CommonSize / UnionSize
    ).


show_concepts :-

    nl,
    writeln(
        '===== DISCOVERED CONCEPTS ====='
    ),

    forall(
        concept(
            Concept,
            Signature,
            _
        ),

        (
            format(
                '~w  signature=~w~n',
                [
                    Concept,
                    Signature
                ]
            ),

            findall(
                Entity,
                concept_member(
                    Concept,
                    Entity,
                    _
                ),
                Members
            ),

            length(
                Members,
                Count
            ),

            format(
                '   members=~w~n',
                [Count]
            ),

            format(
                '   ~w~n~n',
                [Members]
            )
        )
    ).


discover_concept_relations :-

    forall(
        memory_relation(
            Subject,
            Relation,
            Object,
            Weight,
            _
        ),

        discover_relation(
            Subject,
            Relation,
            Object,
            Weight
        )
    ).


discover_relation(
    Subject,
    Relation,
    Object,
    Weight
) :-

    concept_member(
        SubjectConcept,
        Subject,
        SubjectScore
    ),

    concept_member(
        ObjectConcept,
        Object,
        ObjectScore
    ),

    Score is
        Weight *
        SubjectScore *
        ObjectScore,

    add_concept_relation(
        SubjectConcept,
        Relation,
        ObjectConcept,
        Score
    ).


add_concept_relation(
    SubjectConcept,
    Relation,
    ObjectConcept,
    Score
) :-

    concept_relation(
        SubjectConcept,
        Relation,
        ObjectConcept,
        OldScore
    ),

    !,

    NewScore is
        max(
            OldScore,
            Score
        ),

    retract(
        concept_relation(
            SubjectConcept,
            Relation,
            ObjectConcept,
            OldScore
        )
    ),

    assertz(
        concept_relation(
            SubjectConcept,
            Relation,
            ObjectConcept,
            NewScore
        )
    ).

add_concept_relation(
    SubjectConcept,
    Relation,
    ObjectConcept,
    Score
) :-

    assertz(
        concept_relation(
            SubjectConcept,
            Relation,
            ObjectConcept,
            Score
        )
    ).


show_concept_relations :-

    nl,
    writeln(
        '===== CONCEPT RELATIONS ====='
    ),

    forall(
        concept_relation(
            SubjectConcept,
            Relation,
            ObjectConcept,
            Score
        ),

        format(
            '~w --~w--> ~w  score=~2f~n',
            [
                SubjectConcept,
                Relation,
                ObjectConcept,
                Score
            ]
        )
    ).


discover_rules :-

    forall(
        concept_relation(
            SubjectConcept,
            Relation,
            ObjectConcept,
            Score
        ),

        assertz(
            learned_rule(
                rule(
                    SubjectConcept,
                    Relation,
                    ObjectConcept
                ),
                SubjectConcept,
                Relation,
                ObjectConcept,
                Score
            )
        )
    ).


show_rules :-

    nl,
    writeln(
        '===== LEARNED RULES ====='
    ),

    forall(
        learned_rule(
            Rule,
            _,
            _,
            _,
            Score
        ),

        format(
            '~w confidence=~2f~n',
            [
                Rule,
                Score
            ]
        )
    ).


infer(
    Subject,
    Relation,
    Object,
    Score
) :-

    learned_rule(
        _,
        SubjectConcept,
        Relation,
        ObjectConcept,
        RuleScore
    ),

    concept_member(
        SubjectConcept,
        Subject,
        SubjectScore
    ),

    concept_member(
        ObjectConcept,
        Object,
        ObjectScore
    ),

    Score is
        RuleScore *
        SubjectScore *
        ObjectScore.


run_positive_tests :-

    nl,
    writeln(
        '===== POSITIVE TEST ====='
    ),

    findall(
        Result,
        positive_case(Result),
        Results
    ),

    count_results(
        Results,
        pass,
        TP
    ),

    count_results(
        Results,
        fail,
        FN
    ),

    format(
        'TP = ~w~n',
        [TP]
    ),

    format(
        'FN = ~w~n',
        [FN]
    ).


positive_case(Result) :-

    between(
        1,
        30,
        N
    ),

    symbol(
        N,
        Subject
    ),

    object_symbol(
        N,
        Object
    ),

    (
        infer(
            Subject,
            r17,
            Object,
            Score
        )
        ->
        format(
            'PASS  ~w --r17--> ~w  ~2f~n',
            [
                Subject,
                Object,
                Score
            ]
        ),
        Result = pass
        ;
        format(
            'FAIL  ~w --r17--> ~w~n',
            [
                Subject,
                Object
            ]
        ),
        Result = fail
    ).


run_negative_tests :-

    nl,
    writeln(
        '===== NEGATIVE TEST ====='
    ),

    findall(
        Result,
        negative_case(Result),
        Results
    ),

    count_results(
        Results,
        true_negative,
        TN
    ),

    count_results(
        Results,
        false_positive,
        FP
    ),

    format(
        'TN = ~w~n',
        [TN]
    ),

    format(
        'FP = ~w~n',
        [FP]
    ).


negative_case(Result) :-

    between(
        1,
        30,
        N
    ),

    symbol(
        N,
        Subject
    ),

    object_symbol(
        N,
        Object
    ),

    (
        infer(
            Subject,
            r83,
            Object,
            Score
        )
        ->
        format(
            'ERROR ~w --r83--> ~w  ~2f~n',
            [
                Subject,
                Object,
                Score
            ]
        ),
        Result = false_positive
        ;
        format(
            'PASS  reject ~w --r83--> ~w~n',
            [
                Subject,
                Object
            ]
        ),
        Result = true_negative
    ).


count_results(
    Results,
    Value,
    Count
) :-

    include(
        equal(Value),
        Results,
        Selected
    ),

    length(
        Selected,
        Count
    ).


equal(
    Value,
    Value
).


show_metrics :-

    nl,
    writeln(
        '===== METRICS ====='
    ),

    findall(
        X,
        positive_pass(X),
        Positive
    ),

    findall(
        X,
        positive_fail(X),
        PositiveFail
    ),

    findall(
        X,
        negative_false_positive(X),
        NegativeFP
    ),

    findall(
        X,
        negative_true(X),
        NegativeTN
    ),

    length(
        Positive,
        TP
    ),

    length(
        PositiveFail,
        FN
    ),

    length(
        NegativeFP,
        FP
    ),

    length(
        NegativeTN,
        TN
    ),

    PrecisionDenominator is
        TP + FP,

    (
        PrecisionDenominator =:= 0
        ->
        Precision = 0.0
        ;
        Precision is
            TP / PrecisionDenominator
    ),

    Recall is
        TP / (TP + FN),

    F1Denominator is
        Precision + Recall,

    (
        F1Denominator =:= 0
        ->
        F1 = 0.0
        ;
        F1 is
            2.0 *
            Precision *
            Recall /
            F1Denominator
    ),

    Accuracy is
        (TP + TN) /
        (TP + TN + FP + FN),

    format(
        'TP        = ~w~n',
        [TP]
    ),

    format(
        'FP        = ~w~n',
        [FP]
    ),

    format(
        'FN        = ~w~n',
        [FN]
    ),

    format(
        'TN        = ~w~n',
        [TN]
    ),

    format(
        'Precision = ~4f~n',
        [Precision]
    ),

    format(
        'Recall    = ~4f~n',
        [Recall]
    ),

    format(
        'F1        = ~4f~n',
        [F1]
    ),

    format(
        'Accuracy  = ~4f~n',
        [Accuracy]
    ).


positive_pass(X) :-
    between(1,30,N),
    symbol(N,S),
    object_symbol(N,O),
    infer(S,r17,O,_),
    X = N.


positive_fail(X) :-
    between(1,30,N),
    symbol(N,S),
    object_symbol(N,O),
    \+ infer(S,r17,O,_),
    X = N.


negative_false_positive(X) :-
    between(1,30,N),
    symbol(N,S),
    object_symbol(N,O),
    infer(S,r83,O,_),
    X = N.


negative_true(X) :-
    between(1,30,N),
    symbol(N,S),
    object_symbol(N,O),
    \+ infer(S,r83,O,_),
    X = N.
