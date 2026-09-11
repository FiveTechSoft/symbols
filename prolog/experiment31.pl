% experiment31.pl
% EXPERIMENT 31 - FRAME ALIGNMENT (gives vs receives, roles descubiertos)
% Parser: solo posicion + 'to'/'from' (closed-class). NADA de verbos.
% 6 pares mismo-evento/dos-marcos -> merge (6 nodos, no 12) + mapa.
% 2 eventos single-frame frescos -> 4 queries cruzadas + 2 distractores.
:- consult('memory.pl').
:- consult('positional.pl').
:- consult('frame_align.pl').

:- use_module(library(lists)).

:- dynamic check_results/2.

experiment31 :-
    reset_experiment,
    feed_training,
    merge_compute,
    align_phase,
    merge_rewrite,
    feed_test,
    query_phase,
    report_checks.

reset_experiment :-
    clear_memory,
    retractall(check_results(_, _)),
    reset_frames.

feed(S) :-
    tokenize_pos(S, Tokens),
    ( parse_event(Tokens, _) -> true
    ; format('PARSE FAIL: ~w~n', [S]), fail
    ).

% 6 eventos x 2 marcos (mismos participantes por par)
training_pairs([
    ["maria gives book to pedro", "pedro receives book from maria"],
    ["juan gives ball to ana", "ana receives ball from juan"],
    ["leo gives pen to mia", "mia receives pen from leo"],
    ["pedro gives cake to maria", "maria receives cake from pedro"],
    ["ana gives hat to juan", "juan receives hat from ana"],
    ["mia gives box to leo", "leo receives box from mia"]
]).

feed_training :-
    nl, writeln('===== TRAIN: 6 events x 2 frames ====='),
    training_pairs(Pairs),
    forall(member([S1, S2], Pairs),
           ( feed(S1), feed(S2) )),
    findall(E, memory_relation(E, verb, _, _, _), E0),
    sort(E0, Es),
    length(Es, N),
    check(N =:= 12, '12 provisional event nodes').

:- dynamic merged_pairs/1.

merge_compute :-
    nl, writeln('===== MERGE (same participant set) ====='),
    merge_events(Merged),
    length(Merged, NM),
    check(NM =:= 6, '6 merged pairs (no invented merges)'),
    retractall(merged_pairs(_)),
    assertz(merged_pairs(Merged)).

merge_rewrite :-
    merged_pairs(Merged),
    rewrite_merged(Merged),
    findall(E, memory_relation(E, verb, _, _, _), E0),
    sort(E0, Es),
    length(Es, NE),
    check(NE =:= 6, '6 event nodes survive (12 -> 6)').

align_phase :-
    induce_alignment,
    show_alignment,
    check(frame_map(gives, subj, receives, from),
          'map: gives.subj <-> receives.from'),
    check(frame_map(gives, obj, receives, obj),
          'map: gives.obj <-> receives.obj'),
    check(frame_map(gives, to, receives, subj),
          'map: gives.to <-> receives.subj').

% 2 eventos frescos, UN marco cada uno (entidades nuevas)
feed_test :-
    nl, writeln('===== TEST: single-frame fresh events ====='),
    feed("zara gives drum to yago"),
    feed("yago receives flute from zara").

query_phase :-
    nl, writeln('===== CROSS-FRAME QUERIES ====='),
    % E1 en marco gives; Q en ambos marcos
    ask_q("Who gave drum to yago?", zara),
    ask_q("Who receives drum from zara?", yago),
    % E2 en marco receives; Q en ambos marcos
    ask_q("Who receives flute from zara?", yago),
    ask_q("Who gave flute to yago?", zara),
    % distractores: roles invertidos (nadie)
    ask_unknown("Who gave drum to zara?"),
    ask_unknown("Who receives flute from yago?").

% "Who gave X to Y?" -> marco gives: giver=?, theme=X, recipient=Y
ask_q(Q, Expected) :-
    tokenize_pos(Q, Tokens),
    ( Tokens = [who, gave, X, to, Y] ->
        ( query_event(gives, [(subj, Who), (obj, X), (to, Y)], _) ->
            WhoResult = Who
        ; WhoResult = unknown
        )
    ; Tokens = [who, receives, X, from, Y] ->
        ( query_event(receives, [(subj, Who), (obj, X), (from, Y)], _) ->
            WhoResult = Who
        ; WhoResult = unknown
        )
    ),
    ( WhoResult == Expected ->
        format('PASS ~w -> ~w~n', [Q, Expected]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w -> ~w (expected ~w)~n', [Q, WhoResult, Expected]),
      assertz(check_results(Q, fail))
    ).

ask_unknown(Q) :-
    tokenize_pos(Q, Tokens),
    ( Tokens = [who, gave, X, to, Y] ->
        ( query_event(gives, [(subj, _), (obj, X), (to, Y)], _) ->
            Res = found
        ; Res = unknown
        )
    ; Tokens = [who, receives, X, from, Y] ->
        ( query_event(receives, [(subj, _), (obj, X), (from, Y)], _) ->
            Res = found
        ; Res = unknown
        )
    ),
    ( Res == unknown ->
        format('PASS reject ~w~n', [Q]),
        assertz(check_results(Q, pass))
    ; format('FAIL ~w should be unknown~n', [Q]),
      assertz(check_results(Q, fail))
    ).

% --- checks ---

check(Goal, Label) :-
    ( call(Goal) ->
        assertz(check_results(Label, pass)),
        format('PASS ~w~n', [Label])
    ; assertz(check_results(Label, fail)),
      format('FAIL ~w~n', [Label])
    ).

report_checks :-
    nl, writeln('===== SUMMARY ====='),
    findall(1, check_results(_, pass), Ps),
    length(Ps, NP),
    findall(1, check_results(_, fail), Fs),
    length(Fs, NF),
    Total is NP + NF,
    format('passed ~w/~w~n', [NP, Total]).
