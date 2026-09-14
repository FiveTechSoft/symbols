% parser.pl — Structured parser for Spanish and English
% Pipeline: text → tokens → POS tags → phrases → structured relations
%
% Output:
%   sentence(subject(S), verb(V), object(O), attributes(Attrs), location(Loc), time(Time))
%
% Philosophy: the parser produces STRUCTURE, not knowledge.
% The attention layer selects which structures become knowledge.

:- consult('positional.pl').

% ── parse/2 ──────────────────────────────────────────────────────────
% parse(+Text, -Sentence)

parse(Text, Sentence) :-
    tokenize_pos(Text, Tokens),
    parse_tokens(Tokens, Sentence).

% ── parse_tokens/2 ───────────────────────────────────────────────────

parse_tokens(Tokens, sentence(subject(Subj), verb(Verb), object(Obj), attributes(Attrs), location(Loc), time(Time))) :-
    maplist(tag_token, Tokens, Tagged),
    extract_noun_phrases(Tagged, NPs),
    find_verb(Tagged, Verb, VerbIdx),
    find_subject(Tagged, NPs, VerbIdx, Subj),
    find_object(NPs, VerbIdx, Obj),
    extract_pp(Tagged, Attrs, Loc),
    extract_time(Tagged, Time), !.

% ── tag_token/2 ──────────────────────────────────────────────────────

tag_token(Token, tag(Token, det)) :-
    member(Token, [un, una, unos, unas, el, la, los, las, ese, esa, unos, unas]), !.
tag_token(Token, tag(Token, prep)) :-
    member(Token, [de, en, con, por, para, a, desde, hasta, sobre, bajo, entre, sin]), !.
tag_token(Token, tag(Token, conj)) :-
    member(Token, [y, o, pero, sino, que, como, cuando, donde, and, but, or]), !.
tag_token(Token, tag(Token, adv_time)) :-
    member(Token, [ayer, hoy, manana, ahora, siempre, nunca, antes, despues,
                   today, yesterday, tomorrow, now, always, never]), !.
tag_token(Token, tag(Token, adv)) :-
    member(Token, [muy, poco, mucho, nada, algo, todo, rapido, lento,
                   very, little, much, nothing, something, fast, slow]), !.
tag_token(Token, tag(Token, pron)) :-
    member(Token, [yo, tu, el, ella, nosotros, vosotros, ellos, ellas,
                   i, you, he, she, we, they]), !.
tag_token(Token, tag(Token, verb)) :-
    known_verb_lex(Token), !.
tag_token(Token, tag(Token, adj)) :-
    known_adjective(Token), !.
tag_token(Token, tag(Token, noun)) :-
    known_noun(Token), !.
tag_token(Token, tag(Token, unknown)).

% ── Lexicons (Spanish + English) ─────────────────────────────────────

% Verbs
known_verb_lex(compro). known_verb_lex(compra). known_verb_lex(comprar).
known_verb_lex(vendio). known_verb_lex(vende). known_verb_lex(vender).
known_verb_lex(esta). known_verb_lex(estar).
known_verb_lex(es). known_verb_lex(ser).
known_verb_lex(tiene). known_verb_lex(tuvo). known_verb_lex(tener).
known_verb_lex(fue). known_verb_lex(va). known_verb_lex(ir).
known_verb_lex(hizo). known_verb_lex(hace). known_verb_lex(hacer).
known_verb_lex(dijo). known_verb_lex(dice). known_verb_lex(decir).
known_verb_lex(vio). known_verb_lex(ve). known_verb_lex(ver).
known_verb_lex(salio). known_verb_lex(sale). known_verb_lex(salir).
known_verb_lex(entro). known_verb_lex(entra). known_verb_lex(entrar).
known_verb_lex(paso). known_verb_lex(pasa). known_verb_lex(pasar).
known_verb_lex(coro). known_verb_lex(corre). known_verb_lex(correr).
known_verb_lex(corrio). known_verb_lex(corria).
known_verb_lex(canto). known_verb_lex(canta). known_verb_lex(cantar).
known_verb_lex(bailo). known_verb_lex(baila). known_verb_lex(bailar).
known_verb_lex(cambia). known_verb_lex(cambiar).
known_verb_lex(abrio). known_verb_lex(abre). known_verb_lex(abrir).
known_verb_lex(cerro). known_verb_lex(cierra). known_verb_lex(cerrar).
known_verb_lex(comio). known_verb_lex(come). known_verb_lex(comer).
known_verb_lex(bebio). known_verb_lex(bebe). known_verb_lex(beber).
known_verb_lex(leo). known_verb_lex(lee). known_verb_lex(leer).
known_verb_lex(escribio). known_verb_lex(escribe). known_verb_lex(escribir).
known_verb_lex(penso). known_verb_lex(piensa). known_verb_lex(pensar).
known_verb_lex(sup). known_verb_lex(sabe). known_verb_lex(saber).
% English
known_verb_lex(followed). known_verb_lex(follow). known_verb_lex(follows).
known_verb_lex(ate). known_verb_lex(eat). known_verb_lex(eats).
known_verb_lex(drank). known_verb_lex(drink). known_verb_lex(drinks).
known_verb_lex(met). known_verb_lex(meet). known_verb_lex(meets).
known_verb_lex(played). known_verb_lex(play). known_verb_lex(plays).
known_verb_lex(wore). known_verb_lex(wear). known_verb_lex(wears).
known_verb_lex(wrote). known_verb_lex(write). known_verb_lex(writes).
known_verb_lex(came). known_verb_lex(come). known_verb_lex(comes).
known_verb_lex(went). known_verb_lex(go). known_verb_lex(goes).
known_verb_lex(saw). known_verb_lex(see). known_verb_lex(sees).
known_verb_lex(gave). known_verb_lex(give). known_verb_lex(gives).
known_verb_lex(took). known_verb_lex(take). known_verb_lex(takes).
known_verb_lex(found). known_verb_lex(find). known_verb_lex(finds).
known_verb_lex(knew). known_verb_lex(know). known_verb_lex(knows).
known_verb_lex(thought). known_verb_lex(think). known_verb_lex(thinks).
known_verb_lex(said). known_verb_lex(say). known_verb_lex(says).
known_verb_lex(made). known_verb_lex(make). known_verb_lex(makes).
known_verb_lex(ran). known_verb_lex(run). known_verb_lex(runs).
known_verb_lex(sang). known_verb_lex(sing). known_verb_lex(sings).
known_verb_lex(spoke). known_verb_lex(speak). known_verb_lex(speaks).
known_verb_lex(broke). known_verb_lex(break). known_verb_lex(breaks).
known_verb_lex(woke). known_verb_lex(wake). known_verb_lex(wakes).
known_verb_lex(grew). known_verb_lex(grow). known_verb_lex(grows).
known_verb_lex(fell). known_verb_lex(fall). known_verb_lex(falls).
known_verb_lex(rose). known_verb_lex(rise). known_verb_lex(rises).
known_verb_lex(sat). known_verb_lex(sit). known_verb_lex(sits).
known_verb_lex(stood). known_verb_lex(stand). known_verb_lex(stands).
known_verb_lex(held). known_verb_lex(hold). known_verb_lex(holds).
known_verb_lex(kept). known_verb_lex(keep). known_verb_lex(keeps).
known_verb_lex(left). known_verb_lex(leave). known_verb_lex(leaves).
known_verb_lex(lost). known_verb_lex(lose). known_verb_lex(loses).
known_verb_lex(paid). known_verb_lex(pay). known_verb_lex(pays).
known_verb_lex(read). known_verb_lex(put). known_verb_lex(set).
known_verb_lex(showed). known_verb_lex(show). known_verb_lex(shows).
known_verb_lex(told). known_verb_lex(tell). known_verb_lex(tells).
known_verb_lex(understood). known_verb_lex(understand).
known_verb_lex(worked). known_verb_lex(work). known_verb_lex(works).

% Nouns
known_noun(coche). known_noun(casa). known_noun(ciudad).
known_noun(libro). known_noun(mesa). known_noun(silla).
known_noun(perro). known_noun(gato). known_noun(pajaro).
known_noun(hombre). known_noun(mujer). known_noun(nino).
known_noun(profesor). known_noun(alumno). known_noun(medico).
known_noun(escuela). known_noun(hospital). known_noun(tienda).
known_noun(parque). known_noun(calle). known_noun(plaza).
known_noun(pais). known_noun(mundo). known_noun(tierra).
known_noun(tiempo). known_noun(dia). known_noun(noche).
known_noun(madrid). known_noun(barcelona). known_noun(spana).
known_noun(juan). known_noun(maria). known_noun(pedro).
known_noun(manzana). known_noun(arbol). known_noun(flor).
known_noun(carretera). known_noun(rio). known_noun(montana).
% English
known_noun(alice). known_noun(hatter). known_noun(queen).
known_noun(rabbit). known_noun(tea). known_noun(mushroom).
known_noun(coat). known_noun(hole). known_noun(croquet).
known_noun(car). known_noun(house). known_noun(school).
known_noun(tree). known_noun(flower). known_noun(river).
known_noun(mountain). known_noun(road). known_noun(book).

% Adjectives
known_adjective(rojo). known_adjective(azul). known_adjective(verde).
known_adjective(amarillo). known_adjective(negro). known_adjective(blanco).
known_adjective(grande). known_adjective(pequeno). known_adjective(largo).
known_adjective(corto). known_adjective(alto). known_adjective(bajo).
known_adjective(bueno). known_adjective(malo). known_adjective(nuevo).
known_adjective(viejo). known_adjective(bonito). known_adjective(fe0).
known_adjective(rapido). known_adjective(lento). known_adjective(fuerte).
known_adjective(debil). known_adjective(caliente). known_adjective(frio).
% English
known_adjective(red). known_adjective(blue). known_adjective(green).
known_adjective(yellow). known_adjective(white). known_adjective(black).
known_adjective(big). known_adjective(small). known_adjective(large).
known_adjective(long). known_adjective(short). known_adjective(tall).
known_adjective(good). known_adjective(bad). known_adjective(new).
known_adjective(old). known_adjective(beautiful). known_adjective(ugly).
known_adjective(fast). known_adjective(slow). known_adjective(strong).
known_adjective(weak). known_adjective(hot). known_adjective(cold).

% ── extract_noun_phrases/2 ──────────────────────────────────────────
% Pattern 1: det + noun + adj (NP with determiner)
% Pattern 2: adj + noun (bare NP without determiner)

extract_noun_phrases(Tagged, NPs) :-
    % Pattern 1: det + noun + adj
    findall(np(Head, Modifiers, StartIdx, EndIdx), (
        nth0(StartIdx, Tagged, tag(_, det)),
        nth0(NounIdx, Tagged, tag(Head, noun)),
        NounIdx > StartIdx,
        NounIdx - StartIdx =< 3,
        findall(Mod, (
            nth0(ModIdx, Tagged, tag(Mod, adj)),
            ModIdx > NounIdx,
            ModIdx - NounIdx =< 3
        ), Modifiers),
        ( Modifiers = [LastMod|_] ->
            nth0(EndIdx, Tagged, tag(LastMod, adj))
        ; EndIdx = NounIdx
        )
    ), DetNPs),
    % Pattern 2: adj + noun (no determiner, noun not already in DetNPs)
    findall(np(Head, [Mod], ModIdx, NounIdx), (
        nth0(ModIdx, Tagged, tag(Mod, adj)),
        nth0(NounIdx, Tagged, tag(Head, noun)),
        NounIdx > ModIdx,
        NounIdx - ModIdx =< 2,
        \+ member(np(Head, _, _, _), DetNPs)
    ), BareNPs),
    append(DetNPs, BareNPs, NPs), !.
extract_noun_phrases(_, []).

% ── find_verb/3 ──────────────────────────────────────────────────────

find_verb(Tagged, Verb, VerbIdx) :-
    nth0(VerbIdx, Tagged, tag(Verb, verb)), !.

% ── find_subject/4 ──────────────────────────────────────────────────

find_subject(Tagged, NPs, VerbIdx, Subj) :-
    include(np_before(VerbIdx), NPs, BeforeNPs),
    ( BeforeNPs = [np(Head, Mods, _, _)|_] ->
        ( Mods = [_|_] ->
            atomic_list_concat([Head|Mods], '_', Subj)
        ; Subj = Head
        )
    ; ( nth0(Idx, Tagged, tag(Subj, noun)),
        Idx < VerbIdx,
        VerbIdx - Idx =< 3 -> true
      ; Subj = '?unknown'
      )
    ), !.

np_before(VerbIdx, np(_, _, _, EndIdx)) :- EndIdx < VerbIdx.

% ── find_object/3 ───────────────────────────────────────────────────

find_object(NPs, VerbIdx, Obj) :-
    include(np_after(VerbIdx), NPs, AfterNPs),
    ( AfterNPs = [np(Head, Mods, _, _)|_] ->
        ( Mods = [_|_] ->
            atomic_list_concat([Head|Mods], '_', Obj)
        ; Obj = Head
        )
    ; Obj = '?what'
    ), !.

np_after(VerbIdx, np(_, _, StartIdx, _)) :- StartIdx > VerbIdx.

% ── extract_pp/3 ─────────────────────────────────────────────────────

extract_pp(Tagged, Attrs, Loc) :-
    findall(attr(Modified, Prep, Noun), (
        nth0(PrepIdx, Tagged, tag(Prep, prep)),
        nth0(NounIdx, Tagged, tag(Noun, noun)),
        NounIdx > PrepIdx,
        NounIdx - PrepIdx =< 3,
        ( nth0(ModIdx, Tagged, tag(Modified, noun)),
          ModIdx < PrepIdx,
          PrepIdx - ModIdx =< 3 -> true
        ; Modified = '?'
        )
    ), AllAttrs),
    ( member(attr(_, en, Loc), AllAttrs) ->
        select(attr(_, en, Loc), AllAttrs, Attrs)
    ; Loc = '?',
      Attrs = AllAttrs
    ), !.
extract_pp(_, [], '?').

% ── extract_time/2 ──────────────────────────────────────────────────

extract_time(Tagged, Time) :-
    ( member(tag(Time, adv_time), Tagged) -> true ; Time = '?' ), !.
extract_time(_, '?').
