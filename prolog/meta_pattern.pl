% meta_pattern.pl
% Second-order abstraction: reglas sobre reglas.
% - rule_shape/3: borra nombres de relacion -> (Len, Sig).
% - discover_meta/0: agrupa formas identicas con soporte >= 2 reglas.
% - meta_transfer/2: si la induccion concreta rehusa (sin constrained_rule)
%   pero los caminos observados encajan en una META forma, la adopta con
%   las etiquetas concretas halladas por busqueda (sin usar Target).
:- use_module(library(lists)).

:- dynamic meta_rule/4.       % meta_rule(MetaId, Len, Sig, Members)
:- dynamic transferred_rule/4. % transferred_rule(Target, Labels, Sig, Meta)

% rule_shape(+Target, -Len, -Sig)
rule_shape(Target, Len, Sig) :-
    constrained_rule(Target, Path, Sig),
    length(Path, Len).

% discover_meta: agrupa (Len, Sig) identicos con >= 2 miembros.
discover_meta :-
    retractall(meta_rule(_, _, _, _)),
    findall(Len-Sig-T,
            ( constrained_rule(T, P, Sig),
              length(P, Len)
            ),
            Shapes),
    sort(Shapes, UShapes),
    findall(Len-Sig,
            ( member(Len-Sig-_, UShapes) ),
            Keys0),
    sort(Keys0, Keys),
    forall(member(Len-Sig, Keys),
           ( findall(T, member(Len-Sig-T, UShapes), Ts),
             length(Ts, N),
             ( N >= 2 ->
                 atomic_list_concat([chain, '_', Len, '_', N], MetaId),
                 assertz(meta_rule(MetaId, Len, Sig, Ts)),
                 format('META ~w: len=~w sig=~w members=~w~n',
                        [MetaId, Len, Sig, Ts])
             ; format('single shape len=~w sig=~w (~w), no meta~n',
                      [Len, Sig, Ts])
             )
           )).

show_meta_rules :-
    nl, writeln('===== META RULES ====='),
    forall(meta_rule(M, L, S, Ts),
           format('META_RULE(~w, topology_len=~w, sig=~w, covers=~w)~n',
                  [M, L, S, Ts])).

% find_path(+S, +O, +MaxLen, +Avoid, -Labels, -Bs): caminos simples
% dirigidos sin repetir nodos y sin usar la relacion Avoid.
find_path(S, O, MaxLen, Avoid, Labels, Bs) :-
    MaxLen > 0,
    dfs(S, O, MaxLen, Avoid, [S], Labels, Bs).

dfs(S, O, _, Avoid, _, [R], []) :-
    memory_relation(S, R, O, _, _),
    R \== Avoid.
dfs(S, O, MaxLen, Avoid, Visited, [R|Rs], [M|Bs]) :-
    MaxLen > 1,
    memory_relation(S, R, M, _, _),
    R \== Avoid,
    \+ member(M, Visited),
    Max1 is MaxLen - 1,
    dfs(M, O, Max1, Avoid, [M|Visited], Rs, Bs).

% path_shapes(+Target, +MaxLen, -Shapes): (Labels, Len, Sig) observados.
path_shapes(Target, MaxLen, Shapes) :-
    findall(Labels-Len-Sig,
            ( memory_relation(S, Target, O, _, _),
              find_path(S, O, MaxLen, Target, Labels, Bs),
              length(Labels, Len),
              append([S|Bs], [O], Full),
              eq_signature(Full, Sig)
            ),
            Shapes).

% meta_transfer(+Target, +MaxLen): licencia por forma abstracta.
meta_transfer(Target, MaxLen) :-
    \+ constrained_rule(Target, _, _),
    retractall(transferred_rule(Target, _, _, _)),
    path_shapes(Target, MaxLen, Shapes0),
    sort(Shapes0, Shapes),
    findall(Labels-Len-Sig-Meta,
            ( member(Labels-Len-Sig, Shapes),
              meta_rule(Meta, Len, Sig, _)
            ),
            Cands0),
    sort(Cands0, Cands),
    findall(Labels, member(Labels-_-_-_, Cands), Lab0),
    sort(Lab0, Labs),
    ( Labs = [Labels], Cands = [Labels-Len-Sig-Meta|_] ->
        assertz(transferred_rule(Target, Labels, Sig, Meta)),
        assertz(constrained_rule(Target, Labels, Sig)),
        format('TRANSFER ~w :- ~w + ~w via ~w~n',
               [Target, Labels, Sig, Meta])
    ; format('TRANSFER REFUSED ~w (candidates ~w)~n', [Target, Cands]),
      fail
    ).

show_transferred :-
    nl, writeln('===== TRANSFERRED RULES ====='),
    forall(transferred_rule(T, L, S, M),
           format('~w :- ~w + ~w  (licensed by ~w)~n', [T, L, S, M])).
