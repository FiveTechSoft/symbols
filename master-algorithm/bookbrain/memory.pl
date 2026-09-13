:- use_module(library(lists)).

:- dynamic memory_relation/5.

remember_relation(Subject, Relation, Object) :-
    remember_relation(Subject, Relation, Object, 0.10).


remember_relation(Subject, Relation, Object, Weight) :-
    (
        memory_relation(
            Subject,
            Relation,
            Object,
            OldWeight,
            Uses
        )
        ->
        NewUses is Uses + 1,

        NewWeight is
            (OldWeight * Uses + Weight) / NewUses,

        retract(
            memory_relation(
                Subject,
                Relation,
                Object,
                OldWeight,
                Uses
            )
        ),

        assertz(
            memory_relation(
                Subject,
                Relation,
                Object,
                NewWeight,
                NewUses
            )
        )
        ;
        assertz(
            memory_relation(
                Subject,
                Relation,
                Object,
                Weight,
                1
            )
        )
    ).


memory_weight(
    Subject,
    Relation,
    Object,
    Weight
) :-
    memory_relation(
        Subject,
        Relation,
        Object,
        Weight,
        _
    ).


strengthen_memory(
    Subject,
    Relation,
    Object,
    Amount
) :-

    (
        memory_relation(
            Subject,
            Relation,
            Object,
            Weight,
            Uses
        )
        ->

        NewWeight0 is
            Weight + Amount,

        NewWeight is
            min(1.0, NewWeight0),

        NewUses is
            Uses + 1,

        retract(
            memory_relation(
                Subject,
                Relation,
                Object,
                Weight,
                Uses
            )
        ),

        assertz(
            memory_relation(
                Subject,
                Relation,
                Object,
                NewWeight,
                NewUses
            )
        )

        ;

        NewWeight is
            min(1.0, Amount),

        assertz(
            memory_relation(
                Subject,
                Relation,
                Object,
                NewWeight,
                1
            )
        )
    ).


weaken_memory(
    Subject,
    Relation,
    Object,
    Amount
) :-

    (
        memory_relation(
            Subject,
            Relation,
            Object,
            Weight,
            Uses
        )
        ->

        NewWeight0 is
            Weight - Amount,

        NewWeight is
            max(0.0, NewWeight0),

        NewUses is
            Uses + 1,

        retract(
            memory_relation(
                Subject,
                Relation,
                Object,
                Weight,
                Uses
            )
        ),

        assertz(
            memory_relation(
                Subject,
                Relation,
                Object,
                NewWeight,
                NewUses
            )
        )

        ;

        true
    ).


forget_memory(
    Subject,
    Relation,
    Object
) :-
    retractall(
        memory_relation(
            Subject,
            Relation,
            Object,
            _,
            _
        )
    ).


memory_size(Size) :-
    aggregate_all(
        count,
        memory_relation(_,_,_,_,_),
        Size
    ).


show_memory :-
    nl,
    writeln(
        '===== SYMBOLIC MEMORY ====='
    ),

    forall(
        memory_relation(
            Subject,
            Relation,
            Object,
            Weight,
            Uses
        ),

        format(
            '~w --~w--> ~w  weight=~2f  uses=~w~n',
            [
                Subject,
                Relation,
                Object,
                Weight,
                Uses
            ]
        )
    ),

    memory_size(Size),

    format(
        'Relations: ~w~n',
        [Size]
    ),

    nl.


clear_memory :- retractall( memory_relation( _, _, _, _, _ ) ).
