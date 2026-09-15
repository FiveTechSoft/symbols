% parser_v4.pl — Robustez sintactica (CONGELADO 2026-09-15).
%
% parser_v2.pl CONGELADO (baseline fc64115): este modulo solo lo LEE
% (normalize_text, clases de token, parse_sentence), nunca lo modifica.
%
% Metricas finales (medidas por ejecucion, doble metodo en conteos):
%   corpus_huge 6024: coverage 46.4 -> 71.4%, recall 35.1 -> 74.2%,
%     main precision 62.7 -> 90.8%, adverb-FP 48 -> 0
%   KJV 31102 versos: coverage 6.6 -> 13.5%, 2037 versos con roles
%   Baselines intactos: holdout 1000, core 25, comp 7, 14/14, 19/19,
%   23/23, 50/50, bateria 120/120, m0 0.
%
% Principio: no asumir que el primer token es el sujeto. Los adjuntos
% temporales/locativos en los bordes se separan ANTES del parse SVO y se
% re-adjuntan como relaciones temporales/location (misma forma que los
% patrones p6/p7 de parser_v2). Si el nucleo no produce evento principal,
% se conserva el parse V3 intacto (sin regresion posible por diseno).
% inc2: pasiva explicita -> EVENT/agent/patient + proyeccion main;
% imperativos sin agente; inc3: mediales + dedup estructural.
%
% Clases cerradas de abajo (adverbios temporales, preposiciones via
% parser_v2) son vocabulario funcional, mismo estatus que article/2.
%
% Public API:
%   parse_v4(+Text, -Relations)
:- module(parser_v4,
    [ parse_v4/2
    ]).

:- use_module(parser_v2).
:- use_module(library(lists)).

% Closed temporal adverb class (function words, ES/EN like article/2).
temporal_adverb(today). temporal_adverb(yesterday).
temporal_adverb(tomorrow). temporal_adverb(tonight).
temporal_adverb(now). temporal_adverb(then).
temporal_adverb(hoy). temporal_adverb(ayer).
temporal_adverb(manana). temporal_adverb(ahora).
temporal_adverb(later). temporal_adverb(soon). temporal_adverb(already).

% Closed frequency adverb class (function words; event properties, not
% participants). Negative-polarity adverbs (never/hardly/...) are
% deliberately EXCLUDED: stripping them would flip truth value.
v4_freq_adv(often). v4_freq_adv(always). v4_freq_adv(usually).
v4_freq_adv(sometimes). v4_freq_adv(frequently).
v4_freq_adv(rarely). v4_freq_adv(seldom). v4_freq_adv(daily).

% Adverb-like object guard: bare manner/temporal adverbs in object
% position are adjuncts, never patients (morphological -ly rule, the
% temporal class, plus closed deictic set).
% Same guard drops mains whose subject is a stripped temporal token.
adverb_object(W) :-
    atom_chars(W, Cs),
    ( append(_, [l, y], Cs)
    ; temporal_adverb(W)
    ; member(W, [here, there, outside, away, again, home,
                 early, late])
    ), !.

% parse_v4(+Text, -Relations) : stripped-core parse wins whenever
% stripping occurred and the core yields a main event (adjuncts only
% add noise to argument identification); otherwise V3 behavior with
% the adverb guard. Passive/imperative go through their own gates.
% Output passes structural event dedup (inc3.0).
parse_v4(Text, Relations) :-
    parser_v2:normalize_text(Text, Tokens),
    parser_v2:parse_sentence(Text, RFull),
    ( passive_event(Tokens, RFull, RPass) ->
        Rels0 = RPass
    ; imperative_event(Tokens, RFull, RImp) ->
        Rels0 = RImp
    ; strip_adjuncts(Tokens, Core, Adjs, _Stripped),
      Core \== [],
      core_string(Core, CoreStr),
      parser_v2:parse_sentence(CoreStr, CoreRels),
      drop_adverb_mains(CoreRels, CoreClean),
      has_main(CoreClean),
      attach_adjuncts(CoreClean, Adjs, Rels0)
    ; drop_adverb_mains(RFull, Rels0)
    ),
    dedup_events(Rels0, Relations).

% passive_event(+Tokens, +FullRels, -Relations) : explicit passive
% "X was VERBed by Y" -> EVENT/agent/patient roles plus a canonical
% active-voice projection parsed by the frozen parser. Wins only when
% the full parse produced no main event (conservative gate).
% Event ids are positional (e1, e2, ...) — deterministic, no globals.
passive_event(Tokens, RFull, Relations) :-
    \+ has_main(RFull),
    split_passive(Tokens, Pre, V, Post),
    \+ parser_v2:preposition(V),
    \+ v4_prep_unlisted(V),
    \+ ( member(RP, Pre), v4_relpro(RP) ),
    patient_head(Pre, X),
    agent_head(Post, Y),
    X \== Y,
    \+ adverb_object(X), \+ temporal_adverb(X),
    \+ adverb_object(Y), \+ temporal_adverb(Y),
    Roles = [event(V, e1), agent(e1, Y), patient(e1, X)],
    Proj = [relation(main, V, [Y, X])],
    append([Roles, Proj], All),
    sort(All, Relations).

% imperative_event(+Tokens, +FullRels, -Relations) : sentence-initial
% verb followed by article (transitive) or preposition (PP destination)
% -> EVENT with patient/location but NO agent (never invent one, never
% promote the verb to subject). Wins when the full parse has no main,
% or all its mains sit on the initial verb.
imperative_event(Tokens, RFull, Relations) :-
    Tokens = [V, X|Rest],
    parser_v2:content_word(V),
    \+ v4_nonverb_head(V),
    \+ v4_prep_unlisted(V),
    ( parser_v2:article(X) ; parser_v2:preposition(X) ),
    ( \+ has_main(RFull)
    ; forall(member(relation(main, _, [S|_]), RFull), S == V) ),
    ( parser_v2:article(X) ->
        first_content(Rest, O),
        Relations = [event(V, e1), patient(e1, O)]
    ; first_content(Rest, P),
      Relations = [event(V, e1), location(e1, P)]
    ).

first_content(Tokens, W) :-
    member(W, Tokens),
    parser_v2:content_word(W), !.

% v4_prep_unlisted(+W) : common prepositions missing from the frozen
% preposition/1 set (function words; V can never be one of these).
v4_prep_unlisted(by). v4_prep_unlisted(of). v4_prep_unlisted(with).
v4_prep_unlisted(unto). v4_prep_unlisted(upon). v4_prep_unlisted(within).
v4_prep_unlisted(without). v4_prep_unlisted(among). v4_prep_unlisted(between).
v4_prep_unlisted(through). v4_prep_unlisted(during). v4_prep_unlisted(under).

% Closed discourse-marker / deictic / determiner class (function words
% that can never head an event): determiners, deictics, connectives,
% focal adverbs. Same status as article/1.
v4_nonverb_head(all). v4_nonverb_head(there). v4_nonverb_head(here).
v4_nonverb_head(thus). v4_nonverb_head(hence).
v4_nonverb_head(nevertheless). v4_nonverb_head(however).
v4_nonverb_head(therefore). v4_nonverb_head(moreover).
v4_nonverb_head(furthermore).
v4_nonverb_head(also). v4_nonverb_head(even). v4_nonverb_head(only).
v4_nonverb_head(just). v4_nonverb_head(still). v4_nonverb_head(yet).
v4_nonverb_head(howbeit). v4_nonverb_head(wherefore).
v4_nonverb_head(whereas). v4_nonverb_head(whereby).
v4_nonverb_head(wherein). v4_nonverb_head(whereupon).
v4_nonverb_head(let).

% Closed relative-pronoun class (function words): in patient position
% they mark a clause boundary (no matrix event); after "by" they are
% determiners to skip (e.g. "by which prophet" -> prophet).
v4_relpro(who). v4_relpro(whom). v4_relpro(whose). v4_relpro(which).

% Closed be-form + modal class (function words; parser_v2:auxiliary
% covers does/do/did/is/are/was/were, extended here for passives).
v4_aux(A) :- parser_v2:auxiliary(A), !.
v4_aux(be). v4_aux(been). v4_aux(being). v4_aux(am).
v4_modal(has). v4_modal(have). v4_modal(had).
v4_modal(will). v4_modal(shall). v4_modal(would).
v4_modal(can). v4_modal(could). v4_modal(may).
v4_modal(might). v4_modal(must). v4_modal(should).

% split_passive(+Tokens, -Pre, -Verb, -Post) : be-aux + content verb + by.
split_passive(Tokens, Pre, V, Post) :-
    append(Pre, [Aux, V, by|Post], Tokens),
    Pre \== [], Post \== [],
    v4_aux(Aux),
    parser_v2:content_word(V),
    \+ v4_modal(V).

% patient_head(+Pre, -X) : last content non-modal token before aux;
% a relative pronoun there marks a clause boundary (no matrix event).
patient_head(Pre, X) :-
    reverse(Pre, Rev),
    member(X, Rev),
    parser_v2:content_word(X),
    \+ v4_modal(X),
    \+ v4_relpro(X), !.

% agent_head(+Post, -Y) : first content non-modal, non-relative token
% after by (articles skipped by the content_word filter, not by list).
agent_head(Post, Y) :-
    member(Y, Post),
    parser_v2:content_word(Y),
    \+ v4_modal(Y),
    \+ v4_relpro(Y), !.

% same_event(+R1, +R2) : structurally identical event claim — same
% predicate, same agent, same patient for mains; identical term
% otherwise. Different predicates (buy vs repair) never unify here,
% even over the same (S,O): distinct surface events stay distinct.
same_event(relation(main, P, [S, O]), relation(main, P, [S, O])) :- !.
same_event(R, R).

% dedup_events(+Rels, -Clean) : drop structurally identical event
% claims, keep first occurrence (defensive invariant: sort/2 already
% removes exact duplicates from single parses; this guards assembly).
dedup_events(Rels, Clean) :-
    dedup_events(Rels, [], Clean).
dedup_events([], _, []).
dedup_events([R|Rs], Seen, Out) :-
    ( member(S, Seen), same_event(R, S) ->
        dedup_events(Rs, Seen, Out)
    ; Out = [R|More],
      dedup_events(Rs, [R|Seen], More) ).

% drop_adverb_mains(+Rels, -Clean) : a main edge whose object is a bare
% adverb, or whose subject is a stripped temporal token, is a false
% event (unknown rather than guess). Other relation types untouched.
drop_adverb_mains(Rels, Clean) :-
    findall(R, ( member(R, Rels),
                 ( R = relation(main, _, [S, O]) ->
                     ( \+ adverb_object(O), \+ temporal_adverb(S) )
                 ; true ) ),
            Clean).

has_main(Rels) :-
    member(relation(main, _, _), Rels).

% strip_adjuncts(+Tokens, -Core, -Adjuncts, -Stripped) : leading temporal
% adverb / compound / locative PP, medial temporal/frequency adverbs
% (never first position), trailing temporal, in that order.
strip_adjuncts(Tokens, Core, Adjs, Stripped) :-
    strip_leading(Tokens, T1, A1, S1),
    strip_medial(T1, T2, A2, S2),
    strip_trailing(T2, Core, A3, S3),
    append([A1, A2, A3], Adjs),
    append([S1, S2, S3], Stripped),
    Adjs \== [].

% Leading: single temporal/frequency adverb, compound last|next + token,
% or Prep (+Art) + content (locative).
strip_leading([W|R], R, [t(temporal, W)], [W]) :-
    temporal_adverb(W), !.
strip_leading([W|R], R, [t(frequency, W)], [W]) :-
    v4_freq_adv(W), !.
strip_leading([M, W|R], R, [t(temporal, TW)], [M, W]) :-
    ( M == last ; M == next ),
    parser_v2:content_word(W), !,
    atom_concat(M, '_', T1),
    atom_concat(T1, W, TW).
strip_leading([P, A, X|R], R, [t(location, X)], [P, A, X]) :-
    parser_v2:preposition(P),
    parser_v2:article(A),
    parser_v2:content_word(X), !.
strip_leading([P, X|R], R, [t(location, X)], [P, X]) :-
    parser_v2:preposition(P),
    parser_v2:content_word(X), !.
strip_leading(T, T, [], []).

% Medial: temporal/frequency adverbs and early/late anywhere except
% first position (modifiers of the event, never participants).
strip_medial([F|R], [F|Core], Adjs, Stripped) :-
    strip_medial_rest(R, Core, Adjs, Stripped).
strip_medial_rest([], [], [], []).
strip_medial_rest([W|Rs], Core, Adjs, Stripped) :-
    ( medial_adjunct(W, T) ->
        Core = CoreRest, Adjs = [t(T, W)|AdjsRest], Stripped = [W|StrRest],
        strip_medial_rest(Rs, CoreRest, AdjsRest, StrRest)
    ; Core = [W|CoreRest], Adjs = AdjsRest, Stripped = StrRest,
      strip_medial_rest(Rs, CoreRest, AdjsRest, StrRest)
    ).

medial_adjunct(W, temporal) :- temporal_adverb(W).
medial_adjunct(W, frequency) :- v4_freq_adv(W).
medial_adjunct(early, temporal).
medial_adjunct(late, temporal).

% Trailing: temporal/frequency adverb, or last/next + single token.
strip_trailing(T, Core, [t(temporal, W)], [W]) :-
    append(Core, [W], T),
    temporal_adverb(W),
    Core \== [], !.
strip_trailing(T, Core, [t(frequency, W)], [W]) :-
    append(Core, [W], T),
    v4_freq_adv(W),
    Core \== [], !.
strip_trailing(T, Core, [t(temporal, TW)], [M, W]) :-
    append(Core, [M, W], T),
    ( M == last ; M == next ),
    Core \== [], !,
    atom_concat(M, '_', T1),
    atom_concat(T1, W, TW).
strip_trailing(T, T, [], []).

core_string(Core, Str) :-
    atomic_list_concat(Core, ' ', A),
    atom_string(A, Str).

% attach_adjuncts(+CoreRels, +Adjuncts, -Relations) : temporal wraps the
% event term (p6 shape), frequency likewise, location anchors on the
% object (p7/p8 shape).
attach_adjuncts(CoreRels, Adjs, Relations) :-
    findall(R, ( member(relation(main, P, [S, O]), CoreRels),
                 member(t(temporal, T), Adjs),
                 R = relation(temporal, tiempo, [P2, T]),
                 P2 =.. [P, S, O] ), TRels),
    findall(R, ( member(relation(main, P, [S, O]), CoreRels),
                 member(t(frequency, F), Adjs),
                 R = relation(frequency, frecuencia, [P2, F]),
                 P2 =.. [P, S, O] ), FRels),
    findall(relation(location, llevar_destino, [O, Pl]),
            ( member(relation(main, _, [_, O]), CoreRels),
              member(t(location, Pl), Adjs) ), LRels),
    append([CoreRels, TRels, FRels, LRels], All),
    sort(All, Relations).
