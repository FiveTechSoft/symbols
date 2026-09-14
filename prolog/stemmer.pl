% stemmer.pl — Stemmer inglés para normalización de verbos
:- use_module(library(lists)).

% stem(+Word, -Stem) — obtiene la forma base del verbo
% Irregulares primero, luego regulares
stem(W, Base) :- irregular(W, Base), !.
stem(W, Base) :- regular_stem(W, Base).

% ── Irregulares ──────────────────────────────────────────────────────

% be
irregular(was, be). irregular(were, be). irregular(been, be).
irregular(is, be). irregular(are, be). irregular(am, be).

% have
irregular(had, have). irregular(has, have).

% do
irregular(did, do). irregular(does, do).

% go
irregular(went, go). irregular(gone, go).

% come
irregular(came, come).

% see
irregular(saw, see). irregular(seen, see).

% know
irregular(knew, know). irregular(known, know).

% take
irregular(took, take). irregular(taken, take).

% give
irregular(gave, give). irregular(given, give).

% find
irregular(found, find).

% think
irregular(thought, think).

% say
irregular(said, say).

% make
irregular(made, make).

% eat
irregular(ate, eat). irregular(eaten, eat).

% drink
irregular(drank, drink). irregular(drunk, drink).

% run
irregular(ran, run).

% swim
irregular(swam, swim). irregular(swum, swim).

% begin
irregular(began, begin). irregular(begun, begin).

% sing
irregular(sang, sing). irregular(sung, sing).

% ring
irregular(rang, ring). irregular(rung, ring).

% drink
irregular(drink, drink). irregular(drinks, drink).

% meet
irregular(met, meet). irregular(meets, meet).

% read
irregular(read, read). irregular(reads, read).

% write
irregular(wrote, write). irregular(written, write).

% wake
irregular(woke, wake). irregular(woken, wake).

% wear
irregular(wore, wear). irregular(worn, wear).

% bite
irregular(bit, bite). irregular(bitten, bite).

% hide
irregular(hid, hide). irregular(hidden, hide).

% choose
irregular(chose, choose). irregular(chosen, choose).

% freeze
irregular(froze, freeze). irregular(frozen, freeze).

% speak
irregular(spoke, speak). irregular(spoken, speak).

% steal
irregular(stole, steal). irregular(stolen, steal).

% blow
irregular(blew, blow). irregular(blown, blow).

% grow
irregular(grew, grow). irregular(grown, grow).

% know
irregular(knew, know). irregular(known, know).

% throw
irregular(threw, throw). irregular(thrown, throw).

% draw
irregular(drew, draw). irregular(drawn, draw).

% show
irregular(showed, show). irregular(shown, show).

% drive
irregular(drove, drive). irregular(driven, drive).

% ride
irregular(rode, ride). irregular(ridden, ride).

% fall
irregular(fell, fall). irregular(fallen, fall).

% break
irregular(broke, break). irregular(broken, break).

% grow
irregular(grew, grow). irregular(grown, grow).

% know
irregular(knew, know). irregular(known, know).

% throw
irregular(threw, throw). irregular(thrown, throw).

% draw
irregular(drew, draw). irregular(drawn, draw).

% smoke
irregular(smoked, smoke). irregular(smokes, smoke).

% drop
irregular(dropped, drop). irregular(drops, drop).

% grin
irregular(grinned, grin). irregular(grins, grin).

% defy
irregular(defied, defy). irregular(defies, defy).

% recite
irregular(recited, recite). irregular(recites, recite).

% wave
irregular(waved, wave). irregular(waves, wave).

% peep
irregular(peeped, peep). irregular(peeps, peep).

% hope
irregular(hoped, hope). irregular(hopes, hope).

% like
irregular(liked, like). irregular(likes, like).

% love
irregular(loved, love). irregular(loves, love).

% move
irregular(moved, move). irregular(moves, move).

% live
irregular(lived, live). irregular(lives, live).

% use
irregular(used, use). irregular(uses, use).

% close
irregular(closed, close). irregular(closes, close).

% rise
irregular(rose, rise). irregular(risen, rise).

% lose
irregular(lost, lose). irregular(loses, lose).

% choose
irregular(chose, choose). irregular(chosen, choose).

% freeze
irregular(froze, freeze). irregular(frozen, freeze).

% steal
irregular(stole, steal). irregular(stolen, steal).

% speak
irregular(spoke, speak). irregular(spoken, speak).

% break
irregular(broke, break). irregular(broken, break).

% wake
irregular(woke, wake). irregular(woken, wake).

% wear
irregular(wore, wear). irregular(worn, wear).

% bite
irregular(bit, bite). irregular(bitten, bite).

% hide
irregular(hid, hide). irregular(hidden, hide).

% ride
irregular(rode, ride). irregular(ridden, ride).

% drive
irregular(drove, drive). irregular(driven, drive).

% write
irregular(wrote, write). irregular(written, write).

% blow
irregular(blew, blow). irregular(blown, blow).

% grow
irregular(grew, grow). irregular(grown, grow).

% know
irregular(knew, know). irregular(known, know).

% throw
irregular(threw, throw). irregular(thrown, throw).

% draw
irregular(drew, draw). irregular(drawn, draw).

% show
irregular(showed, show). irregular(shown, show).

% lead
irregular(led, lead).

% leave
irregular(left, leave).

% keep
irregular(kept, keep).

% sleep
irregular(slept, sleep).

% feel
irregular(felt, feel).

% hold
irregular(held, hold).

% stand
irregular(stood, stand).

% understand
irregular(understood, understand).

% win
irregular(won, win).

% sit
irregular(sat, sit).

% set
irregular(set, set).

% cut
irregular(cut, cut).

% put
irregular(put, put).

% let
irregular(let, let).

% shut
irregular(shut, shut).

% become
irregular(became, become).

% bring
irregular(brought, bring).

% buy
irregular(bought, buy).

% catch
irregular(caught, catch).

% teach
irregular(taught, teach).

% tell
irregular(told, tell).

% sell
irregular(sold, sell).

% send
irregular(sent, send).

% build
irregular(built, build).

% lose
irregular(lost, lose).

% pay
irregular(paid, pay).

% say
irregular(said, say).

% tell
irregular(told, tell).

% ask
irregular(asked, ask). irregular(asks, ask).

% long
irregular(longed, long). irregular(longs, long).

% cry
irregular(cried, cry). irregular(cries, cry).

% try
irregular(tried, try). irregular(tries, try).

% carry
irregular(carried, carry). irregular(carries, carry).

% hurry
irregular(hurried, hurry). irregular(hurries, hurry).

% study
irregular(studied, study). irregular(studies, study).

% copy
irregular(copied, copy). irregular(copies, copy).

% deny
irregular(denied, deny). irregular(denies, deny).

% reply
irregular(replied, reply). irregular(replies, reply).

% marry
irregular(married, marry). irregular(marries, marry).

% hurry
irregular(hurried, hurry). irregular(hurries, hurry).

% worry
irregular(worried, worry). irregular(worries, worry).

% ── Regulares: sufijos comunes ──────────────────────────────────────

% -ing → quitar, si doblar consonante quitar una, si base+e es verbo usar e
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [i,n,g], Chars),
    StemChars \== [],
    atom_chars(Base0, StemChars),
    ( irregular(Base0, _) -> Base = Base0
    ; atom_concat(Base0, 'e', Base1), irregular(Base1, _) -> Base = Base1
    ; atom_concat(Base0, 'e', Base)
    ),
    !.

% -ed → quitar, si la base termina en consonante y base+e es irregular, usar base+e
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [e,d], Chars),
    StemChars \== [],
    atom_chars(Base0, StemChars),
    ( irregular(Base0, _) -> Base = Base0
    ; atom_concat(Base0, 'e', Base1), irregular(Base1, _) -> Base = Base1
    ; Base = Base0
    ),
    !.

% -ed con e mudo → quitar -d
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [d], Chars),
    append(StemShort, [e], StemChars),
    StemShort \== [],
    atom_chars(Base, StemShort),
    \+ irregular(Base, _),
    !.

% -es → quitar, si la base termina en consonante y base+e es irregular, usar base+e
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [e,s], Chars),
    StemChars \== [],
    atom_chars(Base0, StemChars),
    ( irregular(Base0, _) -> Base = Base0
    ; atom_concat(Base0, 'e', Base1), irregular(Base1, _) -> Base = Base1
    ; Base = Base0
    ),
    !.

% -s → quitar (si no es posesivo ni pronombre)
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [s], Chars),
    StemChars \== [],
    atom_chars(Base, StemChars),
    \+ irregular(Base, _),
    \+ member(Base, [this, his, hers, its, yours, ours, theirs,
                      always, never, sometimes, often, yes,
                      was, were, has, is, am, are,
                      us, them, self, himself, herself, itself,
                      where, when, then, than, them]),
    !.

% -ies → quitar -ies, agregar -y
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [i,e,s], Chars),
    StemChars \== [],
    append(StemShort, [y], StemChars),
    atom_chars(Base, StemShort),
    \+ irregular(Base, _),
    !.

% -ied → quitar -ied, agregar -y
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    append(StemChars, [i,e,d], Chars),
    StemChars \== [],
    append(StemShort, [y], StemChars),
    atom_chars(Base, StemShort),
    \+ irregular(Base, _),
    !.

% -ting → quitar -ting (running → run? no, running → run)
% Manejar doblar consonante: stopped → stop, running → run
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    Chars = [_, C, C, i, n, g | Rest],  % doblar consonante antes de -ing
    append(Short, [C, C, i, n, g | Rest], Chars),
    append(Short, [C], BaseChars),
    atom_chars(Base, BaseChars),
    \+ irregular(Base, _),
    !.

% -tting → running → run (con doblar t)
regular_stem(W, Base) :-
    atom_chars(W, Chars),
    Chars = [_, t, t, i, n, g | Rest],
    append(Short, [t, t, i, n, g | Rest], Chars),
    append(Short, [t], BaseChars),
    atom_chars(Base, BaseChars),
    \+ irregular(Base, _),
    !.

% Fallback: no stemmar
regular_stem(W, W).

% ── normalize_verb(+W, -Base) — normaliza forma verbal a base ───────

normalize_verb(W, Base) :-
    stem(W, Base0),
    % Si el stem es diferente al original, usarlo
    ( W \== Base0 -> Base = Base0 ; Base = W ).

% ── is_verb_form(+W) — true si parece forma verbal ──────────────────

is_verb_form(W) :-
    stem(W, Base),
    W \== Base.

% ── Tests ────────────────────────────────────────────────────────────

:- begin_tests(stemmer).

test(irregular) :-
    stem(ate, eat),
    stem(drank, drink),
    stem(met, meet),
    stem(followed, follow),
    stem(smoked, smoke),
    stem(dropped, drop),
    stem(hosted, host),
    stem(painted, paint),
    stem(sang, sing),
    stem(led, lead),
    stem(slept, sleep),
    stem(grinned, grin),
    stem(turned, turn),
    stem(entered, enter),
    stem(defied, defy),
    stem(recited, recite),
    stem(swam, swim),
    stem(peeped, peep),
    stem(waved, wave),
    stem(found, find),
    stem(held, hold),
    stem(told, tell),
    stem(woke, wake),
    stem(read, read),
    stem(carried, carry),
    stem(wore, wear),
    stem(left, leave),
    stem(kept, keep),
    stem(cried, cry),
    stem(tried, try),
    stem(replied, reply),
    stem(longed, long).

test(regular) :-
    stem(smoked, smoke),
    stem(hosted, host),
    stem(painted, paint),
    stem(played, play),
    stem(asked, ask),
    stem(visited, visit),
    stem(worked, work),
    stem(wanted, want),
    stem(needed, need),
    stem(liked, like),
    stem(loved, love),
    stem(hoped, hope),
    stem(moved, move),
    stem(lived, live),
    stem(walked, walk),
    stem(rushed, rush).

test(regular_s) :-
    stem(smokes, smoke),
    stem(plays, play),
    stem(asks, ask),
    stem(hosts, host),
    stem(paints, paint),
    stem(carries, carry),
    stem(hurries, hurry),
    stem(cries, cry),
    stem(tries, try).

:- end_tests(stemmer).
