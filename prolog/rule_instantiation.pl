% rule_instantiation.pl
% La meta-regla crea REGLAS CONCRETAS NUEVAS en memoria de largo plazo.
% - instantiate_rule/2: justificacion (evidencia + forma + unicidad) o rehuso.
% - INVENTION=0: ningun atomo de la regla nace sin existir en memoria.
% - export/import: la regla sobrevive sin la meta (independencia) y a un
%   volcado/recarga (persistencia entre "sesiones").
:- use_module(library(lists)).

:- dynamic instantiation_record/6.
% instantiation_record(Target, Labels, Sig, Meta, EvidenceN, Invented)

% instantiate_rule(+Target, +MaxLen)
instantiate_rule(Target, MaxLen) :-
    \+ constrained_rule(Target, _, _),
    findall(S-O, memory_relation(S, Target, O, _, _), Obs0),
    sort(Obs0, Obs),
    length(Obs, N),
    ( N =:= 0 ->
        format('INSTANTIATE REFUSED ~w: no_conclusion_evidence~n', [Target]),
        fail
    ; meta_transfer(Target, MaxLen),
      transferred_rule(Target, Labels, Sig, Meta),
      justification_audit(Target, Labels, Sig, Meta, N, Invented),
      ( Invented == [] ->
          assertz(instantiation_record(Target, Labels, Sig, Meta, N,
                                       Invented)),
          format('INSTANTIATED ~w :- ~w + ~w via ~w (evidence=~w)~n',
                 [Target, Labels, Sig, Meta, N])
      ; retract_constrained(Target),
        format('INSTANTIATE REFUSED ~w: invented=~w~n', [Target, Invented]),
        fail
      )
    ).

retract_constrained(Target) :-
    retractall(constrained_rule(Target, _, _)),
    retractall(transferred_rule(Target, _, _, _)).

% justification_audit: todo atomo de la regla existe en memoria.
justification_audit(Target, Labels, _Sig, _Meta, _N, Invented) :-
    findall(R, memory_relation(_, R, _, _, _), Rs0),
    sort(Rs0, Rs),
    findall(Bad, ( member(Bad, [Target|Labels]),
                   \+ member(Bad, Rs)
                 ),
            Invented).

% sig_to_constraints(+Sig, -Vars, -Cs): forma canonica RULE exportable.
sig_to_constraints(Sig, Vars, Cs) :-
    length(Sig, N),
    findall(V, ( between(1, N, I),
                 atom_concat(v, I, V)
               ),
            Vars),
    findall(C, ( nth1(I, Sig, A), nth1(J, Sig, B), I < J,
                 nth1(I, Vars, VA), nth1(J, Vars, VB),
                 ( A =:= B -> C = (VA == VB)
                 ; C = (VA \== VB)
                 )
               ),
            Cs).

% export_rules(+File): RULE canonicas a memoria de largo plazo.
export_rules(File) :-
    open(File, write, Out),
    forall(constrained_rule(T, P, S),
           ( sig_to_constraints(S, Vars, Cs),
             format(Out, '~q.~n',
                    [rule(conclusion(T), body(P), vars(Vars),
                          constraints(Cs), sig(S))])
           )),
    forall(meta_rule(M, L, S, Ts),
           format(Out, '~q.~n', [meta(len(L), sig(S), members(Ts), id(M))])),
    close(Out),
    format('Exported rules to ~w~n', [File]).

% import_rules(+File): recarga y reconstruye dinamicas.
import_rules(File) :-
    consult(File),
    forall(( rule(conclusion(T), body(P), _, _, sig(S)) ),
           ( retractall(constrained_rule(T, _, _)),
             assertz(constrained_rule(T, P, S))
           )),
    forall(( meta(len(L), sig(S), members(Ts), id(M)) ),
           ( retractall(meta_rule(M, _, _, _)),
             assertz(meta_rule(M, L, S, Ts))
           )),
    format('Imported rules from ~w~n', [File]).

wipe_all_rules :-
    retractall(constrained_rule(_, _, _)),
    retractall(transferred_rule(_, _, _, _)),
    retractall(meta_rule(_, _, _, _)),
    retractall(rule(_, _, _, _, _)),
    retractall(meta(_, _, _, _)).

% audit_invention: INVENTION debe ser [] en todos los records.
audit_invention(Invented) :-
    findall(Bad, ( instantiation_record(_, _, _, _, _, Bs),
                   member(Bad, Bs)
                 ),
            Invented).

show_instantiations :-
    nl, writeln('===== INSTANTIATED RULES ====='),
    forall(instantiation_record(T, L, S, M, N, I),
           format('~w :- ~w + ~w via ~w evidence=~w invented=~w~n',
                  [T, L, S, M, N, I])).
