% doc_corpus.pl
% Lector DOCUMENTAL autonomo (EXP56): libro crudo -> memoria + BOOK REPORT.
% Pasada 1: estructura (capitulos/parrafos/frases) + censo de mayusculas.
% Pasada 2: gen_parse (cero lexicon de contenido) + registro de entidades
%   + eventos + proveniencia doc/cap/par/fra. Sin ayuda humana por libro.
% Requiere: corpus.pl (memoria/conflicto) y gen_parse.pl (runner los carga).
% EXP56: via deferred.pl los inserts pueden ir en diferido (dx_track).
:- use_module(library(lists)).
:- consult('deferred.pl').

:- dynamic doc_ent/3.      % doc_ent(NormName, Mentions, LastSent)
:- dynamic doc_last_ent/1. % ultima entidad 3a persona
:- dynamic doc_census/1.   % doc_census([nombres]) pasado-1
:- dynamic doc_count/2.    % doc_count(Key, N)

doc_reset :-
    retractall(doc_ent(_, _, _)),
    retractall(doc_last_ent(_)),
    retractall(doc_census(_)),
    retractall(doc_count(_, _)),
    forall(member(K, [chapters, paragraphs, sentences, mentions, coref,
                      events, frames, triples, unknown_roles, unknown_args,
                      noverb_sents, skipped_frames, unknown_prons]),
           assertz(doc_count(K, 0))).

doc_inc(K) :-
    retract(doc_count(K, N)), !,
    N1 is N + 1,
    assertz(doc_count(K, N1)).

doc_add(K, N) :-
    retract(doc_count(K, X)), !,
    X1 is X + N,
    assertz(doc_count(K, X1)).

% ---------- pasada 1: estructura + censo ----------
% doc_scan(+File, +Alias, -Chapters): Chapters = [ch(Num, Paras)].
doc_scan(File, Alias, Chapters) :-
    open(File, read, S, [encoding(utf8)]),
    read_line_to_string(S, L0),
    doc_skip_preamble(S, L0, LCur),
    doc_read_paras(S, LCur, Paras),
    close(S),
    doc_merge_ch_markers(Paras, Paras2),
    doc_chapters(Paras2, 1, Chapters),
    doc_census_build(Chapters, Census),
    retractall(doc_census(_)),
    assertz(doc_census(Census)),
    length(Census, NCen),
    length(Chapters, NCh),
    format('DOC-SCAN ~w chapters=~w census=~w~n', [Alias, NCh, NCen]).

doc_skip_preamble(S, Line, LCur) :-
    doc_skip_preamble(S, Line, "", LCur).

doc_skip_preamble(S, Line, Fallback, LCur) :-
    ( Line == end_of_file ->
        ( Fallback == "" -> LCur = end_of_file ; LCur = Fallback )
    ; sub_string(Line, _, _, _, "START OF THE PROJECT") ->
        doc_skip_blanks(S, LCur)
    ; sub_string(Line, _, _, _, "END OF THE PROJECT") ->
        LCur = end_of_file
    ; ( Line \== "", Fallback == "" -> FB = Line ; FB = Fallback ),
      read_line_to_string(S, Next),
      doc_skip_preamble(S, Next, FB, LCur)
    ).

doc_skip_blanks(S, LCur) :-
    read_line_to_string(S, Line),
    ( Line == end_of_file -> LCur = end_of_file
    ; Line == "" -> doc_skip_blanks(S, LCur)
    ; LCur = Line
    ).

doc_read_paras(_, end_of_file, []) :- !.
doc_read_paras(S, Line, Paras) :-
    ( Line == "" ->
        read_line_to_string(S, Next),
        doc_read_paras(S, Next, Paras)
    ; sub_string(Line, _, _, _, "END OF THE PROJECT") ->
        Paras = []
    ; doc_is_chapter(Line) ->
        read_line_to_string(S, Next),
        doc_read_paras(S, Next, Rest),
        Paras = [Line|Rest]
    ; doc_para_block(S, Line, Text, Next),
      doc_read_paras(S, Next, Rest),
      Paras = [Text|Rest]
    ).

% Bloque de parrafo: devuelve texto + primera linea no consumida.
doc_para_block(S, Line, Text, Next) :-
    doc_block_lines(S, Line, Lines, Next),
    atomic_list_concat(Lines, ' ', Atom),
    atom_string(Atom, Text).

doc_block_lines(_, end_of_file, [], end_of_file) :- !.
doc_block_lines(S, Line, [], Line) :-
    ( Line == "" -> true
    ; sub_string(Line, _, _, _, "END OF THE PROJECT") -> true
    ; doc_is_chapter(Line) -> true
    ), !.
doc_block_lines(S, Line, [Line|Rest], Next) :-
    read_line_to_string(S, N2),
    doc_block_lines(S, N2, Rest, Next).

doc_is_chapter(Line) :-
    string_lower(Line, Low),
    split_string(Low, " ", " ", [First|_]),
    First == "chapter".

% Fusiona rachas de marcadores de capitulo consecutivos en uno solo
% (algunas fuentes repiten cabecera: portadilla + cabecera corrida).
% Sin perdida: se unen los textos.
doc_merge_ch_markers([], []).
doc_merge_ch_markers([P|Ps], Out) :-
    doc_is_chapter(P), !,
    doc_ch_run(Ps, Rest, [P], Run),
    atomic_list_concat(Run, ' / ', Merged),
    atom_string(Merged, MStr),
    Out = [MStr|More],
    doc_merge_ch_markers(Rest, More).
doc_merge_ch_markers([P|Ps], [P|More]) :-
    doc_merge_ch_markers(Ps, More).

doc_ch_run([], [], Acc, Out) :-
    reverse(Acc, Out).
doc_ch_run([P|Ps], Rest, Acc, Out) :-
    doc_is_chapter(P), !,
    doc_ch_run(Ps, Rest, [P|Acc], Out).
doc_ch_run(Rest, Rest, Acc, Out) :-
    reverse(Acc, Out).

% Agrupa parrafos en capitulos (el marcador es un parrafo propio).
doc_chapters([], _, []).
doc_chapters([P|Ps], N, Chapters) :-
    ( doc_is_chapter(P) ->
        N1 is N + 1,
        doc_collect_ch(Ps, Mine, Rest),
        Chapters = [ch(N1, [P|Mine])|More],
        doc_chapters(Rest, N1, More)
    ; doc_collect_ch([P|Ps], Mine, Rest),
      Chapters = [ch(N, Mine)|More],
      doc_chapters(Rest, N, More)
    ).

doc_collect_ch([], [], []).
doc_collect_ch([P|Ps], [], [P|Ps]) :-
    doc_is_chapter(P), !.
doc_collect_ch([P|Ps], [P|Mine], Rest) :-
    doc_collect_ch(Ps, Mine, Rest).

% Censo: tokens con mayuscula en posicion no-inicial (lowercase, set).
doc_census_build(Chapters, Census) :-
    findall(W, (member(ch(_, Paras), Chapters),
                member(Para, Paras),
                doc_sentences(Para, Sents),
                member(Sn, Sents),
                gen_tokenize(Sn, Lower, Raw),
                nth0(I, Raw, R), I > 0,
                atom_chars(R, [C|_]), char_type(C, upper),
                nth0(I, Lower, W),
                \+ gen_closed(W)),
            Ws),
    sort(Ws, Census).

doc_sentences(Para, Sents) :-
    split_string(Para, ".!?;:,\u2014\u2013", " ", Parts),
    exclude(doc_empty, Parts, Sents).

doc_empty("").

% ---------- pasada 2: parse + memoria ----------
doc_ingest(Alias, Chapters) :-
    doc_census(Census),
    ingest_chapters(Alias, Chapters, Census, 1, 0, _).

ingest_chapters(_, [], _, _, S, S).
ingest_chapters(Alias, [ch(CN, Paras)|Cs], Census, CN0, S0, S) :-
    doc_inc(chapters),
    ingest_paras(Alias, CN, Paras, Census, S0, S1),
    ingest_chapters(Alias, Cs, Census, CN0, S1, S).

ingest_paras(_, _, [], _, S, S).
ingest_paras(Alias, CN, [P|Ps], Census, S0, S) :-
    doc_inc(paragraphs),
    doc_sentences(P, Sents),
    ingest_sents(Alias, CN, Sents, Census, S0, S1),
    ingest_paras(Alias, CN, Ps, Census, S1, S).

ingest_sents(_, _, [], _, S, S).
ingest_sents(Alias, CN, [Sn|Sns], Census, S0, S) :-
    S1 is S0 + 1,
    doc_inc(sentences),
    ( doc_last_ent(L) -> LE = L ; LE = none ),
    gen_tokenize(Sn, Lower, Raw),
    gen_frames(Lower, Raw, Census, LE, Frames, st(NV, NSk, NCf, NUn),
               Mentions),
    doc_add(coref, NCf),
    doc_add(unknown_prons, NUn),
    ( NV == 0 ->
        doc_inc(noverb_sents),
        doc_unknown_words(Lower, Raw)
    ; doc_add(skipped_frames, NSk)
    ),
    format(atom(Ref), '~w_ch~w_s~w', [Alias, CN, S1]),
    ingest_frames(Frames, Mentions, Ref, S1),
    ingest_sents(Alias, CN, Sns, Census, S1, S).

% Palabras de contenido en frases sin verbo (medida de cobertura perdida).
doc_unknown_words(Lower, Raw) :-
    findall(I, (nth0(I, Lower, W),
                \+ gen_closed(W),
                nth0(I, Raw, R),
                \+ gen_pronoun_lo(W)), Is),
    length(Is, N),
    doc_add(unknown_args, N).

gen_pronoun_lo(W) :-
    gen_pronoun(W), !.
gen_pronoun_lo(_) :- fail.

ingest_frames([], Mentions, _, S1) :-
    doc_register_mentions(Mentions, S1).
ingest_frames([frame(S, V, O, PPs)|Fs], Mentions, Ref, S1) :-
    ( frame_args_ok(S, O) ->
        doc_inc(frames),
        doc_inc(events),
        format(atom(E), '~w_e~w', [Ref, S1]),
        doc_emit(S, V, O, PPs, E, Ref),
        doc_register_mentions(Mentions, S1)
    ; doc_inc(unknown_args)
    ),
    ingest_frames(Fs, Mentions, Ref, S1).

frame_args_ok(S, O) :-
    S \== unknown, O \== unknown,
    (S = entity(_) ; S = np(_)),
    (O = entity(_) ; O = np(_)).

doc_emit(S, V, O, PPs, E, Ref) :-
    doc_arg(S, SA),
    doc_arg(O, OA),
    doc_remember((SA, V, OA), Ref),
    doc_remember((SA, actor, E), Ref),
    doc_remember((E, action, V), Ref),
    doc_remember((E, object, OA), Ref),
    forall(member(P-NPV, PPs), doc_pp(P, NPV, E, Ref)).

doc_arg(entity(N), N) :- !.
doc_arg(np(H), H).

doc_remember((S, R, O), Ref) :-
    dx_track(S, R, O, Ref, none),
    doc_inc(triples).

doc_pp(P, NPV, E, Ref) :-
    ( NPV == unknown -> doc_inc(unknown_args)
    ; doc_arg(NPV, NA),
      ( gen_role(P, Role) ->
          doc_remember((E, Role, NA), Ref)
      ; P == of ->
          doc_remember((E, of, NA), Ref)
      ; doc_remember((E, P, NA), Ref),
        doc_inc(unknown_roles)
      )
    ).

% Registro: menciones en orden; narrator no toca last_ent.
doc_register_mentions([], _).
doc_register_mentions([N|Ns], S1) :-
    doc_inc(mentions),
    ( doc_ent(N, C, _) ->
        retract(doc_ent(N, C, _)),
        C1 is C + 1,
        assertz(doc_ent(N, C1, S1))
    ; assertz(doc_ent(N, 1, S1))
    ),
    ( N == narrator -> true
    ; retractall(doc_last_ent(_)),
      assertz(doc_last_ent(N))
    ),
    doc_register_mentions(Ns, S1).

% ---------- BOOK REPORT ----------
doc_report(Alias) :-
    findall(K-N, doc_count(K, N), Rows),
    sort(Rows, SR),
    findall(1, doc_ent(_, _, _), Es),
    length(Es, NE),
    format('BOOK-REPORT ~w~n', [Alias]),
    forall(member(K-N, SR), format('  ~w: ~w~n', [K, N])),
    format('  entities: ~w~n', [NE]),
    format('  top_entities:~n', []),
    findall(C-Nm, doc_ent(Nm, C, _), EC),
    keysort(EC, SE),
    reverse(SE, RE),
    show_ents(RE, 15).

show_ents(_, 0) :- !.
show_ents([], _) :- !.
show_ents([C-N|T], K) :-
    format('    ~w x~w~n', [N, C]),
    K1 is K - 1,
    show_ents(T, K1).
