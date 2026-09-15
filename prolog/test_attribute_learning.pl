% test_attribute_learning.pl — EXP-23 attribute/type induction.
% Baselines FROZEN (fc64115) + novel_relation.pl INTACT: this file only reads
% them. Metric 5 (25/25, 7/7, 1000/1000) is verified separately in the shell.
% Usage: swipl -s test_attribute_learning.pl -g run_exp23 -t halt
:- use_module(attribute_learning).
:- use_module(novel_relation).

:- dynamic tpass/1.
:- dynamic tfail/2.

check(Label, Goal) :-
    ( catch(call(Goal), _, fail) ->
        assertz(tpass(Label)),
        format('  OK ~w~n', [Label])
    ; assertz(tfail(Label, no_proof)),
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

run_exp23 :-
    retractall(tpass(_)), retractall(tfail(_, _)),
    novel_relation:reset_learning,
    attribute_learning:reset_attr_learning,

    format('=== PHASE A: attribute categories from QA (color, size) ===~n', []),
    learn_attributes(
        ["Elena repaired a silver bicycle.",
         "Carlos repaired a red motorcycle.",
         "David repaired a black bicycle.",
         "Sofia repaired a large radio.",
         "Tomas repaired a small mirror."],
        [qa("What color was the bicycle?", silver,
            "Elena repaired a silver bicycle."),
         qa("What color is the motorcycle?", red,
            "Carlos repaired a red motorcycle."),
         qa("What size is the radio?", large,
            "Sofia repaired a large radio."),
         qa("What size is the mirror?", small,
            "Tomas repaired a small mirror.")],
        BindsA),
    check(a1_color_bound, member(color-attr, BindsA)),
    check(a2_size_bound, member(size-attr, BindsA)),
    findall(E, attribute_learning:attr_evidence(color, attr, _, _), CE0),
    sort(CE0, CE), length(CE, NCE),
    check(a3_color_evidences, NCE >= 2),
    findall(E, attribute_learning:attr_evidence(size, attr, _, _), SE0),
    sort(SE0, SE), length(SE, NSE),
    check(a4_size_evidences, NSE >= 2),

    format('=== PHASE B: type induction from copular sentences ===~n', []),
    learn_attributes(
        ["A bicycle is a vehicle.",
         "A motorcycle is a vehicle."],
        [qa("What type of thing is a bicycle?", vehicle,
            "A bicycle is a vehicle."),
         qa("What type of thing is a motorcycle?", vehicle,
            "A motorcycle is a vehicle.")],
        BindsB),
    findall(L, attribute_learning:known_type_rel(L), RelsB),
    check(b1_type_relation, ( member(LB, RelsB),
                              attribute_learning:type_fact(LB, bicycle, vehicle),
                              attribute_learning:type_fact(LB, motorcycle, vehicle) )),
    check(b2_type_bound, member(type-type, BindsB)),
    answer_attr("A bicycle is a vehicle.",
                "What type of thing is a bicycle?", TBike),
    check_eq(b3_type_query, TBike, vehicle),

    format('=== PHASE C: transfer (no new induction calls) ===~n', []),
    answer_attr("Laura repaired a green bicycle.",
                "What color was the bicycle?", CGreen),
    check_eq(c1_novel_value, CGreen, green),
    answer_attr("Laura repaired a green bicycle.",
                "What type of thing is a bicycle?", TBike2),
    check_eq(c2_type_transfers, TBike2, vehicle),
    answer_attr("Peter repaired a blue scooter.",
                "What color is the scooter?", CBlue),
    check_eq(c3_unseen_noun_color, CBlue, blue),
    answer_attr("Peter repaired a blue scooter.",
                "What type of thing is a scooter?", TScoot),
    check_eq(c4_unseen_type_unknown, TScoot, unknown),
    answer_attr("Anna repaired a purple lamp.",
                "What color is the lamp?", CPurple),
    check_eq(c5_novel_value2, CPurple, purple),
    answer_attr("Laura repaired a green camera.",
                "What type of thing is a camera?", TCam),
    check_eq(c6_untyped_unknown, TCam, unknown),

    format('=== PHASE D: composition EXP-22 + EXP-23 ===~n', []),
    learn_from_sentences(
        ["Elena repaired a silver bicycle in Lisbon.",
         "Carlos repaired a motorcycle.",
         "David repaired a watch."],
        _IndD),
    answer_attr("Elena repaired a silver bicycle in Lisbon.",
                "Who repaired the vehicle?", DWho),
    check_eq(d1_who_repaired_vehicle, DWho, elena),
    answer_attr("Elena repaired a silver bicycle in Lisbon.",
                "What did Elena repair?", DWhat),
    check_eq(d2_what_repaired, DWhat, bicycle),
    answer_attr("Elena repaired a silver bicycle in Lisbon.",
                "What color was the vehicle?", DColor),
    check_eq(d3_color_vehicle, DColor, silver),
    answer_attr("Elena repaired a silver bicycle in Lisbon.",
                "Where did Elena repair it?", DWhere),
    check_eq(d4_where_repaired, DWhere, lisbon),

    format('=== PHASE E: unknown stays valid + no-hardcoding ===~n', []),
    answer_attr("Peter repaired a blue scooter.",
                "What type of thing is a dragon?", EDragon),
    check_eq(e1_absent_entity_unknown, EDragon, unknown),
    check(e2_no_hardcoding, \+ module_mentions_content),

    report_exp23.

% m0 restriction: code tokens of attribute_learning (comments and strings
% stripped) must not contain content vocabulary as whole tokens.
% Mechanism words (type/fact/attr/agent/...) are design vocabulary, allowed.
content_words_list([color, size, vehicle, bicycle, motorcycle, scooter,
    camera, lamp, mirror, radio, watch, silver, red, black, green, blue,
    white, gray, large, small, purple, repair, rescue, lisbon, oslo, rome,
    elena, carlos, david, sofia, tomas, laura, peter, anna, is_a]).

module_mentions_content :-
    content_words_list(Stop),
    module_property(attribute_learning, file(Path)),
    open(Path, read, S),
    get_char(S, C0),
    collect_chars(C0, S, Cs),
    close(S),
    strip_code(Cs, Clean),
    atom_chars(A, Clean),
    atom_string(A, Str),
    split_string(Str, " ", "", Parts0),
    exclude_blank(Parts0, Parts),
    member(W, Parts),
    atom_string(Atom, W),
    member(Atom, Stop), !.

exclude_blank([], []) :- !.
exclude_blank([""|T], R) :- !, exclude_blank(T, R).
exclude_blank([H|T], [H|R]) :- exclude_blank(T, R).

collect_chars(end_of_file, _, []) :- !.
collect_chars(C, S, [C|R]) :-
    get_char(S, C2),
    collect_chars(C2, S, R).

% strip_code: drop % comments and "..." strings, keep code; all other
% non-token chars become blanks.
strip_code([], []) :- !.
strip_code(['%'|T], [' '|R]) :- !,
    skip_to_nl(T, T2), strip_code(T2, R).
strip_code(['"'|T], [' '|R]) :- !,
    skip_string(T, T2), strip_code(T2, R).
strip_code([C|T], [D|R]) :-
    ( char_type(C, alnum) -> D = C
    ; C == '_' -> D = C
    ; D = ' '
    ),
    strip_code(T, R).

skip_to_nl([], []) :- !.
skip_to_nl(['\n'|T], ['\n'|T]) :- !.
skip_to_nl([_|T], R) :- skip_to_nl(T, R).

skip_string([], []) :- !.
skip_string(['"'|T], T) :- !.
skip_string(['\\', _|T], R) :- !, skip_string(T, R).
skip_string([_|T], R) :- skip_string(T, R).

report_exp23 :-
    nl,
    findall(L, tpass(L), Ps), length(Ps, NP),
    findall(L, tfail(L, _), Fs), length(Fs, NF),
    Total is NP + NF,
    format('EXP-23 SCORE: ~w/~w~n', [NP, Total]),
    group_rate([a1_color_bound, a2_size_bound, a3_color_evidences,
                a4_size_evidences, c1_novel_value, c3_unseen_noun_color,
                c5_novel_value2], Attr),
    group_rate([b1_type_relation, b2_type_bound, b3_type_query,
                c2_type_transfers], Typ),
    group_rate([c1_novel_value, c2_type_transfers, c3_unseen_noun_color,
                c5_novel_value2], Tr),
    group_rate([d1_who_repaired_vehicle, d2_what_repaired, d3_color_vehicle,
                d4_where_repaired], Comp),
    group_rate([c4_unseen_type_unknown, c6_untyped_unknown,
                e1_absent_entity_unknown], Unk),
    format('  attribute induction: ~w~n', [Attr]),
    format('  type induction: ~w~n', [Typ]),
    format('  transfer: ~w~n', [Tr]),
    format('  composition: ~w~n', [Comp]),
    format('  unknown/no-hallucination: ~w~n', [Unk]),
    ( NF =:= 0 -> format('*** ALL EXP-23 CHECKS PASSED ***~n', [])
    ; format('Failed: ~w~n', [Fs])
    ).

group_rate(Labels, Rate) :-
    findall(L, ( member(L, Labels), tpass(L) ), Ps),
    length(Labels, T), length(Ps, P),
    ( T =:= 0 -> Rate = 'n/a' ; Rate = P/T ).

:- initialization(run_exp23).
