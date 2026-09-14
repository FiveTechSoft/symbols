:- consult('parser.pl').
:- consult('positional.pl').

test_parser :-
    write('========================================'), nl,
    write('  STRUCTURED PARSER TEST'), nl,
    write('========================================'), nl, nl,

    test('Juan compro un coche rojo en Madrid ayer'),
    test('alice followed white rabbit'),
    test('Maria comio una manzana verde en casa hoy'),
    test('Pedro escribio un libro largo'),
    test('El nino corrio rapido en el parque'),

    halt().

test(Text) :-
    parse(Text, S), !,
    write('--- '), write(Text), nl,
    S = sentence(subject(Subj), verb(Verb), object(Obj), attributes(Attrs), location(Loc), time(Time)),
    write('  SUBJECT:  '), write(Subj), nl,
    write('  VERB:     '), write(Verb), nl,
    write('  OBJECT:   '), write(Obj), nl,
    write('  ATTRS:    '), write(Attrs), nl,
    write('  LOCATION: '), write(Loc), nl,
    write('  TIME:     '), write(Time), nl, nl.

:- initialization(test_parser).
