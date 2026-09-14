:- use_module(parser_attention).

debug_test :-
    Text = "Maria visito Madrid en 2025",
    normalize_text(Text, Tokens),

    last(Tokens, Atom2025),
    Hardcoded = 2025,

    format('Normalized: ~w~n', [Atom2025]),
    format('Hardcoded: ~w~n', [Hardcoded]),
    format('Atom chars of normalized: ~w~n', [atom_chars(Atom2025, C1)]),
    format('Atom chars of hardcoded: ~w~n', [atom_chars(Hardcoded, C2)]),
    format('Number codes normalized: ~w~n', [atom_codes(Atom2025, Co1)]),
    format('Number codes hardcoded: ~w~n', [atom_codes(Hardcoded, Co2)]),

    ( number(Atom2025) -> format('Normalized is number~n') ; format('Normalized is NOT number~n') ),
    ( number(Hardcoded) -> format('Hardcoded is number~n') ; format('Hardcoded is NOT number~n') ),

    ( atomic(Atom2025) -> format('Normalized is atomic~n') ; format('Normalized is NOT atomic~n') ),
    ( atomic(Hardcoded) -> format('Hardcoded is atomic~n') ; format('Hardcoded is NOT atomic~n') ),

    halt().

:- initialization(debug_test).
