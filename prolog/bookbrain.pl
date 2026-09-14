% bookbrain/bookbrain.pl — Producto: documento -> document.knowledge.
% Uso: swipl -s bookbrain.pl -g "bookbrain('ruta/libro.txt', alias)" -t halt
% Pipeline: clear -> preprocess -> learn_book -> dump.
% P2: preprocess Canonización — conjunciones, simplificación, glucosa.
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

% learn_book: por linea, con preprocesamiento P2.
learn_book(File) :-
    open(File, read, S, [encoding(utf8)]),
    book_lines_loop(S),
    close(S).

book_lines_loop(S) :-
    read_line_to_string(S, Line),
    ( Line == end_of_file -> true
    ; ( Line == "" -> true
      ; process_line(Line)
      ),
      book_lines_loop(S)
    ).

% ── Procesamiento por línea (P2) ────────────────────────────────────

process_line(Line) :-
    % 1. Preprocesar: simplificar oración
    preprocess_sentence(Line, Simple),
    % 2. Intentar learn_sentence (open_vocab)
    ( learn_sentence(Simple, Stored), Stored \= rejected(_) -> true
    % 3. Intentar SVO con glue stripping
    ; symbolize_text(Simple, Triple), Triple = (A, V, O) ->
        next_sentence_id(Src),
        remember_tracked(A, V, O, Src, none)
    % 4. Intentar división de conjunciones
    ; try_conjunction_split(Simple)
    % 5. Rechazar
    ; true  % silencioso: no mostrar REJECTED para texto literario
    ).

% ── Preprocesamiento de oraciones ────────────────────────────────────

% preprocess_sentence(+In, -Out) — simplifica texto literario
preprocess_sentence(In, Out) :-
    % Quitar comillas y diálogos
    strip_dialog(In, NoDialog),
    % Quitar paréntesis y corchetes
    strip_brackets(NoDialog, NoBrackets),
    % Normalizar espacios
    normalize_spaces(NoBrackets, Out).

% strip_dialog(+In, -Out) — quita texto entre comillas y diálogos
strip_dialog(In, Out) :-
    % "..." → quitar
    sub_string(In, Before, _, After, '"'),
    Before >= 0,
    !,
    sub_string(In, 0, Before, _, BeforeStr),
    AfterStart is Before + 1,
    sub_string(In, AfterStart, _, 0, AfterStr),
    strip_dialog(BeforeStr, B),
    strip_dialog(AfterStr, A),
    atomic_list_concat([B, A], ' ', Out).
strip_dialog(In, In).

% strip_brackets(+In, -Out)
strip_brackets(In, Out) :-
    sub_string(In, Before, _, After, '('),
    Before >= 0,
    !,
    sub_string(In, 0, Before, _, BeforeStr),
    AfterStart is Before + 1,
    sub_string(In, AfterStart, _, 0, AfterStr),
    strip_brackets(BeforeStr, B),
    strip_brackets(AfterStr, A),
    atomic_list_concat([B, A], ' ', Out).
strip_brackets(In, In).

% normalize_spaces(+In, -Out)
normalize_spaces(In, Out) :-
    string_lower(In, Lower),
    % Colapsar espacios múltiples
    re_replace(' +'/g, Lower, ' ', Out).

% ── División de conjunciones ─────────────────────────────────────────

% try_conjunction_split(+Line) — divide "X and Y" y procesa cada parte
try_conjunction_split(Line) :-
    split_at_and(Line, Left, Right),
    Left \== "", Right \== "",
    !,
    process_line(Left),
    process_line(Right).
try_conjunction_split(_).  % sin conjunción, no hacer nada

% split_at_and(+Str, -Left, -Right)
split_at_and(Str, Left, Right) :-
    sub_string(Str, LeftLen, _, RightLen, " and "),
    LeftLen > 0, RightLen > 0,
    sub_string(Str, 0, LeftLen, _, Left),
    RightStart is LeftLen + 5,
    sub_string(Str, RightStart, _, 0, Right).

% ── qlead (duplicado de chat.pl, punto de entrada independiente) ────
qlead(W) :- member(W, [who,what,when,where,why,how,
                       did,does,do,is,are,was,were]), !.
