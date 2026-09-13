% guided_search.pl
% El conocimiento adquirido enfoca la busqueda futura:
% 1. vocabulario incidente (<=2 saltos de los miembros, completo p.o len<=3)
% 2. filtro de extremos (primera rel sale de SS, ultima entra en OS)
% Comparacion justa: mismo score_path, mismos umbrales; se mide
% evaluados, podados, tiempo y ganador (debe coincidir con exhaustive).
:- use_module(library(lists)).

% incident_vocab(+SS, +OS, +Target, -Vocab)
incident_vocab(SS, OS, Target, Vocab) :-
    append(SS, OS, N0),
    incident_rels(N0, R1, N1),
    incident_rels(N1, R2, _),
    append(R1, R2, Rall0),
    sort(Rall0, Rall),
    delete(Rall, Target, Vocab).

incident_rels(Nodes, Rels, Neigh) :-
    findall(R, ( memory_relation(S, R, O, _, _),
                 ( member(S, Nodes) ; member(O, Nodes) )
               ),
            Rels0),
    sort(Rels0, Rels),
    findall(N, ( memory_relation(S, _, O, _, _),
                 ( member(S, Nodes) ; member(O, Nodes) ),
                 ( N = S ; N = O )
               ),
            Neigh0),
    sort(Neigh0, Neigh).

% pattern_ok(+Path, +SS, +OS): extremos conectados.
pattern_ok([R], SS, OS) :-
    ( member(S, SS), memory_relation(S, R, _, _, _) ;
      member(O, OS), memory_relation(_, R, O, _, _)
    ), !.
pattern_ok([R1|Rs], SS, OS) :-
    Rs \== [],
    member(S, SS), memory_relation(S, R1, _, _, _),
    last(Rs, RL),
    member(O, OS), memory_relation(_, RL, O, _, _).

% run_discovery(+Mode, +Target, +MaxLen, -Stats)
% Stats = stats(Winner, F1, Support, Generated, Evaluated, Pruned, Ms).
run_discovery(Mode, Target, MaxLen, Stats) :-
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    ( Mode == exhaustive ->
        findall(R, (memory_relation(_, R, _, _, _), R \== Target), Rs0),
        sort(Rs0, Vocab)
    ; incident_vocab(SS, OS, Target, Vocab)
    ),
    findall(Len, between(1, MaxLen, Len), Lens),
    findall(Path, ( member(Len, Lens),
                    pattern(Len, Vocab, Path)
                  ),
            Generated0),
    length(Generated0, Generated),
    % FAIR: exhaustive evalua TODO (sin filtro); guided filtra por extremos.
    % El filtro es solido: un patron con soporte>0 lo supera siempre, asi
    % que el ganador nunca puede ser podado (solo se podan F1=0 seguros).
    ( Mode == exhaustive ->
        Valid = Generated0
    ; include(pattern_ok_ss_os(SS, OS), Generated0, Valid)
    ),
    length(Valid, Evaluated),
    Pruned is Generated - Evaluated,
    get_time(T0),
    findall(F1-Path-Sup,
            ( member(Path, Valid),
              score_path(Target, SS, OS, Path, F1, Sup)
            ),
            Scored),
    get_time(T1),
    Ms is round((T1 - T0) * 1000),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    Ranked = [BestF1-BestPath-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    Margin >= 0.30, BestF1 >= 0.70,
    Stats = stats(BestPath, BestF1, BestSup, Generated, Evaluated,
                  Pruned, Ms).

pattern_ok_ss_os(SS, OS, Path) :-
    pattern_ok(Path, SS, OS).
