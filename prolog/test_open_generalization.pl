% test_open_generalization.pl — EXP-25 open generalization.
% Baselines FROZEN (fc64115) + novel_relation.pl + attribute_learning.pl +
% multihop.pl INTACT: this file only reads them. Metric 5 (all prior suites)
% is verified separately in the shell.
% Data: open_world_data.pl (test_data(N, Group, Episode, Text, Query, Exp)).
% Usage: swipl -s test_open_generalization.pl -g run_exp25 -t halt
:- use_module(open_generalization).
:- use_module(novel_relation).
:- use_module(attribute_learning).
:- use_module(multihop).
:- consult('open_world_data.pl').

:- dynamic tpass/2.
:- dynamic tfail/3.

run_exp25 :-
    retractall(tpass(_, _)), retractall(tfail(_, _, _)),
    novel_relation:reset_learning,
    attribute_learning:reset_attr_learning,
    multihop:reset_mhop,
    open_generalization:reset_open,

    format('=== LEARN (all worlds upfront, no resets: isolation must hold structurally) ===~n', []),
    learn_from_sentences(
        ["Nora restored a bronze lantern in Prague.",
         "Liam restored a copper kettle in Cairo.",
         "Victor carried a wooden chest in Kyoto.",
         "Mia carried an indigo compass in Seoul.",
         "Sara painted a violet sculpture in Lima.",
         "Omar painted a beige fence in Roma."],
        IndR),
    format('  induced verbs: ~w~n', [IndR]),
    open_learn(
        ["Nora restored a bronze lantern in Prague.",
         "Liam restored a copper kettle in Cairo.",
         "The lantern belongs to a museum.",
         "A museum is an institution.",
         "A lantern is an artifact.",
         "An artifact is an object.",
         "Liam built a stone wall.",
         "Victor carried a wooden chest in Kyoto.",
         "Mia carried an indigo compass in Seoul.",
         "The chest belongs to an archive.",
         "An archive is an institution.",
         "A chest is a container.",
         "A container is an object.",
         "A compass is an instrument.",
         "An instrument is an object.",
         "The chest contains a map.",
         "Sara painted a violet sculpture in Lima.",
         "Omar painted a beige fence in Roma.",
         "Tomas carved a turquoise flute in Oslo.",
         "The sculpture depicts a horse.",
         "A sculpture is an artwork.",
         "An artwork is an object.",
         "A lantern is NOT a container.",
         "In Prague, Nora restored a bronze lantern.",
         "A bronze lantern was restored by Nora in Prague."],
        [qa("What color is the lantern?", bronze,
            "Nora restored a bronze lantern in Prague."),
         qa("What color is the sculpture?", violet,
            "Sara painted a violet sculpture in Lima."),
         qa("What material is the chest?", wooden,
            "Victor carried a wooden chest in Kyoto."),
         qa("What material is the wall?", stone,
            "Liam built a stone wall."),
         qa("What type of thing is the lantern?", artifact,
            "A lantern is an artifact."),
         qa("What type of thing is the chest?", container,
            "A chest is a container."),
         qa("What kind of object is the lantern?", object,
            "A lantern is an artifact."),
         qa("What kind of object is the chest?", object,
            "A chest is a container.")],
        IndO),
    format('  induced relations: ~w~n', [IndO]),

    format('=== ANSWER (120 items) ===~n', []),
    forall(test_data(N, Group, Episode, Text, Query, Expected),
           run_one(N, Group, Episode, Text, Query, Expected)),
    write_open_results,
    report_exp25.

run_one(N, Group, Episode, Text, Query, Expected) :-
    ( catch(answer_open(Text, Query, Got, _), _, fail), nonvar(Got) ->
        ( Got == Expected ->
            assertz(tpass(Group, N))
        ; assertz(tfail(Group, N, Got))
        )
    ; assertz(tfail(Group, N, error))
    ),
    ( N mod 20 =:= 0 -> format('~w ', [N]) ; true ).

write_open_results :-
    open('open_results.txt', write, S),
    forall(test_data(N, Group, Episode, Text, Query, Expected),
           ( ( tpass(Group, N) -> St = pass, Mark = pass
             ; tfail(Group, N, Got) -> St = Got, Mark = fail
             ; St = missing, Mark = fail
             ),
             format(S, '~w|~w|~w|~w|~w|~w~n',
                    [N, Group, Episode, Expected, St, Mark])
           )),
    close(S).

report_exp25 :-
    nl,
    findall(G, tpass(G, _), Ps), length(Ps, NP),
    findall(G, tfail(G, _, _), Fs), length(Fs, NF),
    Total is NP + NF,
    format('EXP-25 SCORE: ~w/~w~n', [NP, Total]),
    forall(member(G, [vocab, attr, type, multihop, compositional,
                      adversarial]),
           ( findall(N, tpass(G, N), GP), length(GP, NGP),
             findall(N, tfail(G, N, _), GF), length(GF, NGF),
             Tot is NGP + NGF,
             format('  ~w: ~w/~w~n', [G, NGP, Tot])
           )),
    % isolation: cross-episode items (Episode AB).
    findall(N, (tpass(adversarial, N),
                test_data(N, adversarial, 'AB', _, _, _)), IsoP),
    findall(N, (tfail(adversarial, N, _),
                test_data(N, adversarial, 'AB', _, _, _)), IsoF),
    length(IsoP, NIP), length(IsoF, NIF), NIso is NIP + NIF,
    format('  isolation(AB): ~w/~w~n', [NIP, NIso]),
    % unknown: every item expecting unknown/no_evidence.
    findall(N, (test_data(N, _, _, _, _, E),
                (E == unknown ; E == no_evidence),
                tpass(_, N)), UnP),
    findall(N, (test_data(N, _, _, _, _, E),
                (E == unknown ; E == no_evidence),
                tfail(_, N, _)), UnF),
    length(UnP, NUP), length(UnF, NUF), NUn is NUP + NUF,
    format('  unknown: ~w/~w~n', [NUP, NUn]),
    % structural probes: fronted/passive episodes.
    findall(N, (tpass(adversarial, N),
                (test_data(N, adversarial, 'F', _, _, _) ;
                 test_data(N, adversarial, 'P', _, _, _))), StP),
    findall(N, (tfail(adversarial, N, _),
                (test_data(N, adversarial, 'F', _, _, _) ;
                 test_data(N, adversarial, 'P', _, _, _))), StF),
    length(StP, NSP), length(StF, NSF), NSt is NSP + NSF,
    format('  structural-probes(F/P): ~w/~w~n', [NSP, NSt]),
    ( NF =:= 0 -> format('*** ALL EXP-25 CHECKS PASSED ***~n', [])
    ; format('Failed: '),
      forall(tfail(G, N, Got),
             ( test_data(N, G, Ep, _, Q, Exp),
               format('~n  #~w [~w/~w] ~w Exp:~w Got:~w',
                      [N, G, Ep, Q, Exp, Got]) )),
      nl
    ),
    format('~nHARDCODING check: see m0 below~n', []),
    ( \+ module_mentions_content ->
        format('HARDCODING = 0~n', []),
        assertz(tpass(m0, 0))
    ; format('HARDCODING VIOLATION (see listing)~n', []),
        assertz(tfail(m0, 0, found))
    ).

% m0 restriction: code tokens of open_generalization (comments and strings
% stripped) must not contain dataset content vocabulary as whole tokens.
% Query-syntax (who/what/which/where/when/how, auxiliaries, of/the/a,
% she/he/it) and mechanism tags (type/poss/mereo/event/attr/fact/rel/hop,
% same status as the frozen main/location/attribute tags) are design
% vocabulary, explicitly allowed.
content_words_list([nora, liam, victor, mia, sara, omar, tomas, lantern,
    kettle, compass, chest, sculpture, fence, flute, wall, map, horse,
    museum, archive, bronze, copper, indigo, violet, beige, turquoise,
    wooden, stone, prague, cairo, kyoto, seoul, lima, roma, oslo, artifact,
    object, container, institution, instrument, artwork, restore, restored,
    carry, carried, paint, painted, build, built, carve, carved, belong,
    belongs, contain, contains, depict, depicts, has, have, part, color,
    material, kind, is_a]).

module_mentions_content :-
    content_words_list(Stop),
    module_property(open_generalization, file(Path)),
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
    member(Atom, Stop),
    format('  leaked token: ~w~n', [Atom]), !.

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

:- initialization(run_exp25).
