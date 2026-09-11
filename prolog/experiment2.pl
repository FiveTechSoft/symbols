:- consult('memory.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment2 :-
    reset_experiment,

    create_training_data,

    nl,
    writeln('=============================================='),
    writeln('       EXPERIMENT 2 - SYMBOLS ONLY'),
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
    forall(
        between(1,20,N),
        create_entity_data(N)
    ).


create_entity_data(N) :-
    symbol_index(N,1,A),
    symbol_index(N,2,B),
    symbol_index(N,3,C),
    symbol_index(N,4,D),

    remember_relation(A,r1,B,1.0),
    remember_relation(A,r1,C,1.0),

    remember_relation(A,r2,D,1.0),

    remember_relation(A,r3,
        relation_symbol(N),
        1.0
    ).


symbol_index(N,Offset,Symbol) :-
    Index is ((N - 1 + Offset) mod 20) + 1,
    symbol(Index,Symbol).


symbol(N,Symbol) :-
    atom_concat(s,N,Symbol).


relation_symbol(N) :-
    atom_concat(t,N,Relation).


entity(Entity) :-
    memory_relation(Entity,_,_,_,_).

entity(Entity) :-
    memory_relation(_,_,Entity,_,_).


discover_concepts :-
    findall(
        Entity,
        entity(Entity),
        Entities0
    ),

    sort(Entities0,Entities),

    forall(
        member(Entity,Entities),
        assign_concept(Entity)
    ).


assign_concept(Entity) :-
    entity_signature(Entity,Signature),

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
        BestScore >= 0.80
        ->
        assertz(
            concept_member(
                BestConcept,
                Entity,
                BestScore
            )
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
    keysort(Matches,Sorted),
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


next_concept_number([],1).

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

    jaccard(S1,S2,SS),
    jaccard(O1,O2,OS),

    Score is (SS + OS) / 2.


jaccard([],[],1.0) :-
    !.

jaccard(A,B,Score) :-

    append(A,B,Combined),

    sort(Combined,Union),

    intersection(
        A,
        B,
        Common
    ),

    length(Union,UnionSize),

    length(Common,CommonSize),

    Score is
        CommonSize / UnionSize.


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
                '~w  ~w~n',
                [
                    Concept,
                    Signature
                ]
            ),

            forall(
                concept_member(
                    Concept,
                    Entity,
                    Score
                ),

                format(
                    '   ~w  score=~2f~n',
                    [
                        Entity,
                        Score
                    ]
                )
            ),

            nl
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

    NewScore is max(
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
            '~w  confidence=~2f~n',
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
        '===== POSITIVE GENERALIZATION ====='
    ),

    findall(
        Result,
        (
            between(1,20,N),

            symbol(N,Subject),

            Target is N,

            symbol(
                Target,
                Object
            ),

            (
                infer(
                    Subject,
                    r1,
                    Object,
                    _
                )
                ->
                Result = pass
                ;
                Result = fail
            )
        ),
        Results
    ),

    count_results(
        Results,
        pass,
        TP
    ),

    length(
        Results,
        Total
    ),

    FN is Total - TP,

    format(
        'TP = ~w~n',
        [TP]
    ),

    format(
        'FN = ~w~n',
        [FN]
    ).


run_negative_tests :-

    nl,
    writeln(
        '===== NEGATIVE GENERALIZATION ====='
    ),

    findall(
        Result,
        (
            between(1,20,N),

            symbol(N,Subject),

            symbol(
                N,
                Object
            ),

            (
                infer(
                    Subject,
                    r3,
                    Object,
                    _
                )
                ->
                Result = false_positive
                ;
                Result = true_negative
            )
        ),
        Results
    ),

    count_results(
        Results,
        false_positive,
        FP
    ),

    count_results(
        Results,
        true_negative,
        TN
    ),

    format(
        'FP = ~w~n',
        [FP]
    ),

    format(
        'TN = ~w~n',
        [TN]
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


equal(Value,Value).


show_metrics :-

    nl,
    writeln(
        '===== METRICS ====='
    ),

    positive_count(TP),
    negative_count(FP),

    TotalPositive is 20,
    TotalNegative is 20,

    FN is TotalPositive - TP,
    TN is TotalNegative - FP,

    PrecisionDenominator is TP + FP,

    (
        PrecisionDenominator =:= 0
        ->
        Precision = 0.0
        ;
        Precision is
            TP / PrecisionDenominator
    ),

    Recall is
        TP / TotalPositive,

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
        (TotalPositive + TotalNegative),

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


positive_count(Count) :-

    findall(
        X,
        (
            between(1,20,N),

            symbol(N,S),

            symbol(N,O),

            infer(
                S,
                r1,
                O,
                _
            ),

            X = 1
        ),
        Values
    ),

    length(
        Values,
        Count
    ).


negative_count(Count) :-

    findall(
        X,
        (
            between(1,20,N),

            symbol(N,S),

            symbol(N,O),

            infer(
                S,
                r3,
                O,
                _),

            X = 1
        ),
        Values
    ),

    length(
        Values,
        Count
    ).
