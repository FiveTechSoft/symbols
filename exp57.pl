% exp57.pl — EXP57: KJV 22k hechos con pipeline diferido (10x scale).
% Uso: swipl -s exp57.pl -g exp57 -t halt
% Fases: READ COLLECT INSERT INDEX REPLAY + stats + equivalencia contra
% kjv_memory.pl (ingesta eager histórica). Modo single (comparable).
:- consult('corpus.pl').
:- consult('kjv_parse.pl').
:- consult('kjv_corpus.pl').

:- use_module(library(lists)).

exp57 :-
    exp57_core(0, full).

% Curva de escala: mismos pipeline en prefijos por versiculos.
exp57_curve :-
    forall(member(V, [6000, 12000, 18000, 24000]),
           exp57_core(V, V)),
    exp57_core(0, full).

exp57_core(MaxV, Tag) :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    consult('kjv_memory.pl'),
    kjv_reset,
    dx_reset,
    dx_deferred_on,
    set_kjv_mode(single),
    kjv_collect_on,
    timed(read_lines, TR),
    format('E57-READ ~w ms=~w~n', [Tag, TR]),
    timed(kjv_load_limit(MaxV), TC),
    kjv_count(verses, NV),
    kjv_count(sents, NS),
    kjv_count(stored, NSt),
    findall(1, kjv_staged(_, _, _, _), StL),
    length(StL, NStaged),
    format('E57-COLLECT ~w verses=~w sents=~w stored=~w staged=~w ms=~w~n',
           [Tag, NV, NS, NSt, NStaged, TC]),
    kjv_collect_off,
    timed(insert_staged, TI),
    memory_size(NF0),
    format('E57-INSERT ~w memfacts=~w ms=~w~n', [Tag, NF0, TI]),
    retractall(kjv_staged(_, _, _, _)),
    timed(dx_build_indexes, TX),
    format('E57-INDEX ~w ms=~w~n', [Tag, TX]),
    timed(dx_replay, TRp),
    format('E57-REPLAY ~w ms=~w~n', [Tag, TRp]),
    Tot is TR + TC + TI + TX + TRp,
    memory_size(NF),
    e57_symbols(NSym),
    findall(1, prov_log(conflict, _), CL),
    length(CL, NC),
    findall(1, prov(_, _, _, _), PL),
    length(PL, NP),
    e57_ram(RAM),
    format('E57-TOTAL ~w ms=~w facts=~w staged=~w symbols=~w conflicts=~w provenances=~w ram=~w~n',
           [Tag, Tot, NF, NStaged, NSym, NC, NP, RAM]),
    ( Tag == full -> e57_equiv ; true ).

timed(Goal, Ms) :-
    get_time(T0),
    call(Goal),
    get_time(T1),
    Ms is round((T1 - T0) * 1000).

read_lines :-
    open('corpus_biblia/kjv.txt', read, S, [encoding(utf8)]),
    read_lns(S, N),
    close(S),
    format('E57-READ-LINES n=~w~n', [N]).

read_lns(S, N) :-
    read_line_to_string(S, L),
    ( L == end_of_file -> N = 0
    ; read_lns(S, M), N is M + 1
    ).

insert_staged :-
    forall(kjv_staged(S, R, O, Ref),
           dx_remember(S, R, O, Ref, none)).

e57_symbols(NSym) :-
    findall(E, (memory_relation(E, _, _, _, _) ;
                memory_relation(_, _, E, _, _)), E0),
    sort(E0, Es),
    length(Es, NSym).

e57_ram(RAM) :-
    ( catch(statistics(heapused, RAM), _, fail) -> true
    ; catch(statistics(heap_used, RAM), _, fail) -> true
    ; catch(statistics(memory, RAM), _, fail) -> true
    ; RAM = -1
    ).

% Equivalencia contra kjv_memory.pl (eager): M pesos + P estados.
e57_equiv :-
    consult('kjv_memory.pl'),
    findall(1, (memfact(S, R, O, W, _),
                \+ memory_relation(S, R, O, W, _)), MissM),
    length(MissM, NMM),
    findall(1, (memory_relation(S, R, O, W, _),
                \+ memfact(S, R, O, W, _)), ExtraM),
    length(ExtraM, NXM),
    findall(1, (provfact(S, R, O, Ref, Tm, St),
                \+ prov(S, R, O, info(Ref, Tm, St))), MissP),
    length(MissP, NMP),
    findall(1, (prov(S, R, O, info(Ref, Tm, St)),
                \+ provfact(S, R, O, Ref, Tm, St)), ExtraP),
    length(ExtraP, NXP),
    format('E57-EQUIV mem_missing=~w mem_extra=~w prov_missing=~w prov_extra=~w~n',
           [NMM, NXM, NMP, NXP]).
