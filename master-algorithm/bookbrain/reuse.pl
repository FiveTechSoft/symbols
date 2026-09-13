% reuse.pl
% Variable reuse: la regla aprende la FIRMA DE IGUALDAD de sus posiciones
%   Full = [S | Internals..., O]  ->  Sig = particion canonica.
% r4: [1,2,3,4] (A\==B)   r5: [1,2,2,3] (A==A)   r6: [1,2,1] (X==X).
% Si el soporte observado mezcla firmas -> se REHUSA inducir (control).
:- use_module(library(lists)).

:- dynamic constrained_rule/3.  % constrained_rule(Target, Path, Sig)

% full_bindings(+S, +O, +Path, -Full)
full_bindings(S, O, Path, Full) :-
    path_bindings(S, O, Path, Bs),
    append([S|Bs], [O], Full).

% eq_signature(+Values, -Sig): primera-ocurrencia -> indice.
eq_signature(Values, Sig) :-
    eqsig(Values, [], Sig).

eqsig([], _, []).
eqsig([V|Vs], Seen, [I|Is]) :-
    ( member(V-I, Seen) -> Seen1 = Seen
    ; length(Seen, N), I is N + 1, Seen1 = [V-I|Seen]
    ),
    eqsig(Vs, Seen1, Is).

% induce_constrained(+Target, +Path): firma uniforme o rehuso.
induce_constrained(Target, Path) :-
    retractall(constrained_rule(Target, _, _)),
    composed_rule(Target, Path, _),
    findall(Sig,
            ( memory_relation(S, Target, O, _, _),
              full_bindings(S, O, Path, Full),
              eq_signature(Full, Sig)
            ),
            Sigs0),
    sort(Sigs0, Sigs),
    ( Sigs = [Sig] ->
        assertz(constrained_rule(Target, Path, Sig)),
        format('Constrained: ~w :- ~w + sig=~w~n', [Target, Path, Sig])
    ; format('REFUSED ~w :- ~w (mixed signatures ~w)~n',
             [Target, Path, Sigs]),
      fail
    ).

% reuse_predict(+S, +Target, +O): camino + firma exacta.
reuse_predict(S, Target, O) :-
    constrained_rule(Target, Path, ReqSig),
    full_bindings(S, O, Path, Full),
    eq_signature(Full, ReqSig).

show_constrained_rules :-
    nl, writeln('===== CONSTRAINED RULES ====='),
    forall(constrained_rule(T, P, S),
           format('RULE(rels=~w, conclusion=~w, sig=~w)~n', [P, T, S])).
