% bookbrain/chat.pl — chat libre sobre document.knowledge.
% Uso: swipl -s chat.pl -g "chat('alice.knowledge.pl')" -t halt
% (o entubando preguntas por stdin). Formas: who/what/did/why/where/when,
% "why?" solo (sigue a la ultima respuesta), "help", "quit".
% Estado de dialogo: last_fact/3. Ante lo desconocido: UNKNOWN (nunca inventa).
% Multi-respuesta capada a 5 + resto contado. Sin caidas: todo catch.
:- consult('../corpus.pl').
:- consult('../question_parser.pl').
:- consult('../gen_parse.pl').

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
    ; chat_ask(Toks)
    ).

gen_qmark('?').

chat_help :-
    writeln('who <verb> <obj>? | what did <s> <verb>? | did <s> <verb> <obj>?'),
    writeln('why did <s> <verb> <obj>? | where did <s> <verb>? | when did <s> <verb>?'),
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

% qnorm: quita cerradas, une con _.
qnorm(Toks, Name) :-
    findall(W, (member(W, Toks), \+ gen_closed(W)), Ws),
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
