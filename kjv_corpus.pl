% kjv_corpus.pl
% Ingesta AUTOMATIZADA de texto crudo: lee corpus_biblia/kjv.txt TAL CUAL
% (wrap Gutenberg, marcadores N:M en cualquier posicion, cabeceras,
% preambulo). Sin modificar el corpus: toda la logica vive aqui.
% Requiere (orden del runner): corpus.pl, kjv_parse.pl, kjv_corpus.pl.
%   kjv_load_limit(Max): ingiere hasta Max versiculos (0 = todo el archivo).
%   Contadores: books, verses, sents, stored, rejects. Verificacion: al
%   final verses debe ser 31102 (conteo KJV auditado por dos metodos).
% EXP56: via deferred.pl (consultado abajo) los inserts pueden ir en
% diferido (dx_deferred_on) o en modo collect (kjv_collecting: extrae y
% aparca en kjv_staged/4 sin tocar memoria).
:- use_module(library(lists)).
:- consult('deferred.pl').

:- dynamic kjv_book/2.    % kjv_book(BI, Title)
:- dynamic kjv_count/2.   % kjv_count(Key, N) globales: books, verses, sents, stored, rejects
:- dynamic kjv_bcount/2.  % kjv_bcount(Key, N) del libro en curso: bverses, bsents, bstored, brejects
:- dynamic kjv_bstart/1.  % kjv_bstart(Ms) instante en que empezo el libro en curso
% EXP55: modo de simbolizacion conmutable. single = una tripleta (EXP54,
% por defecto y protegido); event = frames multi-verbo + roles (ev_parse).
:- dynamic kjv_mode/1.

set_kjv_mode(M) :-
    retractall(kjv_mode(_)),
    assertz(kjv_mode(M)).

kjv_cur_mode(M) :- kjv_mode(M), !.
kjv_cur_mode(single).

:- dynamic kjv_collecting/0.
:- dynamic kjv_staged/4.   % kjv_staged(S, R, O, Ref) extraido, sin ingerir

kjv_collect_on :-
    retractall(kjv_collecting),
    retractall(kjv_staged(_, _, _, _)),
    assertz(kjv_collecting).
kjv_collect_off :-
    retractall(kjv_collecting).

kjv_reset :-
    retractall(kjv_book(_, _)),
    retractall(kjv_count(_, _)),
    retractall(kjv_bcount(_, _)),
    retractall(kjv_bstart(_)),
    forall(member(K, [books, verses, sents, stored, rejects,
                      frames, skipped_frames, dropped_verbs]),
           assertz(kjv_count(K, 0))),
    kjv_book_locals_reset,
    reset_pronouns.

kjv_book_locals_reset :-
    retractall(kjv_bcount(_, _)),
    forall(member(K, [bverses, bsents, bstored, brejects,
                      bframes, bskipped_frames, bdropped_verbs]),
           assertz(kjv_bcount(K, 0))),
    statistics(runtime, [Now, _]),
    assertz(kjv_bstart(Now)).

kjv_inc(K) :-
    retract(kjv_count(K, N)), !,
    N1 is N + 1,
    assertz(kjv_count(K, N1)).

kjv_binc(K) :-
    retract(kjv_bcount(K, N)), !,
    N1 is N + 1,
    assertz(kjv_bcount(K, N1)).

% ---------- entrada ----------
kjv_load_limit(Max) :-
    open('corpus_biblia/kjv.txt', read, S, [encoding(utf8)]),
    read_line_to_string(S, L0),
    kjv_skip_preamble(S, L0, "", FirstBook, LCur),
    kjv_reset,
    kjv_new_book(FirstBook),
    kjv_lines(S, LCur, 1, noverse, [], Max),
    close(S),
    kjv_count(verses, NV),
    format('kjv verses ingested: ~w~n', [NV]).

% Preambulo: se salta hasta la primera linea con marcador; la ultima linea
% no vacia anterior es la cabecera del libro 1 (Genesis). Devuelve tambien
% la linea actual (ya es cuerpo) para no perderla.
kjv_skip_preamble(S, Line, Last, FirstBook, LCur) :-
    ( Line == end_of_file ->
        LCur = end_of_file,
        ( Last == "" -> FirstBook = 'unknown_book_1'
        ; kjv_clean_header_line(Last, FirstBook) )
    ; kjv_line_tokens(Line, Toks),
      kjv_split_markers(Toks, _, Segs),
      Segs \== [] ->
        LCur = Line,
        ( Last == "" -> FirstBook = 'unknown_book_1'
        ; kjv_clean_header_line(Last, FirstBook) )
    ; ( kjv_line_tokens(Line, T) , T == [] -> Last2 = Last ; Last2 = Line ),
      read_line_to_string(S, Next),
      kjv_skip_preamble(S, Next, Last2, FirstBook, LCur)
    ).

% Bucle principal: BI = indice libro, Open = noverse | verse(C,V,Toks),
% Orphans = tokens huerfanos (cabeceras o titulos internos).
kjv_lines(_, end_of_file, BI, Open, _, _) :- !,
    kjv_close_verse(Open, BI),
    kjv_report_book(BI).
kjv_lines(S, Line, BI, Open, Orphans, Max) :-
    kjv_count(verses, N),
    ( Max > 0, N >= Max ->
        kjv_close_verse(Open, BI),
        kjv_report_book(BI)
    ; kjv_proc_line(Line, BI, Open, Orphans, BI2, Open2, Orphans2),
      read_line_to_string(S, Next),
      kjv_lines(S, Next, BI2, Open2, Orphans2, Max)
    ).

kjv_proc_line(Line, BI, Open, Orphans, BI2, Open2, Orphans2) :-
    kjv_line_tokens(Line, Toks),
    ( Toks == [] ->
        % blanco: cierra el versiculo; las huerfanas sobreviven (cabeceras
        % multilinea como Eclesiastes: 'Ecclesiastes'/'or'/'The Preacher').
        kjv_close_verse(Open, BI),
        Open2 = noverse, BI2 = BI, Orphans2 = Orphans
    ; kjv_split_markers(Toks, Head, Segs),
      ( Segs == [] ->
          ( Open = verse(C, V, T0) ->
              append(T0, Head, T1),
              Open2 = verse(C, V, T1),
              BI2 = BI, Orphans2 = Orphans
          ; append(Orphans, Head, Orphans2),
            Open2 = noverse, BI2 = BI
          )
      ; % la Head cierra/va al versiculo abierto (o a huerfanas)
        ( Open = verse(C0, V0, T0) ->
            append(T0, Head, T1),
            kjv_close_verse(verse(C0, V0, T1), BI)
        ; append(Orphans, Head, OrphansMid),
          Orphans2a = OrphansMid
        ),
        ( Open = verse(_, _, _) -> Orphans2a = Orphans ; true ),
        kjv_fold_segs(Segs, BI, Orphans2a, BI2, Open2, Orphans2)
      )
    ).

% Pliega los segmentos con marcador de una linea: cada uno cierra el
% anterior y abre uno nuevo. Un 1:1 marca libro nuevo (cabecera = huerfanas
% limpiadas); si no, las huerfanas (titulos de salmo) se anteponen al texto.
kjv_fold_segs([], BI, Orphans, BI, noverse, Orphans) :- !.
kjv_fold_segs([(C, V, T)|Segs], BI, Orphans, BIOut, OpenOut, OrphansOut) :-
    ( C == 1, V == 1 ->
        kjv_clean_header_tokens(Orphans, Title),
        ( Title == '' -> BI2 = BI ; kjv_new_book(Title), kjv_count(books, BI2) ),
        Open2 = verse(C, V, T),
        Orphans2 = []
    ; append(Orphans, T, T2),
      Open2 = verse(C, V, T2),
      BI2 = BI,
      Orphans2 = []
    ),
    ( Segs == [] ->
        BIOut = BI2, OpenOut = Open2, OrphansOut = Orphans2
    ; kjv_close_verse(Open2, BI2),
      kjv_fold_rest(Segs, BI2, BIOut, OpenOut, OrphansOut)
    ).

% Segmentos 2..n de la misma linea: el anterior ya se cerro; ninguno puede
% ser 1:1 salvo error del texto (se acepta como versiculo normal).
kjv_fold_rest([(C, V, T)], BI, BI, verse(C, V, T), []) :- !.
kjv_fold_rest([(C, V, T)|Segs], BI, BIOut, OpenOut, OrphansOut) :-
    kjv_close_verse(verse(C, V, T), BI),
    kjv_fold_rest(Segs, BI, BIOut, OpenOut, OrphansOut).

kjv_close_verse(noverse, _) :- !.
kjv_close_verse(verse(C, V, Toks), BI) :-
    kjv_ingest_verse(BI, C, V, Toks).

kjv_ingest_verse(BI, C, V, Toks) :-
    kjv_inc(verses),
    kjv_binc(bverses),
    format(atom(Ref), 'b~w_~w_~w', [BI, C, V]),
    atomic_list_concat(Toks, ' ', TxtAtom),
    atom_string(TxtAtom, Txt),
    split_string(Txt, ".!?;:", " ", Sents0),
    exclude(kjv_empty_str, Sents0, Sents),
    forall(member(Sn, Sents), kjv_ingest_sent(Sn, Ref)).

kjv_ingest_sent(Sn, Ref) :-
    kjv_inc(sents),
    kjv_binc(bsents),
    kjv_cur_mode(Mode),
    ( Mode == event ->
        ev_symbolize(Sn, Ref, Triples, NF, NSk, ND),
        kjv_addn(frames, bframes, NF),
        kjv_addn(skipped_frames, bskipped_frames, NSk),
        kjv_addn(dropped_verbs, bdropped_verbs, ND)
    ; ( symbolize_kjv(Sn, (S, R, O)) ->
            Triples = [(S, R, O)]
      ; Triples = []
      )
    ),
    ( Triples == [] ->
        kjv_inc(rejects),
        kjv_binc(brejects)
    ; forall(member((S, R, O), Triples),
             kjv_put(S, R, O, Ref))
    ).

% kjv_put: aparca (collect) o inserta (dx_track = eager/diferido).
kjv_put(S, R, O, Ref) :-
    ( kjv_collecting ->
        assertz(kjv_staged(S, R, O, Ref))
    ; dx_track(S, R, O, Ref, none)
    ),
    kjv_inc(stored),
    kjv_binc(bstored).

% Suma N a un contador global y su espejo de libro.
kjv_addn(_, _, 0) :- !.
kjv_addn(G, B, N) :-
    retract(kjv_count(G, X)), X1 is X + N, assertz(kjv_count(G, X1)),
    retract(kjv_bcount(B, Y)), Y1 is Y + N, assertz(kjv_bcount(B, Y1)).

kjv_empty_str("").

% ---------- libros ----------
% Al abrir un libro nuevo se publica la metrica del anterior: tiempo y
% porcentaje de exito (stored/sents). Asi se mide libro por libro.
kjv_new_book(Title) :-
    kjv_count(books, Prev),
    ( Prev > 0 -> kjv_report_book(Prev) ; true ),
    N1 is Prev + 1,
    retract(kjv_count(books, Prev)),
    assertz(kjv_count(books, N1)),
    assertz(kjv_book(N1, Title)),
    kjv_book_locals_reset.

kjv_report_book(BI) :-
    kjv_book(BI, Title),
    kjv_bcount(bverses, NV),
    kjv_bcount(bsents, NS),
    kjv_bcount(bstored, NSt),
    kjv_bcount(brejects, NR),
    kjv_bcount(bframes, NF),
    kjv_bcount(bskipped_frames, NSk),
    kjv_bcount(bdropped_verbs, ND),
    kjv_bstart(T0),
    statistics(runtime, [Now, _]),
    Ms is Now - T0,
    ( NS > 0 -> Y is 100 * NSt / NS ; Y = 0 ),
    format('BOOK ~w | verses=~w sents=~w stored=~w rejects=~w yield=~1f% frames=~w skipfr=~w dropv=~w ms=~w | ~w~n',
           [BI, NV, NS, NSt, NR, Y, NF, NSk, ND, Ms, Title]).

kjv_list_books :-
    forall(kjv_book(BI, T), format('book ~w: ~w~n', [BI, T])).

% ---------- tokens y marcadores ----------
kjv_line_tokens(Line, Toks) :-
    split_string(Line, " ", " \t\r", Strs),
    exclude(kjv_empty_str, Strs, NonEmpty),
    maplist(kjv_to_atom, NonEmpty, Toks).

kjv_to_atom(Str, Atom) :- atom_string(Atom, Str).

% Parte una linea en Head (texto antes del primer marcador) y segmentos
% (C,V,Texto). Determinista.
kjv_split_markers(Toks, Head, Segs) :-
    kjv_take_until_marker(Toks, Head, Rest),
    kjv_take_segs(Rest, Segs).

kjv_take_until_marker([], [], []).
kjv_take_until_marker([T|Ts], [], [T|Ts]) :-
    kjv_is_marker(T, _, _), !.
kjv_take_until_marker([T|Ts], [T|H], Rest) :-
    kjv_take_until_marker(Ts, H, Rest).

kjv_take_segs([], []) :- !.
kjv_take_segs([M|Ts], [(C, V, Txt)|Segs]) :-
    kjv_is_marker(M, C, V), !,
    kjv_take_until_marker(Ts, Txt, Rest),
    kjv_take_segs(Rest, Segs).
kjv_take_segs([_|Ts], Segs) :-
    kjv_take_segs(Ts, Segs).

% Marcador N:M con digitos estrictos (sin depender de pcre).
kjv_is_marker(Token, Ch, V) :-
    atom_chars(Token, Cs),
    append(DC, [':'|VC], Cs),
    DC = [_|_], VC = [_|_],
    kjv_all_digits(DC),
    kjv_all_digits(VC), !,
    number_chars(Ch, DC),
    number_chars(V, VC).

kjv_all_digits([]).
kjv_all_digits([C|Cs]) :-
    char_code(C, N), N >= 48, N =< 57,
    kjv_all_digits(Cs).

% ---------- limpieza de cabeceras (tokens) ----------
kjv_clean_header_line(Line, Title) :-
    kjv_line_tokens(Line, Toks),
    kjv_clean_header_tokens(Toks, Title).

kjv_clean_header_tokens(Toks, Title) :-
    kjv_drop_stars(Toks, T1),
    kjv_drop_amen(T1, T2),
    kjv_drop_testament(T2, T3),
    ( T3 == [] -> Title = '' ; atomic_list_concat(T3, ' ', Title) ).

kjv_drop_stars([T|Ts], Out) :-
    kjv_all_stars(T), !,
    kjv_drop_stars(Ts, Out).
kjv_drop_stars(L, L).

kjv_all_stars(T) :-
    atom_chars(T, Cs), Cs \== [],
    forall(member(C, Cs), C == '*').

kjv_drop_amen([T|Ts], Out) :-
    downcase_atom(T, D),
    ( D == amen ; D == 'amen.' ), !,
    kjv_drop_amen(Ts, Out).
kjv_drop_amen(L, L).

kjv_drop_testament(Toks, Rest) :-
    maplist(downcase_atom, Toks, Low),
    ( append([the, old, testament, of, the, king, james, version, of, the, bible], R, Low),
      R \== [] ->
        kjv_take_suffix(Toks, R, Rest)
    ; append([the, new, testament, of, the, king, james, bible], R, Low),
      R \== [] ->
        kjv_take_suffix(Toks, R, Rest)
    ; Rest = Toks
    ).

kjv_take_suffix(Toks, R, Rest) :-
    length(R, N), length(Toks, L), Drop is L - N,
    length(Pre, Drop), append(Pre, Rest, Toks).
