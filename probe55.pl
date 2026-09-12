% probe55.pl — reproduce el setof de middles y duplicados de functionals.
:- consult('corpus.pl').

p55 :-
    clear_memory,
    retractall(prov(_, _, _, _)),
    consult('kjv_memory.pl'),
    forall(memfact(S, R, O, W, U), assertz(memory_relation(S, R, O, W, U))),
    % consulta EXACTA de exp54_1:pair_support
    setof(B, A^R^(memory_relation(A, R, B, _, _),
                  memory_relation(B, _, _, _, _)), Ms),
    length(Ms, NM),
    format('middles_query=~w first=~q~n', [NM, Ms]),
    % functionals: buscar duplicados reales con longitudes
    setof(R, S^O^memory_relation(S, R, O, _, _), Rs),
    length(Rs, NR),
    findall(R, (member(R, Rs), func_rel(R)), Fs),
    length(Fs, NF),
    sort(Fs, UF),
    length(UF, NU),
    format('rels=~w func=~w uniqfunc=~w~n', [NR, NF, NU]),
    findall(L-R, (member(R, Fs), atom_length(R, L)), LR),
    msort(LR, SLR),
    findall(R, (member(R, Fs),
                aggregate_all(count, member(R, Fs), C), C > 1), Dups),
    sort(Dups, UD),
    forall(member(D, UD),
           (findall(L, (member(D2, Fs), D2 == D, atom_length(D2, L)), _),
            atom_chars(D, Cs),
            maplist(char_code, Cs, Ns),
            format('dup: ~q len=~w codes=~w~n', [D, Ns]))).

func_rel(R) :-
    setof(S, O^memory_relation(S, R, O, _, _), Ss),
    forall(member(S, Ss),
           (setof(O, memory_relation(S, R, O, _, _), [_]))).
