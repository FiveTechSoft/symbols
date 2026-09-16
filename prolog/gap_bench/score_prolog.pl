% score_prolog.pl â€” independent second-method scorer (hand-transcribed keys)
% Usage: swipl -q -f score_prolog.pl -g run -t halt

:- use_module(library(readutil)).
:- use_module(library(pcre)).
:- dynamic res/4.

split_ws(Atom, Words) :-
    atom_string(Atom, S),
    string_lower(S, L),
    split_string(L, " ", " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", Ts),
    exclude(=(""), Ts, Clean),
    maplist(atom_string, Words, Clean).

idk_word(W) :- member(W, [know, unknown, idea]).

% spec(ID, Groups, IdkCorrect, MinCount)  -- hand-transcribed from battery.json
spec(a01, [[lord]], false, 0).
spec(a02, [[word]], false, 0).
spec(a03, [[lord]], false, 0).
spec(a04, [[wind]], false, 0).
spec(a05, [[men]], false, 0).
spec(a06, [[people]], false, 0).
spec(a07, [[lord]], false, 0).
spec(a08, [[soul]], false, 0).
spec(a09, [[afraid, fear, scared]], false, 0).
spec(a10, [[mariners, sailors]], false, 0).
spec(a11, [[lord]], false, 0).
spec(a12, [[jonah]], false, 0).
spec(a13, [[yes]], false, 0).
spec(a14, [[waters, sea]], false, 0).
spec(a15, [[yes]], false, 0).
spec(a16, [[yes]], false, 0).
spec(a17, [[yes]], false, 0).
spec(a18, [[yes]], false, 0).
spec(a19, [], true, 0).
spec(a20, [[no, idk]], true, 0).
spec(b01, [[paris]], false, 0).
spec(b02, [[shakespeare]], false, 0).
spec(b03, [[water]], false, 0).
spec(b04, [[seven, '7']], false, 0).
spec(b05, [[blue]], false, 0).
spec(b06, [[cow]], false, 0).
spec(b07, [[four, '4']], false, 0).
spec(b08, [[spanish]], false, 0).
spec(b09, [[hello, hi, hey, greetings, help, welcome]], false, 0).
spec(b10, [[fine, well, good, great, ok, doing]], false, 0).
spec(b11, [[welcome, pleasure, anytime, glad]], false, 0).
spec(b12, [[goodbye, bye, farewell, see]], false, 0).
spec(b13, [[fish], [once, there, tale, story]], false, 0).
spec(b14, [[sea, ocean, wave]], false, 0).
spec(b15, [[animal, creature, water, aquatic, gill, swim, lives]], false, 0).
spec(b16, [[cat, dog, cow, lion, tiger, bird, fish, horse, elephant, mouse, sheep, goat, wolf, bear, pig, rabbit]], false, 3).
spec(b17, [[assistant, ai, model, language, program, prolog, system, bot, robot, knowledge]], false, 0).
spec(b18, [[yes, no, learn, can, cannot, able]], false, 0).
spec(b19, [[yes, jonah]], false, 0).
spec(b20, [[answer, question, learn, remember, help, chat, know, fact, memory]], false, 0).

bin(spec(_, Groups, _, MinC), Words, Bin) :-
    ( MinC > 0 ->
        nth0(0, Groups, G),
        include(member_word(G), Words, Hits),
        length(Hits, C),
        ( C >= MinC -> Bin = correct
        ; C > 0 -> Bin = partial
        ; include(idk_word, Words, [_|_]) -> Bin = unknown
        ; Bin = wrong )
    ; Groups == [] ->
        ( include(idk_word, Words, [_|_]) -> Bin = unknown ; Bin = wrong )
    ; Groups = [Only] ->
        ( include(member_word(Only), Words, [_|_]) -> Bin = correct
        ; include(idk_word, Words, [_|_]) -> Bin = unknown
        ; Bin = wrong )
    ; maplist(group_hit(Words), Groups, GR),
      ( \+ member(no, GR) -> Bin = correct
      ; member(yes, GR) -> Bin = partial
      ; include(idk_word, Words, [_|_]) -> Bin = unknown
      ; Bin = wrong )
    ).

group_hit(Words, G, yes) :- include(member_word(G), Words, [_|_]), !.
group_hit(_, _, no).

member_word(G, W) :- memberchk(W, G).

credit2(correct, _, 1.0).
credit2(partial, _, 0.5).
credit2(unknown, true, 1.0).
credit2(unknown, false, 0.0).
credit2(wrong, _, 0.0).

read_answers(Path, Dict) :-
    setup_call_cleanup(
      open(Path, read, S),
      ( findall(Line,
            ( repeat,
              read_line_to_string(S, Line),
              ( Line == end_of_file -> !, fail ; true ) ),
            Lines),
        close(S) ),
      true),
    findall(ID-Ans,
      ( member(Line, Lines),
        re_matchsub('^(Q\\d+|[AB]\\d\\d) \\|\\|\\| (.*?) \\|\\|\\| (.*)$', Line, Sub, []),
        normalize_id(Sub.get(1), ID),
        Ans = Sub.get(3) ),
      Pairs),
    dict_create(Dict, answers, Pairs).

normalize_id(Raw, Norm) :-
    atom_string(A2, Raw),
    downcase_atom(A2, A3),
    ( ( sub_atom(A3, 0, 1, _, a) ; sub_atom(A3, 0, 1, _, b) ) -> Norm = A3
    ; sub_atom(A3, 1, _, 0, Rest),
      atom_number(Rest, N),
      ( N =< 20 -> format(atom(Norm), 'a~|~`0t~d~2+', [N])
      ; M is N - 20, format(atom(Norm), 'b~|~`0t~d~2+', [M]) ) ).

run :-
    read_answers('gap_prolog_raw.txt', P),
    read_answers('gap_tiny_raw.txt', T),
    retractall(res(_, _, _, _)),
    forall(spec(ID, G, IdkOK, MC),
           forall(member(Sys, [p, t]),
             ( ( Sys = p -> Dict = P ; Dict = T ),
               ( get_dict(ID, Dict, Ans) ->
                   split_ws(Ans, Ws),
                   bin(spec(ID, G, IdkOK, MC), Ws, Bin)
               ; format('MISSING ~w ~w~n', [Sys, ID]),
                 Bin = wrong ),
               credit2(Bin, IdkOK, Cr),
               assertz(res(Sys, ID, Bin, Cr)) ))),
    forall(member(Sys, [p, t]),
      ( findall(Cr, res(Sys, _, correct, Cr), L1),
        findall(Cr, res(Sys, _, partial, Cr), L2),
        findall(Cr, res(Sys, _, wrong, Cr), L3),
        findall(Cr, res(Sys, _, unknown, Cr), L4),
        length(L1, C1), length(L2, C2), length(L3, C3), length(L4, C4),
        sum_list(L1, S1), sum_list(L2, S2), sum_list(L4, S4),
        Score is S1 + S2 + S4,
        ( Sys = p -> Name = prolog ; Name = tinyllama ),
        format('~w: correct=~w partial=~w wrong=~w unknown=~w score=~1f/40~n',
               [Name, C1, C2, C3, C4, Score]),
        forall(member(Tier, [a, b]),
          ( findall(Cr, (res(Sys, ID, Bin, Cr), sub_atom(ID, 0, 1, _, Tier)), LT),
            length(LT, NT),
            sum_list(LT, ST),
            format('  tier~w ~w: ~1f/~w~n', [Tier, Name, ST, NT]) )))),
    tell('bins_prolog.txt'),
    forall(res(Sys, ID, Bin, Cr), format('~w,~w,~w,~1f~n', [Sys, ID, Bin, Cr])),
    told,
    halt.