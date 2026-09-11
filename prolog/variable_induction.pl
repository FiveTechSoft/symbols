% variable_induction.pl
% Induccion de variables sin sufijos, sin numeros, sin funcion programada.
% Regla descubierta: S --Target--> O  ssi  existe B: S--R-->B y O--R-->B.
% B es variable existencial emergente (el "puente" compartido).
% El modulo busca entre todas las relaciones R candidatas la que mejor
% explica Target (F1 sobre la rejilla SC x OC de los conceptos).
:- use_module(library(lists)).

:- dynamic induced_rule/4.   % induced_rule(Target, BridgeRel, F1, Support)

% discover_shared_neighbor_rule(+Target)
% Requiere: conceptos y concept_member ya descubiertos, y al menos una
% relacion conceptual SC --Target--> OC aprendida.
discover_shared_neighbor_rule(Target) :-
    retractall(induced_rule(_, _, _, _)),
    concept_relation(SC, Target, OC, _),
    findall(S, concept_member(SC, S, _), SS0),
    findall(O, concept_member(OC, O, _), OS0),
    sort(SS0, SS), sort(OS0, OS),
    findall(R, (memory_relation(_, R, _, _, _), R \== Target), Rs0),
    sort(Rs0, Rs),
    findall(F1-R-Sup,
            ( member(R, Rs),
              score_bridge(Target, SS, OS, R, F1, Sup)
            ),
            Scored),
    keysort(Scored, Sorted),
    reverse(Sorted, Ranked),
    format('Bridge candidates for ~w (SC=~w OC=~w):~n', [Target, SC, OC]),
    forall(member(F1-R-Sup, Ranked),
           format('  R=~w F1=~4f support=~w~n', [R, F1, Sup])),
    Ranked = [BestF1-BestR-BestSup|Rest],
    ( Rest = [SecondF1-_-_|_] -> true ; SecondF1 = 0.0 ),
    Margin is BestF1 - SecondF1,
    % Nota: los pares ocultos comparten puente pero no estan en memoria
    % como Target, asi que Precision < 1.0 es ESPERABLE (es la
    % generalizacion que buscamos). Criterio: mejor F1 >= 0.70 con
    % margen claro sobre el segundo candidato.
    Margin >= 0.30, BestF1 >= 0.70,
    assertz(induced_rule(Target, BestR, BestF1, BestSup)),
    format('Induced: ~w(S,O) :- ~w(S,B), ~w(O,B)  (F1=~4f)~n',
           [Target, BestR, BestR, BestF1]).

% score_bridge(+Target, +SS, +OS, +R, -F1, -Support)
score_bridge(Target, SS, OS, R, F1, Support) :-
    findall(S-O,
            ( member(S, SS), member(O, OS),
              memory_relation(S, Target, O, _, _)
            ),
            TargetPairs0),
    sort(TargetPairs0, TargetPairs),
    length(TargetPairs, Support),
    ( Support =:= 0 -> F1 = 0.0
    ; findall(S-O,
              ( member(S, SS), member(O, OS),
                shares_bridge(S, O, R)
              ),
              Sharing0),
      sort(Sharing0, Sharing),
      intersection(TargetPairs, Sharing, TP),
      length(TP, NTP),
      length(Sharing, NS),
      ( NS =:= 0 -> P = 0.0 ; P is NTP / NS ),
      Rcall is NTP / Support,
      ( P + Rcall =:= 0 -> F1 = 0.0
      ; F1 is 2 * P * Rcall / (P + Rcall)
      )
    ).

shares_bridge(S, O, R) :-
    memory_relation(S, R, B, _, _),
    memory_relation(O, R, B, _, _).

% induced_predict(+S, +Target, +O): aplica la regla con variable.
induced_predict(S, Target, O) :-
    induced_rule(Target, R, _, _),
    shares_bridge(S, O, R).

show_induced_rules :-
    nl, writeln('===== INDUCED VARIABLE RULES ====='),
    forall(induced_rule(T, R, F1, Sup),
           format('~w(S,O) :- ~w(S,B), ~w(O,B)  F1=~4f support=~w~n',
                  [T, R, R, F1, Sup])).
