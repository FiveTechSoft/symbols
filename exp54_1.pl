% exp54_1.pl — EXP54.1: estadistica relacional del KJV ingerido.
% Uso: swipl -s exp54_1.pl -g exp54_1 -t halt
% Carga kjv_memory.pl (segundos) y publica: histograma de relaciones,
% top simbolos por grado, pares r1->r2 con soporte (middles), triangulos
% cerrados r1+r2->r3 con soporte y libros distintos, relaciones funcionales.
:- consult('corpus.pl').

:- use_module(library(lists)).
:- use_module(library(aggregate)).

exp54_1 :-
    load_biblia,
    rel_histogram,
    top_symbols,
    pair_support,
    triangles,
    functionals.

load_biblia :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    retractall(prov_log(_, _)),
    consult('kjv_memory.pl'),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    forall(provfact(S, R, O, Ref, T, St), assertz(prov(S, R, O, info(Ref, T, St)))),
    memory_size(NF),
    format('EXP54.1 loaded facts=~w~n', [NF]).

% ---- histograma de relaciones ----
rel_histogram :-
    findall(R, memory_relation(_, R, _, _, _), Rs),
    msort(Rs, Sorted),
    clumped_counts(Sorted, Counts),
    length(Rs, Total),
    findall(C, member(C-_, Counts), Cs),
    sum_list(Cs, Sum),
    sort_counts_desc(Counts, Desc),
    length(Desc, NRel),
    format('RELATIONS distinct=~w total=~w checksum=~w~n', [NRel, Total, Sum]),
    forall(member(C-R, Desc), format('REL ~w ~w~n', [R, C])).

sum_list([], 0).
sum_list([H|T], S) :- sum_list(T, S0), S is S0 + H.

clumped_counts([], []).
clumped_counts([H|T], [C-H|Rest]) :-
    clump(H, T, C, R),
    clumped_counts(R, Rest).

clump(H, [], 1, []).
clump(H, [H|T], C, R) :- !, clump(H, T, C0, R), C is C0 + 1.
clump(_, L, 1, L).

sort_counts_desc(Counts, Desc) :-
    findall(C-R, member(C-R, Counts), Pairs),
    keysort(Pairs, Asc),
    reverse(Asc, Desc).

% ---- top simbolos por grado ----
top_symbols :-
    findall(S, memory_relation(S, _, _, _, _), Ss),
    msort(Ss, SS),
    clumped_counts(SS, SC),
    sort_counts_desc(SC, SDesc),
    pr_top('SUBJ', SDesc, 20),
    findall(O, memory_relation(_, _, O, _, _), Os),
    msort(Os, OS),
    clumped_counts(OS, OC),
    sort_counts_desc(OC, ODesc),
    pr_top('OBJ', ODesc, 20).

pr_top(_, [], _) :- !.
pr_top(_, _, 0) :- !.
pr_top(Tag, [C-X|T], K) :-
    format('~w ~w ~w~n', [Tag, X, C]),
    K1 is K - 1,
    pr_top(Tag, T, K1).

% ---- pares r1->r2: soporte = middles distintos (SIN ^: findall+sort) ----
pair_support :-
    findall(S, memory_relation(S, _, _, _, _), Ss0),
    sort(Ss0, SS),
    findall(O, memory_relation(_, _, O, _, _), Os0),
    sort(Os0, OO),
    ord_intersection(SS, OO, Ms),
    length(Ms, NM),
    format('MIDDLES ~w~n', [NM]),
    findall(R1-R2, (member(B, Ms),
                    findall(R1, memory_relation(_, R1, B, _, _), I0),
                    sort(I0, R1s),
                    findall(R2, memory_relation(B, R2, _, _, _), O0),
                    sort(O0, R2s),
                    member(R1, R1s), member(R2, R2s)), Pairs),
    msort(Pairs, SP),
    clumped_counts(SP, PC),
    sort_pair_counts(PC, Desc),
    pr_pairs(Desc, 30).

sort_pair_counts(Counts, Desc) :-
    findall(C-(R1-R2), member(C-(R1-R2), Counts), P),
    keysort(P, Asc),
    reverse(Asc, Desc).

pr_pairs([], _) :- !.
pr_pairs(_, 0) :- !.
pr_pairs([C-(R1-R2)|T], K) :-
    format('PAIR ~w -> ~w support_middles=~w~n', [R1, R2, C]),
    K1 is K - 1,
    pr_pairs(T, K1).

% ---- triangulos: r1+r2->r3 con soporte y libros ----
% Solo top-12 pares por soporte; middles hub (pre*post>20000) se saltan y
% se cuentan (honesto). Soporte = pares (A,C) distintos; libros distintos
% via proveniencia bBI_C_V.
triangles :-
    findall(S, memory_relation(S, _, _, _, _), Ss0),
    sort(Ss0, SS),
    findall(O, memory_relation(_, _, O, _, _), Os0),
    sort(Os0, OO),
    ord_intersection(SS, OO, Ms),
    findall(R1-R2, (member(B, Ms),
                    findall(R1, memory_relation(_, R1, B, _, _), I0),
                    sort(I0, R1s),
                    findall(R2, memory_relation(B, R2, _, _, _), O0),
                    sort(O0, R2s),
                    member(R1, R1s), member(R2, R2s)), Pairs),
    msort(Pairs, SP),
    clumped_counts(SP, PC),
    sort_pair_counts(PC, [Top|Rest]),
    Top = _-(TR1-TR2),
    format('TRI-TOP-SEED ~w -> ~w~n', [TR1, TR2]),
    take_pairs([Top|Rest], 12, Dozen),
    forall(member(_-(R1-R2), Dozen), tri_for_pair(R1, R2)).

take_pairs(_, 0, []) :- !.
take_pairs([], _, []) :- !.
take_pairs([H|T], K, [H|R]) :- K1 is K - 1, take_pairs(T, K1, R).

tri_for_pair(R1, R2) :-
    format('TRI-PAIR ~w -> ~w ...~n', [R1, R2]),
    % middles con ambas relaciones (B ground en todo lo que sigue)
    findall(B, (memory_relation(_, R1, B, _, _),
                memory_relation(B, R2, _, _, _)), Bs0),
    sort(Bs0, Bs),
    findall(B, (member(B, Bs),
                findall(A, memory_relation(A, R1, B, _, _), Pre0),
                length(Pre0, Pre),
                findall(C, memory_relation(B, R2, C, _, _), Post0),
                length(Post0, Post),
                Pre * Post > 20000), Hubs),
    length(Hubs, NH),
    findall(R3-(A-C)-BI,
            (member(B, Bs),
             \+ member(B, Hubs),
             memory_relation(A, R1, B, _, _),
             memory_relation(B, R2, C, _, _),
             memory_relation(A, R3, C, _, _),
             R3 \== R1, R3 \== R2,
             prov(A, R3, C, info(Ref, _, _)),
             kjv_ref_book(Ref, BI)),
            Hits),
    length(Hits, NHits),
    ( Hits == [] ->
        format('TRI-CLOSE ~w + ~w -> (none)~n', [R1, R2])
    ; findall(R3, member(R3-_-_, Hits), R30),
      sort(R30, R3s),
      forall(member(R3, R3s),
             (findall(AC, member(R3-AC-_, Hits), AC0),
              sort(AC0, ACs),
              length(ACs, NSup),
              findall(BI, member(R3-_-BI, Hits), BI0),
              sort(BI0, BIs),
              length(BIs, NB),
              format('TRI-CLOSE ~w + ~w -> ~w support_pairs=~w books=~w~n',
                     [R1, R2, R3, NSup, NB])))
    ),
    format('TRI-HITS ~w -> ~w raw=~w hubs_skipped=~w~n', [R1, R2, NHits, NH]).

% Cuota anti-hub: el producto pre*post del middle debe ser <= 20000.
pre_post_ok(A, R1, B, R2, C) :-
    aggregate_all(count, memory_relation(_, R1, B, _, _), Pre),
    aggregate_all(count, memory_relation(B, R2, _, _, _), Post),
    Pre * Post =< 20000.

sort_tri_counts(Counts, Desc) :-
    findall(C-(R3-AC-BI), member(C-(R3-AC-BI), Counts), P),
    keysort(P, Asc),
    reverse(Asc, Desc).

pr_tris([], _) :- !.
pr_tris(_, 0) :- !.
pr_tris([C-(R3-_-BI)|T], K) :-
    format('TRI-CLOSE ~w + ~w -> ~w support=~w book_sample=~w~n',
           ['?', '?', R3, C, BI]),
    K1 is K - 1,
    pr_tris(T, K1).

kjv_ref_book(Ref, BI) :-
    atom(Ref),
    atom_concat(b, Rest, Ref),
    split_string(Rest, "_", "", [BIS|_]),
    number_string(BI, BIS), !.
kjv_ref_book(_, 0).

% ---- relaciones funcionales: cada sujeto un solo objeto (SIN ^) ----
functionals :-
    findall(R, memory_relation(_, R, _, _, _), R0),
    sort(R0, Rs),
    findall(R, (member(R, Rs), func_rel(R)), Fs),
    sort(Fs, UF),
    length(Fs, NF),
    length(UF, NU),
    format('FUNCTIONAL n=~w uniq=~w : ~w~n', [NF, NU, UF]).

func_rel(R) :-
    findall(S, memory_relation(S, R, _, _, _), S0),
    sort(S0, Ss),
    forall(member(S, Ss),
           (findall(O, memory_relation(S, R, O, _, _), O0),
            sort(O0, [_]))).
