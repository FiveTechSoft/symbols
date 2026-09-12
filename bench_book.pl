% bench_book.pl — Benchmark de ingesta por fases (baseline reproducible).
% Uso: swipl -s bench_book.pl -g "bench_book('books/alice.txt', alice)" -t halt
% Fases separadas: READ SCAN TOK FRAMES REG MEM, mas micro A/B BARE
% (assert pelado) para aislar el coste de prov+conflict de remember_tracked.
% No cambia doc_corpus/gen_parse: reutiliza sus predicados.
:- consult('corpus.pl').
:- consult('gen_parse.pl').
:- consult('doc_corpus.pl').

:- use_module(library(lists)).
:- dynamic bench_fact/3.

bench_book(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(bench_fact(_, _, _)),
    doc_reset,
    bench_t(read_file(File, Lines), MsRead, _),
    length(Lines, NLines),
    format('BENCH ~w READ lines=~w ms=~2f~n', [Alias, NLines, MsRead]),
    bench_t(doc_scan(File, Alias, Chapters), MsScan, _),
    doc_census(Census),
    length(Census, NCen),
    format('BENCH ~w SCAN census=~w ms=~2f~n', [Alias, NCen, MsScan]),
    bench_t(collect_sents(Chapters, Sents), MsSent, _),
    length(Sents, NSents),
    format('BENCH ~w SENT sents=~w ms=~2f~n', [Alias, NSents, MsSent]),
    bench_t(tokenize_all(Sents, Toks), MsTok, _),
    format('BENCH ~w TOK ms=~2f~n', [Alias, MsTok]),
    doc_census(Census),
    bench_t(frames_all(Alias, Toks, Census, FR), MsFrames, _),
    length_flat(FR, NFrames),
    format('BENCH ~w FRAMES frames=~w ms=~2f~n', [Alias, NFrames, MsFrames]),
    bench_t(reg_all(FR, NMent), MsReg, _),
    format('BENCH ~w REG mentions=~w ms=~2f~n', [Alias, NMent, MsReg]),
    bench_t(mem_all(FR, Alias, NTri), MsMem, _),
    format('BENCH ~w MEM triples=~w ms=~2f~n', [Alias, NTri, MsMem]),
    bench_t(bare_all(FR), MsBare, _),
    format('BENCH ~w BARE ms=~2f~n', [Alias, MsBare]),
    memory_size(NF),
    MsTot is MsRead + MsScan + MsSent + MsTok + MsFrames + MsReg + MsMem,
    format('BENCH ~w TOTAL ms=~2f memfacts=~w~n', [Alias, MsTot, NF]),
    format('BENCH ~w UNIT ms_per_sent=~4f ms_per_triple=~4f ms_per_fact=~4f~n',
           [Alias, MsTot / NSents, MsTot / NTri, MsTot / NF]).

bench_t(Goal, Ms, _N) :-
    get_time(T0),
    call(Goal),
    get_time(T1),
    Ms is (T1 - T0) * 1000.

read_file(File, Lines) :-
    open(File, read, S, [encoding(utf8)]),
    read_all_lines(S, Lines),
    close(S),
    length(Lines, _).

read_all_lines(S, Lines) :-
    read_line_to_string(S, L),
    ( L == end_of_file -> Lines = []
    ; read_all_lines(S, R), Lines = [L|R]
    ).

% Sents = [(CN, SId, Text)] con el mismo orden del lector.
collect_sents(Chapters, Sents) :-
    findall((CN, SId, Sn),
            (member(ch(CN, Paras), Chapters),
             member(P, Paras),
             doc_sentences(P, Ss),
             nth1(SId, Ss, Sn)),
            Sents).

tokenize_all(Sents, Toks) :-
    findall((CN, SId, Lower, Raw),
            (member((CN, SId, Sn), Sents),
             gen_tokenize(Sn, Lower, Raw)),
            Toks).

% FR = [(Ref, Frames, Mentions)]; LastEnt roscado sin registro.
frames_all(Alias, Toks, Census, FR) :-
    frames_loop(Alias, Toks, Census, none, FR, 0, _NF).

frames_loop(_, [], _, _, [], NF, NF).
frames_loop(Alias, [(CN, SId, Lower, Raw)|Ts], Census, LE, [(Ref, Frames, Ments)|R], N0, NF) :-
    format(atom(Ref), '~w_ch~w_s~w', [Alias, CN, SId]),
    gen_frames(Lower, Raw, Census, LE, Frames, _, Ments),
    last_entity_of(Ments, LE, LE2),
    length(Frames, K),
    N1 is N0 + K,
    frames_loop(Alias, Ts, Census, LE2, R, N1, NF).

last_entity_of([], LE, LE).
last_entity_of([narrator|Ms], LE, Out) :- !,
    last_entity_of(Ms, LE, Out).
last_entity_of([N|Ms], _, Out) :-
    last_entity_of(Ms, N, Out).

% Registro documental (asserts) sobre las menciones ya extraidas.
reg_all(FR, NMent) :-
    reg_loop(FR, 0, NMent).

reg_loop([], N, N).
reg_loop([(_, _, Ments)|R], N0, N) :-
    length(Ments, K),
    N1 is N0 + K,
    forall(member(M, Ments), reg_one(M)),
    reg_loop(R, N1, N).

reg_one(N) :-
    ( doc_ent(N, C, _) ->
        retract(doc_ent(N, C, _)),
        C1 is C + 1,
        assertz(doc_ent(N, C1, 0))
    ; assertz(doc_ent(N, 1, 0))
    ),
    ( N == narrator -> true
    ; retractall(doc_last_ent(_)),
      assertz(doc_last_ent(N))
    ).

% Memoria real (remember_tracked = peso + prov + conflicto).
mem_all(FR, Alias, NTri) :-
    mem_loop(FR, Alias, 0, NTri).

mem_loop([], _, N, N).
mem_loop([(Ref, Frames, _)|R], Alias, N0, N) :-
    emit_frames(Frames, Ref, 1, K),
    N1 is N0 + K,
    mem_loop(R, Alias, N1, N).

emit_frames([], _, _, 0).
emit_frames([frame(S, V, O, PPs)|Fs], Ref, Idx, N) :-
    format(atom(E), '~w_e~w', [Ref, Idx]),
    ( frame_ok(S, O) ->
        doc_arg(S, SA),
        doc_arg(O, OA),
        remember_tracked(SA, V, OA, Ref, none),
        doc_inc(triples),
        remember_tracked(SA, actor, E, Ref, none),
        doc_inc(triples),
        K0 = 2
    ; K0 = 0
    ),
    emit_pps(PPs, E, Ref, K1),
    Idx1 is Idx + 1,
    emit_frames(Fs, Ref, Idx1, K2),
    N is K0 + K1 + K2.

frame_ok(S, O) :-
    S \== unknown, O \== unknown,
    (S = entity(_) ; S = np(_)),
    (O = entity(_) ; O = np(_)).

emit_pps([], _, _, 0).
emit_pps([P-NPV|Ps], E, Ref, N) :-
    ( NPV == unknown -> K = 0
    ; doc_arg(NPV, NA),
      ( gen_role(P, Role) -> R = Role
      ; R = P
      ),
      remember_tracked(E, R, NA, Ref, none),
      doc_inc(triples),
      K = 1
    ),
    emit_pps(Ps, E, Ref, K2),
    N is K + K2.

length_flat(FR, N) :-
    findall(K, (member((_, Frames, _), FR), length(Frames, K)), Ks),
    bench_sum(Ks, N).

bench_sum([], 0).
bench_sum([H|T], S) :- bench_sum(T, S0), S is S0 + H.

% Micro A/B: mismo triples con assert pelado (sin prov/conflicto/peso).
bare_all(FR) :-
    bare_loop(FR).

bare_loop([]).
bare_loop([(_, Frames, _)|R]) :-
    bare_frames(Frames),
    bare_loop(R).

bare_frames([]).
bare_frames([frame(S, V, O, PPs)|Fs]) :-
    ( frame_ok(S, O) ->
        doc_arg(S, SA),
        doc_arg(O, OA),
        assertz(bench_fact(SA, V, OA)),
        bare_pps(PPs)
    ; true
    ),
    bare_frames(Fs).

bare_pps([]).
bare_pps([_-NPV|Ps]) :-
    ( NPV == unknown -> true
    ; doc_arg(NPV, _)
    ),
    bare_pps(Ps).
