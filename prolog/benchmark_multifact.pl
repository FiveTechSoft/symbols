% benchmark_multifact.pl — MULTI-FACT A/B: direct KB access vs symbolic attention.
% KB: multifact.knowledge.pl (15 memfacts, 6 entities, 5 relations; (V,O) unique
%     per S for clean who-V scoring; first-asserted S = luis as known baseline bias).
% The hard question: does attention's context SELECTION add value over direct KB?
%
% M1 (13 standalone): f(Atom) ok iff answer atom appears in raw; unk = honest unknown.
% M2 (5 dialog-conditioned): A states fact (learn), B asks follow-up; ok iff B's
%    answer matches the M1-expected slot of the STATED fact (not any fact).
% Reload KB before EVERY condition; dialog_reset before every condition.
%
% Conditions: direct (attention off) / att_t1 / att_t03.

:- consult('chat.pl').
:- consult('chat_attention.pl').
:- use_module(library(lists)).

:- dynamic mf_result/6.

kb_path('C:/Users/Anto/AppData/Local/Temp/opencode/multifact.knowledge.pl').

% M1: standalone who-questions (13)
m1(1,  "who works in paris",      f(luis),    'bias: luis asserted first').
m1(2,  "who works in london",     f(maria),   'maria work london').
m1(3,  "who works in madrid",     f(juan),    'juan work madrid').
m1(4,  "who lives in madrid",     f(juan),    'juan live madrid').
m1(5,  "who lives in paris",      f(maria),   'maria live paris (luis works there)').
m1(6,  "who lives in rome",       f(ana),     'ana live rome').
m1(7,  "who visited paris",       f(juan),    'juan visit paris').
m1(8,  "who visited london",      f(luis),    'luis visit london').
m1(9,  "who visited madrid",      f(ana),     'ana+pedro visit madrid (amb 2, ok first)').
m1(10, "who owns a cat",          f(maria),   'maria own cat').
m1(11, "who owns a car",          f(luis),    'luis own car').
m1(12, "who speaks spanish",      f(juan),    'juan speak spanish').
m1(13, "who speaks french",       f(ana),     'ana speak french').

% M2: dialog-conditioned (5). Each: A-statement text, B-question text,
% expected slot (the fact just stated). dialog_note pushes S and O;
% "where does he work" must resolve via dialog stack, not KB-first.
m2(14, "juan visited paris",       "where does he work",    f(madrid), 'A: juan visited paris -> he=juan').
m2(15, "maria lives in paris",     "what does she own",     f(cat),    'A: maria -> she=maria').
m2(16, "luis visited london",      "who owns a car",        f(luis),   'A: luis -> he=luis').
m2(17, "ana visited madrid",       "what does she speak",   f(french), 'A: ana -> she=ana').
m2(18, "pedro works in rome",      "who works in rome",     f(pedro),  'A: pedro -> recall pedro').

run :-
    retractall(mf_result(_,_,_,_,_,_)),
    write('========================================'), nl,
    write('  MULTI-FACT: DIRECT vs ATTENTION (15 facts)'), nl,
    write('========================================'), nl, nl,
    run_condition(direct),
    run_condition(att_t1),
    run_condition(att_t03),
    summary,
    dump_tsv,
    halt.

run_condition(Category) :-
    configure(Category),
    kb_path(KB),
    bb_load(KB),
    dialog_reset,
    forall(m1(Id, Text, Expected, _D), mf_run(Category, m1, Id, Text, Expected)),
    dialog_reset,
    forall(m2(Id, A, B, Expected, _D), mf_run_dialog(Category, Id, A, B, Expected)).

configure(direct) :-
    retractall(attention_enabled),
    write('  [direct: attention disabled]'), nl.
configure(att_t1) :-
    assertz(attention_enabled),
    set_params(1.0, 5, 0.0),
    write('  [attention T=1.0 top_k=5]'), nl.
configure(att_t03) :-
    assertz(attention_enabled),
    set_params(0.3, 5, 0.0),
    write('  [attention T=0.3 top_k=5]'), nl.

set_params(T, K, M) :-
    retractall(attention_temperature(_)),
    assertz(attention_temperature(T)),
    retractall(attention_top_k(_)),
    assertz(attention_top_k(K)),
    retractall(attention_min_score(_)),
    assertz(attention_min_score(M)).

% ── M1 runner ────────────────────────────────────────────────────────
mf_run(Category, M, Id, Text, Expected) :-
    catch(with_output_to(atom(Raw), chat_line(Text)), _E, Raw = '<error>'),
    ( var(Raw) -> Raw2 = '<error>' ; Raw2 = Raw ),
    rels_examined(Category, Text, RelsExamined),
    ( atom(Raw2), mf_score(Expected, Raw2, ok) -> Score = ok ; Score = fail ),
    assertz(mf_result(Category, M, Id, Raw2, Score, RelsExamined)).

% ── M2 runner: A states (learn), B asks ──────────────────────────────
% dialog_reset per PAIR: stacks must not accumulate entities across
% pairs (2+ candidates makes dialog_resolve refuse -> pronoun unresolved).
mf_run_dialog(Category, Id, A, B, Expected) :-
    dialog_reset,
    % A: statement (fresh KB each condition, so learn is idempotent-safe)
    catch(with_output_to(atom(RawA), chat_line(A)), _E, RawA = '<error>'),
    catch(with_output_to(atom(RawB), chat_line(B)), _E, RawB = '<error>'),
    ( var(RawB) -> RawB2 = '<error>' ; RawB2 = RawB ),
    ( Category \= direct ->
        tokens_of(B, Toks),
        ( catch(chat_attention_focus(Toks, focus(_, Rels, _)), _E2, fail) ->
            length(Rels, RelsExamined)
        ; RelsExamined = 0 )
    ; RelsExamined = 0 ),
    ( atom(RawB2), mf_score(Expected, RawB2, ok) -> Score = ok ; Score = fail ),
    assertz(mf_result(Category, m2, Id, RawB2, Score, RelsExamined)).

rels_examined(direct, _, 0) :- !.
rels_examined(_, Text, R) :-
    tokens_of(Text, Toks),
    ( catch(chat_attention_focus(Toks, focus(_, Rels, _)), _E, fail) ->
        length(Rels, R)
    ; R = 0 ).

tokens_of(Text, Toks) :-
    downcase_atom(Text, L),
    ( tokenize_atom(L, Ts) -> Toks = Ts
    ; atom_chars(L, Chars), tokens_fallback(Chars, [], Toks) ).

tokens_fallback([], Acc, Toks) :- !, reverse(Acc, Toks).
tokens_fallback(Chars, Acc, Toks) :-
    ( append(Ws, [' '|Rest], Chars) -> Ws2 = Ws, Rest2 = Rest
    ; Ws2 = Chars, Rest2 = []
    ),
    ( Ws2 \= [] -> atom_chars(W, Ws2), tokens_fallback(Rest2, [W|Acc], Toks)
    ; tokens_fallback(Rest2, Acc, Toks)
    ).

mf_score(f(Atom), Raw, ok) :-
    downcase_atom(Raw, L),
    ( sub_atom(L, _, _, _, 'don\'t know') -> fail
    ; sub_atom(L, _, _, _, 'unknown') -> fail
    ; downcase_atom(Atom, LA),
      sub_atom(L, _, _, _, LA)
    ).

summary :-
    nl, write('========================================'), nl,
    write('  RESULTS'), nl,
    write('========================================'), nl, nl,
    summary_cat(direct,  'DIRECT (attention off)'),
    summary_cat(att_t1,  'ATTENTION T=1.0'),
    summary_cat(att_t03, 'ATTENTION T=0.3'),
    nl,
    write('  BREAKDOWN (M1 standalone / M2 dialog)'), nl,
    breakdown(direct), breakdown(att_t1), breakdown(att_t03),
    nl,
    write('  DETAILED'), nl,
    detailed(direct), detailed(att_t1), detailed(att_t03).

summary_cat(Cat, Label) :-
    findall(Id, mf_result(Cat, _, Id, _, ok, _), OKs),
    findall(Id, mf_result(Cat, _, Id, _, fail, _), Fails),
    length(OKs, OK), length(Fails, F),
    Total is OK + F,
    ( Total > 0 -> Pct is OK * 100 / Total ; Pct = 0 ),
    format('  ~w: ~w/~w (~1f%)~n', [Label, OK, Total, Pct]),
    findall(R, mf_result(Cat, _, _, _, _, R), Rs),
    ( Rs \= [] -> sumlist(Rs, S), length(Rs, N), A is S / N,
                  format('    avg rels examined: ~2f~n', [A]) ; true ),
    ( Total > 0 -> format('    [sanity] total=~w~n', [Total]) ; true ).

breakdown(Cat) :-
    findall(Id, (mf_result(Cat, m1, Id, _, ok, _)), IOk),
    findall(Id, (mf_result(Cat, m1, Id, _, fail, _)), IFail),
    findall(Id, (mf_result(Cat, m2, Id, _, ok, _)), DOk),
    findall(Id, (mf_result(Cat, m2, Id, _, fail, _)), DFail),
    length(IOk, N1), length(IFail, N2), length(DOk, N3), length(DFail, N4),
    T1 is N1 + N2, T2 is N3 + N4,
    format('  ~w: M1 ~w/~w  M2 ~w/~w~n', [Cat, N1, T1, N3, T2]).

detailed(Cat) :-
    ( mf_result(Cat, M, Id, Raw, fail, R),
      ( M = m1, m1(Id, Text, Expected, _)
      ; M = m2, m2(Id, _A, Text, Expected, _) ),
      format('  FAIL ~w [~w] ~w => ~w (expected ~w) [~w rels]~n',
             [Id, M, Text, Raw, Expected, R]),
      fail
    ; format('  (end of fails for ~w)~n', [Cat]) ).

dump_tsv :-
    open('C:/Users/Anto/AppData/Local/Temp/opencode/multifact_ab.tsv', write, S),
    forall(mf_result(Cat, M, Id, Raw, Score, R),
           format(S, '~w\t~w\t~w\t~w\t~w\t~w\n', [Cat, M, Id, Score, R, Raw])),
    close(S),
    write('TSV written: multifact_ab.tsv'), nl.