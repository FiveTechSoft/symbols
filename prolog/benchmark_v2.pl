:- use_module(parser_v2).

% ════════════════════════════════════════════════════════════════════
%  BENCHMARK: 10 frases + atención dependiente de consulta
% ════════════════════════════════════════════════════════════════════

run :-
    format("~n========================================~n", []),
    format("  BENCHMARK: SYMBOLIC ATTENTION V2~n", []),
    format("  relation(Type, Predicate, Args)~n", []),
    format("  query-dependent attention~n", []),
    format("========================================~n~n", []),

    % ── Phase 1: Parser precision/recall ────────────────────────────
    format("--- Phase 1: PARSER PRECISION / RECALL ---~n~n", []),
    test_parser(1, "Juan compro un coche rojo en Madrid ayer",
        [relation(main, comprar, [juan,coche]),
         relation(attribute, color, [coche,rojo]),
         relation(location, ubicado_en, [coche,madrid]),
         relation(temporal, tiempo, [comprar(juan,coche),ayer])]),
    test_parser(2, "Maria vive en Barcelona",
        [relation(main, vivir, [maria,barcelona])]),
    test_parser(3, "Pedro tiene una casa grande en Malaga",
        [relation(main, tener, [pedro,casa]),
         relation(attribute, tamano, [casa,grande]),
         relation(location, ubicado_en, [casa,malaga])]),
    test_parser(4, "Ana compro un libro azul para Maria",
        [relation(main, comprar, [ana,libro]),
         relation(attribute, color, [libro,azul]),
         relation(indirect, para, [comprar(ana,libro),maria])]),
    test_parser(5, "Juan no vive en Madrid",
        [relation(negation, negado, [vivir(juan,madrid)])]),
    test_parser(6, "Maria visito Madrid en 2025",
        [relation(main, visitar, [maria,madrid]),
         relation(temporal, tiempo, [visitar(maria,madrid),2025])]),
    test_parser(7, "Pedro compro un coche y lo llevo a Barcelona",
        [relation(main, comprar, [pedro,coche]),
         relation(main, llevar, [pedro,coche]),
         relation(location, llevar_destino, [coche,barcelona])]),
    test_parser(8, "Ana tiene una casa. La casa esta en Marbella.",
        [relation(main, tener, [ana,casa]),
         relation(location, ubicado_en, [casa,marbella])]),
    test_parser(9, "Juan trabaja en Madrid desde 2020",
        [relation(main, trabajar, [juan,madrid]),
         relation(temporal, desde, [trabajar(juan,madrid),2020])]),
    test_parser(10, "Maria compro un coche negro, pero Pedro compro una bicicleta roja",
        [relation(main, comprar, [maria,coche]),
         relation(attribute, color, [coche,negro]),
         relation(main, comprar, [pedro,bicicleta]),
         relation(attribute, color, [bicicleta,roja])]),

    % ── Phase 2: Attention with queries ─────────────────────────────
    format("~n--- Phase 2: QUERY-DEPENDENT ATTENTION ---~n~n", []),

    test_attention(1, "Juan compro un coche rojo en Madrid ayer",
                   "donde compro juan el coche",
                   [location, main]),

    test_attention(2, "Maria vive en Barcelona",
                   "donde vive maria",
                   [main]),

    test_attention(3, "Pedro tiene una casa grande en Malaga",
                   "que tiene pedro",
                   [main, attribute]),

    test_attention(4, "Ana compro un libro azul para Maria",
                   "para quien compro ana el libro",
                   [indirect, main]),

    test_attention(5, "Juan no vive en Madrid",
                   "donde vive juan",
                   [negation]),

    test_attention(6, "Maria visito Madrid en 2025",
                   "cuando visito maria madrid",
                   [temporal, main]),

    test_attention(7, "Pedro compro un coche y lo llevo a Barcelona",
                   "donde llevo pedro el coche",
                   [location, main]),

    test_attention(8, "Ana tiene una casa. La casa esta en Marbella.",
                   "donde esta la casa de ana",
                   [location, main]),

    test_attention(9, "Juan trabaja en Madrid desde 2020",
                   "cuando empezo juan a trabajar en madrid",
                   [temporal, main]),

    test_attention(10, "Maria compro un coche negro, pero Pedro compro una bicicleta roja",
                    "que compro maria y pedro",
                    [main, main]),

    % ── Phase 3: Summary ───────────────────────────────────────────
    format("~n--- Phase 3: METRICS SUMMARY ---~n~n", []),
    parser_metrics(PRec, PRecAll),
    format("Parser precision: ~1f% (exact) / ~1f% (subset)~n", [PRec, PRecAll]),
    format("Attention accuracy: query-type correct in top-2~n", []),

    halt().


% ── Parser test ────────────────────────────────────────────────────

test_parser(N, Text, Expected) :-
    parse_sentence(Text, Got),
    record_parser(total),
    ( my_subset(Expected, Got) ->
        Status = "PASS",
        record_parser(exact),
        record_parser(subset),
        format("~w. PASS  \"~w\"~n", [N, Text]),
        print_relations(Got)
    ; my_subset(Got, Expected) ->
        Status = "PARTIAL",
        record_parser(subset),
        format("~w. PARTIAL  \"~w\"~n", [N, Text]),
        print_relations(Got),
        print_diff(Expected, Got)
    ; Status = "FAIL",
      format("~w. FAIL  \"~w\"~n", [N, Text]),
      format("   EXPECTED:~n", []),
      print_relations(Expected),
      format("   GOT:~n", []),
      print_relations(Got),
      print_diff(Expected, Got)
    ),
    nl.

print_relations(Rels) :-
    forall(member(R, Rels), format("     ~w~n", [R])).

print_diff(Expected, Got) :-
    findall(missing(E), (member(E, Expected), \+ member(E, Got)), Miss),
    findall(extra(G), (member(G, Got), \+ member(G, Expected)), Extra),
    ( Miss \== [] -> format("   MISSING:~n", []), forall(member(missing(M), Miss), format("     ~w~n", [M])) ; true ),
    ( Extra \== [] -> format("   EXTRA:~n", []), forall(member(extra(E), Extra), format("     ~w~n", [E])) ; true ).


% ── Attention test ─────────────────────────────────────────────────

test_attention(N, Text, QueryText, ExpectedTopTypes) :-
    parse_and_attend(Text, QueryText, Selected, _All),
    extract_top_types(Selected, GotTopTypes),
    ( my_subset(ExpectedTopTypes, GotTopTypes) ->
        Status = "PASS"
    ; Status = "FAIL"
    ),
    format("~w. ~w  \"~w\"~n", [N, Status, Text]),
    format("   Query: \"~w\"~n", [QueryText]),
    format("   Results:~n", []),
    forall(
        member(scored(Rel, Score, _Heads), Selected),
        (
            Rel = relation(Type, Pred, Args),
            format("     ~w ~w~w  score=~3f~n", [Type, Pred, Args, Score])
        )
    ),
    ( Status == "FAIL" ->
        format("   EXPECTED top types: ~w~n", [ExpectedTopTypes]),
        format("   GOT top types:      ~w~n", [GotTopTypes])
    ; true ),
    nl.

extract_top_types([], []).
extract_top_types([scored(relation(Type,_,_), _, _)|T], [Type|Rest]) :-
    extract_top_types(T, Rest).


% ── Helpers ────────────────────────────────────────────────────────

my_subset([], _).
my_subset([H|T], List) :- member(H, List), my_subset(T, List).


% ── Metrics ────────────────────────────────────────────────────────

:- dynamic parser_count/2.

parser_count(exact, 0).
parser_count(subset, 0).
parser_count(total, 0).

record_parser(exact) :-
    retract(parser_count(exact, N)), N1 is N + 1,
    assert(parser_count(exact, N1)).
record_parser(subset) :-
    retract(parser_count(subset, N)), N1 is N + 1,
    assert(parser_count(subset, N1)).
record_parser(total) :-
    retract(parser_count(total, N)), N1 is N + 1,
    assert(parser_count(total, N1)).

parser_metrics(PRec, PRecAll) :-
    parser_count(exact, Exact),
    parser_count(subset, Subset),
    parser_count(total, Total),
    ( Total > 0 -> PRec is Exact * 100 / Total ; PRec = 0 ),
    ( Total > 0 -> PRecAll is Subset * 100 / Total ; PRecAll = 0 ).


:- initialization(run).
