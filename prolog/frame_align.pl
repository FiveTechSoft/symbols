% frame_align.pl
% Alineacion de marcos linguisticos via eventos reificados.
% Parser: UNICAMENTE estructura posicional + preposiciones 'to'/'from'
% (closed-class declarado). Roles: subj/obj/Prep tal cual aparecen;
% NADA especifico de verbo. La correspondencia entre marcos se DESCUBRE
% alineando eventos con el mismo conjunto de participantes.
% Evento: ev_N + hechos (ev, verb/subj/obj/Prep, valor).
:- use_module(library(lists)).

:- dynamic event_counter/1.
:- dynamic frame_map/4.
% frame_map(VerbA, SlotA, VerbB, SlotB): el slot A equivale al slot B.

reset_frames :-
    retractall(event_counter(_)),
    retractall(frame_map(_, _, _, _)).

next_event(E) :-
    ( retract(event_counter(N)) -> true ; N = 0 ),
    N1 is N + 1,
    assertz(event_counter(N1)),
    atomic_list_concat([ev_, N1], E).

% parse_event(+Tokens, -E): [S,V,O,Prep,P] con Prep en {to,from}.
parse_event(Tokens, E) :-
    Tokens = [S, V, O, Prep, P],
    member(Prep, [to, from]),
    \+ member(V, [is, a]),
    next_event(E),
    remember_relation(E, verb, V, 1.0),
    remember_relation(E, subj, S, 1.0),
    remember_relation(E, obj, O, 1.0),
    remember_relation(E, Prep, P, 1.0).

% participants(+E, -SortedSet)
participants(E, Ps) :-
    findall(X, ( memory_relation(E, R, X, _, _),
                 member(R, [subj, obj, to, from])
               ),
            X0),
    sort(X0, Ps).

% merge_events: mismo conjunto de participantes -> mismo evento.
% Devuelve lista de pares fusionados. Sin fusiones inventadas.
merge_events(Merged) :-
    findall(E, memory_relation(E, verb, _, _, _), E0),
    sort(E0, Es),
    merge_pairs(Es, Merged).

merge_pairs([], []).
merge_pairs([E|Es], Merged) :-
    participants(E, P),
    findall(M, ( member(M, Es),
                 participants(M, P)
               ),
            Same),
    ( Same == [] ->
        merge_pairs(Es, Merged)
    ; Merged = [(E, Same)|Rest],
      merge_pairs(Es, Rest)
    ).

% rewrite_merged(+Pairs): reescribe hechos al representante (minimo).
rewrite_merged(Pairs) :-
    forall(member((E, Same), Pairs),
           ( sort([E|Same], [Rep|_]),
             forall(member(M, Same),
                    ( forall(memory_relation(M, R, O, W, U),
                             ( memory_relation(Rep, R, O, _, _) -> true
                             ; assertz(memory_relation(Rep, R, O, W, U))
                             )),
                      retractall(memory_relation(M, _, _, _, _))
                    ))
           )).

% induce_alignment: por cada par fusionado, correspondencias por valor.
% Solo hechos con soporte en TODOS los pares fusionados.
induce_alignment :-
    retractall(frame_map(_, _, _, _)),
    merge_events(Merged),
    findall(EvPair, member(EvPair, Merged), Pairs),
    Pairs \== [],
    pairs_slots(Pairs, SlotPairs),
    sort(SlotPairs, Unique),
    forall(member((VA, SA, VB, SB), Unique),
           ( assertz(frame_map(VA, SA, VB, SB)),
             assertz(frame_map(VB, SB, VA, SA))
           )).

% slots alineados por evento: mismo valor en ambos marcos del par.
pairs_slots(Pairs, SlotPairs) :-
    findall((VA, SA, VB, SB),
            ( member((E, Same), Pairs),
              member(M, Same),
              memory_relation(E, verb, VA, _, _),
              memory_relation(M, verb, VB, _, _),
              VA \== VB,
              memory_relation(E, SA, X, _, _),
              role_slot(SA),
              memory_relation(M, SB, X, _, _),
              role_slot(SB)
            ),
            SlotPairs).

role_slot(subj). role_slot(obj). role_slot(to). role_slot(from).

% show_alignment: informe legible.
show_alignment :-
    nl, writeln('===== FRAME ALIGNMENT ====='),
    forall(frame_map(VA, SA, VB, SB),
           format('~w.~w <-> ~w.~w~n', [VA, SA, VB, SB])).

% query_event(+Verb, +Slots, -E): Slots = [(Slot,Value)...] en ese marco.
% Si el verbo no es el almacenado, traduce via frame_map (1 paso).
query_event(Verb, Slots, E) :-
    memory_relation(E, verb, Verb, _, _),
    slots_match(E, Slots), !.
query_event(Verb, Slots, E) :-
    memory_relation(E, verb, Stored, _, _),
    Stored \== Verb,
    slots_translate(Verb, Slots, Stored, TSlots),
    slots_match(E, TSlots).

slots_match(_, []).
slots_match(E, [(S, X)|Ss]) :-
    memory_relation(E, S, X, _, _),
    slots_match(E, Ss).

slots_translate(VA, Slots, VB, TSlots) :-
    map_slots(VA, VB, Slots, TSlots),
    length(TSlots, N),
    length(Slots, N),
    N > 0.

% map_slots SIN findall: findall copiaria las variables respuesta y las
% desconectaria del llamante. Mapeo 1:1 directo (corte: primer mapa vale).
map_slots(_, _, [], []).
map_slots(VA, VB, [(SA, X)|Ss], [(SB, X)|Ts]) :-
    frame_map(VA, SA, VB, SB), !,
    map_slots(VA, VB, Ss, Ts).
