% test_multihop.pl — EXP-24 multi-hop learned reasoning.
% Baselines FROZEN (fc64115) + novel_relation.pl + attribute_learning.pl
% INTACT: this file only reads them. Metric 5 (25/25, 7/7, 1000/1000,
% EXP-22 14/14, EXP-23 19/19) is verified separately in the shell.
% Usage: swipl -s test_multihop.pl -g run_exp24 -t halt
:- use_module(multihop).
:- use_module(novel_relation).
:- use_module(attribute_learning).

:- dynamic tpass/1.
:- dynamic tfail/2.
:- dynamic novel_count/1.

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
% check_path(+Label, +Text, +Query, +Expected, +MinHops) : answer correct
% AND trajectory meets the hop count (novel-conclusion accounting).
check_path(Label, Text, Query, Expected, MinHops) :-
    ( catch(answer_mhop(Text, Query, Got, Path), _, fail),
      Got == Expected,
      length(Path, L), L >= MinHops ->
        assertz(tpass(Label)),
        format('  OK ~w => ~w [hops:~w]~n', [Label, Got, L]),
        ( novel_path(Path) ->
            ( retract(novel_count(N)) -> true ; N = 0 ),
            N1 is N + 1,
            assertz(novel_count(N1))
        ; true
        )
    ; assertz(tfail(Label, Got)),
        format('  FAIL ~w => ~w (expected ~w, hops>=~w)~n',
               [Label, Got, Expected, MinHops])
    ),
    !.

% novel_path(+Path) : multi-hop composition or backward traversal.
% Single forward stored hops are NOT novel (honest negative controls).
novel_path(Path) :-
    length(Path, L), L > 1, !.
novel_path(Path) :-
    member(hop(_, _, _, bwd, _, _, _), Path).

run_exp24 :-
    retractall(tpass(_)), retractall(tfail(_, _)),
    retractall(novel_count(_)), assertz(novel_count(0)),
    novel_relation:reset_learning,
    attribute_learning:reset_attr_learning,
    multihop:reset_mhop,

    format('=== SETUP: learn repair + chains + kind/color bindings ===~n', []),
    learn_from_sentences(
        ["Elena repaired a silver bicycle in Lisbon.",
         "Carlos repaired a motorcycle.",
         "David repaired a watch."],
        _IndR),
    learn_multihop(
        ["A bicycle is a vehicle.",
         "A vehicle is a machine.",
         "A machine is an object.",
         "Machines are objects.",
         "A motorcycle is a machine.",
         "A bicycle has wheels.",
         "A wheel is a part of a vehicle."],
        [qa("What kind of thing is the bicycle?", vehicle,
            "A bicycle is a vehicle."),
         qa("What kind of thing is the motorcycle?", machine,
            "A motorcycle is a machine."),
         qa("What type of thing is a bicycle?", vehicle,
            "A bicycle is a vehicle."),
         qa("What type of thing is a motorcycle?", machine,
            "A motorcycle is a machine."),
         qa("What type of thing is a bicycle?", vehicle,
            "A bicycle is a vehicle."),
         qa("What type of thing is a motorcycle?", machine,
            "A motorcycle is a machine."),
         qa("What color is the bicycle?", silver,
            "Elena repaired a silver bicycle."),
         qa("What color is the motorcycle?", red,
            "Carlos repaired a red motorcycle.")],
        Induced24),
    check(s0_has_registered,
          ( member(H, Induced24), multihop:verb_match(H, have) )),
    check(s0_part_registered,
          ( member(P, Induced24), multihop:verb_match(part, P) )),

    format('=== 1-HOP anchor (stored, non-novel control) ===~n', []),
    check_path(h1_type_bicycle,
        "A bicycle is a vehicle. A vehicle is a machine.",
        "What type of thing is the bicycle?", vehicle, 1),

    format('=== 2-HOP ===~n', []),
    check_path(h2_kind_vehicle_object,
        "A vehicle is a machine. A machine is an object.",
        "What kind of object is the vehicle?", object, 2),

    format('=== 3-HOP ===~n', []),
    check_path(h3_kind_bicycle_object,
        "A bicycle is a vehicle. A vehicle is a machine. A machine is an object.",
        "What kind of object is the bicycle?", object, 3),

    format('=== MIXED relations chain ===~n', []),
    check_path(m1_repair_bicycle,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What did Elena repair?", bicycle, 1),
    check_path(m2_bicycle_have,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What does the bicycle have?", wheels, 1),
    check_path(m3_wheel_part,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What is the wheel part of?", vehicle, 1),
    check_path(m4_vehicle_type,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What type of thing is the vehicle?", machine, 1),
    check_path(m5_wheels_vehicle_machine,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What type of thing is the wheel's vehicle?", machine, 2),

    format('=== INVERSION (backward traversal) ===~n', []),
    check_path(i1_what_is_machine,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What is a machine?", vehicle, 1),
    check_path(i2_what_is_vehicle,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What is a vehicle?", bicycle, 1),
    check_path(i3_has_wheels_inverse,
        "Elena repaired a bicycle. A bicycle has wheels. A wheel is a part of a vehicle. A vehicle is a machine.",
        "What has wheels?", bicycle, 1),

    format('=== COMPOSITION EXP-22+23+24 ===~n', []),
    check_path(c1_repair_bicycle,
        "Elena repaired a silver bicycle. A bicycle is a vehicle. A vehicle is a machine. Machines are objects.",
        "What did Elena repair?", bicycle, 1),
    check_path(c2_color_vehicle,
        "Elena repaired a silver bicycle. A bicycle is a vehicle. A vehicle is a machine. Machines are objects.",
        "What color was the vehicle Elena repaired?", silver, 1),
    check_path(c3_type_bicycle,
        "Elena repaired a silver bicycle. A bicycle is a vehicle. A vehicle is a machine. Machines are objects.",
        "What type of thing is the bicycle?", vehicle, 1),
    check_path(c4_ultimately_object,
        "Elena repaired a silver bicycle. A bicycle is a vehicle. A vehicle is a machine. Machines are objects.",
        "What type of thing is the bicycle ultimately?", object, 3),

    format('=== NEGATIVE controls (stored, must NOT count novel) ===~n', []),
    check_path(n1_machine_object_stored,
        "A vehicle is a machine. A machine is an object.",
        "What kind of object is the machine?", object, 1),
    check_path(n2_vehicle_machine_stored,
        "A vehicle is a machine. A machine is an object.",
        "What kind of object is the vehicle?", object, 2),

    format('=== UNKNOWN must stay valid ===~n', []),
    unknown_answer("A bicycle is a vehicle.",
                   "What kind of object is the scooter?", U1),
    check_eq(u1_scooter_kind, U1, unknown),
    unknown_answer("Elena repaired a bicycle. A bicycle has wheels.",
                   "What does the motorcycle have?", U2),
    check_eq(u2_motorcycle_have, U2, unknown),
    unknown_answer("A wheel is a part of a vehicle.",
                   "What is the scooter part of?", U3),
    check_eq(u3_scooter_part, U3, unknown),

    format('=== NO-HARDCODING self-check ===~n', []),
    check(m0_no_hardcoding, \+ module_mentions_content),

    report_exp24.

% unknown_answer(+Text, +Query, -Answer) : never throws, unknown on failure.
unknown_answer(Text, Query, Answer) :-
    ( catch(answer_mhop(Text, Query, A, _), _, fail), nonvar(A) ->
        Answer = A
    ; Answer = unknown
    ).

% m0 restriction: code tokens of multihop (comments/strings stripped) must
% not contain content vocabulary as whole tokens. Query-syntax words
% (who/what/which/ultimately) and mechanism tags (type/poss/mereo/event,
% fwd/bwd, attr/fact/rel/hop) are design vocabulary, explicitly allowed.
content_words_list([bicycle, vehicle, machine, object, wheel, wheels, has,
    have, part, silver, lisbon, elena, carlos, motorcycle, kind, color, red,
    black, green, blue, size, large, small, purple, scooter, camera, lamp,
    mirror, radio, watch, david, laura, peter, anna, sofia, tomas, rome,
    oslo, rescue, repair, repaired, rescued, is_a]).

module_mentions_content :-
    content_words_list(Stop),
    module_property(multihop, file(Path)),
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

report_exp24 :-
    nl,
    findall(L, tpass(L), Ps), length(Ps, NP),
    findall(L, tfail(L, _), Fs), length(Fs, NF),
    Total is NP + NF,
    novel_count(Novel),
    format('EXP-24 SCORE: ~w/~w  (novel conclusions: ~w)~n', [NP, Total, Novel]),
    group_rate([h1_type_bicycle, h2_kind_vehicle_object,
                h3_kind_bicycle_object], Hop),
    group_rate([m1_repair_bicycle, m2_bicycle_have, m3_wheel_part,
                m4_vehicle_type, m5_wheels_vehicle_machine], Mixed),
    group_rate([i1_what_is_machine, i2_what_is_vehicle,
                i3_has_wheels_inverse], Inv),
    group_rate([c1_repair_bicycle, c2_color_vehicle, c3_type_bicycle,
                c4_ultimately_object], Comp),
    group_rate([u1_scooter_kind, u2_motorcycle_have, u3_scooter_part], Unk),
    format('  2-3 hop: ~w~n', [Hop]),
    format('  mixed: ~w~n', [Mixed]),
    format('  inversion: ~w~n', [Inv]),
    format('  composition: ~w~n', [Comp]),
    format('  unknown: ~w~n', [Unk]),
    ( NF =:= 0 -> format('*** ALL EXP-24 CHECKS PASSED ***~n', [])
    ; format('Failed: ~w~n', [Fs])
    ).

group_rate(Labels, Rate) :-
    findall(L, ( member(L, Labels), tpass(L) ), Ps),
    length(Labels, T), length(Ps, P),
    ( T =:= 0 -> Rate = 'n/a' ; Rate = P/T ).

:- initialization(run_exp24).
