% probe57.pl — disecciona setof con ^ anidado vs simple.
:- dynamic m/3.

p57 :-
    retractall(m(_, _, _)),
    assertz(m(god, create, heaven)),
    assertz(m(god, divide, light)),
    assertz(m(moses, lead, people)),
    assertz(m(people, see, land)),
    assertz(m(david, rule, israel)),
    assertz(m(israel, fear, god)),
    % 1. anidado tal cual
    setof(B, A^R^(m(A, R, B), m(B, _, _)), M1),
    format('nested: ~w~n', [M1]),
    % 2. un solo ^ con conjuncion (A libre -> multigrupo; forzar backtracking)
    findall(B, setof(B, R^(m(A, R, B), m(B, _, _)), B), Gs2),
    format('single-hat groups: ~w~n', [Gs2]),
    % 3. bagof anidado con backtracking total
    findall(B, bagof(B, A^R^(m(A, R, B), m(B, _, _)), B), Gs3),
    format('bagof-nested groups: ~w~n', [Gs3]),
    % 4. func_rel minimal
    setof(R, S^O^m(S, R, O), Rs),
    format('rels: ~w~n', [Rs]),
    findall(R, (member(R, Rs), fr(R)), Fs),
    format('func: ~w~n', [Fs]).

fr(R) :-
    setof(S, O^m(S, R, O), Ss),
    forall(member(S, Ss), (setof(O, m(S, R, O), [_]))).
