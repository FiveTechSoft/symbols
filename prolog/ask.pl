% bookbrain/ask.pl — ASK general sobre document.knowledge.
% Uso desde bookbrain/: swipl -s ask.pl -g "askq(alice, 'Who opened door?')" -t halt
% Formas (verbos en superficie, sin canonizar):
%   Who <V> <OBJ>?     -> inversa + pruebas + fuentes
%   What did <S> <V>?  -> directa + pruebas + fuentes
%   Did <S> <V> <OBJ>? -> yes + prueba / no
%   Why did <S> <V> <OBJ>? -> explicacion + fuentes
% Sin evidencia: UNKNOWN (nunca inventa). Reutiliza verbalize/proof_for.
:- consult('corpus.pl').
:- consult('question_parser.pl').
:- consult('english_graph.pl').

% Interfaz de datos del .knowledge.pl cargado en runtime por
% bb_load_knowledge/1. Declarada, no definida.
:- dynamic memfact/5.
:- dynamic provfact/6.

% Shim del dialecto bookbrain (ver chat.pl): delega en tokenize_en/2
% + pliega tildes (misma tabla, misma licencia).
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
% Sin listas cerradas hardcodeadas: contenido = simbolo del mapa vivo.
bb_content(W) :-
    ( memory_relation(W, _, _, _, _)
    ; memory_relation(_, W, _, _, _)
    ; memory_relation(_, _, W, _, _)
    ), !.

:- use_module(library(lists)).

:- discontiguous ask_evidence/1.

bb_load_knowledge(File) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    consult(File),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    forall(provfact(S, R, O, Ref, T, St), assertz(prov(S, R, O, info(Ref, T, St)))),
    memory_size(NF),
    format('ASK loaded facts=~w from ~w~n', [NF, File]).

% askq(+Alias, +Question): Alias solo etiqueta la salida.
askq(Alias, Q) :-
    gen_tokenize(Q, Lower, _),
    exclude(gen_qmark, Lower, Toks),
    ( ask_form(Toks, Ans) -> true ; Ans = unknown ),
    bb_say(Alias, Q, Ans),
    ask_evidence(Ans).

% Presentacion propia (las clausulas especificas de verbalize/3 exigen
% pregunta ligada; aqui solo la generica de explanation es segura).
bb_say(Alias, Q, answer([X], _, _)) :-
    format('~w Q: ~w~nA: ~w.~n', [Alias, Q, X]).
bb_say(Alias, Q, answer(Xs, _, _)) :-
    atomic_list_concat(Xs, ', ', L),
    format('~w Q: ~w~nA: ~w.~n', [Alias, Q, L]).
bb_say(Alias, Q, yes) :-
    format('~w Q: ~w~nA: Yes.~n', [Alias, Q]).
bb_say(Alias, Q, no) :-
    format('~w Q: ~w~nA: No.~n', [Alias, Q]).
bb_say(Alias, Q, unknown) :-
    format('~w Q: ~w~nA: I don''t know.~n', [Alias, Q]).
bb_say(Alias, Q, explanation(S, R, O, Proof)) :-
    verbalize(explanation(S, R, O, Proof), _, Text),
    format('~w Q: ~w~nA: ~w~n', [Alias, Q, Text]).

gen_qmark('?').

% norm: quita glue (lo que el mapa no conoce), une con _.
norm_name(Toks, Name) :-
    findall(W, (member(W, Toks), bb_content(W)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name).

% Puente morfologico bidireccional: la pregunta y la memoria usan formas
% de superficie distintas del mismo verbo (infraestructura de lengua).
% Coinciden si son iguales o comparten stem (con desdoblado simetrico:
% ver chat.pl, misma garantia de no-regresion). D9: alias ensenados
% (A,means|significa,C), un nivel, como en chat.pl.
bb_rel_forms(V, Rs) :-
    bb_rel_forms_direct(V, Rs0),
    findall(R, (irregular_form(V, Base),
                bb_rel_forms_direct(Base, RBase), member(R, RBase)), RsI),
    findall(R, (means_triple(V, C),
                bb_rel_forms_direct(C, RC), member(R, RC)), Rs1),
    append([Rs0, RsI, Rs1], Rall),
    sort(Rall, Rs),
    Rs \== [].

means_triple(V, C) :-
    memory_relation(V, means, C, _, _).
means_triple(V, C) :-
    memory_relation(V, significa, C, _, _).

bb_rel_forms_direct(V, Rs) :-
    bb_stem(V, St0),
    bb_ddouble(St0, St),
    findall(R, (memory_relation(_, R, _, _, _),
                ( R == V ; (bb_stem(R, RSt0), bb_ddouble(RSt0, RSt),
                            stem_match(St, RSt), R \== V) )), R0),
    sort(R0, Rs).

stem_match(A, B) :-
    A == B
 ;  atom_concat(A, 'e', B)
 ;  atom_concat(B, 'e', A).

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

ask_form([who, V|Rest], answer(Xs, retrieved, Facts)) :-
    norm_name(Rest, O),
    bb_rel_forms(V, Rs),
    findall(S-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), SF),
    SF \== [],
    findall(S, member(S-_, SF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, SF), Facts).
ask_form([what, did, S|VR], answer(Xs, retrieved, Facts)) :-
    VR = [V],
    bb_rel_forms(V, Rs),
    findall(O-(S, Vr, O), (member(Vr, Rs), memory_relation(S, Vr, O, _, _)), OF),
    OF \== [],
    findall(O, member(O-_, OF), Xs0),
    sort(Xs0, Xs),
    findall(F, member(_-F, OF), Facts).
ask_form([did, S, V|Rest], yes) :-
    norm_name(Rest, O),
    bb_rel_forms(V, Rs),
    member(Vr, Rs),
    memory_relation(S, Vr, O, _, _), !.
ask_form([did|_], no).
ask_form([why, did, S, V|Rest], explanation(S, Vr, O, Proof)) :-
    norm_name(Rest, O),
    bb_rel_forms(V, Rs),
    member(Vr, Rs),
    proof_for(S, Vr, O, Proof).

% Fuentes de cada hecho de la respuesta.
ask_evidence(answer(_, _, Facts)) :- !,
    forall(member((S, R, O), Facts),
           ( prov(S, R, O, info(Ref, _, _)) ->
               format('  proof: ~w --~w--> ~w  [~w]~n', [S, R, O, Ref])
           ; format('  proof: ~w --~w--> ~w  [noref]~n', [S, R, O])
           )).
ask_evidence(yes) :-
    writeln('  (affirmed by memory; ask Why for proof)').
ask_evidence(explanation(S, R, O, Proof)) :-
    format('  proof: ~w --~w--> ~w via ~w~n', [S, R, O, Proof]),
    forall(member(Step, Proof), ev_source(Step)).
ev_source(fact(A, B, C)) :- !,
    ( prov(A, B, C, info(Ref, _, _)) ->
        format('    [~w]~n', [Ref])
    ; format('    [noref]~n', [])
    ).
ev_source((A, B, C)) :-
    ( prov(A, B, C, info(Ref, _, _)) ->
        format('    [~w]~n', [Ref])
    ; format('    [noref]~n', [])
    ), !.
ev_source(Other) :-
    format('    [~w]~n', [Other]).
ask_evidence(no) :-
    writeln('  No: absent from memory.'), !.
ask_evidence(_) :-
    writeln('  UNKNOWN: no evidence in memory.').
