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

% Interfaz de datos: la aporta el .knowledge.pl que bb_load/1 consulta en
% runtime (memfact/5 hechos, provfact/6 provenance). Declarada, no definida.
:- dynamic memfact/5.
:- dynamic provfact/6.

% Shim del dialecto bookbrain (gen_parse.pl no existe en este arbol):
% delega en el tokenizador del motor. Infraestructura, no lexico.
gen_tokenize(S, Toks, _) :-
    ( string(S) -> S2 = S ; atom_string(S, S2) ),
    tokenize_en(S2, Toks).
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

chat(File) :-
    bb_load(File),
    writeln('BookBrain chat. Ask me anything (quit to exit).'),
    chat_loop.

bb_load(File) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    retractall(last_fact(_, _, _)),
    consult(File),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    forall(provfact(S, R, O, Ref, T, St), assertz(prov(S, R, O, info(Ref, T, St)))),
    memory_size(NF),
    format('Loaded ~w facts from ~w.~n', [NF, File]).

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
    exclude(gen_qmark, Lower, Toks),
    ( Toks == [] -> true
    ; Toks == [quit] -> writeln('Bye.')
    ; Toks == [exit] -> writeln('Bye.')
    ; Toks == [bye] -> writeln('Bye.')
    ; Toks == [help] -> chat_help
    ; Toks == [why] -> chat_why_bare
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
    ; is_question(L) -> chat_ask(Toks)
    ; chat_learn(L, Toks)
    ).

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
chat_learn(L, Toks) :-
    ( chat_form(Toks, _, _) -> chat_ask(Toks)
    ; learn_sentence(L, stored(A, V, O, Src)) ->
        format('Learned [~w]: ~w --~w--> ~w.~n', [Src, A, V, O]),
        chat_set_last(A, V, O)
    ; learn_sentence(L, stored_identity(X, Y, _)) ->
        format('Learned: ~w is ~w.~n', [X, Y])
    ; chat_learn_positional(L)
    ; chat_ask(Toks)
    ).

chat_learn_positional(L) :-
    ( string(L) -> S = L ; atom_string(L, S) ),
    sub_string(S, _, _, 0, "."),
    gen_tokenize(S, Toks, _),
    Toks = [F|_],
    \+ qlead(F),
    symbolize_text(S, (A, V, O)),
    next_sentence_id(Src),
    remember_tracked(A, V, O, Src, none),
    format('Learned [~w]: ~w --~w--> ~w (by structure).~n', [Src, A, V, O]),
    chat_set_last(A, V, O).

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

run_len(H, [], 1, []).
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
    writeln('Teach me facts without "?": Alice visits Paris. (I learn them)'),
    writeln('why? (about last answer) | quit').

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
    ( chat_form(Toks, Kind, Ans) -> true ; Kind = say, Ans = unknown ),
    chat_say(Kind, Ans).

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

% qnorm: quita glue, une con _. Solo sobrevive lo que el mapa conoce.
qnorm(Toks, Name) :-
    findall(W, (member(W, Toks), bb_content(W)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name).

% Puente morfologico bidireccional (igual que ask.pl).
bb_rel_forms(V, Rs) :-
    bb_stem(V, St),
    findall(R, (memory_relation(_, R, _, _, _),
                ( R == V ; (bb_stem(R, St), R \== V) )), R0),
    sort(R0, Rs),
    Rs \== [].

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
    chat_remember(Xs, Facts),
    chat_sources(Facts).
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
    assertz(last_fact(S, V, O)).

chat_sources([]).
chat_sources([(S, R, O)|T]) :-
    ( prov(S, R, O, info(Ref, _, _)) ->
        format('  [~w] ~w --~w--> ~w~n', [Ref, S, R, O])
    ; format('  [noref] ~w --~w--> ~w~n', [S, R, O])
    ),
    chat_sources(T).
