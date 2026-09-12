% bookbrain/ask.pl — ASK general sobre document.knowledge.
% Uso desde bookbrain/: swipl -s ask.pl -g "askq(alice, 'Who opened door?')" -t halt
% Formas (verbos en superficie, sin canonizar):
%   Who <V> <OBJ>?     -> inversa + pruebas + fuentes
%   What did <S> <V>?  -> directa + pruebas + fuentes
%   Did <S> <V> <OBJ>? -> yes + prueba / no
%   Why did <S> <V> <OBJ>? -> explicacion + fuentes
% Sin evidencia: UNKNOWN (nunca inventa). Reutiliza verbalize/proof_for.
:- consult('../corpus.pl').
:- consult('../question_parser.pl').
:- consult('../gen_parse.pl').

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

% norm: quita cerradas, une con _ (igual que entidades).
norm_name(Toks, Name) :-
    findall(W, (member(W, Toks), \+ gen_closed(W)), Ws),
    Ws \== [],
    atomic_list_concat(Ws, '_', Name).

% Expansion morfologica del verbo de la pregunta a formas de superficie
% presentes en memoria (infraestructura de lengua, no dominio).
bb_verb_forms(V, Forms) :-
    findall(F, bb_vform(V, F), F0),
    sort([V|F0], Forms).

bb_vform(V, F) :- atom_concat(V, ed, F).
bb_vform(V, F) :- sub_atom(V, _, 1, 0, e), atom_concat(V, d, F).
bb_vform(V, F) :- atom_concat(V, s, F).
bb_vform(V, F) :- atom_concat(V, ing, F).

bb_rel_forms(V, Rs) :-
    bb_verb_forms(V, Forms),
    findall(R, (member(R, Forms), memory_relation(_, R, _, _, _)), R0),
    sort(R0, Rs),
    Rs \== [].

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
