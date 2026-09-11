% experiment49.pl
% EXPERIMENT 49 - HYPOTHESIS AND COUNTEREVIDENCE (aceptar o rechazar)
% El modelo hipotetiza equivalencias (RNueva ~= visits) y las somete a
% prueba estructural: ACEPTA si encadena por la relacion compartida
% `in` sin inversion de roles; RECHAZA con razon si los roles estan
% invertidos (sujeto conocido-ciudad). Ciclo completo:
% observar -> hipotesis -> sondear -> evidencia -> aceptar/rechazar ->
% memoria -> razonar -> proof. Confianza = soporte (0.5/1.0/0.0).
:- consult('conversation.pl').
:- consult('experiment46.pl').

:- use_module(library(lists)).

:- dynamic hypothesis/4.
:- dynamic rel_map/2.

dialogue49 :-
    load_demo,
    nl, writeln('===== DIALOGUE 49 (hypothesize, test, accept/reject) ====='),
    t("Does alba reach norway?", yes(_)),
    t_tell("Mira tours Paris.", learned((mira, tours, paris))),
    hypothesize(tours, visits),
    check(hypothesis(tours, visits, pending, 0.5),
          'hypothesis tours~=visits pending (0.5)'),
    % sondeo: la cadena por `in` existe (soporte) antes de aceptar.
    check((memory_relation(mira, tours, paris, _, _),
           memory_relation(paris, in, france, _, _)),
          'probe finds chain support (tours+in)'),
    accept_hypothesis(tours, visits),
    check(hypothesis(tours, visits, accepted, 1.0),
          'tours~=visits ACCEPTED on chain support'),
    check(rel_map(visits, tours), 'map records accepted pair'),
    t_caters(mira, france, yes),
    t("Why?", proof(_)),
    t_tell("Paris allures Mira.", learned((paris, allures, mira))),
    hypothesize(allures, visits),
    check(hypothesis(allures, visits, pending, 0.5),
          'hypothesis allures~=visits pending (0.5)'),
    reject_counterevidence(allures, visits),
    check(hypothesis(allures, visits, rejected, 0.0),
          'allures~=visits REJECTED on role reversal'),
    check(\+ rel_map(visits, allures), 'map refuses rejected pair'),
    t_caters(mira, london, unknown),
    t("Why?", noproof(_)),
    t("Does alba reach norway?", yes(_)),
    report_checks.

% ---------- hipotesis ----------
% Toda R nueva con par (persona?, ciudad?) se hipotetiza pendiente.
hypothesize(RNew, RKnown) :-
    retractall(hypothesis(RNew, RKnown, _, _)),
    assertz(hypothesis(RNew, RKnown, pending, 0.5)),
    format('Hypothesis: ~w ~~= ~w (structural confidence 0.5).~n',
           [RNew, RKnown]).

% city_role(X): X conocido como objeto de visits (ciudad del mundo).
city_role(X) :-
    memory_relation(_, visits, X, _, _), !.

% reversal: sujeto conocido-ciudad y objeto NO conocido-ciudad.
role_reversal(RNew) :-
    memory_relation(S, RNew, O, _, _),
    city_role(S),
    \+ city_role(O), !.

% ACEPTAR: sin inversion + cadena por `in` compartida (soporte).
accept_hypothesis(RNew, RKnown) :-
    \+ role_reversal(RNew),
    memory_relation(S, RNew, C, _, _),
    memory_relation(C, in, _, _, _),
    retractall(hypothesis(RNew, RKnown, _, _)),
    assertz(hypothesis(RNew, RKnown, accepted, 1.0)),
    assertz(rel_map(RKnown, RNew)),
    format('Accepted: ~w ~~= ~w (chain support).~n', [RNew, RKnown]).

% RECHAZAR: inversion de roles detectada; sin mapa, sin prediccion.
reject_counterevidence(RNew, RKnown) :-
    role_reversal(RNew),
    retractall(hypothesis(RNew, RKnown, _, _)),
    assertz(hypothesis(RNew, RKnown, rejected, 0.0)),
    format('Rejected: ~w ~~= ~w. Reason: role reversal ', [RNew, RKnown]),
    format('(subject is a known city, object is not).~n', []).

% ---------- caters por hipotesis aceptada ----------
ask_caters(P, K, yes(Proof)) :-
    hypothesis(RNew, visits, accepted, _),
    rel_map(visits, RNew),
    memory_relation(P, RNew, C, _, _),
    memory_relation(C, in, K, _, _), !,
    Proof = [hypothesis(RNew, accepted), map([visits-RNew]),
             rule(caters, [RNew, in]), (P, RNew, C), (C, in, K)].
ask_caters(_, _, unknown).

t_caters(P, K, yes) :-
    format('> caters ~w ~w?~n', [P, K]),
    ask_caters(P, K, A),
    say(A),
    remember_proof(A),
    ( A = yes(_) ->
        check(true, caters-yes)
    ; format('MISMATCH caters ~w ~w got ~w~n', [P, K, A]),
      check(false, caters-yes)).
t_caters(P, K, unknown) :-
    format('> caters ~w ~w?~n', [P, K]),
    ask_caters(P, K, A),
    say(A),
    remember_proof(A),
    ( A = unknown ->
        check(true, caters-unknown)
    ; format('MISMATCH caters ~w ~w got ~w~n', [P, K, A]),
      check(false, caters-unknown)).

% t/2, t_tell/2, remember_proof/1, say/1, check/2, report_checks/0
% heredados de conversation.pl / experiment46.pl.
