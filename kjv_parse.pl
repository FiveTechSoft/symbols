% kjv_parse.pl
% Simbolizador KJV: verbo abierto arcaico + slots posicionales NEAREST.
% NO redefine ni toca predicados de otros ficheros (regla de hierro):
% solo predicados nuevos kjv_*. Reutiliza de open_vocab.pl el tokenizador,
% skip_word/aux_skip/verb_form_word/typename y el mapeo pronominal.
% Una frase KJV larga ("In the beginning God created the heaven") no cabe
% en los slots singleton estrictos: S = contenido mas cercano antes del
% verbo (el mas a la derecha), O = contenido mas cercano despues (el mas
% a la izquierda). Anadir una familia verbal = solo hechos kjv_verb/2.
:- consult('open_vocab.pl').
:- use_module(library(lists)).

% ===== familias verbales KJV: kjv_verb(Canonico, Forma) =====
% Habla / decir
kjv_verb(say, say). kjv_verb(say, says). kjv_verb(say, said).
kjv_verb(say, saith). kjv_verb(say, sayeth). kjv_verb(say, sayest).
kjv_verb(speak, speak). kjv_verb(speak, speaks). kjv_verb(speak, spake).
kjv_verb(speak, spoke). kjv_verb(speak, spoken). kjv_verb(speak, speaketh).
kjv_verb(answer, answer). kjv_verb(answer, answered). kjv_verb(answer, answereth).
kjv_verb(command, command). kjv_verb(command, commanded). kjv_verb(command, commandeth).
kjv_verb(ask, ask). kjv_verb(ask, asked). kjv_verb(ask, asketh).
kjv_verb(call, call). kjv_verb(call, called). kjv_verb(call, calleth).
kjv_verb(cry, cry). kjv_verb(cry, cried). kjv_verb(cry, crieth).
kjv_verb(proclaim, proclaim). kjv_verb(proclaim, proclaimed).
kjv_verb(declare, declare). kjv_verb(declare, declared).
kjv_verb(testify, testify). kjv_verb(testify, testified).
kjv_verb(preach, preach). kjv_verb(preach, preached).
kjv_verb(prophesy, prophesy). kjv_verb(prophesy, prophesied).
kjv_verb(teach, teach). kjv_verb(teach, teacheth). kjv_verb(teach, taught).
kjv_verb(pray, pray). kjv_verb(pray, prayed). kjv_verb(pray, prayeth).
kjv_verb(confess, confess). kjv_verb(confess, confessed).
kjv_verb(swear, swear). kjv_verb(swear, sware). kjv_verb(swear, sworn). kjv_verb(swear, sweareth).
kjv_verb(vow, vow). kjv_verb(vow, vowed).
kjv_verb(promise, promise). kjv_verb(promise, promised).
kjv_verb(complain, complain). kjv_verb(complain, complained). kjv_verb(complain, murmured).
% Crear / engendrar / nacer
kjv_verb(create, create). kjv_verb(create, created). kjv_verb(create, createth).
kjv_verb(make, make). kjv_verb(make, maketh). kjv_verb(make, made). kjv_verb(make, madest).
kjv_verb(form, form). kjv_verb(form, formed).
kjv_verb(beget, beget). kjv_verb(beget, begat). kjv_verb(beget, begot). kjv_verb(beget, begotten).
kjv_verb(bear, bear). kjv_verb(bear, beareth). kjv_verb(bear, bare).
kjv_verb(bear, bore). kjv_verb(bear, born).
kjv_verb(conceive, conceive). kjv_verb(conceive, conceived).
% Quitar / matar / destruir
kjv_verb(kill, kill). kjv_verb(kill, killed). kjv_verb(kill, killeth).
kjv_verb(kill, slay). kjv_verb(kill, slayeth). kjv_verb(kill, slew). kjv_verb(kill, slain).
kjv_verb(smite, smite). kjv_verb(smite, smiteth). kjv_verb(smite, smote). kjv_verb(smite, smitten).
kjv_verb(destroy, destroy). kjv_verb(destroy, destroyed). kjv_verb(destroy, destroyeth).
kjv_verb(burn, burn). kjv_verb(burn, burned). kjv_verb(burn, burnt). kjv_verb(burn, burneth).
kjv_verb(stone, stone). kjv_verb(stone, stoned).
kjv_verb(crucify, crucify). kjv_verb(crucify, crucified).
kjv_verb(betray, betray). kjv_verb(betray, betrayed).
kjv_verb(deny, deny). kjv_verb(deny, denied).
kjv_verb(forsake, forsake). kjv_verb(forsake, forsook). kjv_verb(forsake, forsaken).
kjv_verb(steal, steal). kjv_verb(steal, stole). kjv_verb(steal, stolen).
kjv_verb(sin, sin). kjv_verb(sin, sinned).
% Habitar / reinar / juzgar
kjv_verb(dwell, dwell). kjv_verb(dwell, dwelleth). kjv_verb(dwell, dwelt). kjv_verb(dwell, dwellest).
kjv_verb(abide, abide). kjv_verb(abide, abode).
kjv_verb(reign, reign). kjv_verb(reign, reigned). kjv_verb(reign, reigneth).
kjv_verb(rule, rule). kjv_verb(rule, ruled). kjv_verb(rule, ruleth).
kjv_verb(judge, judge). kjv_verb(judge, judged). kjv_verb(judge, judgeth).
kjv_verb(choose, choose). kjv_verb(choose, chose). kjv_verb(choose, chosen). kjv_verb(choose, chooseth).
kjv_verb(appoint, appoint). kjv_verb(appoint, appointed).
kjv_verb(ordain, ordain). kjv_verb(ordain, ordained).
kjv_verb(anoint, anoint). kjv_verb(anoint, anointed).
% Dar / tomar / llevar
kjv_verb(give, give). kjv_verb(give, giveth). kjv_verb(give, gave). kjv_verb(give, gavest). kjv_verb(give, given).
kjv_verb(take, take). kjv_verb(take, taketh). kjv_verb(take, took). kjv_verb(take, takest). kjv_verb(take, taken).
kjv_verb(bring, bring). kjv_verb(bring, bringeth). kjv_verb(bring, brought).
kjv_verb(carry, carry). kjv_verb(carry, carried).
kjv_verb(lay, lay). kjv_verb(lay, layeth). kjv_verb(lay, laid). kjv_verb(lay, lain).
kjv_verb(put, put). kjv_verb(put, putteth).
kjv_verb(set, set). kjv_verb(set, setteth).
kjv_verb(lift, lift). kjv_verb(lift, lifted).
kjv_verb(cast, cast). kjv_verb(cast, casteth).
kjv_verb(pour, pour). kjv_verb(pour, poured).
kjv_verb(lend, lend). kjv_verb(lend, lent).
kjv_verb(pay, pay). kjv_verb(pay, paid).
% Ir / venir / moverse
kjv_verb(go, go). kjv_verb(go, goeth). kjv_verb(go, goest). kjv_verb(go, went). kjv_verb(go, gone).
kjv_verb(come, come). kjv_verb(come, cometh). kjv_verb(come, came). kjv_verb(come, comest).
kjv_verb(send, send). kjv_verb(send, sendeth). kjv_verb(send, sent).
kjv_verb(return, return). kjv_verb(return, returned). kjv_verb(return, returneth).
kjv_verb(turn, turn). kjv_verb(turn, turned). kjv_verb(turn, turneth).
kjv_verb(pass, pass). kjv_verb(pass, passed). kjv_verb(pass, passeth).
kjv_verb(arise, arise). kjv_verb(arise, ariseth). kjv_verb(arise, arose). kjv_verb(arise, arisen).
kjv_verb(rise, rise). kjv_verb(rise, riseth). kjv_verb(rise, rose). kjv_verb(rise, risen).
kjv_verb(awake, awake). kjv_verb(awake, awaketh). kjv_verb(awake, awoke).
kjv_verb(sleep, sleep). kjv_verb(sleep, sleepeth). kjv_verb(sleep, slept).
kjv_verb(walk, walk). kjv_verb(walk, walked).
kjv_verb(run, run). kjv_verb(run, ran).
kjv_verb(ride, ride). kjv_verb(ride, rode).
kjv_verb(ascend, ascend). kjv_verb(ascend, ascended).
kjv_verb(descend, descend). kjv_verb(descend, descended).
kjv_verb(enter, enter). kjv_verb(enter, entered). kjv_verb(enter, entereth).
kjv_verb(depart, depart). kjv_verb(depart, departed).
kjv_verb(flee, flee). kjv_verb(flee, fled).
kjv_verb(escape, escape). kjv_verb(escape, escaped).
kjv_verb(pursue, pursue). kjv_verb(pursue, pursued).
kjv_verb(follow, follow). kjv_verb(follow, followed).
kjv_verb(lead, lead). kjv_verb(lead, led). kjv_verb(lead, leadeth).
kjv_verb(guide, guide). kjv_verb(guide, guided).
kjv_verb(gather, gather). kjv_verb(gather, gathered).
kjv_verb(assemble, assemble). kjv_verb(assemble, assembled).
kjv_verb(scatter, scatter). kjv_verb(scatter, scattered).
kjv_verb(fight, fight). kjv_verb(fight, fought).
kjv_verb(prevail, prevail). kjv_verb(prevail, prevailed).
% Percibir / saber
kjv_verb(see, see). kjv_verb(see, seeth). kjv_verb(see, saw). kjv_verb(see, sawest). kjv_verb(see, seen).
kjv_verb(behold, behold).
kjv_verb(look, look). kjv_verb(look, looked). kjv_verb(look, looketh).
kjv_verb(hear, hear). kjv_verb(hear, heareth). kjv_verb(hear, heard). kjv_verb(hear, heardest).
kjv_verb(hearken, hearken). kjv_verb(hearken, hearkened).
kjv_verb(know, know). kjv_verb(know, knoweth). kjv_verb(know, knew). kjv_verb(know, known). kjv_verb(know, knowest).
kjv_verb(understand, understand). kjv_verb(understand, understood).
kjv_verb(remember, remember). kjv_verb(remember, remembered). kjv_verb(remember, remembereth).
kjv_verb(forget, forget). kjv_verb(forget, forgot). kjv_verb(forget, forgotten).
kjv_verb(seek, seek). kjv_verb(seek, seeketh). kjv_verb(seek, sought).
kjv_verb(find, find). kjv_verb(find, findeth). kjv_verb(find, found).
kjv_verb(search, search). kjv_verb(search, searched).
kjv_verb(dream, dream). kjv_verb(dream, dreamed).
kjv_verb(watch, watch). kjv_verb(watch, watched).
% Bendecir / amar / servir / adorar
kjv_verb(bless, bless). kjv_verb(bless, blessed). kjv_verb(bless, blesseth).
kjv_verb(curse, curse). kjv_verb(curse, cursed). kjv_verb(curse, curseth).
kjv_verb(love, love). kjv_verb(love, loved). kjv_verb(love, loveth).
kjv_verb(hate, hate). kjv_verb(hate, hated).
kjv_verb(fear, fear). kjv_verb(fear, feared).
kjv_verb(trust, trust). kjv_verb(trust, trusted).
kjv_verb(obey, obey). kjv_verb(obey, obeyed).
kjv_verb(serve, serve). kjv_verb(serve, served).
kjv_verb(worship, worship). kjv_verb(worship, worshipped).
kjv_verb(thank, thank). kjv_verb(thank, thanked).
kjv_verb(sing, sing). kjv_verb(sing, sang). kjv_verb(sing, sung).
kjv_verb(rejoice, rejoice). kjv_verb(rejoice, rejoiced).
kjv_verb(weep, weep). kjv_verb(weep, wept).
kjv_verb(mourn, mourn). kjv_verb(mourn, mourned).
kjv_verb(laugh, laugh). kjv_verb(laugh, laughed).
kjv_verb(dance, dance). kjv_verb(dance, danced).
% Ofrecer / sacrificar / salvar / perdonar
kjv_verb(offer, offer). kjv_verb(offer, offered). kjv_verb(offer, offereth).
kjv_verb(sacrifice, sacrifice). kjv_verb(sacrifice, sacrificed).
kjv_verb(deliver, deliver). kjv_verb(deliver, delivered).
kjv_verb(save, save). kjv_verb(save, saved). kjv_verb(save, saveth).
kjv_verb(redeem, redeem). kjv_verb(redeem, redeemed).
kjv_verb(forgive, forgive). kjv_verb(forgive, forgave). kjv_verb(forgive, forgiven).
kjv_verb(pardon, pardon). kjv_verb(pardon, pardoned).
kjv_verb(spare, spare). kjv_verb(spare, spared).
kjv_verb(pity, pity). kjv_verb(pity, pitied).
kjv_verb(avenge, avenge). kjv_verb(avenge, avenged).
kjv_verb(reward, reward). kjv_verb(reward, rewarded).
kjv_verb(accuse, accuse). kjv_verb(accuse, accused).
kjv_verb(condemn, condemn). kjv_verb(condemn, condemned).
kjv_verb(justify, justify). kjv_verb(justify, justified).
kjv_verb(repent, repent). kjv_verb(repent, repented).
kjv_verb(tempt, tempt). kjv_verb(tempt, tempted).
kjv_verb(suffer, suffer). kjv_verb(suffer, suffered).
kjv_verb(persecute, persecute). kjv_verb(persecute, persecuted).
kjv_verb(afflict, afflict). kjv_verb(afflict, afflicted).
kjv_verb(comfort, comfort). kjv_verb(comfort, comforted).
kjv_verb(strengthen, strengthen). kjv_verb(strengthen, strengthened).
kjv_verb(harden, harden). kjv_verb(harden, hardened).
% Construir / romper / llenar
kjv_verb(build, build). kjv_verb(build, buildeth). kjv_verb(build, built). kjv_verb(build, builded).
kjv_verb(break, break). kjv_verb(break, breaketh). kjv_verb(break, broke). kjv_verb(break, broken).
kjv_verb(divide, divide). kjv_verb(divide, divided).
kjv_verb(fill, fill). kjv_verb(fill, filled). kjv_verb(fill, filleth).
kjv_verb(cover, cover). kjv_verb(cover, covered).
kjv_verb(open, open). kjv_verb(open, opened). kjv_verb(open, openeth).
kjv_verb(shut, shut).
kjv_verb(finish, finish). kjv_verb(finish, finished).
kjv_verb(fulfil, fulfil). kjv_verb(fulfil, fulfilled). kjv_verb(fulfil, fulfill).
kjv_verb(cease, cease). kjv_verb(cease, ceased).
kjv_verb(rest, rest). kjv_verb(rest, rested).
kjv_verb(sow, sow). kjv_verb(sow, sowed). kjv_verb(sow, sown).
kjv_verb(reap, reap). kjv_verb(reap, reaped).
kjv_verb(dig, dig). kjv_verb(dig, digged).
kjv_verb(water, water). kjv_verb(water, watered).
kjv_verb(sprinkle, sprinkle). kjv_verb(sprinkle, sprinkled).
kjv_verb(wash, wash). kjv_verb(wash, washed).
kjv_verb(touch, touch). kjv_verb(touch, touched).
kjv_verb(kiss, kiss). kjv_verb(kiss, kissed).
kjv_verb(embrace, embrace). kjv_verb(embrace, embraced).
kjv_verb(meet, meet). kjv_verb(meet, met).
% Comer / beber / morir / enterrar
kjv_verb(drink, drink). kjv_verb(drink, drinketh). kjv_verb(drink, drank). kjv_verb(drink, drunk).
kjv_verb(die, die). kjv_verb(die, died). kjv_verb(die, dieth).
kjv_verb(bury, bury). kjv_verb(bury, buried).
kjv_verb(marry, marry). kjv_verb(marry, married).
kjv_verb(divorce, divorce). kjv_verb(divorce, divorced).
kjv_verb(cleave, cleave). kjv_verb(cleave, clave).
kjv_verb(lie, lie). kjv_verb(lie, lied). kjv_verb(lie, lay). kjv_verb(lie, laid). kjv_verb(lie, lain).
kjv_verb(stand, stand). kjv_verb(stand, standeth). kjv_verb(stand, stood).
kjv_verb(sit, sit). kjv_verb(sit, sat).
kjv_verb(fall, fall). kjv_verb(fall, falleth). kjv_verb(fall, fell). kjv_verb(fall, fallen).
kjv_verb(leave, leave). kjv_verb(leave, left).
kjv_verb(continue, continue). kjv_verb(continue, continued).
kjv_verb(endure, endure). kjv_verb(endure, endured).
% Poseer / contar / escribir / sellar
kjv_verb(possess, possess). kjv_verb(possess, possessed).
kjv_verb(inherit, inherit). kjv_verb(inherit, inherited).
kjv_verb(obtain, obtain). kjv_verb(obtain, obtained).
kjv_verb(lose, lose). kjv_verb(lose, lost).
kjv_verb(sell, sell). kjv_verb(sell, sold).
kjv_verb(count, count). kjv_verb(count, counted).
kjv_verb(number, number). kjv_verb(number, numbered).
kjv_verb(measure, measure). kjv_verb(measure, measured).
kjv_verb(weigh, weigh). kjv_verb(weigh, weighed).
kjv_verb(write, write). kjv_verb(write, writeth). kjv_verb(write, wrote). kjv_verb(write, written).
kjv_verb(seal, seal). kjv_verb(seal, sealed).
kjv_verb(hope, hope). kjv_verb(hope, hoped).
kjv_verb(wait, wait). kjv_verb(wait, waited).
kjv_verb(desire, desire). kjv_verb(desire, desired).
kjv_verb(covet, covet). kjv_verb(covet, coveted).
kjv_verb(commit, commit). kjv_verb(commit, committed).
kjv_verb(prove, prove). kjv_verb(prove, proved).
kjv_verb(try, try). kjv_verb(try, tried).

% ===== auxiliares arcaicos (nunca contenido, nunca verbo) =====
kjv_aux(hath). kjv_aux(hast). kjv_aux(doth). kjv_aux(dost). kjv_aux(didst).
kjv_aux(art). kjv_aux(wert). kjv_aux(be). kjv_aux(am). kjv_aux(are).
kjv_aux(were). kjv_aux(been). kjv_aux(being).

% ===== clase funcional cerrada KJV (deixis, modales, prep, conj, numeros) =====
% Pronombres arcaicos y deicticos sin resolucion: se saltan (rechazo honesto
% si la frase los necesita como unico argumento).
kjv_skip(thou). kjv_skip(thee). kjv_skip(thy). kjv_skip(thine). kjv_skip(ye).
kjv_skip(you). kjv_skip(your). kjv_skip(yours).
kjv_skip(i). kjv_skip(me). kjv_skip(my). kjv_skip(mine).
kjv_skip(we). kjv_skip(us). kjv_skip(our). kjv_skip(ours).
kjv_skip(it). kjv_skip(its). kjv_skip(this). kjv_skip(that).
kjv_skip(these). kjv_skip(those). kjv_skip(which). kjv_skip(who).
kjv_skip(whom). kjv_skip(whose).
kjv_skip(what). kjv_skip(whether).
% Modales y auxiliares modernos
kjv_skip(will). kjv_skip(shall). kjv_skip(may). kjv_skip(might).
kjv_skip(must). kjv_skip(can). kjv_skip(could). kjv_skip(would).
kjv_skip(should). kjv_skip(do). kjv_skip(did). kjv_skip(does). kjv_skip(done).
kjv_skip(let).
% Negacion y adverbios funcionales
kjv_skip(not). kjv_skip(nor). kjv_skip(never). kjv_skip(ever).
kjv_skip(so). kjv_skip(very). kjv_skip(too). kjv_skip(also). kjv_skip(even).
kjv_skip(still). kjv_skip(yet). kjv_skip(now). kjv_skip(then).
kjv_skip(when). kjv_skip(where). kjv_skip(there). kjv_skip(here).
kjv_skip(thus). kjv_skip(hence). kjv_skip(therefore). kjv_skip(wherefore).
kjv_skip(however). kjv_skip(moreover). kjv_skip(nevertheless).
kjv_skip(else). kjv_skip(otherwise).
% Conjunciones
kjv_skip(and). kjv_skip(but). kjv_skip(or). kjv_skip(for). kjv_skip(as).
kjv_skip(if). kjv_skip(than). kjv_skip(though). kjv_skip(although).
kjv_skip(while). kjv_skip(because). kjv_skip(lest).
% Preposiciones
kjv_skip(unto). kjv_skip(upon). kjv_skip(onto). kjv_skip(into).
kjv_skip(from). kjv_skip(with). kjv_skip(by). kjv_skip(of). kjv_skip(to).
kjv_skip(up). kjv_skip(out). kjv_skip(over). kjv_skip(under).
kjv_skip(against). kjv_skip(between). kjv_skip(among). kjv_skip(amongst).
kjv_skip(through). kjv_skip(before). kjv_skip(after). kjv_skip(around).
kjv_skip(about). kjv_skip(at). kjv_skip(on). kjv_skip(off).
kjv_skip(toward). kjv_skip(towards). kjv_skip(along). kjv_skip(amid).
kjv_skip(amidst). kjv_skip(beside). kjv_skip(besides). kjv_skip(except).
kjv_skip(without). kjv_skip(within). kjv_skip(beyond). kjv_skip(across).
kjv_skip(behind). kjv_skip(below). kjv_skip(beneath). kjv_skip(down).
kjv_skip(during). kjv_skip(past). kjv_skip(since). kjv_skip(till).
kjv_skip(until). kjv_skip(alongside).
% Determinantes y cuantificadores
kjv_skip(all). kjv_skip(every). kjv_skip(each). kjv_skip(any). kjv_skip(some).
kjv_skip(such). kjv_skip(own). kjv_skip(same). kjv_skip(other). kjv_skip(another).
kjv_skip(much). kjv_skip(many). kjv_skip(more). kjv_skip(most). kjv_skip(few).
kjv_skip(little). kjv_skip(both). kjv_skip(either). kjv_skip(neither).
kjv_skip(no). kjv_skip(none). kjv_skip(sundry).
% Numerales cardinales (palabra) y ordinales comunes
kjv_skip(one). kjv_skip(two). kjv_skip(three). kjv_skip(four). kjv_skip(five).
kjv_skip(six). kjv_skip(seven). kjv_skip(eight). kjv_skip(nine). kjv_skip(ten).
kjv_skip(eleven). kjv_skip(twelve). kjv_skip(thirteen). kjv_skip(fourteen).
kjv_skip(fifteen). kjv_skip(sixteen). kjv_skip(seventeen). kjv_skip(eighteen).
kjv_skip(nineteen). kjv_skip(twenty). kjv_skip(thirty). kjv_skip(forty).
kjv_skip(fifty). kjv_skip(sixty). kjv_skip(seventy). kjv_skip(eighty).
kjv_skip(ninety). kjv_skip(hundred). kjv_skip(thousand). kjv_skip(million).
kjv_skip(score). kjv_skip(threescore). kjv_skip(fourscore).
kjv_skip(first). kjv_skip(second). kjv_skip(third). kjv_skip(fourth).
kjv_skip(fifth). kjv_skip(sixth). kjv_skip(seventh). kjv_skip(eighth).
kjv_skip(ninth). kjv_skip(tenth).

% ===== contenido KJV =====
kjv_verb_form(T) :- kjv_verb(_, T), !.
kjv_verb_form(T) :- verb_form_word(T), !.

kjv_content(T) :-
    atom(T),
    \+ kjv_skip(T),
    \+ skip_word(T),
    \+ aux_skip(T),
    \+ kjv_aux(T),
    \+ kjv_verb_form(T),
    \+ typename(T).

% ===== deteccion: verbo mas a la izquierda que casa con alguna familia =====
kjv_verb_at(Tokens, Canon, I) :-
    nth0(I, Tokens, W),
    kjv_verb(Canon, W), !.

% ===== slots nearest =====
% Slots nearest via keysort (sin redefinir predicados de library(lists)):
% S = contenido con indice mayor antes del verbo; O = menor despues.
kjv_before(Tokens, I, S) :-
    findall(J-C, (nth0(J, Tokens, C), J < I, kjv_content(C)), Pairs),
    Pairs \== [],
    keysort(Pairs, Sorted),
    last(Sorted, _-S).

kjv_after(Tokens, I, O) :-
    findall(J-C, (nth0(J, Tokens, C), J > I, kjv_content(C)), Pairs),
    Pairs \== [],
    keysort(Pairs, [_-O|_]).

% ===== entrada unica =====
% symbolize_kjv(+Sentence, -Triple): falla si no hay verbo o falta S u O.
symbolize_kjv(Sentence, (S, V, O)) :-
    tokenize_en(Sentence, T0),
    map_pronouns_pure(T0, Tokens, Maps),
    kjv_verb_at(Tokens, V, I),
    kjv_before(Tokens, I, S),
    kjv_after(Tokens, I, O),
    assert_mentions(Maps).
