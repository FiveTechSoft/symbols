% bookbrain/chat.pl — chat libre sobre document.knowledge.
% Uso: swipl -s chat.pl -g "chat('alice_clean.knowledge.pl')" -t halt
% Varios libros: chat(['alice_clean.knowledge.pl','plataforma_clean.knowledge.pl'])
% save/load: el .knowledge.pl ya es el mapa procesado; load suma, save vuelca.
% (o entubando preguntas por stdin). Formas: who/what/did/why/where/when,
% "why?" solo (sigue a la ultima respuesta), "help", "quit".
% Estado de dialogo: last_fact/3. Ante lo desconocido: UNKNOWN (nunca inventa).
% Multi-respuesta capada a 5 + resto contado. Sin caidas: todo catch.
:- consult('corpus.pl').
:- consult('question_parser.pl').
:- consult('english_graph.pl').
:- consult('positional.pl').
:- consult('dialog_ref.pl').
:- consult('gaps.pl').

% Interfaz de datos: la aporta el .knowledge.pl que bb_load/1 consulta en
% runtime (memfact/5 hechos, provfact/6 provenance). Declarada, no definida.
:- dynamic memfact/5.
:- dynamic provfact/6.

% Shim del dialecto bookbrain (gen_parse.pl no existe en este arbol):
% delega en el tokenizador del motor + pliega tildes a ASCII
% (diacriticos cerrados, como FoldAccents del harness C: la eñe se
% conserva como letra). Infraestructura, no lexico.
gen_tokenize(S, Toks, _) :-
    ( string(S) -> S2 = S ; atom_string(S, S2) ),
    tokenize_en(S2, T0),
    maplist(fold_accents, T0, Toks).

fold_accents(A, F) :-
    atom_chars(A, Cs),
    maplist(fold_char, Cs, Fs),
    atom_chars(F, Fs).

fold_char(C, F) :-
    member(C-F,
           ['á'-a, 'à'-a, 'ä'-a, 'â'-a, 'Á'-a, 'À'-a, 'Ä'-a, 'Â'-a,
            'é'-e, 'è'-e, 'ë'-e, 'ê'-e, 'É'-e, 'È'-e, 'Ë'-e, 'Ê'-e,
            'í'-i, 'ì'-i, 'ï'-i, 'î'-i, 'Í'-i, 'Ì'-i, 'Ï'-i, 'Î'-i,
            'ó'-o, 'ò'-o, 'ö'-o, 'ô'-o, 'Ó'-o, 'Ò'-o, 'Ö'-o, 'Ô'-o,
            'ú'-u, 'ù'-u, 'ü'-u, 'û'-u, 'Ú'-u, 'Ù'-u, 'Ü'-u, 'Û'-u]), !.
fold_char(C, C).
% Sin listas cerradas hardcodeadas: palabra de contenido = la que el mapa
% vivo conoce como simbolo (sujeto, relacion u objeto). El resto es glue
% y se ignora. Doctrina roadmap: nada open-class hardcodeado.
bb_content(W) :-
    ( memory_relation(W, _, _, _, _)
    ; memory_relation(_, W, _, _, _)
    ; memory_relation(_, _, W, _, _)
    ), !.

:- use_module(library(lists)).

chat_quit_token(T) :- member(T, [quit, exit, bye]).

:- dynamic last_fact/3.

% Esqueleto de la ultima pregunta con forma (D2): la elipsis lo continua.
:- dynamic dialog_lastq/1.

% Ultima lista respondida + cuanto se mostro (D7 more).
:- dynamic dialog_lastlist/1.
:- dynamic dialog_lastoff/1.

% Aclaracion pendiente (D3): pregunta original + pronombre + candidatos.
% Una linea que responda con un candidato prosigue; cualquier otra cosa
% la abandona y se procesa normal.
:- dynamic dialog_pending/3.

% Ensenanza pendiente (D4 iniciativa): (Sujeto, Verbo) con objeto
% ausente. La siguiente linea de una palabra lo completa y se aprende
% directo (remember_tracked con provenance sentence_N); otra cosa la
% abandona. Single-shot como D3.
:- dynamic dialog_pending_teach/2.

% Relaciones ensenadas en sesion: candidatas a descubrir (P3). Se siembran
% en chat_learn y las consume el comando discover. Interfaz propia.
:- dynamic told_rel/1.
:- dynamic dialog_last_input/1.

chat(File) :-
    atom(File), !,
    bb_load(File),
    writeln('BookBrain chat. Ask me anything (quit to exit).'),
    chat_loop.
chat(Files) :-
    is_list(Files),
    Files = [F|Rest],
    bb_load(F),
    maplist(bb_add, Rest),
    writeln('BookBrain chat. Ask me anything (quit to exit).'),
    chat_loop.

bb_load(File) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(last_fact(_, _, _)),
    retractall(told_rel(_)),
    retractall(dialog_lastq(_)),
    retractall(dialog_lastlist(_)),
    retractall(dialog_lastoff(_)),
    retractall(dialog_pending(_, _, _)),
    retractall(dialog_pending_teach(_, _)),
    retractall(dialog_last_input(_)),
    dialog_reset,
    retractall(gapfact(_, _, _, _, _)),
    retractall(gapseq(_)),
    gaps_reset,
    bb_import(File),
    gap_load,
    memory_size(NF),
    format('Loaded ~w facts from ~w.~n', [NF, File]).

% Suma un .knowledge.pl ya procesado sin borrar lo cargado.
bb_add(File) :-
    bb_import(File),
    gap_load,
    memory_size(NF),
    format('Added ~w. Memory now ~w facts.~n', [File, NF]).

% Lee memfact/provfact/gapfact como datos (no consult: el segundo
% archivo no puede redefinir el primero).
bb_import(File) :-
    setup_call_cleanup(
        open(File, read, S, [encoding(utf8)]),
        bb_import_stream(S),
        close(S)).

bb_import_stream(S) :-
    read_term(S, T, []),
    ( T == end_of_file -> true
    ; bb_import_term(T),
      bb_import_stream(S)
    ).

bb_import_term((:- _)) :- !.
bb_import_term(memfact(A, R, O, W, U)) :-
    ( memory_relation(A, R, O, _, _) -> true
    ; assertz(memory_relation(A, R, O, W, U))
    ), !.
bb_import_term(provfact(A, R, O, Ref, T, St)) :-
    ( prov(A, R, O, _) -> true
    ; assertz(prov(A, R, O, info(Ref, T, St)))
    ), !.
bb_import_term(gapfact(Id, Q, St, C, E)) :-
    assertz(gapfact(Id, Q, St, C, E)), !.
bb_import_term(gapseq(N)) :-
    assertz(gapseq(N)), !.
bb_import_term(_).

% save [nombre]: vuelca la memoria viva (base + lo ensenado en sesion) al
% mismo formato memfact/provfact que bb_load/1 lee. Sin nombre -> session.
% Solo nombres simples (sin rutas): el save nunca sale de esta carpeta.
chat_save_cmd([save, Name|_]) :-
    atom(Name), Name \== save, Name \== '..',
    \+ sub_atom(Name, _, _, _, '/'),
    \+ sub_atom(Name, _, _, _, '\\'),
    chat_save(Name), !.

% load [nombre]: suma un .knowledge.pl ya procesado (sin borrar).
chat_load_cmd([load, Name|_]) :-
    atom(Name), Name \== load, Name \== '..',
    \+ sub_atom(Name, _, _, _, '/'),
    \+ sub_atom(Name, _, _, _, '\\'),
    atom_concat(Name, '.knowledge.pl', F),
    ( exists_file(F) -> bb_add(F)
    ; format('No file ~w~n', [F])
    ), !.

chat_save(Alias) :-
    atom_concat(Alias, '.knowledge.pl', Out),
    open(Out, write, S),
    forall(memory_relation(A, R, O, W, U),
           format(S, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
    forall(prov(A, R, O, info(Ref, T, St)),
           format(S, 'provfact(~q,~q,~q,~q,~q,~q).~n', [A, R, O, Ref, T, St])),
    gap_save(S),
    close(S),
    memory_size(NF),
    format('Saved ~w facts -> ~w~n', [NF, Out]).

% discover [rel]: induce reglas sobre lo ensenado en sesion (P3) via
% learn_cycle_guided (conceptos + guided + constrained). Sin args: todas
% las relaciones contadas en told_rel/1. Explicito, no automatico: el
% coste sigue al vocabulario (EXP51/52) y un discover sorpresa en un mapa
% grande pararia el dialogo. Las preguntas usan las reglas despues, con
% prueba atribuida (solve_slot/proof_for: hechos primero, reglas despues).
chat_discover_cmd([discover, R]) :-
    atom(R), R \== discover,
    chat_discover_targets([R]), !.

chat_discover_all :-
    findall(R, told_rel(R), Rs0),
    sort(Rs0, Rs),
    ( Rs == [] ->
        writeln('Nothing taught yet: teach me facts ending with a period first.')
    ; chat_discover_targets(Rs)
    ).

chat_discover_targets(Rs) :-
    learn_cycle_guided(Rs, Rows),
    forall(member(row(T, Path, F1, Gen, Ev, Ms), Rows),
           ( constrained_rule(T, _, _) ->
               format('rule: ~w :- ~w (F1=~2f, gen=~w, eval=~w, ~wms)~n',
                      [T, Path, F1, Gen, Ev, Ms])
           ; format('no rule kept for ~w (best ~w, F1=~2f)~n', [T, Path, F1])
           )).

chat_loop :-
    write('> '),
    flush_output,
    catch(read_line_to_string(user_input, L), _, L = end_of_file),
    ( L == end_of_file -> chat_autosave, writeln('Bye.')
    ; gen_tokenize(L, Lower, _),
      exclude(gen_qmark, Lower, T0),
      T0 = [T], chat_quit_token(T) ->
          chat_autosave, writeln('Bye.')
    ; chat_line(L),
      chat_loop
    ).

chat_autosave :-
    catch(chat_save(session), _, true).

chat_line(L) :-
    catch(chat_line_guarded(L), E, format('UNKNOWN (~w).~n', [E])).

chat_line_guarded(L) :-
    retractall(dialog_last_input(_)),
    assertz(dialog_last_input(L)),
    gen_tokenize(L, Lower, _),
    exclude(gen_qmark, Lower, Toks0),
    chat_line_dispatch(L, Toks0).

chat_line_dispatch(L, Toks0) :-
    % D3: la respuesta a una aclaracion prosigue la pregunta original.
    % D4: la respuesta a un sondeo completa la ensenanza.
    % Cualquier otra linea abandona pendientes (single-shot).
    ( dialog_pending(Q0, P0, Cs0),
      dialog_match_reply(Toks0, Cs0, C0) ->
        retractall(dialog_pending(_, _, _)),
        retractall(dialog_pending_teach(_, _)),
        maplist(dl_repl(P0, C0), Q0, ToksP),
        dialog_substitute(ToksP, Toks, _),
        chat_line_tokens(L, Toks, yes)
    ; dialog_pending_teach(S, V),
      dialog_why_ask(Toks0) ->
        dialog_why_need(S, V)
    ; dialog_pending_teach(S, V),
      dialog_teach_reply(Toks0, O) ->
        retractall(dialog_pending(_, _, _)),
        retractall(dialog_pending_teach(_, _)),
        chat_learn_direct(S, V, O)
    ; retractall(dialog_pending(_, _, _)),
      retractall(dialog_pending_teach(_, _)),
      dialog_substitute(Toks0, Toks, Changed),
      chat_line_tokens(L, Toks, Changed)
    ).

% Respuesta de una palabra (articulos fuera) que no sea comando,
% pronombre ni palabra cerrada: autoridad del usuario, vale novel.
dialog_teach_reply(Toks, O) :-
    exclude(is_article, Toks, [O]),
    atom(O),
    \+ member(O, [quit, exit, bye, help, why]),
    \+ dialog_pronoun(O),
    \+ member(O, [what, who, when, where, why, how, which, whom,
                  and, or, not, no, yes, do, does, did, is, are]).

is_article(W) :- member(W, [the, a, an]).

% Aprendizaje directo del slot ausente, con provenance sentence_N.
chat_learn_direct(S, V, O) :-
    next_sentence_id(Src),
    remember_tracked(S, V, O, Src, none),
    chat_after_learn(Src, S, V, O).

% D4 iniciativa (patron EXP48): asercion de 2 tokens [Sujeto, Verbo]
% con S conocido y V con familia en memoria = slot objeto ausente. El
% modelo pregunta ("What did S V?") y la siguiente linea de una palabra
% completa el hecho. Sin el par, camino normal (UNKNOWN honesto).
dialog_probe_missing([S, V]) :-
    \+ qlead(S),
    atom_length(S, LS), LS >= 2,
    \+ is_determiner(V),
    atom_length(V, LV), LV >= 3,
    ( qnorm([S], S2) -> true ; S2 = S ),
    bb_stem(V, VB),
    format('What did ~w ~w?~n', [S2, VB]),
    assertz(dialog_pending_teach(S2, V)).

% Bratko Why (no How): por que el sondeo, sin abandonar el slot.
dialog_why_ask([why]).
dialog_why_ask([porque]).
dialog_why_ask([por, que]).

dialog_why_need(S, V) :-
    bb_stem(V, VB),
    format('Because I need the object of ~w ~w.~n', [S, VB]).

chat_line_tokens(L, Toks, Changed) :-
    ( Toks == [] -> true
    ; Toks = [T], chat_quit_token(T) -> writeln('Bye.')
    ; Toks == [help] -> chat_help
    ; Toks == [ayuda] -> chat_help
    ; Toks == [why] -> chat_why_bare
    ; chat_social(Toks) -> true
    ; Toks == [more] -> dialog_more
    ; Toks == [mas] -> dialog_more
    ; Toks == [the, rest] -> dialog_more
    ; Toks == [save] -> chat_save(session)
    ; chat_save_cmd(Toks) -> true
    ; chat_load_cmd(Toks) -> true
    ; Toks == [discover] -> chat_discover_all
    ; chat_discover_cmd(Toks) -> true
    ; chat_greet(Toks) -> true
    ; chat_about(Toks) -> true
    ; book_other(Toks, B) -> about_entity(B, es)
    ; tell_me_bare(Toks) ->
        ( member(cuentame, Toks) -> chat_list_books(es)
        ; chat_list_books(en)
        )
    ; Toks == [who, are, you] -> chat_identity(en)
    ; Toks == [what, are, you] -> chat_identity(en)
    ; Toks == [quien, eres] -> chat_identity(es)
    ; Toks == [quienes, somos] -> chat_identity(es)
    ; ( Toks == [who, do, you, know] ; Toks == [whom, do, you, know] ) ->
        chat_known(en)
    ; ( Toks == [quien, conoces] ; Toks == [a, quien, conoces] ) ->
        chat_known(es)
    ; is_question(L) ->
        ( dialog_about_book(Toks) -> true
        ; dialog_meta_es(Toks) -> true
        ; dialog_single_about(Toks) -> true
        ; dialog_graph_resolve(Toks, ToksR) -> chat_ask(ToksR)
        ; dialog_ambiguity_narrow(Toks, P, Cs) ->
            dialog_clarify(P, Cs),
            assertz(dialog_pending(Toks, P, Cs))
        ; dialog_ellipsis(Toks) -> true
        ; chat_ask(Toks)
        )
    ; chat_learn(L, Toks, Changed)
    ).

% D5 actos meta en espanol (sintaxis de dialogo, como help/quit: que
% sabes / de que trata / protagonistas -> lo conocido). Verbos de
% contenido en espanol ("quien abrio...") quedan FUERA a proposito:
% exigen lexico verbal que no existe; responden unknown honesto.
% "de que …?" sin anclas: de+que ya son interrogativos cerrados.
dialog_meta_es([de, que|Rest]) :-
    Rest \== [],
    book_pin(Rest, B), !,
    about_entity(B, es).
dialog_meta_es([de, que|Rest]) :-
    Rest \== [],
    book_anaphora(Rest, B), !,
    about_entity(B, es).
dialog_meta_es([de, que|Rest]) :-
    Rest \== [],
    \+ graph_pins([de, que|Rest], _, _), !,
    chat_known(es).

% Unico ente = un titulo (is book) y ninguna rel viva: el mapa
% describe ese libro (appears_in, author, set_in).
% Titulo dicho (plataforma, alice_in_wonderland). Sin pin_rel:
% ese camino se cuelga con verbos novel (parece) via means/inflect.
book_pin(Toks, B) :-
    nl_tokens(Toks, Packed), !,
    live_books(Bs),
    findall(X, (member(X, Bs), packed_has(Packed, X)), [B]),
    % Si hay rel viva que toca el titulo (author, appears_in), no
    % volcar el about: lo responde graph_ask.
    findall(R, (member(W, Packed), pin_rel(W, R), rel_touches([B], R)), RR),
    RR == [],
    packed_names(Packed, Ents),
    exclude(==(B), Ents, []).

packed_has(Packed, B) :-
    member(W, Packed),
    qnorm_atom(W, B).

packed_names(Packed, Ents) :-
    findall(E, (member(W, Packed), qnorm_atom(W, E), map_entity(E)), Es),
    sort(Es, Ents).

book_type(T) :-
    live_books(Bs),
    member(B, Bs),
    memory_relation(B, is, T, _, _),
    \+ member(T, Bs).

other_word(W) :-
    member(W, [other, others, otro, otra, otros, otras]).

% "el libro" / "the book": tipo del grafo o cero entes; recencia
% en la pila (ultimo titulo mencionado).
book_anaphora(Toks, B) :-
    live_books(Bs), Bs \== [],
    member(D, Toks),
    is_determiner(D),
    nl_tokens(Toks, Packed),
    \+ include(packed_has(Packed), Bs, [_|_]),
    \+ (member(W, Packed), other_word(W)),
    packed_names(Packed, Ents),
    ( Ents == []
    ; Ents = [T], book_type(T)
    ),
    last_book(Bs, B).

% "de que trata el libro": libro generico con verbo novel.
book_anaphora(Rest, B) :-
    member(D, Rest),
    is_determiner(D),
    member(W, Rest),
    atom_length(W, L), L >= 3,
    \+ bb_content(W),
    \+ qlead(W),
    live_books([B]), !.

% "el otro libro": el titulo que no es el de recencia.
book_other(Toks, B) :-
    live_books(Bs),
    member(W, Toks),
    other_word(W),
    nl_tokens(Toks, Packed),
    \+ include(packed_has(Packed), Bs, [_|_]),
    last_book(Bs, Last),
    findall(X, (member(X, Bs), X \== Last), [B]).

last_book(Bs, B) :-
    dialog_stack(subj, Ss),
    member(B, Ss),
    memberchk(B, Bs), !.
last_book(Bs, B) :-
    dialog_stack(obj, Os),
    member(B, Os),
    memberchk(B, Bs), !.
last_book([B], B).

% Dos titulos vivos y "el otro": rels comunes del grafo, no invencion.
book_compare(Toks) :-
    live_books(Bs),
    Bs = [_,_|_],
    nl_tokens(Toks, Packed),
    member(W, Packed),
    other_word(W),
    \+ include(packed_has(Packed), Bs, [_|_]).

% "que X?" y X no vive en el mapa: pregunta por el propio KB.
% Si el mapa tiene (Titulo, is, book), lista esos titulos (el grafo
% distingue; no hay lista de libros en codigo).
dialog_about_book([que, X]) :-
    \+ bb_content(X),
    \+ pin_rel(X, _), !,
    chat_list_books(es).
dialog_about_book([what, X]) :-
    \+ bb_content(X),
    \+ pin_rel(X, _), !,
    chat_list_books(en).

live_books(Bs) :-
    findall(B, memory_relation(B, is, book, _, _), B0),
    sort(B0, Bs).

chat_compare_books(_Lang) :-
    live_books(Bs),
    findall(R, (append(_, [B1|Rest], Bs), member(B2, Rest),
                memory_relation(B1, R, _, _, _),
                memory_relation(B2, R, _, _, _)), R0),
    sort(R0, Rs),
    ( Rs == [] -> chat_list_books(en)
    ; forall(member(R, Rs),
             forall(member(B, Bs),
                    forall(memory_relation(B, R, O, _, _),
                           (surface_sent((B, R, O), Line), writeln(Line)))))
    ).

chat_list_books(Lang) :-
    live_books(Bs),
    memory_size(NF),
    ( Bs == [] ->
        ( Lang == es ->
            format('Hablamos de un libro con ~w hechos. ', [NF])
        ; format('This book holds ~w facts. ', [NF])
        ),
        chat_known(Lang)
    ; length(Bs, NB),
      atomic_list_concat(Bs, ', ', L),
      ( Lang == es ->
          format('Hay ~w libros (~w hechos): ~w.~n', [NB, NF, L])
      ; format('There are ~w books (~w facts): ~w.~n', [NB, NF, L])
      ),
      chat_known(Lang)
    ).

% D5 entidad suelta con '?': resumen (about). Sin '?' va a learn
% (D3-abandon intacto: "Madrid" sin '?' sigue unknown).
dialog_single_about([X]) :-
    qnorm([X], X), !,
    about_entity(X, en).

% Respuesta D3 = el candidato (articulos/glue fuera). Un interrogativo
% o mas de un ancla es pregunta nueva: se abandona el pending.
dialog_match_reply(Toks, Cs, C) :-
    \+ (member(W, Toks), qlead(W)),
    nl_tokens(Toks, Packed),
    exclude(about_glue, Packed, [Name]),
    qnorm([Name], C),
    member(C, Cs).

dl_repl(P, C, W, O) :- ( W == P -> O = C ; O = W ).

% El grafo desambigua: si el verbo anclado solo lo tiene un candidato
% en el rol del pronombre, se sustituye. Si quedan 2+, se pregunta
% solo entre esos (no entre todo el stack).
dialog_graph_resolve(Toks, Out) :-
    member(P, Toks),
    dialog_pronoun(P),
    dialog_candidates(P, Cs0),
    length(Cs0, N), N >= 2,
    graph_narrow(Toks, P, Cs0, [C]),
    maplist(dl_repl(P, C), Toks, Out).

dialog_ambiguity_narrow(Toks, P, Cs) :-
    dialog_ambiguity(Toks, P, Cs0),
    graph_narrow(Toks, P, Cs0, Cs),
    length(Cs, M), M >= 2.

graph_narrow(Toks, P, Cs0, Cs) :-
    exclude(==(P), Toks, Rest),
    ( Rest \== [],
      nl_tokens(Rest, Packed),
      findall(R, (member(W, Packed), pin_rel(W, R)), R0),
      sort(R0, Rels),
      Rels \== [] ->
        dialog_tier(P, Role),
        include(has_role_rel(Rels, Role), Cs0, Cs1),
        ( Cs1 == [] -> Cs = Cs0 ; Cs = Cs1 )
    ; Cs = Cs0
    ).

has_role_rel(Rels, subj, E) :-
    member(R, Rels), memory_relation(E, R, _, _, _), !.
has_role_rel(Rels, obj, E) :-
    member(R, Rels), memory_relation(_, R, E, _, _), !.
has_role_rel(Rels, both, E) :-
    ( has_role_rel(Rels, subj, E) ; has_role_rel(Rels, obj, E) ), !.

% Pregunta de aclaracion: hasta 5 candidatos en orden de recencia.
dialog_clarify(_P, Cs) :-
    take_first(Cs, 5, Show),
    join_or(Show, L),
    format('Do you mean ~w?~n', [L]).

take_first(_, 0, []) :- !.
take_first([], _, []) :- !.
take_first([H|T], K, [H|R]) :-
    K1 is K - 1,
    take_first(T, K1, R).

join_or([A, B], L) :- !, atomic_list_concat([A, ' or ', B], L).
join_or([A, B|T], L) :-
    atomic_list_concat([A, ', '], H),
    join_or([B|T], R),
    atom_concat(H, R, L).
join_or([A], A).

% Pregunta = termina en '?'. Todo lo demas es candidato a ensenar, nunca
% a responder: las preguntas jamas se ingieren (doctrina roadmap).
is_question(L) :-
    ( string(L) -> S = L ; atom_string(L, S) ),
    sub_string(S, _, _, 0, "?"), !.

% chat_learn: ingesta en sesion (O(1) streaming, provenance sentence_N).
% Repetir un hecho lo refuerza (Uses++), no lo duplica. Dos vias:
% 1) open_vocab (verbos conocidos); 2) SVO estructural puro (positional.pl,
%    EXP30: sujeto=primer token, objeto=ultimo, relacion=tramo intermedio),
%    solo si termina en '.' (acto declarativo = sintaxis, no lexico).
% Guardia: si encaja en forma de pregunta, se responde y NO se aprende.
% Sin triple ni forma: chat_ask (UNKNOWN honesto).
% chat_learn: pronouns never get stored. Unresolvable pronoun ->
% honest refusal; resolved ones were substituted upstream, so the
% learned sentence is rebuilt from names (Changed==yes) instead of
% the raw line. Plain lines learn verbatim (zero behavior change).
chat_learn(L, Toks, Changed) :-
    ( member(Neg, [not, never, no]),
      member(Neg, Toks) ->
        writeln('I don''t learn negated facts. Tell me what IS true.')
    ; dialog_has_pronoun(Toks) ->
        writeln('I don''t know who that is. Use names and I will learn it.')
    ; ( Changed == yes ->
          atomic_list_concat(Toks, ' ', B),
          atom_string(B, S0),
          string_concat(S0, ".", LS)
      ; ( string(L) -> LS = L ; atom_string(L, LS) )
      ),
      chat_learn_sent(LS, Toks)
    ).

chat_learn_sent(LS, Toks) :-
    ( chat_form(Toks, _, _) -> chat_ask(Toks)
    ; Toks = [W|_], qlead(W) -> chat_ask(Toks)
    ; book_pin(Toks, B) ->
        ask_lang(Toks, Lang),
        about_entity(B, Lang)
    ; dialog_probe_missing(Toks) -> true
    ; chat_learn_conj(Toks) -> true
    ; learn_sentence(LS, stored(A, V, O, Src)) ->
        chat_after_learn(Src, A, V, O)
    ; learn_sentence(LS, stored_identity(X, Y, _)) ->
        format('Learned: ~w is ~w.~n', [X, Y]),
        chat_recheck_gaps
    ; parse_svo_line(LS, (A, V, O)),
      memory_relation(_, V, _, _, _) ->
        chat_store(A, V, O)
    ; chat_learn_positional(Toks)
    ; novel_short(Toks) -> true
    ; chat_ask(Toks)
    ).

% SVO estructural: el sujeto es el primer token, el objeto el ultimo,
% la relacion el tramo intermedio. Ya estamos en la via de ensenar
% (no hay '?' ni interrogativo inicial): el punto es opcional.
chat_learn_positional(Toks) :-
    Toks = [F | _],
    \+ qlead(F),
    \+ novel_short(Toks),
    positional_triple(Toks, A, V, O),
    chat_store(A, V, O).

% "S V O and V2 O2" / "S V O and O2": and/y ya es conjuncion cerrada
% (D2). El sujeto se comparte; el segundo verbo, si es novel y unico
% cerca de uno vivo del mismo par, se recanoniza (ate~eat).
chat_learn_conj(Toks) :-
    append(Left, [And|Right], Toks),
    member(And, [and, y]),
    Left \== [], Right \== [],
    positional_triple(Left, A, V, O),
    conj_right(A, V, Right, A2, V2, O2),
    (A, V, O) \== (A2, V2, O2),
    chat_store(A, V, O),
    chat_store(A2, V2, O2).

% Primero el objeto solo (and the queen): comparte el verbo. Si se
% antepone el sujeto a un determinante, el 3-token lo tomaria por rel.
% Full SVO on right with new subject: two independent facts.
conj_right(A, _V, Right, A2, V2, O2) :-
    Right = [First|Rest],
    First \== A,
    \+ is_determiner(First),
    Rest \== [],
    positional_triple(Right, A2, V2, O2),
    \+ is_determiner(V2).
% Object only: shares subject from left.
conj_right(A, V, Right, A, V, O2) :-
    nl_tokens(Right, [O2]),
    O2 \== A,
    O2 \== V.
conj_right(A, _V, Right, A, V2, O2) :-
    positional_triple([A|Right], A, V2, O2),
    \+ is_determiner(V2).

chat_store(A, V, O) :-
    \+ is_determiner(A),
    \+ is_determiner(V),
    \+ is_determiner(O),
    canon_rel(A, V, O, R),
    next_sentence_id(Src),
    remember_tracked(A, R, O, Src, none),
    chat_after_learn(Src, A, R, O).

% Verbo novel a distancia <=2 del unico predicado vivo del par (o
% del sujeto si el objeto es nuevo): se reusa, no se duplica.
canon_rel(S, V, O, R) :-
    findall(Rx, (memory_relation(S, Rx, O, _, _),
                 symbol_dist(V, Rx, D), D =< 2), R0),
    sort(R0, [R]), !.
canon_rel(S, V, _O, R) :-
    \+ live_symbol(V),
    findall(Rx, (memory_relation(S, Rx, _, _, _),
                 symbol_dist(V, Rx, D), D =< 2), R0),
    sort(R0, [R]), !.
canon_rel(_S, V, _O, V).

chat_after_learn(Src, A, V, O) :-
    chat_learned(Src, A, V, O),
    chat_note_told(V),
    chat_set_last(A, V, O),
    chat_recheck_gaps.

% Todos los tokens novel y alguno de longitud <= 2: es glue/clitico,
% no un nombre (Zorin reaches Oslo sobrevive: todos >= 4).
novel_short(Toks) :-
    nl_tokens(Toks, Packed),
    forall(member(W, Packed), \+ live_symbol(W)),
    member(W, Packed),
    atom_length(W, L),
    L =< 2.

chat_learned(Src, A, V, O) :-
    surface_sent((A, V, O), Line),
    format('Learned [~w]: ~w~n', [Src, Line]).

% Hueco = la pregunta en superficie que el mapa no cubrio. Sin lexico:
% al ensenar se reintenta cada abierta; si ahora hay ancla, se cierra.
chat_recheck_gaps :-
    ( gap(_, _, open, _, _) -> gap_recheck(chat_try_question) ; true ).

chat_try_question(Q, Line) :-
    gen_tokenize(Q, Lower, _),
    exclude(gen_qmark, Lower, Toks),
    ( chat_form(Toks, _, Ans), chat_ans_line(Ans, Line)
    ; graph_ask(Toks, _, Ans), chat_ans_line(Ans, Line)
    ; es_form(Toks, _, Ans), chat_ans_line(Ans, Line)
    ),
    Line \== unknown.

chat_ans_line(answer(_, [T|_]), Line) :-
    surface_sent(T, Line).
chat_ans_line(yes((S, V, O)), Line) :-
    surface_sent((S, V, O), Line).
chat_ans_line(explanation(S, V, O, _), Line) :-
    surface_sent((S, V, O), Line).

chat_unknown :-
    writeln("I don't know."),
    chat_note_gap.

chat_note_gap :-
    dialog_last_input(Q),
    is_question(Q), !,
    gap_open(Q, chat).
chat_note_gap.

% SVO tras empaquetar spans del mapa y soltar glue (tokens que el
% grafo no conoce pegados a una entidad que si). Sin listas: el
% articulo cae porque no es simbolo; "Zorin" se queda porque el
% vecino tampoco lo es (nombre novel).
positional_triple(Toks, A, V, O) :-
    nl_tokens(Toks, Packed),
    append([A | Mid], [O], Packed),
    Mid \== [],
    mid_rel(Mid, V).

mid_rel(Mid, V) :-
    include(bb_content, Mid, Known0),
    exclude(is_determiner, Known0, Known),
    Known \== [], !,
    atomic_list_concat(Known, '_', V).
mid_rel(Mid, V) :-
    exclude(is_determiner, Mid, Mid2),
    Mid2 \== [],
    longest_mid(Mid2, V).

longest_mid([H|T], Best) :-
    atom_length(H, L),
    longest_mid(T, H, L, Best).
longest_mid([], B, _, B).
longest_mid([H|T], Acc, AL, Best) :-
    atom_length(H, L),
    ( L > AL -> longest_mid(T, H, L, Best)
    ; longest_mid(T, Acc, AL, Best)
    ).

% Empaqueta el tramo mas largo que ya es simbolo vivo (white+rabbit
% = white_rabbit). Lo que no matchea se deja token a token.
nl_tokens(Toks, Out) :-
    span_pack(Toks, Packed0), !,
    exclude(qlead, Packed0, Packed),
    % Tres tokens: el del medio es el hueco de relacion (SVO), aunque
    % el objeto ya viva en el mapa. Glue solo en tramos mas largos.
    ( Packed = [A, M, _],
      \+ is_determiner(A), \+ is_determiner(M) -> Out = Packed
    ; drop_glue(Packed, Out), !
    ),
    Out \== [].

span_pack([], []).
span_pack(Toks, [Name|Rest]) :-
    longest_span(Toks, Name, N),
    length(Used, N),
    append(Used, Tail, Toks),
    span_pack(Tail, Rest).

longest_span(Toks, Name, N) :-
    length(Toks, Max),
    try_span(Toks, Max, Name, N).

try_span(Toks, L, Name, L) :-
    L >= 2,
    length(Used, L),
    append(Used, _, Toks),
    atomic_list_concat(Used, '_', Name),
    bb_content(Name), !.
try_span(Toks, L, Name, N) :-
    L > 1,
    L1 is L - 1,
    try_span(Toks, L1, Name, N).
try_span([W|_], _, W, 1).

% Glue = desconocido pegado a un simbolo conocido. No es lexico:
% si el mapa no lo tiene, no es ancla.
drop_glue([], []).
drop_glue([U], [U]) :- !.
% Determinante + ente: siempre cae (sintaxis). Aunque un error
% previo hubiera guardado "the" como relacion, no es ancla.
drop_glue([U, B|T], Out) :-
    is_determiner(U),
    map_entity(B), !,
    drop_glue([B|T], Out).
% Prep corta entre un novel L>=3 y un ente (dets en medio caen):
% "happened to the baby" conserva happened+to+baby.
drop_glue([U, P|T], Out) :-
    \+ live_symbol(U), atom_length(U, LU), LU >= 3,
    \+ is_determiner(P), \+ live_symbol(P),
    atom_length(P, LP), LP =< 3,
    append(Dets, [B|Rest], T),
    forall(member(D, Dets), is_determiner(D)),
    map_entity(B), !,
    drop_glue(Rest, R2),
    Out = [U, P, B|R2].
drop_glue([U|T], Out) :-
    T \== [],
    atom_length(U, L), L =< 2, !,
    drop_glue(T, Out).
drop_glue([U|T], Out) :-
    qlead(U), !,
    drop_glue(T, Out).
drop_glue([A, U|T], Out) :-
    qlead(U), !,
    drop_glue([A|T], Out).
drop_glue([U, B|T], Out) :-
    glue_token(U),
    \+ pin_rel(U, _),
    map_entity(B),
    ( is_determiner(U) ; atom_length(U, L), L =< 2 ), !,
    drop_glue([B|T], Out).
drop_glue([A, U, B|T], Out) :-
    glue_token(U),
    \+ pin_rel(U, _),
    bb_content(A),
    bb_content(B),
    ( map_entity(A) ; map_entity(B) ), !,
    drop_glue([A, B|T], Out).
drop_glue([W|T], [W|R]) :-
    drop_glue(T, R).

glue_token(W) :- \+ bb_content(W).
map_entity(W) :-
    bb_content(W),
    ( memory_relation(W, _, _, _, _)
    ; memory_relation(_, _, W, _, _)
    ).

% Determinantes cerrados EN/ES fuera del SVO estructural (sintaxis;
% las preposiciones se conservan: portan significado).
is_determiner(W) :- member(W, [the, a, an, el, la, los, las, un, una]).

chat_note_told(V) :-
    ( told_rel(V) -> true ; assertz(told_rel(V)) ).

% Guardia anti-preguntas: la via estructural jamas ingiere forma
% interrogativa ("Who reaches Oslo." no es un hecho). Lista cerrada de
% auxiliares/interrogativos = sintaxis (como '?' o '.'), nunca lexico
% de contenido: jamas puede ser sujeto de una declarativa.
qlead(W) :- member(W, [who,what,when,where,why,how,which,
                       did,does,do,is,are,was,were,
                       quien,quienes,que,cual,cuales,there]), !.

chat_identity(Lang) :-
    memory_size(NF),
    ( Lang == es ->
        format('Soy BookBrain: un asistente simbolico con ~w hechos en memoria.~n', [NF]),
        writeln('Respondo con pruebas y fuentes; si no se, digo que no se.')
    ; format('I am BookBrain: a symbolic assistant with ~w facts in memory.~n', [NF]),
      writeln('I answer with proofs and sources; when I lack evidence, I say so.')
    ).

% doc_ent no persiste en .knowledge; top sujetos por hechos (sin eventos
% ni menciones sin resolver).
chat_known(Lang) :-
    findall(S, (memory_relation(S, _, _, _, _),
                \+ sub_atom(S, _, _, _, '_ch'),
                \+ sub_atom(S, _, 3, 0, 'he_'),
                \+ sub_atom(S, _, 5, 0, 'she_'),
                S \== narrator), Ss0),
    msort(Ss0, Ss),
    clump_counts(Ss, CC),
    keysort_counts(CC, Desc),
    take_names(Desc, 10, Top),
    atomic_list_concat(Top, ', ', L),
    ( Lang == es ->
        format('Conozco a: ~w.~n', [L])
    ; format('I know: ~w.~n', [L])
    ).

clump_counts([], []).
clump_counts([H|T], [C-H|R]) :-
    run_len(H, T, C, Rest),
    clump_counts(Rest, R).

run_len(_, [], 1, []).
run_len(H, [H|T], C, R) :- !, run_len(H, T, C0, R), C is C0 + 1.
run_len(_, L, 1, L).

keysort_counts(CC, Desc) :-
    findall(C-S, member(C-S, CC), P),
    keysort(P, A),
    reverse(A, Desc).

take_names(_, 0, []) :- !.
take_names([], _, []) :- !.
take_names([_-S|T], K, [S|R]) :-
    K1 is K - 1,
    take_names(T, K1, R).

% Saludos ES/EN.
chat_greet([W]) :-
    member(W, [hola, hello, hi, hey, buenas, saludos]), !,
    ( member(W, [hola, buenas, saludos]) ->
        writeln('Hola. Preguntame lo que quieras sobre el libro.')
    ; writeln('Hello. Ask me anything about the book.')
    ).
chat_greet([buenos, dias]) :- !, writeln('Hola. Preguntame lo que quieras sobre el libro.').
chat_greet([buenas, tardes]) :- !, writeln('Hola. Preguntame lo que quieras sobre el libro.').
chat_greet([buenas, noches]) :- !, writeln('Hola. Preguntame lo que quieras sobre el libro.').

% Hablame de X / tell me about X / who is X / quien es X.
% Resumen de entidad: primeros hechos (no-eventos) + total.
chat_about(Toks) :-
    about_target(Toks, Lang, Name), !,
    about_entity(Name, Lang).

about_target([y|Rest], Lang, Name) :- Rest \== [], about_target(Rest, Lang, Name).
about_target([and|Rest], Lang, Name) :- Rest \== [], about_target(Rest, Lang, Name).
about_target([hablame, de|Rest], es, Name) :- qnorm(Rest, Name).
about_target([hablame|Rest], es, Name) :- qnorm(Rest, Name).
about_target([cuentame, de|Rest], es, Name) :- qnorm(Rest, Name).
about_target([cuentame|Rest], es, Name) :- qnorm(Rest, Name).
about_target([quien, es|Rest], es, Name) :- qnorm(Rest, Name).
about_target([quien, fue|Rest], es, Name) :- qnorm(Rest, Name).
about_target([que, es|Rest], es, Name) :- qnorm(Rest, Name).
about_target([tell, me, about|Rest], en, Name) :- qnorm(Rest, Name).
about_target([talk, about|Rest], en, Name) :- qnorm(Rest, Name).
about_target([who, is|Rest], en, Name) :- qnorm(Rest, Name).
about_target([what, is|Rest], en, Name) :- qnorm(Rest, Name).

about_entity(Name, Lang) :-
    findall((R, O), (memory_relation(Name, R, O, _, _),
                     \+ sub_atom(O, _, _, _, '_ch')), Facts0),
    sort(Facts0, Facts),
    length(Facts, N),
    ( N > 0 ->
        about_show(Name, Lang, N, outgoing),
        show_facts(Name, Facts, 8),
        about_sources(Name),
        about_cast(Name),
        dialog_reset,
        dialog_note([Name], subj),
        about_note_book(Name),
        chat_set_last(Name, _, _)
    ; findall((S, R), memory_relation(S, R, Name, _, _), In0),
      sort(In0, In),
      In \== [] ->
        length(In, NI),
        about_show(Name, Lang, NI, incoming),
        show_facts_in(Name, In, 8),
        dialog_reset,
        dialog_note([Name], obj),
        about_note_book(Name),
        ( In = [(S0, R0)|_] -> chat_set_last(S0, R0, Name) ; true )
    ; ( Lang == es ->
            format('No se casi nada de ~w.~n', [Name])
      ; format('I know almost nothing about ~w.~n', [Name])
      )
    ).

about_show(Name, es, N, _) :-
    format('Esto se de ~w (~w hechos):~n', [Name, N]).
about_show(Name, en, N, _) :-
    format('This is what I know about ~w (~w facts):~n', [Name, N]).

% Quien aparece en un titulo: triples (S, appears_in, Libro) del mapa.
about_cast(Name) :-
    memory_relation(Name, is, book, _, _),
    findall((S, appears_in, Name),
            memory_relation(S, appears_in, Name, _, _), Ts0),
    sort(Ts0, Ts),
    Ts \== [], !,
    forall(member(T, Ts), (surface_sent(T, Line), format('  ~w~n', [Line]))).
about_cast(_).

% El titulo sigue en el stack de objetos para "el libro".
about_note_book(Name) :-
    memory_relation(Name, appears_in, Book, _, _), !,
    dialog_note([Book], obj).
about_note_book(Name) :-
    memory_relation(Name, is, book, _, _), !,
    dialog_note([Name], obj).
about_note_book(_).

show_facts(_, _, 0) :- !.
show_facts(_, [], _) :- !.
show_facts(N, [(R, O)|T], K) :-
    surface_sent((N, R, O), Line),
    format('  ~w~n', [Line]),
    K1 is K - 1,
    show_facts(N, T, K1).

show_facts_in(_, _, 0) :- !.
show_facts_in(_, [], _) :- !.
show_facts_in(Name, [(S, R)|T], K) :-
    surface_sent((S, R, Name), Line),
    format('  ~w~n', [Line]),
    K1 is K - 1,
    show_facts_in(Name, T, K1).

% Fuentes de los hechos mostrados (3 primeras con Ref).
about_sources(Name) :-
    findall((R, O, Ref), (memory_relation(Name, R, O, _, _),
                          prov(Name, R, O, info(Ref, _, _)),
                          \+ sub_atom(O, _, _, _, '_ch')), F0),
    sort(F0, Fs),
    show_src(Fs, 3).

show_src(_, 0) :- !.
show_src([], _) :- !.
show_src([(_, O, Ref)|T], K) :-
    format('  [~w] ~w~n', [Ref, O]),
    K1 is K - 1,
    show_src(T, K1).

gen_qmark('?').

chat_help :-
    writeln('Questions (English):'),
    writeln('  who <verb> <obj>?          -- Who did it?'),
    writeln('  what did <s> <verb>?       -- What did they do?'),
    writeln('  did <s> <verb> <obj>?      -- Yes/no check'),
    writeln('  why did <s> <verb> <obj>?  -- Explanation + proof'),
    writeln('  where did <s> <verb>?      -- Location'),
    writeln('  when did <s> <verb>?       -- Time'),
    writeln('  how many <verb> <obj>?     -- Count'),
    writeln('Questions (Spanish):'),
    writeln('  quien <verbo> <?objeto>?   -- Quien lo hizo?'),
    writeln('  que <verbo> <?sujeto>?     -- Que hicieron?'),
    writeln('Teach me facts (SVO, period optional):'),
    writeln('  Zorin reaches Oslo.        -- Direct assertion'),
    writeln('  Ana abrio puerta.           -- Spanish SVO'),
    writeln('Pronouns: he/she/it follow conversation (one clear antecedent).'),
    writeln('  With several antecedents I ask who you mean (answer a name).'),
    writeln('Ellipsis: "And Madrid?" continues the last question.'),
    writeln('Incomplete teaching ("Ana opened.") gets asked back once.'),
    writeln('Commands: more | discover [relation] | save [name] | load [name]'),
    writeln('  more (see the rest of long lists)'),
    writeln('  discover (find rules in what you taught me)'),
    writeln('  save [name] (dump processed memory to file)'),
    writeln('  load [name] (add a .knowledge.pl file)'),
    writeln('Social: gracias/thanks | adios/goodbye | how are you'),
    writeln('why? (about last answer) | quit').

% D10 pegamento social minimo (texto UI, no vocabulario del motor:
% pares fijos de cortesia; lo abierto como "hablas como..." sigue
% unknown honesto).
chat_social([gracias]) :- !, writeln('De nada. Preguntame lo que quieras.').
chat_social([thanks]) :- !, writeln('You are welcome. Ask me anything.').
chat_social([thank, you]) :- !, writeln('You are welcome. Ask me anything.').
chat_social([adios]) :- !, writeln('Adios, hasta pronto.').
chat_social([goodbye]) :- !, writeln('Goodbye, see you soon.').
chat_social([como, estas]) :- !, writeln('Bien. Y ahora, pregunta lo que quieras.').
chat_social([que, tal]) :- !, writeln('Todo bien. Preguntame lo que quieras.').
chat_social([how, are, you]) :- !, writeln('I am well. Now ask me anything.').

chat_why_bare :-
    ( last_fact(S, V, O) ->
        ( proof_for(S, V, O, Proof) ->
            surface_sent((S, V, O), Line),
            format('Because: ~w~n', [Line]),
            chat_sources([(S, V, O)]),
            format('  via ~w~n', [Proof])
        ; writeln("I don't know why.")
        )
    ; writeln('Nothing to explain yet. Ask something first.')
    ).

% Helper: remembers last question and outputs answer
chat_remember_and_say(Toks, Kind, Ans) :-
    retractall(dialog_lastq(_)),
    assertz(dialog_lastq(Toks)),
    chat_say(Kind, Ans).

chat_ask(Toks) :-
    ( dialog_has_pronoun(Toks) -> chat_unknown
    ; rel_split(Toks, Left, Right), last_ent(Left, _) ->
        ( rel_join(Left, Right) -> chat_ask(Left)
        ; chat_unknown
        )
    ; ask_conj(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_or(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_or_vp(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_except(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_only(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_both(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_neither(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_else(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; ask_same(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; pol_neg(Toks, Clean),
      ( once(chat_form(Clean, _, A0))
      ; once(graph_ask(Clean, _, A0))
      ),
      pol_flip(A0, Ans) ->
        chat_remember_and_say(Toks, say, Ans)
    ; chat_form(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; book_pin(Toks, B) ->
        ask_lang(Toks, Lang),
        about_entity(B, Lang)
    ; book_other(Toks, B) ->
        ask_lang(Toks, Lang),
        about_entity(B, Lang)
    ; book_anaphora(Toks, B) ->
        ask_lang(Toks, Lang),
        about_entity(B, Lang)
    ; book_compare(Toks) ->
        ask_lang(Toks, Lang),
        chat_compare_books(Lang)
    ; graph_ask(Toks, Kind, Ans) -> chat_remember_and_say(Toks, Kind, Ans)
    ; es_form(Toks, Kind, Ans) ->
        retractall(dialog_lastq(_)),
        assertz(dialog_lastq(Toks)),
        chat_say_es(Kind, Ans)
    ; length(Toks, 1), Toks = [W],
      atom_length(W, L), L >= 3,
      last_fact(_, _, _),
      \+ graph_pins(Toks, _, _) ->
        chat_why_bare
    ; length(Toks, 2),
      member(Wq, Toks), qlead(Wq),
      last_fact(_, _, _),
      \+ graph_pins(Toks, _, _) ->
        chat_why_bare
    ; dialog_last_input(Q), is_question(Q),
      \+ graph_pins(Toks, _, _),
      \+ has_unknown_content(Toks),
      live_books([_|_]) ->
        ask_lang(Toks, Lang),
        chat_list_books(Lang)
    ; chat_ask_qp(Toks)
    ).

ask_lang(Toks, es) :-
    member(W, Toks),
    qlead(W),
    member(W, [quien, quienes, que, cual, cuales]), !.
ask_lang(Toks, es) :-
    member(de, Toks), !.
ask_lang(_, en).

% True if Toks contains a content word (≥3 chars, not qlead/determiner)
% that is unknown to the knowledge base. Used to prevent listing books
% for questions with unrecognized verbs/entities.
has_unknown_content(Toks) :-
    member(W, Toks),
    atom_length(W, L), L >= 3,
    \+ qlead(W),
    \+ is_determiner(W),
    \+ bb_content(W),
    \+ pin_rel(W, _).

% Relativa (Bratko DCG, sin lexico): "that"/"which"/"que" en medio
% filtran el ente que precede. El join es un hecho vivo, o unknown.
rel_mark(that).
rel_mark(which).
rel_mark(que).

rel_split(Toks, Left, Right) :-
    append(Left, [M|Right], Toks),
    rel_mark(M),
    Left \== [], Right \== [],
    Toks \= [M|_].

last_ent(Toks, E) :-
    nl_tokens(Toks, Packed), !,
    reverse(Packed, Rev),
    member(W, Rev),
    pin_ent(W, E), !.

rel_join(Left, Right) :-
    last_ent(Left, E),
    rel_holds(Right, E).

rel_holds(Right, E) :-
    nl_tokens(Right, Packed), !,
    pins_of(Packed, Rels, Ents),
    ( member(R, Rels),
      ( member(S, Ents),
        ( memory_relation(S, R, E, _, _)
        ; memory_relation(E, R, S, _, _)
        )
      ; Ents == [],
        ( memory_relation(E, R, _, _, _)
        ; memory_relation(_, R, E, _, _)
        )
      )
    ; Rels == [], Ents = [S],
      ( memory_relation(S, _, E, _, _)
      ; memory_relation(E, _, S, _, _)
      )
    ), !.

% "did SVO and SVO": and/y cerrados. Las dos mitades contra el mapa.
ask_conj(Toks, Kind, Ans) :-
    append(Left, [And|Right], Toks),
    member(And, [and, y]),
    Left = [Did|_],
    member(Did, [did, does, do]),
    Right \== [],
    \+ (member(W, Right), qlead(W)),
    conj_right_q(Did, Right, RQ),
    once(chat_form(Left, _, A1)),
    once(chat_form(RQ, _, A2)),
    conj_merge(A1, A2, Kind, Ans).

conj_right_q(Did, Right, [Did|Right]) :-
    Right \= [Did|_], !.
conj_right_q(_, Right, Right).

conj_merge(yes(F1), yes(F2), say, yes_both(F1, F2)).
conj_merge(no, _, say, no).
conj_merge(_, no, say, no).

% "did alice or michel find key": or/o cerrado.
ask_or(Toks, Kind, Ans) :-
    append([Did, A], [Or, B|Rest], Toks),
    member(Did, [did, does, do]),
    member(Or, [or, o]),
    qnorm([A], E1), qnorm([B], E2),
    map_entity(E1), map_entity(E2),
    Rest \== [],
    once(chat_form([Did, A|Rest], _, A1)),
    once(chat_form([Did, B|Rest], _, A2)),
    or_merge(A1, A2, Kind, Ans).

or_merge(yes(F), no, say, yes(F)).
or_merge(no, yes(F), say, yes(F)).
or_merge(yes(F1), yes(F2), say, yes_both(F1, F2)).
or_merge(no, no, say, no).

% "did alice eat cake or find the key": mismo sujeto, dos VP.
ask_or_vp(Toks, Kind, Ans) :-
    append([Did, S|Left], [Or|Right], Toks),
    member(Did, [did, does, do]),
    member(Or, [or, o]),
    qnorm([S], E),
    map_entity(E),
    Left \== [], Right \== [],
    Right = [B0|_],
    \+ map_entity(B0),
    once(chat_form([Did, S|Left], _, A1)),
    once(chat_form([Did, S|Right], _, A2)),
    or_merge(A1, A2, Kind, Ans).

except_mark(besides).
except_mark(except).
except_mark(excepto).
except_mark(salvo).

% "who besides alice met the hatter": el ente tras besides se excluye.
ask_except(Toks, say, Ans) :-
    append(Left, [Ex, ETok|Right], Toks),
    except_mark(Ex),
    qnorm([ETok], E),
    map_entity(E),
    append(Left, Right, Core),
    Core \== [],
    once(( chat_form(Core, say, answer(Xs0, Facts0))
         ; graph_ask(Core, say, answer(Xs0, Facts0))
         )),
    exclude(==(E), Xs0, Xs),
    ( Xs == [] -> Ans = nobody
    ; include(fact_mentions(Xs), Facts0, Facts),
      Facts \== [],
      Ans = answer(Xs, Facts)
    ).

fact_mentions(Xs, (S, _, O)) :-
    ( member(S, Xs) ; member(O, Xs) ).

is_book_ent(B) :-
    memory_relation(B, is, book, _, _).

% "did only alice find the key": unico sujeto de ese V-O.
ask_only(Toks, say, Ans) :-
    append(Left, [only, STok|Right], Toks),
    Left \== [],
    qnorm([STok], S),
    map_entity(S),
    append(Left, [STok|Right], Core),
    once(chat_form(Core, _, A0)),
    ( A0 = yes((S, V, O)),
      findall(X, memory_relation(X, V, O, _, _), Xs0),
      sort(Xs0, [S]) -> Ans = yes((S, V, O))
    ; Ans = no
    ).

% "did alice meet both the hatter and the queen".
ask_both(Toks, Kind, Ans) :-
    append(Pre, [both|Rest], Toks),
    append(O1t, [And|O2t], Rest),
    member(And, [and, y]),
    O1t \== [], O2t \== [],
    Pre \== [],
    append(Pre, O1t, C1),
    append(Pre, O2t, C2),
    once(chat_form(C1, _, A1)),
    once(chat_form(C2, _, A2)),
    conj_merge(A1, A2, Kind, Ans).

% "did neither alice nor michel eat the key".
ask_neither(Toks, say, Ans) :-
    append(Left, [neither, A, nor, B|Right], Toks),
    member(Did, Left),
    member(Did, [did, does, do]),
    once(chat_form([Did, A|Right], _, A1)),
    once(chat_form([Did, B|Right], _, A2)),
    ( A1 = no, A2 = no -> Ans = yes_bare
    ; Ans = no
    ).

% "who else met the hatter": excluye el ultimo sujeto de la pila.
ask_else(Toks, say, Ans) :-
    select(else, Toks, Core),
    Core \== [],
    once(( chat_form(Core, say, answer(Xs0, F0))
         ; graph_ask(Core, say, answer(Xs0, F0))
         )),
    ( dialog_stack(subj, [Last|_]) ->
        exclude(==(Last), Xs0, Xs)
    ; Xs = Xs0
    ),
    ( Xs == [] -> Ans = nobody
    ; include(fact_mentions(Xs), F0, Facts),
      Facts \== [],
      Ans = answer(Xs, Facts)
    ).

% "same author" de dos titulos: rel compartida distinta de `is`.
ask_same(Toks, say, Ans) :-
    member(same, Toks),
    nl_tokens(Toks, Packed),
    live_books(Bs),
    include(packed_has(Packed), Bs, [B1, B2]),
    findall(R, (memory_relation(B1, R, _, _, _),
                memory_relation(B2, R, _, _, _),
                R \== is), Rs0),
    sort(Rs0, Rs),
    ( member(W, Packed), pin_rel(W, R), memberchk(R, Rs) -> true
    ; Rs = [R]
    ),
    memory_relation(B1, R, O1, _, _),
    memory_relation(B2, R, O2, _, _),
    ( O1 == O2 -> Ans = yes_bare ; Ans = no
    ).

% never/not invierte el si/no (polaridad cerrada, no lexico).
pol_neg(Toks, Clean) :-
    member(Neg, [never, not]),
    select(Neg, Toks, Clean),
    Clean \== [].

pol_flip(yes(_), no).
pol_flip(no, yes_bare).

tell_me_bare([tell, me]) :- !.
tell_me_bare([tell, me|Rest]) :-
    Rest \== [],
    \+ member(about, Rest),
    \+ graph_pins([tell, me|Rest], _, _).
tell_me_bare([cuentame]) :- !.
tell_me_bare([cuentame|Rest]) :-
    Rest \== [],
    \+ member(de, Rest),
    \+ graph_pins([cuentame|Rest], _, _).
% entidad anclan la consulta; el resto (interrogativos, determinantes,
% palabras nuevas) es el hueco. Sin listas de contenido: si el grafo
% no conoce el ancla, falla honesto.
graph_ask(Toks, say, Ans) :-
    nl_tokens(Toks, Packed),
    \+ member(why, Packed),
    pins_of(Packed, Rels, Ents),
    graph_fill(Packed, Rels, Ents, Ans).

graph_pins(Toks, Rels, Ents) :-
    nl_tokens(Toks, Packed),
    pins_of(Packed, Rels, Ents).

pins_of(Packed, Rels, Ents) :-
    findall(R, (member(W, Packed), pin_rel(W, R)), R0),
    sort(R0, Rels0),
    findall(E, (member(W, Packed), pin_ent(W, E)), E0),
    sort(E0, Ents),
    ( Ents \== [], Rels0 \== [] ->
        include(rel_touches(Ents), Rels0, Rels1),
        ( Rels1 == [] -> Rels = [] ; Rels = Rels1 )
    ; Rels = Rels0
    ),
    ( Rels \== [] ; Ents \== [] ).

rel_touches(Ents, R) :-
    member(E, Ents),
    ( memory_relation(E, R, _, _, _)
    ; memory_relation(_, R, E, _, _)
    ), !.

pin_rel(W, R) :-
    bb_rel_forms(W, Rs),
    member(R, Rs).
% set → set_in, appear → appears_in: stem del prefijo + '_' vivo.
pin_rel(W, R) :-
    \+ map_entity(W),
    atom_length(W, L), L >= 3,
    memory_relation(_, R, _, _, _),
    atom_concat(Prefix, Rest, R),
    sub_atom(Rest, 0, 1, _, '_'),
    inflect_same(W, Prefix).
pin_rel(W, R) :-
    \+ bb_rel_forms(W, _),
    \+ map_entity(W),
    fuzzy_rel(W, R).

fuzzy_rel(W, R) :-
    atom_chars(W, [F|Cs]),
    length([F|Cs], L),
    L >= 3,
    findall(D-Rel, (memory_relation(_, Rel, _, _, _),
                    atom_chars(Rel, [F|_]),
                    atom_length(Rel, LR),
                    abs(LR - L) =< 1,
                    symbol_dist(W, Rel, D),
                    D =< 2), DS),
    keysort(DS, [D0-R|_]),
    \+ (member(D1-R1, DS), R1 \== R, D1 =:= D0).

% Entidad del mapa que no es relacion: si un token ya es verbo en
% memoria, ancla como rel (pin_rel), no como objeto accidental.
pin_ent(W, E) :-
    \+ pin_rel(W, _),
    qnorm_atom(W, E),
    ( memory_relation(E, _, _, _, _)
    ; memory_relation(_, _, E, _, _)
    ).

:- discontiguous graph_fill/4.
graph_fill(_, Rels, [E], answer(Xs, Facts)) :-
    Rels \== [],
    findall(S-(S, R, E), (member(R, Rels), memory_relation(S, R, E, _, _)), SF),
    SF \== [],
    pack_pins(SF, Xs, Facts).
graph_fill(_, Rels, [E], answer(Xs, Facts)) :-
    Rels \== [],
    findall(O-(E, R, O), (member(R, Rels), memory_relation(E, R, O, _, _)), OF),
    OF \== [],
    pack_pins(OF, Xs, Facts).
graph_fill(Toks, Rels, [], answer(Xs, Facts)) :-
    Rels \== [],
    \+ stray_unknown(Toks, Rels),
    findall(S-(S, R, O), (member(R, Rels), memory_relation(S, R, O, _, _)), SF),
    SF \== [],
    pack_pins(SF, Xs, Facts).

% Token que no es interrogativo ni el verbo ancla: un objeto/sujeto
% novel ("ventana") impide volcar todos los hechos de esa relacion.
stray_unknown(Toks, Rels) :-
    member(W, Toks),
    \+ qlead(W),
    \+ member(W, Rels),
    \+ pin_rel(W, _),
    \+ bb_content(W),
    atom_length(W, L),
    L >= 2.

% Desconocido DETRAS del ente: verbo novel (go). Delante (there ana)
% no bloquea el about.
stray_after(Packed, E) :-
    append(_, [E|Rest], Packed),
    member(W, Rest),
    \+ qlead(W),
    \+ bb_content(W),
    \+ pin_rel(W, _),
    atom_length(W, L),
    L >= 2.

% About solo si no queda un verbo novel (L>=3) junto al ente.
pack_is_about(Packed, E) :-
    member(E, Packed),
    forall(member(W, Packed), (W == E ; about_glue(W))).

about_glue(W) :- qlead(W).
about_glue(W) :- is_determiner(W).
about_glue(W) :- atom_length(W, L), L =< 2.
about_glue(else).
about_glue(mas).

% "que paso con X" / "what happened to X": un verbo novel (L>=3) y el
% resto glue (qlead/det/prep L=<3). "to" cae por longitud; el evento
% sigue anclando el ente.
happened_glue(W) :- about_glue(W).
happened_glue(W) :- \+ live_symbol(W), atom_length(W, L), L =< 3.

happened_event(W) :-
    \+ happened_glue(W),
    \+ live_symbol(W).

happened_to(Packed, E) :-
    pin_ent(E, E),
    member(E, Packed),
    include(happened_event, Packed, [U]),
    U \== E,
    atom_length(U, L), L >= 3,
    member(P, Packed),
    P \== U, P \== E,
    \+ live_symbol(P),
    atom_length(P, LP), LP =< 3.
% Un solo ente conocido y un token desconocido cerca (Levenshtein)
% del UNICO predicado que toca ese ente: ate~eat sobre cake, sin lista.
graph_fill(Toks, [], [E], answer(Xs, Facts)) :-
    findall(R, (memory_relation(_, R, E, _, _)
              ; memory_relation(E, R, _, _, _)), R0),
    sort(R0, Rs),
    member(R, Rs),
    unknown_near(Toks, R),
    \+ (member(R2, Rs), R2 \== R, unknown_near(Toks, R2)),
    findall(S-(S, R, E), memory_relation(S, R, E, _, _), SF0),
    findall(O-(E, R, O), memory_relation(E, R, O, _, _), OF0),
    append(SF0, OF0, All),
    All \== [],
    pack_pins(All, Xs, Facts).
% Un ente, ninguna relacion anclada, sin objeto novel: son los hechos
% de ese ente (what did X …? se deduce del mapa, no de un verbo lista).
graph_fill(Packed, [], [E], answer(Xs, Facts)) :-
    ( pack_is_about(Packed, E) ; happened_to(Packed, E) ),
    \+ stray_after(Packed, E),
    findall(O-(E, R, O), memory_relation(E, R, O, _, _), OF),
    OF \== [],
    pack_pins(OF, Xs, Facts).

graph_fill(_, Rels, [A, B], yes((S, R, O))) :-
    member(R, Rels),
    ( memory_relation(A, R, B, _, _) -> S = A, O = B
    ; memory_relation(B, R, A, _, _) -> S = B, O = A
    ).
% "is michel in plataforma": appears_in es la rel de elenco (ensenada).
graph_fill(_, [], [A, B], yes((A, appears_in, B))) :-
    memory_relation(A, appears_in, B, _, _).
graph_fill(_, [], [A, B], yes((B, appears_in, A))) :-
    memory_relation(B, appears_in, A, _, _).
graph_fill(_, [], Ents, Ans) :-
    length(Ents, 3),
    include(is_book_ent, Ents, [Book]),
    subtract(Ents, [Book], [A, B]),
    ( memory_relation(A, appears_in, Book, _, _),
      memory_relation(B, appears_in, Book, _, _) ->
        Ans = yes_both((A, appears_in, Book), (B, appears_in, Book))
    ; Ans = no
    ).
graph_fill(_, [], [A, B], no) :-
    ( memory_relation(A, is, book, _, _)
    ; memory_relation(B, is, book, _, _)
    ),
    \+ memory_relation(A, appears_in, B, _, _),
    \+ memory_relation(B, appears_in, A, _, _).
graph_fill(Toks, [], [A, B], yes((S, R, O))) :-
    findall(Rx, (memory_relation(A, Rx, B, _, _)
               ; memory_relation(B, Rx, A, _, _)), R0),
    sort(R0, Rs),
    member(R, Rs),
    unknown_near_d(Toks, R, 3),
    \+ (member(R2, Rs), R2 \== R, unknown_near_d(Toks, R2, 3)),
    ( memory_relation(A, R, B, _, _) -> S = A, O = B
    ; S = B, O = A
    ).

unknown_near(Toks, R) :-
    unknown_near_d(Toks, R, 2).

unknown_near_d(Toks, R, MaxD) :-
    member(W, Toks),
    \+ bb_content(W),
    \+ qlead(W),
    atom_length(W, L),
    L >= 3,
    atom_length(R, LR),
    abs(LR - L) =< 1,
    symbol_dist(W, R, D),
    D =< MaxD.

pack_pins(Pairs, Xs, Facts) :-
    findall(X, member(X-_, Pairs), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, Pairs), Facts).

% D6 formas ES (espejo estructural de chat_form; interrogativos cerrados
% como '?' y qlead). Sin lexico verbal ES: V debe existir en memoria.
% quien [quien,V|Resto]: sujetos con (S,V,O). que [que,V|Resto]:
% objetos de (S,V) con S del resto. did [V,S,O] (orden VSO).
es_form([quien, V|Rest], say, answer(Xs, Facts)) :-
    qnorm(Rest, O),
    bb_rel_forms(V, Rs),
    findall(S-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), SF),
    SF \== [],
    findall(S, member(S-_, SF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, SF), Facts).
es_form([que, V|Rest], say, answer(Xs, Facts)) :-
    qnorm(Rest, S),
    bb_rel_forms(V, Rs),
    findall(O-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), OF),
    OF \== [],
    findall(O, member(O-_, OF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, OF), Facts).
% quien V E? pero nadie V a E: "No" honesto (evita fallback a outgoing).
es_form([quien, V|Rest], say, no) :-
    qnorm(Rest, O),
    O \== [],
    \+ (bb_rel_forms(V, Rs), member(Vr, Rs), memory_relation(_, Vr, O, _, _)).
es_form([V, S, O], say, YesNo) :-
    V \== quien, V \== que,
    qnorm_atom(S, S2),
    qnorm_atom(O, O2),
    bb_rel_forms(V, Rs),
    member(Vr, Rs),
    ( memory_relation(S2, Vr, O2, _, _) -> YesNo = yes((S2, Vr, O2))
    ; YesNo = no
    ).

% Salida ES: listas y pruebas identicas (lengua-neutral); Si/No/No-lo-se
% localizados.
chat_say_es(say, answer(Xs, Facts)) :-
    chat_say(say, answer(Xs, Facts)).
chat_say_es(say, yes((S, V, O))) :-
    writeln('Sí.'),
    chat_set_last(S, V, O),
    chat_sources([(S, V, O)]).
chat_say_es(say, no) :-
    writeln('No.').
chat_say_es(_, unknown) :-
    writeln('No lo sé.'),
    chat_note_gap.

% D2 elipsis: "And Madrid?" continua el esqueleto anterior con X en el
% slot (who/did/why/where/when: ultimo; what: sujeto). Solo formas con
% marcador (and | what/how about) y X entidad unica: disjunto de las
% formas completas por construccion. Sonda con evidencia (probe) o se
% deja al camino normal (UNKNOWN honesto). Caso especial: who + X
% persona sin objeto -> verificar candidato (Did X V O?).
% Tras responder, chat_ask actualiza LastQ: encadena ("And Oslo?").
dialog_ellipsis([and|Rest]) :-
    Rest \== [], \+ (member(W, Rest), qlead(W)), !,
    dialog_ell_span(Rest, continue).
dialog_ellipsis([y|Rest]) :-
    Rest \== [], \+ (member(W, Rest), qlead(W)), !,
    dialog_ell_span(Rest, continue).
dialog_ellipsis([what, about|Rest]) :- Rest \== [], !, dialog_ell_span(Rest, about).
dialog_ellipsis([how, about|Rest]) :- Rest \== [], !, dialog_ell_span(Rest, about).
dialog_ellipsis(_) :- fail.

dialog_ell_span(Rest, Mode) :-
    nl_tokens(Rest, Packed),
    Packed = [X],
    dialog_ell_entity(X, Mode).

% what/how about X: si X tiene hechos salientes, es el tema (el mapa
% lo dice). Si no, cae a continuar LastQ. And/y siempre continua.
dialog_ell_entity(X, about) :-
    qnorm([X], X),
    memory_relation(X, _, _, _, _),
    about_entity(X, en), !.
dialog_ell_entity(X, about) :-
    dialog_ell_entity(X, continue).
dialog_ell_entity(X, continue) :-
    ( qnorm([X], XN) -> X2 = XN ; X2 = X ),
    dialog_lastq(LQ),
    ( dialog_ell_build(LQ, X2, NewT), dialog_probe(NewT) -> chat_ask(NewT)
    ; dialog_ell_verify(LQ, X2)
    ; lastq_rel(LQ, V), graph_ask([V, X2], _, _) -> chat_ask([V, X2])
    ; dialog_ell_build(LQ, X2, NewT) -> chat_ask(NewT)
    ), !.

lastq_rel(LQ, V) :-
    member(W, LQ),
    pin_rel(W, V).

% Verificar-candidato: who [who,V,O] + X persona -> Did X V O?
dialog_ell_verify([who, V, O], X) :-
    dialog_in_subj(X),
    dialog_probe([did, X, V, O]), !,
    chat_ask([did, X, V, O]).

dialog_ell_build([who, V, _O], X, [who, V, X]) :-
    X \== V.
dialog_ell_build([what, did, _S, V], X, [what, did, X, V]) :-
    X \== V.
dialog_ell_build([did, S, V, _O], X, [did, S, V, X]) :-
    X \== S, X \== V.
dialog_ell_build([why, did | Mid], X, [why, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).
dialog_ell_build([where, did | Mid], X, [where, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).
dialog_ell_build([when, did | Mid], X, [when, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).
% Spanish: quien V O -> quien V X
dialog_ell_build([quien, V, _O], X, [quien, V, X]) :-
    X \== V.
% Spanish: que V S -> que V X
dialog_ell_build([que, V, _S], X, [que, V, X]) :-
    X \== V.
dialog_ell_build(LQ, X, New) :-
    append(Pre, [_Last], LQ),
    Pre \== [],
    \+ member(X, Pre),
    append(Pre, [X], New).

% Sonda: la forma resuelve con respuestas (yes/no son compuestos con
% la tripla; no, solo de did, es respuesta cerrada genuina porque las
% entidades vienen resueltas del esqueleto o de qnorm).
dialog_probe(T) :-
    ( chat_form(T, _, answer(Xs, _)) -> Xs \== []
    ; chat_form(T, _, yes(_)) -> true
    ; chat_form(T, _, no) -> true
    ; chat_form(T, _, explanation(_, _, _, _)) -> true
    ; graph_ask(T, _, answer(Xs2, _)), Xs2 \== []
    ; graph_ask(T, _, yes(_))
    ).

% X en tier de sujetos (persona candidata a verificar).
dialog_in_subj(X) :-
    dialog_stack(subj, Ss),
    member(X, Ss).

% Fallback con reglas: si la memoria directa no basta, question_parser
% (solve_slot: hechos, luego constrained rules inducidas, luego unknown).
% Solo se llega aqui cuando chat_form falla: lo ya respondido no cambia.
chat_ask_qp(Toks) :-
    atomic_list_concat(Toks, ' ', S),
    ( parse_question(S, QP), answer_query(QP, Ans) ->
        chat_say_qp(Ans)
    ; chat_unknown
    ).

chat_say_qp(answer(Xs, retrieved, Facts)) :-
    chat_say(say, answer(Xs, Facts)).
chat_say_qp(answer(Xs, reasoned, Proofs)) :-
    cap_list(Xs, 5, Show, Rest),
    atomic_list_concat(Show, ', ', L),
    ( Rest == 0 -> format('~w. (reasoned)~n', [L])
    ; format('~w... (and ~w more, reasoned)~n', [L, Rest])
    ),
    show_rule_proofs(Proofs, 3),
    remember_proof(Proofs).
chat_say_qp(yes) :- writeln('Yes.').
chat_say_qp(no) :- writeln('No.').
chat_say_qp(explanation(S, V, O, Proof)) :-
    chat_say(say, explanation(S, V, O, Proof)).
chat_say_qp(unknown) :- chat_unknown.

cap_list(Xs, K, Show, Rest) :-
    length(Xs, N),
    ( N =< K -> Show = Xs, Rest = 0
    ; length(Show, K), append(Show, _, Xs), Rest is N - K
    ).

show_rule_proofs(_, 0) :- !.
show_rule_proofs([], _) :- !.
show_rule_proofs([[rule(R, Path, _)|Steps]|T], K) :-
    format('  via rule ~w :- ~w~n', [R, Path]),
    show_steps(Steps),
    K1 is K - 1,
    show_rule_proofs(T, K1).
show_rule_proofs([_|T], K) :-
    show_rule_proofs(T, K).

show_steps([]).
show_steps([(A, R, B)|T]) :-
    ( prov(A, R, B, info(Ref, _, _)) ->
        format('    [~w] ~w --~w--> ~w~n', [Ref, A, R, B])
    ; format('    [noref] ~w --~w--> ~w~n', [A, R, B])
    ),
    show_steps(T).

remember_proof([[rule(_, _, _),(S, V, O)|_]|_]) :- !,
    chat_set_last(S, V, O).
remember_proof(_).

% ---- formas ----
% Sujeto multi-palabra: V = primer token con formas en memoria.
last_live_ent(Toks, E) :-
    reverse(Toks, Rev),
    member(W, Rev),
    pin_ent(W, E), !.

% Hueco verbal: formas vivas (stem/alias) o, si el par S-O tiene un
% solo predicado a distancia <= 2, ese (find~found). Sin lista.
did_split(Mid, S, V, O) :-
    nl_tokens(Mid, Packed),
    append(Pre, [Vw|Post], Packed),
    Pre \== [], Post \== [],
    qnorm(Pre, S),
    qnorm(Post, O),
    ( pin_rel(Vw, V), memory_relation(S, V, O, _, _)
    ; findall(R, memory_relation(S, R, O, _, _), R0),
      sort(R0, [V]),
      atom_length(Vw, L), L >= 3,
      symbol_dist(Vw, V, D), D =< 2
    ; pin_rel(Vw, V)
    ).

chat_form([who, V|Rest], say, answer(Xs, Facts)) :-
    qnorm(Rest, O),
    bb_rel_forms(V, Rs),
    findall(S-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), SF),
    SF \== [],
    findall(S, member(S-_, SF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, SF), Facts).
% who V E? but no one V's E: honest "no" (prevents graph fallback to outgoing).
chat_form([who, V|Rest], say, no) :-
    qnorm(Rest, O),
    O \== [],
    \+ (bb_rel_forms(V, Rs), member(Vr, Rs), memory_relation(_, Vr, O, _, _)).
chat_form([what, did|Mid], say, answer(Xs, Facts)) :-
    append(Pre, [V], Mid),
    Pre \== [],
    qnorm(Pre, S),
    bb_rel_forms(V, Rs),
    findall(O-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), OF),
    OF \== [],
    findall(O, member(O-_, OF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, OF), Facts).
% D7 how-many (M2 en dialogo): cuenta objetos distintos de (S, V).
% El resto nominal ("books") lo filtra qnorm; el sujeto manda.
chat_form([how, many|Mid], say, count(N)) :-
    nl_tokens(Mid, Packed),
    pins_of(Packed, Rels, Ents),
    Rels \== [], Ents = [E],
    findall(S, (member(R, Rels), memory_relation(S, R, E, _, _)), Ss0),
    sort(Ss0, Ss),
    Ss \== [],
    length(Ss, N).
chat_form([how, many|Mid], say, count(N)) :-
    live_books(Bs), Bs \== [],
    nl_tokens(Mid, [W]),
    book_type(T),
    inflect_same(W, T),
    length(Bs, N).
chat_form([how, many|Mid], say, count(N)) :-
    append(Pre, [V], Mid),
    Pre \== [],
    last_live_ent(Pre, S),
    bb_rel_forms(V, Rs),
    findall(O, (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), Os0),
    sort(Os0, Os),
    Os \== [],
    length(Os, N).
chat_form([did|Mid], say, YesNo) :-
    did_split(Mid, S, V, O),
    ( memory_relation(S, V, O, _, _) -> YesNo = yes((S, V, O))
    ; YesNo = no
    ).
chat_form([why, did|Mid], say, explanation(S, V, O, Proof)) :-
    did_split(Mid, S, V, O),
    proof_for(S, V, O, Proof).
chat_form([where, did|Mid], say, answer(Xs, Facts)) :-
    append(Pre, [V], Mid),
    Pre \== [],
    qnorm(Pre, S),
    bb_rel_forms(V, Rs),
    findall(L-(E, location, L),
            (member(Vr, Rs),
             memory_relation(S, actor, E, _, _),
             memory_relation(E, action, Vr, _, _),
             memory_relation(E, location, L, _, _)),
            LF),
    LF \== [],
    findall(L, member(L-_, LF), Xs0),
    sort(Xs0, Xs),
    findall((E, location, L), member(_-(E, location, L), LF), Facts).
chat_form([when, did|Mid], say, answer(Xs, Facts)) :-
    append(Pre, [V], Mid),
    Pre \== [],
    qnorm(Pre, S),
    bb_rel_forms(V, Rs),
    findall(Tm-(E, time, Tm),
            (member(Vr, Rs),
             memory_relation(S, actor, E, _, _),
             memory_relation(E, action, Vr, _, _),
             memory_relation(E, time, Tm, _, _)),
            LF),
    LF \== [],
    findall(Tm, member(Tm-_, LF), Xs0),
    sort(Xs0, Xs),
    findall((E, time, Tm), member(_-(E, time, Tm), LF), Facts).

% qnorm (nombre publico): por token, alias ensenado gana; si no,
% contenido propio; si nada resuelve, fallback difuso (Levenshtein
% =< 2, len >= 3, unico mejor o nada: nunca azar) y el join debe
% existir. La respuesta final exige tripla real: lo difuso propone,
% la evidencia dispone.
qnorm(Toks, Name) :-
    nl_tokens(Toks, Packed),
    Packed \== [],
    atomic_list_concat(Packed, '_', Name),
    bb_content(Name), !.
qnorm(Toks, Name) :-
    findall(W2, (member(W, Toks), fuzzy_keep(W, W2)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name),
    bb_content(Name),
    \+ member(Name, Toks),
    !.

% Alias ensenados ("puerta means door"): el mapeo gana al propio
% token (el mapeo ES el conocimiento; el hecho means sigue consultable
% directo). Sin triplas means, identico al exacto por construccion.
qnorm_atom(W, C) :-
    means_triple(W, C),
    bb_content(C), !.
qnorm_atom(W, W) :-
    bb_content(W), !.

fuzzy_keep(W, W) :- bb_content(W), !.
fuzzy_keep(W, C) :- fuzzy_one(W, C).
fuzzy_one(W, C) :-
    atom_chars(W, [F|Cs]),
    length([F|Cs], L),
    L >= 3,
    findall(D-S, (memory_symbol(S),
                  atom_chars(S, [F|_]),
                  atom_length(S, LS),
                  abs(LS - L) =< 2,
                  symbol_dist(W, S, D),
                  D =< 2), DS),
    keysort(DS, [D0-C|_]),
    \+ ( member(D1-C1, DS), C1 \== C, D1 =:= D0 ).

memory_symbol(S) :- memory_relation(S, _, _, _, _).
memory_symbol(S) :- memory_relation(_, _, S, _, _).

% Levenshtein por filas (DP lineal en el producto).
symbol_dist(A, B, D) :-
    atom_chars(A, CA),
    atom_chars(B, CB),
    length(CB, N),
    numlist(0, N, R0),
    lev_loop(CA, CB, R0, D).

lev_loop([], _, Row, D) :- last(Row, D), !.
lev_loop([A|As], B, Prev, D) :-
    Prev = [P0|_],
    C0 is P0 + 1,
    lev_cells(A, B, Prev, C0, Tail),
    lev_loop(As, B, [C0|Tail], D).

lev_cells(_, [], _, _, []).
lev_cells(A, [B|Bs], [Dg, Up|Rest], Left, [C|Cs]) :-
    ( A == B -> Cost = 0 ; Cost = 1 ),
    C is min(Up + 1, min(Left + 1, Dg + Cost)),
    lev_cells(A, Bs, [Up|Rest], C, Cs).

% Puente morfologico bidireccional (igual que ask.pl), con desdoblado
% simetrico de consonante final (trimmed->trimm->trim): f aplicada en
% ambos lados conserva todos los matches previos por construccion
% (si a==b entonces f(a)==f(b)) y anade la clase stopped/dropped.
% D9 lexico aprendido: triplas (A,means|significa,C) ensenadas declaran
% alias de verbo ("abrio means opened"); se resuelven un nivel, sin
% cadenas. Sin triplas means, identico a antes por construccion.
bb_rel_forms(V, Rs) :-
    bb_rel_forms_direct(V, Rs0),
    findall(R, (means_triple(V, C),
                bb_rel_forms_direct(C, RC), member(R, RC)), Rs1),
    findall(R, (irregular_form(V, Base),
                bb_rel_forms_direct(Base, RB), member(R, RB)), Rs2),
    append([Rs0, Rs1, Rs2], Rall),
    sort(Rall, Rs),
    Rs \== [].

means_triple(V, C) :-
    memory_relation(V, means, C, _, _).
means_triple(V, C) :-
    memory_relation(V, significa, C, _, _).
% love hereda loves means ama: mismo stem o +s (bb_stem('es')
% recorta loves a lov). Sufijos cerrados, no lexico.
means_triple(V, C) :-
    memory_relation(A, Rel, C, _, _),
    ( Rel == means ; Rel == significa ),
    A \== V,
    inflect_same(V, A).

% Irregular verb forms: past/participle → base form. Closed list.
% Allows "ate"→"eat", "went"→"go", etc. in bb_rel_forms resolution.
irregular_form(ate, eat).
irregular_form(went, go).
irregular_form(saw, see).
irregular_form(found, find).
irregular_form(got, get).
irregular_form(made, make).
irregular_form(said, say).
irregular_form(took, take).
irregular_form(gave, give).
irregular_form(knew, know).
irregular_form(thought, think).
irregular_form(came, come).
irregular_form(ran, run).
irregular_form(led, lead).
irregular_form(felt, feel).
irregular_form(brought, bring).
irregular_form(began, begin).
irregular_form(kept, keep).
irregular_form(held, hold).
irregular_form(wrote, write).
irregular_form(stood, stand).
irregular_form(heard, hear).
irregular_form(paid, pay).
irregular_form(met, meet).
irregular_form(ate, eat).
irregular_form(drunk, drink).
irregular_form(drank, drink).
irregular_form(entered, enter).
irregular_form(followed, follow).
irregular_form(defied, defy).
irregular_form(peeped, peep).

inflect_same(A, B) :-
    bb_stem(A, S0), bb_ddouble(S0, X),
    bb_stem(B, S1), bb_ddouble(S1, X).
inflect_same(A, B) :-
    ( atom_concat(A, s, B) ; atom_concat(B, s, A)
    ; atom_concat(A, es, B) ; atom_concat(B, es, A)
    ; atom_concat(A, ed, B) ; atom_concat(B, ed, A)
    ; atom_concat(A, ing, B) ; atom_concat(B, ing, A)
    ).

bb_rel_forms_direct(V, Rs) :-
    bb_stem(V, St0),
    bb_ddouble(St0, St),
    findall(R, (memory_relation(_, R, _, _, _),
                ( R == V ; (bb_stem(R, RSt0), bb_ddouble(RSt0, RSt),
                            RSt == St, R \== V) )), R0),
    sort(R0, Rs).

% bb_ddouble: una de dos letras finales iguales fuera (ingles: la
% consonante se dobla ante -ed/-ing; call/full/well nunca se tocan
% porque el desdoblado es simetrico en la comparacion).
bb_ddouble(W, D) :-
    sub_atom(W, _, 1, 0, C),
    sub_atom(W, _, 1, 1, C), !,
    sub_atom(W, 0, _, 1, D).
bb_ddouble(W, W).

bb_stem(W, St) :-
    ( sub_atom(W, _, 3, 0, 'ied') ->
        sub_atom(W, 0, _, 3, Pre), atom_concat(Pre, 'y', St)
    ; sub_atom(W, _, 3, 0, 'ies') ->
        sub_atom(W, 0, _, 3, Pre2), atom_concat(Pre2, 'y', St)
    ; sub_atom(W, _, 2, 0, 'ed') ->
        sub_atom(W, 0, _, 2, St)
    ; sub_atom(W, _, 3, 0, 'ing') ->
        sub_atom(W, 0, _, 3, St)
    ; sub_atom(W, _, 2, 0, 'es') ->
        sub_atom(W, 0, _, 2, St)
    ; ( sub_atom(W, _, 1, 0, 's') ->
          sub_atom(W, 0, _, 1, St0), St = St0
      ; St = W
      )
    ).

% ---- salida ----
:- discontiguous chat_say/2.
chat_say(say, answer(Xs, Facts)) :-
    ( Facts \== [] ->
        cap_list(Facts, 5, Show, Rest),
        forall(member(T, Show), (surface_sent(T, Line), writeln(Line))),
        ( Rest == 0 -> true
        ; format('... (and ~w more)~n', [Rest])
        ),
        length(Show, Shown)
    ; length(Xs, N),
      ( N =< 5 -> Show = Xs, Rest = 0
      ; length(Show, 5), append(Show, _, Xs), Rest is N - 5
      ),
      atomic_list_concat(Show, ', ', L),
      ( Rest == 0 -> format('~w.~n', [L])
      ; format('~w... (and ~w more)~n', [L, Rest])
      ),
      length(Show, Shown)
    ),
    retractall(dialog_lastlist(_)),
    ( Facts \== [] -> assertz(dialog_lastlist(Facts))
    ; assertz(dialog_lastlist(Xs))
    ),
    retractall(dialog_lastoff(_)),
    assertz(dialog_lastoff(Shown)),
    chat_remember(Xs, Facts),
    chat_sources(Facts).

% Superficie SVO desde el grafo: los '_' del simbolo son espacios
% (white_rabbit -> "white rabbit"). Sin lexico extra.
% C3: la frase ES la tripla. phrase/2 genera y parsea. Los terminales
% salen del mapa vivo (simbolo partido por '_'), nunca de una lista.
surface_sent((S, V, O), Line) :-
    ( phrase(svo(S, V, O), Toks) ->
        atomic_list_concat(Toks, ' ', Body),
        atom_concat(Body, '.', Line)
    ; surface_atom(S, Ss),
      surface_atom(V, Vs),
      surface_atom(O, Os),
      format(atom(Line), '~w ~w ~w.', [Ss, Vs, Os])
    ).

surface_atom(A, Out) :-
    atomic_list_concat(Parts, '_', A),
    atomic_list_concat(Parts, ' ', Out).

parse_svo_line(Line, (S, V, O)) :-
    gen_tokenize(Line, Toks, _),
    exclude(gen_qmark, Toks, T0),
    T0 \== [],
    phrase(svo(S, V, O), T0).

% phrase/2 llama svo/5 (NT + dos extra). Sin { }/1: el preflight
% no admite el wrap DCG de llaves, y los terminales siguen saliendo
% del mapa (span_words), no de una lista.
svo(S, V, O, A, D) :-
    map_span(S, A, B),
    map_span(V, B, C),
    map_span(O, C, D).

map_span(Sym, A, C) :-
    span_words(Sym, Ws),
    dcg_toks(Ws, A, C).

span_words(Sym, Ws) :-
    nonvar(Sym), !,
    bb_content(Sym),
    atomic_list_concat(Ws, '_', Sym).
span_words(Sym, Ws) :-
    findall(N-S-W,
            ( live_symbol(S),
              atomic_list_concat(W, '_', S),
              length(W, N) ),
            All0),
    sort(All0, All),
    sort(0, @>=, All, Sorted),
    member(_-Sym-Ws, Sorted).

% Sin el corte de bb_content/1: hay que enumerar el mapa para parsear.
live_symbol(S) :- memory_relation(S, _, _, _, _).
live_symbol(S) :- memory_relation(_, S, _, _, _).
live_symbol(S) :- memory_relation(_, _, S, _, _).

dcg_toks([W], [W|R], R).
dcg_toks([W|Rest], [W|T], R) :-
    dcg_toks(Rest, T, R).

% D7 more: continua la ultima lista plana (las razonadas muestran sus
% 3 pruebas y ahi terminan: "Nothing more.").
dialog_more :-
    dialog_lastlist(Items),
    dialog_lastoff(K),
    length(Items, N),
    K < N, !,
    Want is min(K + 5, N) - K,
    length(Prefix, K),
    append(Prefix, Rest, Items),
    length(Show, Want),
    append(Show, _, Rest),
    K2 is K + Want,
    more_show(Show, K2, N),
    retractall(dialog_lastoff(_)),
    assertz(dialog_lastoff(K2)).
dialog_more :-
    writeln('Nothing more.').

more_show(Show, K2, N) :-
    Show = [(_, _, _)|_], !,
    forall(member(T, Show), (surface_sent(T, Line), writeln(Line))),
    ( K2 < N -> R is N - K2, format('... (and ~w more)~n', [R]) ; true ).
more_show(Show, K2, N) :-
    atomic_list_concat(Show, ', ', L),
    ( K2 < N -> R is N - K2, format('~w... (and ~w more)~n', [L, R])
    ; format('~w.~n', [L])
    ).

% D7 how-many: cuenta objetos distintos de (S, V) (M2 en dialogo).
chat_say(say, count(N)) :-
    format('~w.~n', [N]).
chat_say(say, yes((S, V, O))) :-
    writeln('Yes.'),
    surface_sent((S, V, O), Line),
    writeln(Line),
    chat_set_last(S, V, O),
    chat_sources([(S, V, O)]).
chat_say(say, yes_both((S1, V1, O1), (S2, V2, O2))) :-
    writeln('Yes.'),
    surface_sent((S1, V1, O1), L1), writeln(L1),
    surface_sent((S2, V2, O2), L2), writeln(L2),
    chat_set_last(S2, V2, O2),
    chat_sources([(S1, V1, O1), (S2, V2, O2)]).
chat_say(say, no) :-
    writeln('No.').
chat_say(say, yes_bare) :-
    writeln('Yes.').
chat_say(say, nobody) :-
    writeln('No one else.').
chat_say(say, explanation(S, Vr, O, Proof)) :-
    surface_sent((S, Vr, O), Line),
    writeln(Line),
    chat_set_last(S, Vr, O),
    format('  via ~w~n', [Proof]),
    chat_sources([(S, Vr, O)]).
chat_say(_, unknown) :-
    chat_unknown.

chat_remember(Xs, Facts) :-
    ( Facts = [(S, V, O)|_] -> chat_set_last(S, V, O)
    ; Xs = [X] -> chat_set_last(X, _, _)
    ; true
    ).

chat_set_last(S, V, O) :-
    retractall(last_fact(_, _, _)),
    assertz(last_fact(S, V, O)),
    dialog_note([S], subj),
    dialog_note([O], obj).

chat_sources(Facts) :-
    cap_list(Facts, 5, Show, Rest),
    chat_sources_show(Show),
    ( Rest == 0 -> true
    ; format('  ... (~w more)~n', [Rest])
    ).

chat_sources_show([]).
chat_sources_show([(S, R, O)|T]) :-
    ( prov(S, R, O, info(Ref, _, _)) ->
        format('  [~w] ~w --~w--> ~w~n', [Ref, S, R, O])
    ; format('  [noref] ~w --~w--> ~w~n', [S, R, O])
    ),
    chat_sources_show(T).
