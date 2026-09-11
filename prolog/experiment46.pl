% experiment46.pl
% EXPERIMENT 46 - CONVERSATIONAL MEMORY (ciclo de vida)
% tell/1 introduce hechos en lenguaje natural durante la conversacion;
% alimentan la MISMA memoria y abstracciones del corpus (load_demo de
% conversation.pl, sin redescubrir nada). ask/2 responde con prueba o
% UNKNOWN. Ciclo: experiencia -> memoria -> razonamiento -> respuesta
% -> experiencia nueva. Ningun modulo tocado (solo este fichero).
:- consult('conversation.pl').

:- use_module(library(lists)).

% tell(+SentenceString, -Result): parse + recuerda, o no entiende.
tell(S, learned(Triple)) :-
    symbolize_natural(S, (Sub, Rel, Obj)), !,
    Triple = (Sub, Rel, Obj),
    remember_relation(Sub, Rel, Obj, 1.0),
    writeln('Learned.').
tell(_, not_understood) :-
    writeln('I didn\'t understand that.').

dialogue46 :-
    load_demo,
    nl, writeln('===== DIALOGUE 46 (told facts feed abstractions) ====='),
    t_tell("John lives in Madrid.", learned((john, lives_in, madrid))),
    t("Where does John live?", some(_, _, _)),
    t_tell("John visits Paris.", learned((john, visits, paris))),
    t("Does John visit Paris?", yes(_)),
    t("Does John reach France?", yes(_)),
    t("Why?", proof(_)),
    t("Does John reach London?", no),
    t_tell("John visits Roma.", learned((john, visits, roma))),
    t_has("Where does John reach?", france),
    t_has("Where does John reach?", italy),
    t("Where does John live?", some(_, _, _)),
    t("Does zorin visit madrid?", unknown),
    t_tell("John sings loudly.", not_understood),
    t("Does John reach London?", no),
    report_checks.

t_tell(S, Expected) :-
    format('> ~w~n', [S]),
    tell(S, R),
    ( R == Expected ->
        check(true, S)
    ; format('MISMATCH got ~w want ~w~n', [R, Expected]),
      check(false, S)).

% t/2, t_has/2, match_expected/2, check/2, report_checks/0 heredados
% de conversation.pl (mismo dialogo, nueva experiencia).
