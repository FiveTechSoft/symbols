% bookbrain/chat.pl — chat libre sobre document.knowledge.
% Uso: swipl -s chat.pl -g "chat('alice.knowledge.pl')" -t halt
% (o entubando preguntas por stdin). Formas: who/what/did/why/where/when,
% "why?" solo (sigue a la ultima respuesta), "help", "quit".
% Estado de dialogo: last_fact/3. Ante lo desconocido: UNKNOWN (nunca inventa).
% Multi-respuesta capada a 5 + resto contado. Sin caidas: todo catch.
:- consult('corpus.pl').
:- consult('question_parser.pl').
:- consult('english_graph.pl').
:- consult('positional.pl').
:- consult('dialog_ref.pl').

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

chat(File) :-
    bb_load(File),
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
    dialog_reset,
    dialog_reset,
    consult(File),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    forall(provfact(S, R, O, Ref, T, St), assertz(prov(S, R, O, info(Ref, T, St)))),
    memory_size(NF),
    format('Loaded ~w facts from ~w.~n', [NF, File]).

% save [nombre]: vuelca la memoria viva (base + lo ensenado en sesion) al
% mismo formato memfact/provfact que bb_load/1 lee. Sin nombre -> session.
% Solo nombres simples (sin rutas): el save nunca sale de esta carpeta.
chat_save_cmd([save, Name|_]) :-
    atom(Name), Name \== save, Name \== '..',
    \+ sub_atom(Name, _, _, _, '/'),
    \+ sub_atom(Name, _, _, _, '\\'),
    chat_save(Name), !.

chat_save(Alias) :-
    atom_concat(Alias, '.knowledge.pl', Out),
    open(Out, write, S),
    forall(memory_relation(A, R, O, W, U),
           format(S, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
    forall(prov(A, R, O, info(Ref, T, St)),
           format(S, 'provfact(~q,~q,~q,~q,~q,~q).~n', [A, R, O, Ref, T, St])),
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
    ( L == end_of_file -> writeln('Bye.')
    ; gen_tokenize(L, Lower, _),
      exclude(gen_qmark, Lower, T0),
      ( T0 == [quit] ; T0 == [exit] ; T0 == [bye] ) -> writeln('Bye.')
    ; chat_line(L),
      chat_loop
    ).

chat_line(L) :-
    catch(chat_line_guarded(L), E, format('UNKNOWN (~w).~n', [E])).

chat_line_guarded(L) :-
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
    format('Learned [~w]: ~w --~w--> ~w.~n', [Src, S, V, O]),
    chat_note_told(V),
    chat_set_last(S, V, O).

% D4 iniciativa (patron EXP48): asercion de 2 tokens [Sujeto, Verbo]
% con S conocido y V con familia en memoria = slot objeto ausente. El
% modelo pregunta ("What did S V?") y la siguiente linea de una palabra
% completa el hecho. Sin el par, camino normal (UNKNOWN honesto).
dialog_probe_missing([S, V]) :-
    qnorm([S], S),
    bb_rel_forms(V, _),
    bb_stem(V, VB),
    format('What did ~w ~w?~n', [S, VB]),
    assertz(dialog_pending_teach(S, V)).

chat_line_tokens(L, Toks, Changed) :-
    ( Toks == [] -> true
    ; Toks == [quit] -> writeln('Bye.')
    ; Toks == [exit] -> writeln('Bye.')
    ; Toks == [bye] -> writeln('Bye.')
    ; Toks == [help] -> chat_help
    ; Toks == [why] -> chat_why_bare
    ; Toks == [more] -> dialog_more
    ; Toks == [mas] -> dialog_more
    ; Toks == [the, rest] -> dialog_more
    ; Toks == [save] -> chat_save(session)
    ; chat_save_cmd(Toks) -> true
    ; Toks == [discover] -> chat_discover_all
    ; chat_discover_cmd(Toks) -> true
    ; chat_greet(Toks) -> true
    ; chat_about(Toks) -> true
    ; Toks == [who, are, you] -> chat_identity(en)
    ; Toks == [what, are, you] -> chat_identity(en)
    ; Toks == [quien, eres] -> chat_identity(es)
    ; Toks == [quienes, somos] -> chat_identity(es)
    ; ( Toks == [who, do, you, know] ; Toks == [whom, do, you, know] ) ->
        chat_known(en)
    ; ( Toks == [quien, conoces] ; Toks == [a, quien, conoces] ) ->
        chat_known(es)
    ; chat_is_spanish(Toks) -> chat_spanish_help
    ; is_question(L) ->
        ( dialog_meta_es(Toks) -> true
        ; dialog_single_about(Toks) -> true
        ; dialog_ambiguity(Toks, P, Cs) ->
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
dialog_meta_es(Toks) :-
    member(W, Toks),
    member(W, [sabes, trata, protagonistas]), !,
    chat_known(es).

% D5 entidad suelta con '?': resumen (about). Sin '?' va a learn
% (D3-abandon intacto: "Madrid" sin '?' sigue unknown).
dialog_single_about([X]) :-
    qnorm([X], X), !,
    about_entity(X, en).

% D3: respuesta = entidad candidata (qnorm) -> prosigue; si no, nada.
dialog_match_reply(Toks, Cs, C) :-
    qnorm(Toks, C),
    member(C, Cs).

dl_repl(P, C, W, O) :- ( W == P -> O = C ; O = W ).

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
    ( dialog_has_pronoun(Toks) ->
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
    ; dialog_probe_missing(Toks) -> true
    ; learn_sentence(LS, stored(A, V, O, Src)) ->
        format('Learned [~w]: ~w --~w--> ~w.~n', [Src, A, V, O]),
        chat_note_told(V),
        chat_set_last(A, V, O)
    ; learn_sentence(LS, stored_identity(X, Y, _)) ->
        format('Learned: ~w is ~w.~n', [X, Y])
    ; chat_learn_positional(LS)
    ; chat_ask(Toks)
    ).

chat_learn_positional(L) :-
    ( string(L) -> S = L ; atom_string(L, S) ),
    sub_string(S, _, _, 0, "."),
    gen_tokenize(S, T0, _),
    T0 = [F | _],
    \+ qlead(F),
    positional_triple(T0, A, V, O),
    next_sentence_id(Src),
    remember_tracked(A, V, O, Src, none),
    format('Learned [~w]: ~w --~w--> ~w (by structure).~n', [Src, A, V, O]),
    chat_note_told(V),
    chat_set_last(A, V, O).

% SVO estructural con determinantes fuera SOLO del tramo intermedio:
% primero = sujeto y ultimo = objeto siempre se conservan ("LA" como
% objeto en MIEMBRO_DE LA es contenido). Sin tramo -> falla honesto.
positional_triple(Toks, A, V, O) :-
    append([A | Mid], [O], Toks),
    Mid \== [],
    exclude(is_determiner, Mid, Mid2),
    Mid2 \== [],
    atomic_list_concat(Mid2, '_', V).

% Determinantes cerrados EN/ES fuera del SVO estructural (sintaxis;
% las preposiciones se conservan: portan significado).
is_determiner(W) :- member(W, [the, a, an, el, la, los, las, un, una]).

chat_note_told(V) :-
    ( told_rel(V) -> true ; assertz(told_rel(V)) ).

% Guardia anti-preguntas: la via estructural jamas ingiere forma
% interrogativa ("Who reaches Oslo." no es un hecho). Lista cerrada de
% auxiliares/interrogativos = sintaxis (como '?' o '.'), nunca lexico
% de contenido: jamas puede ser sujeto de una declarativa.
qlead(W) :- member(W, [who,what,when,where,why,how,
                       did,does,do,is,are,was,were]), !.

% Deteccion de español sin listas: el corpus es ingles ASCII; una entrada
% con caracteres no-ASCII (tildes, ñ, ¿¡) es casi seguro español.
% Sin tildes cae a UNKNOWN honesto, sin inventar.
chat_is_spanish(Toks) :-
    member(W, Toks),
    atom_chars(W, Cs),
    member(C, Cs),
    char_code(C, N), N > 127, !.

chat_spanish_help :-
    writeln('De momento solo entiendo preguntas en ingles, porque el libro esta en ingles.'),
    writeln('Prueba por ejemplo:'),
    writeln('  Who opened door?'),
    writeln('  What did footman open?'),
    writeln('  Why did Alice open door?'),
    writeln('  Who do you know?').

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
    about_target(Toks, Lang, Name),
    about_entity(Name, Lang).

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
    ( N == 0 ->
        ( Lang == es ->
            format('No se casi nada de ~w.~n', [Name])
        ; format('I know almost nothing about ~w.~n', [Name])
        )
    ; ( Lang == es ->
            format('Esto se de ~w (~w hechos):~n', [Name, N])
      ; format('This is what I know about ~w (~w facts):~n', [Name, N])
      ),
      show_facts(Facts, 8),
      about_sources(Name),
      chat_set_last(Name, _, _)
    ).

show_facts(_, 0) :- !.
show_facts([], _) :- !.
show_facts([(R, O)|T], K) :-
    format('  - ~w -> ~w~n', [R, O]),
    K1 is K - 1,
    show_facts(T, K1).

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
    writeln('who <verb> <obj>? | what did <s> <verb>? | did <s> <verb> <obj>?'),
    writeln('why did <s> <verb> <obj>? | where did <s> <verb>? | when did <s> <verb>?'),
    writeln('Teach me facts ending with a period: Zorin reaches Oslo.'),
    writeln('he/she/it follow the conversation (one clear antecedent);'),
    writeln('with several antecedents I ask who you mean (answer a name).'),
    writeln('"And Madrid?" continues the last question (evidence or unknown).'),
    writeln('unfinished teaching ("Ana opened.") gets asked back once.'),
    writeln('En español: ensena "Ana abrio puerta." y pregunta "Quien abrio puerta?".'),
    writeln('more (see the rest of long lists) | how many (count).'),
    writeln('ambiguous pronouns get an honest unknown, never a guess.'),
    writeln('discover [relation] (find rules in what you taught me)'),
    writeln('save [name] (keep session memory) | why? (about last answer) | quit').

chat_why_bare :-
    ( last_fact(S, V, O) ->
        ( proof_for(S, V, O, Proof) ->
            format('Because: ~w --~w--> ~w.~n', [S, V, O]),
            chat_sources([(S, V, O)]),
            format('  via ~w~n', [Proof])
        ; writeln("I don't know why.")
        )
    ; writeln('Nothing to explain yet. Ask something first.')
    ).

chat_ask(Toks) :-
    ( dialog_has_pronoun(Toks) -> writeln("I don't know.")
    ; chat_form(Toks, Kind, Ans) ->
        retractall(dialog_lastq(_)),
        assertz(dialog_lastq(Toks)),
        chat_say(Kind, Ans)
    ; es_form(Toks, Kind, Ans) ->
        retractall(dialog_lastq(_)),
        assertz(dialog_lastq(Toks)),
        chat_say_es(Kind, Ans)
    ; chat_ask_qp(Toks)
    ).

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
es_form([V, S, O], say, YesNo) :-
    V \== quien, V \== que,
    bb_content(S), bb_content(O),
    bb_rel_forms(V, Rs),
    member(Vr, Rs),
    ( memory_relation(S, Vr, O, _, _) -> YesNo = yes((S, Vr, O))
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
    writeln('No lo sé.').

% D2 elipsis: "And Madrid?" continua el esqueleto anterior con X en el
% slot (who/did/why/where/when: ultimo; what: sujeto). Solo formas con
% marcador (and | what/how about) y X entidad unica: disjunto de las
% formas completas por construccion. Sonda con evidencia (probe) o se
% deja al camino normal (UNKNOWN honesto). Caso especial: who + X
% persona sin objeto -> verificar candidato (Did X V O?).
% Tras responder, chat_ask actualiza LastQ: encadena ("And Oslo?").
dialog_ellipsis([and, X]) :- !, dialog_ell_entity(X).
dialog_ellipsis([what, about, X]) :- !, dialog_ell_entity(X).
dialog_ellipsis([how, about, X]) :- !, dialog_ell_entity(X).
dialog_ellipsis(_) :- fail.

dialog_ell_entity(X) :-
    qnorm([X], X),
    dialog_lastq(LQ),
    dialog_ell_build(LQ, X, NewT),
    ( dialog_probe(NewT) -> chat_ask(NewT)
    ; dialog_ell_verify(LQ, X)
    ).

% Verificar-candidato: who [who,V,O] + X persona -> Did X V O?
dialog_ell_verify([who, V, O], X) :-
    dialog_in_subj(X),
    dialog_probe([did, X, V, O]), !,
    chat_ask([did, X, V, O]).

dialog_ell_build([who, V, _O], X, [who, V, X]).
dialog_ell_build([what, did, _S, V], X, [what, did, X, V]).
dialog_ell_build([did, S, V, _O], X, [did, S, V, X]).
dialog_ell_build([why, did | Mid], X, [why, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).
dialog_ell_build([where, did | Mid], X, [where, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).
dialog_ell_build([when, did | Mid], X, [when, did | Mid2]) :-
    append(Pre, [_], Mid),
    append(Pre, [X], Mid2).

% Sonda: la forma resuelve con respuestas (yes/no son compuestos con
% la tripla; no, solo de did, es respuesta cerrada genuina porque las
% entidades vienen resueltas del esqueleto o de qnorm).
dialog_probe(T) :-
    ( chat_form(T, _, answer(Xs, _)) -> Xs \== []
    ; chat_form(T, _, yes(_)) -> true
    ; chat_form(T, _, no) -> true
    ; chat_form(T, _, explanation(_, _, _, _)) -> true
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
    ; writeln("I don't know.")
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
chat_say_qp(unknown) :- writeln("I don't know.").

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
did_split(Mid, S, V, O) :-
    append(Pre, [V|Post], Mid),
    Pre \== [], Post \== [],
    qnorm(Pre, S),
    qnorm(Post, O),
    bb_rel_forms(V, _).

chat_form([who, V|Rest], say, answer(Xs, Facts)) :-
    qnorm(Rest, O),
    bb_rel_forms(V, Rs),
    findall(S-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), SF),
    SF \== [],
    findall(S, member(S-_, SF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, SF), Facts).
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
    append(Pre, [V], Mid),
    Pre \== [],
    qnorm(Pre, S),
    bb_rel_forms(V, Rs),
    findall(O, (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), Os0),
    sort(Os0, Os),
    Os \== [],
    length(Os, N).
chat_form([did|Mid], say, YesNo) :-
    ( did_split(Mid, S, V, O),
      bb_rel_forms(V, Rs),
      member(Vr, Rs),
      memory_relation(S, Vr, O, _, _) ->
        YesNo = yes((S, Vr, O))
    ; YesNo = no
    ).
chat_form([why, did|Mid], say, explanation(S, Vr, O, Proof)) :-
    did_split(Mid, S, V, O),
    bb_rel_forms(V, Rs),
    member(Vr, Rs),
    proof_for(S, Vr, O, Proof).
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

% qnorm_exact: quita glue, une con _. Solo sobrevive lo que el mapa conoce.
qnorm_exact(Toks, Name) :-
    findall(W, (member(W, Toks), bb_content(W)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name).
% qnorm (nombre publico): exacto primero (cero cambio de conducta), cada
% token a su simbolo mas cercano (Levenshtein =< 2, len >= 3, unico
% mejor o nada: nunca azar) y el join debe existir. La respuesta final
% exige tripla real: lo difuso propone, la evidencia dispone.
qnorm(Toks, Name) :-
    qnorm_exact(Toks, Name), !.
qnorm(Toks, Name) :-
    findall(W2, (member(W, Toks), fuzzy_keep(W, W2)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name),
    bb_content(Name),
    format('(assuming ~w)~n', [Name]).

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
bb_rel_forms(V, Rs) :-
    bb_stem(V, St0),
    bb_ddouble(St0, St),
    findall(R, (memory_relation(_, R, _, _, _),
                ( R == V ; (bb_stem(R, RSt0), bb_ddouble(RSt0, RSt),
                            RSt == St, R \== V) )), R0),
    sort(R0, Rs),
    Rs \== [].

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
chat_say(say, answer(Xs, Facts)) :-
    length(Xs, N),
    ( N =< 5 -> Show = Xs, Rest = 0
    ; length(Show, 5), append(Show, _, Xs), Rest is N - 5
    ),
    atomic_list_concat(Show, ', ', L),
    ( Rest == 0 -> format('~w.~n', [L])
    ; format('~w... (and ~w more)~n', [L, Rest])
    ),
    length(Show, Shown),
    retractall(dialog_lastlist(_)),
    assertz(dialog_lastlist(Xs)),
    retractall(dialog_lastoff(_)),
    assertz(dialog_lastoff(Shown)),
    chat_remember(Xs, Facts),
    chat_sources(Facts).

% D7 more: continua la ultima lista plana (las razonadas muestran sus
% 3 pruebas y ahi terminan: "Nothing more.").
dialog_more :-
    dialog_lastlist(Xs),
    dialog_lastoff(K),
    length(Xs, N),
    K < N, !,
    Want is min(K + 5, N) - K,
    length(Prefix, K),
    append(Prefix, Rest, Xs),
    length(Show, Want),
    append(Show, _, Rest),
    K2 is K + Want,
    atomic_list_concat(Show, ', ', L),
    ( K2 < N -> R is N - K2, format('~w... (and ~w more)~n', [L, R])
    ; format('~w.~n', [L])
    ),
    retractall(dialog_lastoff(_)),
    assertz(dialog_lastoff(K2)).
dialog_more :-
    writeln('Nothing more.').

% D7 how-many: cuenta objetos distintos de (S, V) (M2 en dialogo).
chat_say(say, count(N)) :-
    format('~w.~n', [N]).
chat_say(say, yes((S, V, O))) :-
    writeln('Yes.'),
    chat_set_last(S, V, O),
    chat_sources([(S, V, O)]).
chat_say(say, no) :-
    writeln('No.').
chat_say(say, explanation(S, Vr, O, Proof)) :-
    format('~w --~w--> ~w.~n', [S, Vr, O]),
    chat_set_last(S, Vr, O),
    format('  via ~w~n', [Proof]),
    chat_sources([(S, Vr, O)]).
chat_say(_, unknown) :-
    writeln("I don't know.").

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

chat_sources([]).
chat_sources([(S, R, O)|T]) :-
    ( prov(S, R, O, info(Ref, _, _)) ->
        format('  [~w] ~w --~w--> ~w~n', [Ref, S, R, O])
    ; format('  [noref] ~w --~w--> ~w~n', [S, R, O])
    ),
    chat_sources(T).
