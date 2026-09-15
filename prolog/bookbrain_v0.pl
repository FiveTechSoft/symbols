% bookbrain_v0.pl — ingest a clause file through parser_v4 -> memfact/5 KB
:- use_module(parser_v2).
:- use_module(parser_v4).
:- use_module(library(lists)).

:- dynamic memfact/5.
:- dynamic bb0_dropped/3.

% bb0_particle: adverb/particle tokens that adjunct the verb, never
% participants (measured garbage: sent->out, compassed->about).
bb0_particle(out). bb0_particle(about). bb0_particle(up). bb0_particle(down).
bb0_particle(forth). bb0_particle(off). bb0_particle(away). bb0_particle(hence).
bb0_particle(hither). bb0_particle(thither). bb0_particle(aside).
bb0_particle(apart). bb0_particle(astray). bb0_particle(afore).

% bb0_verbform: verb shapes that surface as S/O when the real
% participant is outside the clause (measured: went->joppa,
% shipmaster->said). Closed irregular list + safe KJV suffix rules
% ('eth' has no noun collisions; -ed needs length>=5 so seed/need/
% deed survive; NO -ing rule: king/thing/morning are nouns).
bb0_verbform(W) :- bb0_irregular(W), !.
bb0_verbform(W) :- atom_chars(W, Cs), append(_, [e,t,h], Cs), !.
bb0_verbform(W) :- atom_chars(W, Cs), append(_, [e,d], Cs),
                   atom_length(W, L), L >= 5, !.

bb0_irregular(went). bb0_irregular(said). bb0_irregular(came).
bb0_irregular(saw). bb0_irregular(took). bb0_irregular(gave).
bb0_irregular(made). bb0_irregular(found). bb0_irregular(got).
bb0_irregular(stood). bb0_irregular(fell). bb0_irregular(ate).
bb0_irregular(did). bb0_irregular(knew). bb0_irregular(thought).
bb0_irregular(brought). bb0_irregular(spake). bb0_irregular(sent).
bb0_irregular(kept). bb0_irregular(held). bb0_irregular(told).
bb0_irregular(fled). bb0_irregular(dwelt). bb0_irregular(rose).
bb0_irregular(ran). bb0_irregular(flew). bb0_irregular(chose).
bb0_irregular(drew). bb0_irregular(drank). bb0_irregular(woke).

bb0_ok(S, _V, O) :-
    \+ parser_v2:r5_function(S),
    \+ parser_v2:r5_function(O),
    \+ bb0_particle(S), \+ bb0_particle(O),
    \+ bb0_verbform(S), \+ bb0_verbform(O).

bb0(In, Out) :-
    retractall(memfact(_,_,_,_,_)),
    retractall(bb0_dropped(_,_,_)),
    setup_call_cleanup(open(In, read, S1),
        ( repeat,
            read_line_to_string(S1, L0),
            ( L0 == end_of_file -> !, true
            ; ( split_string(L0, "\t", "", [_, Text]) -> true ; Text = L0 ),
              ( parser_v4:parse_v4(Text, Rs) -> true ; Rs = [] ),
              forall(member(relation(main, V, [S, O]), Rs),
                     ( O == unknown -> true
                     ; \+ bb0_ok(S, V, O) -> assertz(bb0_dropped(S, V, O))
                     ; assertz(memfact(S, V, O, 1.0, bb0)) )),
              fail )
        ),
        close(S1)),
    setup_call_cleanup(open(Out, write, S2),
        forall(memfact(A, R, O, W, U),
               format(S2, 'memfact(~q,~q,~q,~q,~q).~n', [A, R, O, W, U])),
        close(S2)),
    aggregate_all(count, memfact(_,_,_,_,_), N),
    aggregate_all(count, bb0_dropped(_,_,_), D),
    format('BOOKBRAIN-V0 facts=~w dropped=~w -> ~w~n', [N, D, Out]),
    halt.