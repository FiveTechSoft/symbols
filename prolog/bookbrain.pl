% bookbrain/bookbrain.pl — Producto: documento -> document.knowledge.
% Uso desde prolog/: swipl -s bookbrain.pl -g "bookbrain('ruta/libro.txt', alias)" -t halt
% Pipeline directo sobre corpus.pl: clear -> learn_book -> dump.
% (El pipeline diferido doc_scan/doc_ingest/dx_* no existe en este arbol;
% las refs de provenance son sentence_N. Mismo formato memfact/5 +
% provfact/6 que cargan chat.pl y ask.pl. Motor intacto.)
% CONTRATO DE ENTRADA (medido 2026-09-12, P2): una frase corta y canonica
% SVO por linea. Texto literario crudo (dialogo, subordinacion, citas)
% viola el limite documentado de positional.pl y da precision ~2%: la
% salida jamas se usa sin pasar la puerta de 50 (roadmap sec. 5).
:- consult('corpus.pl').
:- consult('positional.pl').

:- use_module(library(lists)).

bookbrain(File, Alias) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    get_time(T0),
    learn_book(File),
    get_time(T1),
    Ms is round((T1 - T0) * 1000),
    atom_concat(Alias, '.knowledge.pl', Out),
    open(Out, write, S),
    forall(memory_relation(A, R, O, W, U),
           format(S, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
    forall(prov(A, R, O, info(Ref, T, St)),
           format(S, 'provfact(~q,~q,~q,~q,~q,~q).~n', [A, R, O, Ref, T, St])),
    close(S),
    memory_size(NF),
    format('BOOKBRAIN ~w facts=~w ms=~w -> ~w~n',
           [Alias, NF, Ms, Out]).

% learn_book: por linea, open_vocab y si rechaza, SVO estructural
% (misma doctrina que chat_learn en chat.pl: '.' declarativo + veto
% qlead a interrogativas; sin listas de contenido).
learn_book(File) :-
    open(File, read, S, [encoding(utf8)]),
    book_lines_loop(S),
    close(S).

book_lines_loop(S) :-
    read_line_to_string(S, Line),
    ( Line == end_of_file -> true
    ; ( Line == "" -> true
      ; ( learn_sentence(Line, Stored), Stored \= rejected(_) -> true
        ; book_positional(Line) -> true
        ; format('REJECTED: ~w~n', [Line])
        )
      ),
      book_lines_loop(S)
    ).

book_positional(Line) :-
    ( string(Line) -> S = Line ; atom_string(Line, S) ),
    sub_string(S, _, _, 0, "."),
    tokenize_pos(S, Toks),
    Toks = [F|_],
    \+ qlead(F),
    parse_svo(Toks, (A, V, O)),
    next_sentence_id(Src),
    remember_tracked(A, V, O, Src, none).

% Duplicado de chat.pl (punto de entrada independiente): lista cerrada
% de auxiliares/interrogativos = sintaxis. Mantener sincronizado.
qlead(W) :- member(W, [who,what,when,where,why,how,
                       did,does,do,is,are,was,were]), !.
