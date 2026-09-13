% mejora.pl — Fixed Spanish exam over live Master Algorithm (never invent).
% Loaded by python -m motor vive; can also: swipl -q -s mejora.pl -g mejora_once -t halt
:- consult('live.pl').

% exam_item(Id, Question, Expect).
% Expect = known | unknown | identity | growth
exam_item(1,  'que recurrence fib?',                          known).
exam_item(2,  'que transfers_to fib?',                        known).
exam_item(3,  'fib transfers_to lucas?',                      known).
exam_item(4,  'hablame de transfer_pell_to_fib_2_1',          known).
exam_item(5,  'hablame de cassini_fib',                       known).
exam_item(6,  'hablame de geo_midline_euclid_conjectures_p0', known).
exam_item(7,  'hablame de geo_isosceles_lemma_reuse_p2',      known).
exam_item(8,  'que pisano fib?',                              known).
exam_item(9,  'que es filotaxis?',                            unknown).
exam_item(10, 'que es el alma?',                              unknown).
exam_item(11, 'quien eres?',                                  identity).
exam_item(12, 'creciste',                                     growth).

mejora_boot :-
    gaps_reset,
    clear_memory,
    catch(retractall(prov(_,_,_,_)), _, true),
    catch(retractall(prov_log(_,_)), _, true),
    ma_boot.

% Classify captured answer text.
is_unknown_text(T) :-
    ( sub_string(T, _, _, _, "I don't know")
    ; sub_string(T, _, _, _, "No lo s")
    ; sub_string(T, _, _, _, "UNKNOWN")
    ; sub_string(T, _, _, _, "No se casi nada")
    ; sub_string(T, _, _, _, "No sé casi nada")
    ; T == ""
    ), !.

ask_capture(Q, Out) :-
    with_output_to(string(Out0),
        ignore(catch(once(chat_line(Q)), E, format('UNKNOWN (~w).~n', [E])))),
    Out = Out0.

ask_special(identity, Out) :-
    with_output_to(string(Out), ma_say_who), !.
ask_special(growth, Out) :-
    with_output_to(string(Out), ma_growth), !.

run_one(Id, Q, Expect, Result) :-
    ( member(Expect, [identity, growth]) ->
        ask_special(Expect, Out)
    ; ask_capture(Q, Out)
    ),
    classify(Expect, Out, Verdict),
    Result = result(Id, Q, Expect, Verdict, Out),
    format('EXAM ~w expect=~w verdict=~w~nQ: ~w~nA: ~w~n---~n',
           [Id, Expect, Verdict, Q, Out]).

classify(known, Out, known) :- \+ is_unknown_text(Out), !.
classify(known, Out, unknown_on_known) :- is_unknown_text(Out), !.
classify(unknown, Out, correct_unknown) :- is_unknown_text(Out), !.
classify(unknown, Out, false_known) :- \+ is_unknown_text(Out), !.
classify(identity, Out, identity_ok) :-
    ( sub_string(Out, _, _, _, "Master Algorithm")
    ; sub_string(Out, _, _, _, "motor")
    ), !.
classify(identity, _, identity_bad) :- !.
classify(growth, Out, growth_ok) :-
    ( sub_string(Out, _, _, _, "hechos")
    ; sub_string(Out, _, _, _, "tipos")
    ; sub_string(Out, _, _, _, "memoria")
    ), !.
classify(growth, _, growth_bad) :- !.

% Open gaps for questions that SHOULD be answerable but weren't.
open_wrong_gaps(Results) :-
    forall(
        member(result(_, Q, known, unknown_on_known, _), Results),
        ( gap(_, Q, open, _, _) -> true ; gap_open(Q, exam_wrong_unknown) )
    ).

% Only wrong-UNKNOWNs (expect known) drive the next tick — never filotaxis/alma.
collect_gap_hints(Results, Hints) :-
    findall(H,
            ( member(result(_, Q, known, unknown_on_known, _), Results),
              gap_to_hint(Q, H)
            ),
            H0),
    sort(H0, Hints).

gap_to_hint(Q, Hint) :-
    string_lower(Q, L),
    ( sub_string(L, _, _, _, "recurrence") -> Hint = 'sequences::linear_recurrences'
    ; sub_string(L, _, _, _, "transfers")  -> Hint = 'sequences::transfer_recurrence'
    ; sub_string(L, _, _, _, "pell")       -> Hint = 'sequences::transfer_recurrence'
    ; sub_string(L, _, _, _, "cassini")    -> Hint = 'sequences::bilinear_fib'
    ; sub_string(L, _, _, _, "pisano")     -> Hint = 'sequences::modular_periods'
    ; sub_string(L, _, _, _, "geo_")       -> Hint = 'geometry::euclid_conjectures'
    ; sub_string(L, _, _, _, "lemma")      -> Hint = 'geometry::lemma_reuse'
    ; sub_string(L, _, _, _, "isos")       -> Hint = 'geometry::lemma_reuse'
    ; fail
    ).

score_results(Results, Score) :-
    findall(1, member(result(_,_,known,known,_), Results), Ks),
    findall(1, member(result(_,_,unknown,correct_unknown,_), Results), Us),
    findall(1, member(result(_,_,known,unknown_on_known,_), Results), WU),
    findall(1, member(result(_,_,identity,identity_ok,_), Results), IdOk),
    findall(1, member(result(_,_,growth,growth_ok,_), Results), GrOk),
    length(Ks, NK), length(Us, NU), length(WU, NWU),
    length(IdOk, NId), length(GrOk, NGr),
    length(Results, N),
    Score = score{
        n: N,
        known: NK,
        correct_unknown: NU,
        unknown_on_known: NWU,
        identity_ok: NId,
        growth_ok: NGr
    },
    format('SCORE known=~w correct_unknown=~w unknown_on_known=~w identity=~w growth=~w / ~w~n',
           [NK, NU, NWU, NId, NGr, N]).

% One full exam after boot. Prints machine-readable JSON-ish lines for Python.
mejora_exam :-
    mejora_boot,
    findall(result(Id, Q, Exp, Ver, Out),
            ( exam_item(Id, Q, Exp),
              run_one(Id, Q, Exp, result(Id, Q, Exp, Ver, Out))
            ),
            Results),
    open_wrong_gaps(Results),
    score_results(Results, Score),
    collect_gap_hints(Results, Hints),
    format('HINTS ~w~n', [Hints]),
    memory_size(Mem),
    format('MEM ~w~n', [Mem]),
    % Emit one RESULT line per item for the Python parser.
    forall(member(result(Id, Q, Exp, Ver, Out), Results),
           ( atom_string(Ver, Vs),
             format('RESULT|~w|~w|~w|~w~n', [Id, Exp, Ver, Q]),
             format('ANSWER|~w|~s~n', [Id, Out])
           )),
    format('SCORELINE|~w|~w|~w|~w|~w|~w~n',
           [Score.n, Score.known, Score.correct_unknown,
            Score.unknown_on_known, Score.identity_ok, Score.growth_ok]),
    format('HINTLINE|~w~n', [Hints]),
    gap_report.

mejora_once :-
    mejora_exam.
