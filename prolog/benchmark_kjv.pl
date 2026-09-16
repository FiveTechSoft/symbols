% benchmark_kjv.pl — KJV REAL-TEXT A/B: direct KB access vs symbolic attention.
% KB: kjv_rj.knowledge.pl (16 memfacts from Ruth+Jonah, extracted by the parser
%     from real verses — includes noise: 'visitar' FP from p6 misparse,
%     'certain man' junk, attribute facts). 133 clauses in, 16 facts out.
% Hard question: on a noisy KB built from real text, does attention's
% SELECTION improve answers over direct KB access?
%
% Question battery derived from actual memfacts (gold = the fact itself):
%   who visited X  / who feared lord / who came to jonah ...
% Each fact asked once; scoring: expected subject atom in raw, no "don't know".
%
% Conditions: direct / att_t1 / att_t03. Entry: -g run

:- consult('chat.pl').
:- consult('chat_attention.pl').
:- use_module(library(lists)).

:- dynamic kq_result/6.

kb_path('C:/Users/Anto/AppData/Local/Temp/opencode/kjv_rj.knowledge.pl').

% q(Id, Question, ExpectedSubject, Note)
kq(1,  "who visited amminadab",  ram,     'ram visitar amminadab').
kq(2,  "who visited salmon",     nahshon, 'nahshon visitar salmon').
kq(3,  "who visited obed",       boaz,    'boaz visitar obed').
kq(4,  "who visited david",      jesse,   'jesse visitar david').
kq(5,  "who feared the lord",    men,     'men feared lord').
kq(6,  "who came to jonah",      word,    'word came jonah').
kq(7,  "who came unto jonah",    word,    'unto variant').
kq(8,  "who grants a find",      lord,    'lord grant find').
kq(9,  "who recompenses work",   lord,    'lord recompense work').
kq(10, "who visited night",      tarry,   'tarry visitar night').
kq(11, "who said witnesses",     elders,  'elders said witnesses').
kq(12, "who beat the head",      sun,     'sun beat head').
kq(13, "who feared lord",        men,     'no-the variant of 5').
kq(14, "what came to jonah",     word,    'what V E form').
kq(15, "who visited salmon",     nahshon, 'repeat for stability').
kq(16, "who comes to jonah",     word,    'tense variant came->comes stem').
kq(17, "who visits amminadab",   ram,     'visits 3sg form').
kq(18, "who feareth lord",       men,     'eth form').

run :-
    retractall(kq_result(_,_,_,_,_,_)),
    write('========================================'), nl,
    write('  KJV REAL-TEXT: DIRECT vs ATTENTION (16 facts)'), nl,
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
    forall(kq(Id, Text, Expected, _D), kq_run(Category, Id, Text, Expected)).

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

kq_run(Category, Id, Text, Expected) :-
    catch(with_output_to(atom(Raw), chat_line(Text)), _E, Raw = '<error>'),
    ( var(Raw) -> Raw2 = '<error>' ; Raw2 = Raw ),
    rels_examined(Category, Text, RelsExamined),
    ( atom(Raw2), kq_score(Expected, Raw2, ok) -> Score = ok ; Score = fail ),
    assertz(kq_result(Category, Id, Text, Raw2, Score, RelsExamined)).

rels_examined(direct, _, 0) :- !.
rels_examined(_, Text, R) :-
    downcase_atom(Text, L),
    ( tokenize_atom(L, Ts) -> Toks = Ts ; Ts = [] ),
    ( catch(chat_attention_focus(Toks, focus(_, Rels, _)), _E, fail) ->
        length(Rels, R)
    ; R = 0 ).

kq_score(Expected, Raw, ok) :-
    downcase_atom(Raw, L),
    ( sub_atom(L, _, _, _, 'don\'t know') -> fail
    ; sub_atom(L, _, _, _, 'unknown') -> fail
    ; downcase_atom(Expected, LE),
      sub_atom(L, _, _, _, LE)
    ).

summary :-
    nl, write('========================================'), nl,
    write('  RESULTS'), nl,
    write('========================================'), nl, nl,
    summary_cat(direct,  'DIRECT (attention off)'),
    summary_cat(att_t1,  'ATTENTION T=1.0'),
    summary_cat(att_t03, 'ATTENTION T=0.3'),
    nl, write('  DETAILED FAILS'), nl,
    detailed(direct), detailed(att_t1), detailed(att_t03).

summary_cat(Cat, Label) :-
    findall(Id, kq_result(Cat, _, Id, _, ok, _), OKs),
    findall(Id, kq_result(Cat, _, Id, _, fail, _), Fails),
    length(OKs, OK), length(Fails, F),
    Total is OK + F,
    ( Total > 0 -> Pct is OK * 100 / Total ; Pct = 0 ),
    format('  ~w: ~w/~w (~1f%)~n', [Label, OK, Total, Pct]),
    findall(R, kq_result(Cat, _, _, _, _, R), Rs),
    ( Rs \= [] -> sumlist(Rs, S), length(Rs, N), A is S / N,
                  format('    avg rels examined: ~2f~n', [A]) ; true ).

detailed(Cat) :-
    ( kq_result(Cat, Id, Text, Raw, fail, R),
      kq(Id, Text, Expected, _),
      format('  FAIL ~w ~w => ~w (expected ~w) [~w rels]~n',
             [Id, Text, Raw, Expected, R]),
      fail
    ; format('  (end of fails for ~w)~n', [Cat]) ).

dump_tsv :-
    open('C:/Users/Anto/AppData/Local/Temp/opencode/kjv_ab.tsv', write, S),
    forall(kq_result(Cat, Id, Text, Raw, Score, R),
           format(S, '~w\t~w\t~w\t~w\t~w\t~w\n', [Cat, Id, Score, R, Text, Raw])),
    close(S),
    write('TSV written: kjv_ab.tsv'), nl.