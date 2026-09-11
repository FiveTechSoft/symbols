:- consult('memory.pl').

:- use_module(library(lists)).

:- dynamic concept/3.
:- dynamic concept_member/3.
:- dynamic concept_relation/4.
:- dynamic learned_rule/5.

experiment :-
    reset_experiment,
    create_training_data,

    nl,
    writeln('=============================================='),
    writeln('       SYMBOLIC GENERALIZATION EXPERIMENT'),
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

    positive_tests,
    negative_tests,

    show_statistics.


reset_experiment :-
    clear_memory,
    retractall(concept(_,_,_)),
    retractall(concept_member(_,_,_)),
    retractall(concept_relation(_,_,_,_)),
    retractall(learned_rule(_,_,_,_,_)).


create_training_data :-
    forall(
        between(1,20,N),
        create_person_data(N)
    ).


create_person_data(N) :-
    Person = person(N),

    food_index(N,1,F1),
    food_index(N,2,F2),
    food_index(N,3,F3),

    CityIndex is ((N - 1) mod 5) + 1,

    remember_relation(
        Person,
        compra,
        food(F1),
        1.0
    ),

    remember_relation(
        Person,
        compra,
        food(F2),
        1.0
    ),

    remember_relation(
        Person,
        come,
        food(F3),
        1.0
    ),

    remember_relation(
        Person,
        vive_en,
        city(CityIndex),
        1.0
    ).


food_index(N,Offset,Index) :-
    Index is ((N - 1 + Offset) mod 20) + 1.


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

    Score is
        CommonSize / UnionSize.


show_concepts :-

    nl,
    writeln('===== DISCOVERED CONCEPTS ====='),

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
                    '   ~w  ~2f~n',
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


positive_tests :-

    nl,
    writeln(
        '===== POSITIVE GENERALIZATION ====='
    ),

    forall(
        between(1,20,N),

        (
            Person = person(N),
            Food = food(N),

            (
                infer(
                    Person,
                    compra,
                    Food,
                    Score
                )
                ->
                format(
                    'PASS  ~w compra ~w  score=~2f~n',
                    [
                        Person,
                        Food,
                        Score
                    ]
                )
                ;
                format(
                    'FAIL  ~w compra ~w~n',
                    [
                        Person,
                        Food
                    ]
                )
            )
        )
    ).


negative_tests :-

    nl,
    writeln(
        '===== NEGATIVE GENERALIZATION ====='
    ),

    forall(
        between(1,20,N),

        (
            Person = person(N),
            Food = food(N),

            (
                infer(
                    Person,
                    vive_en,
                    Food,
                    Score
                )
                ->
                format(
                    'ERROR ~w vive_en ~w  score=~2f~n',
                    [
                        Person,
                        Food,
                        Score
                    ]
                )
                ;
                format(
                    'PASS  reject ~w vive_en ~w~n',
                    [
                        Person,
                        Food
                    ]
                )
            )
        )
    ).


show_statistics :-

    nl,
    writeln(
        '=============================================='
    ),

    findall(
        Concept,
        concept(Concept,_,_),
        Concepts
    ),

    findall(
        Rule,
        learned_rule(
            Rule,
            _,
            _,
            _,
            _
        ),
        Rules
    ),

    length(
        Concepts,
        ConceptCount
    ),

    length(
        Rules,
        RuleCount
    ),

    format(
        'Concepts discovered: ~w~n',
        [ConceptCount]
    ),

    format(
        'Rules discovered:    ~w~n',
        [RuleCount]
    ),

    writeln(
        '=============================================='
    ).