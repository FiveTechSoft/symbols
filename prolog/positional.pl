% positional.pl — Simbolizador SVO mejorado (P2)
:- use_module(library(lists)).

% ── Tokenización ──────────────────────────────────────────────────────

tokenize_pos(Sentence, Tokens) :-
    string_lower(Sentence, Lower),
    normalize_space(string(Norm), Lower),
    string_chars(Norm, Chars),
    % Reemplazar puntuación por espacio (no eliminar)
    maplist(punct_to_space, Chars, SpaceChars),
    string_chars(SpaceStr, SpaceChars),
    split_string(SpaceStr, " ", " ", Parts),
    exclude(pos_empty, Parts, NonEmpty),
    maplist(atom_string, Tokens0, NonEmpty),
    maplist(clean_token, Tokens, Tokens0).

% punct_to_space(+In, -Out) — puntuación → espacio
punct_to_space(X, ' ') :- pos_punct(X), !.
punct_to_space(X, X).

pos_punct('.'). pos_punct(','). pos_punct(';'). pos_punct(':').
pos_punct('?'). pos_punct('!'). pos_punct('"'). pos_punct('`').
pos_punct('\''). pos_punct('('). pos_punct(')'). pos_punct('[').
pos_punct(']'). pos_punct('{'). pos_punct('}').
pos_punct('\u2018'). pos_punct('\u2019').  % smart quotes
pos_punct('\u201C'). pos_punct('\u201D').
pos_punct('-'). pos_punct('\u2014'). pos_punct('\u2013').  % hyphens, em-dash, en-dash
pos_punct('_'). pos_punct('/').
pos_empty("").

% clean_token(-Out, +In) — quita residuos de puntuación
clean_token(Out, In) :-
    atom_string(In, S0),
    % Quitar comillas residuales al inicio/final
    normalize_space(string(S1), S0),
    atom_string(Out, S1).
clean_token(Out, In) :-
    atom_string(In, Out).

% ── Glue tokens ───────────────────────────────────────────────────────

is_glue(the). is_glue(a). is_glue(an).
is_glue(of). is_glue(to). is_glue(in). is_glue(at).
is_glue(on). is_glue(for). is_glue(by). is_glue(with).
is_glue(from). is_glue(into). is_glue(through).
is_glue(about). is_glue(over). is_glue(under).
is_glue(down). is_glue(up). is_glue(out). is_glue(off).
is_glue(around). is_glue(along). is_glue(across). is_glue(between).
is_glue(has). is_glue(had). is_glue(have). is_glue(having).
is_glue(was). is_glue(were). is_glue(been).
is_glue(will). is_glue(would). is_glue(could). is_glue(should).
is_glue(may). is_glue(might). is_glue(must). is_glue(can).
is_glue(do). is_glue(does). is_glue(did).
is_glue(yes).
is_glue(who). is_glue(whom).
is_glue(whose). is_glue(where). is_glue(when). is_glue(while).
is_glue(if). is_glue(unless). is_glue(because). is_glue(since).
is_glue(although). is_glue(though). is_glue(whether).
is_glue(then). is_glue(else). is_glue(so). is_glue(very).
is_glue(these). is_glue(those).
is_glue(some). is_glue(any). is_glue(every).
is_glue(each). is_glue(all). is_glue(much). is_glue(many).
% Posesivos y determinantes (objetos no los necesitan)
is_glue(his). is_glue(her). is_glue(its). is_glue(my). is_glue(your).
% "only" como modificador adjetival
is_glue(only). is_glue(just). is_glue(also).

% ── Verbos conocidos (para detectar posición del verbo) ──────────────

known_verb(followed). known_verb(follow). known_verb(follows).
known_verb(eat). known_verb(eats). known_verb(ate). known_verb(eaten).
known_verb(drink). known_verb(drinks). known_verb(drank). known_verb(drunk).
known_verb(meet). known_verb(meets). known_verb(met).
known_verb(smoke). known_verb(smokes). known_verb(smoked).
known_verb(has). known_verb(have). known_verb(had).
known_verb(host). known_verb(hosts). known_verb(hosted).
known_verb(play). known_verb(plays). known_verb(played).
known_verb(threaten). known_verb(threatens). known_verb(threatened).
known_verb(judge). known_verb(judges). known_verb(judged).
known_verb(paint). known_verb(paints). known_verb(painted).
known_verb(sing). known_verb(sings). known_verb(sang). known_verb(sung).
known_verb(lead). known_verb(leads). known_verb(led).
known_verb(attend). known_verb(attends). known_verb(attended).
known_verb(wake). known_verb(wakes). known_verb(woke). known_verb(woken).
known_verb(read). known_verb(reads).
known_verb(carry). known_verb(carries). known_verb(carried).
known_verb(wear). known_verb(wears). known_verb(wore). known_verb(worn).
known_verb(drop). known_verb(drops). known_verb(dropped).
known_verb(advise). known_verb(advises). known_verb(advised).
known_verb(turn). known_verb(turns). known_verb(turned).
known_verb(nurse). known_verb(nurses). known_verb(nursed).
known_verb(offer). known_verb(offers). known_verb(offered).
known_verb(tell). known_verb(tells). known_verb(told).
known_verb(wave). known_verb(waves). known_verb(waved).
known_verb(find). known_verb(finds). known_verb(found).
known_verb(enter). known_verb(enters). known_verb(entered).
known_verb(defy). known_verb(defies). known_verb(defied).
known_verb(hold). known_verb(holds). known_verb(held).
known_verb(recite). known_verb(recites). known_verb(recited).
known_verb(swim). known_verb(swims). known_verb(swam). known_verb(swum).
known_verb(peep). known_verb(peeps). known_verb(peeped).
known_verb(want). known_verb(wants). known_verb(wanted).
known_verb(like). known_verb(likes). known_verb(liked).
known_verb(sit). known_verb(sits). known_verb(sat).
known_verb(go). known_verb(goes). known_verb(went). known_verb(gone).
known_verb(come). known_verb(comes). known_verb(came).
known_verb(run). known_verb(runs). known_verb(ran).
known_verb(get). known_verb(gets). known_verb(got).
known_verb(make). known_verb(makes). known_verb(made).
known_verb(say). known_verb(says). known_verb(said).
known_verb(take). known_verb(takes). known_verb(took). known_verb(taken).
known_verb(see). known_verb(sees). known_verb(saw). known_verb(seen).
known_verb(know). known_verb(knows). known_verb(knew). known_verb(known).
known_verb(think). known_verb(thinks). known_verb(thought).
known_verb(begin). known_verb(begins). known_verb(began). known_verb(begun).
known_verb(feel). known_verb(feels). known_verb(felt).
known_verb(leave). known_verb(leaves). known_verb(left).
known_verb(put). known_verb(puts).
known_verb(use). known_verb(uses). known_verb(used).
known_verb(call). known_verb(calls). known_verb(called).
known_verb(let). known_verb(lets).
known_verb(try). known_verb(tries). known_verb(tried).
known_verb(ask). known_verb(asks). known_verb(asked).
known_verb(need). known_verb(needs). known_verb(needed).
known_verb(keep). known_verb(keeps). known_verb(kept).
known_verb(open). known_verb(opens). known_verb(opened).
known_verb(walk). known_verb(walks). known_verb(walked).
known_verb(rush). known_verb(rushes). known_verb(rushed).
known_verb(burn). known_verb(burns). known_verb(burned).
known_verb(stop). known_verb(stops). known_verb(stopped).
known_verb(happen). known_verb(happens). known_verb(happened).
known_verb(speak). known_verb(speaks). known_verb(spoke). known_verb(spoken).
known_verb(bite). known_verb(bites). known_verb(bit). known_verb(bitten).
known_verb(pick). known_verb(picks). known_verb(picked).
known_verb(shout). known_verb(shouts). known_verb(shouted).
known_verb(stay). known_verb(stays). known_verb(stayed).
known_verb(grow). known_verb(grows). known_verb(grew). known_verb(grown).
known_verb(lose). known_verb(loses). known_verb(lost).
known_verb(match). known_verb(matches). known_verb(matched).
known_verb(cry). known_verb(cries). known_verb(cried).
known_verb(wait). known_verb(waits). known_verb(waited).
known_verb(long). known_verb(longs). known_verb(longed).
known_verb(change). known_verb(changes). known_verb(changed).
known_verb(disappear). known_verb(disappears). known_verb(disappeared).
known_verb(press). known_verb(presses). known_verb(pressed).
known_verb(grin). known_verb(grins). known_verb(grinned).

% ── Noun phrases (multi-word subjects/objects) ────────────────────────

% Token que puede ser parte de un nombre compuesto
noun_token(T) :- atom(T), \+ is_glue(T), \+ known_verb(T), atom_length(T, L), L >= 2.

% ── parse_svo_clean: extracción SVO inteligente ──────────────────────

% find_verb_pos_orig(+Tokens, -Pos) — posición del primer verbo conocido (no-glue)
find_verb_pos_orig([], _) :- fail.
find_verb_pos_orig([T|Rest], Pos) :-
    is_glue(T), !,
    find_verb_pos_orig(Rest, Pos0),
    Pos is Pos0 + 1.
find_verb_pos_orig([T|_], 0) :- known_verb(T), !.
% Detección morfológica: -ing, -ed, -s como verbos
find_verb_pos_orig([T|_], 0) :- verb_morph(T), !.
find_verb_pos_orig([_|Rest], Pos) :-
    find_verb_pos_orig(Rest, Pos0),
    Pos is Pos0 + 1.

% verb_morph(+Token) — true si el token parece forma verbal
verb_morph(T) :-
    atom(T), atom_length(T, L), L >= 4,
    atom_chars(T, Chars),
    append(_, [i, n, g], Chars).  % termina en -ing
verb_morph(T) :-
    atom(T), atom_length(T, L), L >= 3,
    atom_chars(T, Chars),
    append(_, [e, d], Chars).  % termina en -ed
verb_morph(T) :-
    atom(T), atom_length(T, L), L >= 3,
    atom_chars(T, Chars),
    append(_, [s], Chars),
    \+ member(T, [this, his, hers, its, yours, ours, theirs,
                   always, never, sometimes, often, yes,
                   was, were, has, is, am, are,
                   us, them, self, himself, herself, itself]).

% first_preposition_pos(+Tokens, +AfterIdx, -Pos) — primera prep después de idx
first_preposition_pos(Tokens, AfterIdx, Pos) :-
    length(Prefix, AfterIdx),
    append(Prefix, Rest, Tokens),
    first_prep_in_list(Rest, 0, Pos0),
    Pos is AfterIdx + Pos0.

first_prep_in_list([], _, _) :- fail.
first_prep_in_list([T|_], Pos, Pos) :-
    is_glue(T),
    member(T, [of, to, in, at, on, for, by, with, from, into, through,
               about, over, under, down, up, out, off, around, along,
               across, between]), !.
first_prep_in_list([_|Rest], Pos0, Pos) :-
    Pos1 is Pos0 + 1,
    first_prep_in_list(Rest, Pos1, Pos).

% Primero intentar dividir por conjunciones
parse_svo_clean(Tokens, Triples) :-
    member(and, Tokens),
    split_conjunction(Tokens, Parts),
    Parts \== [Tokens],
    !,
    maplist(parse_svo_single, Parts, Triples0),
    flatten(Triples0, TriplesList),
    Triples = TriplesList.

% Caso normal: S V O — trabajar con tokens originales
parse_svo_clean(Tokens, (S, V, O)) :-
    Tokens \== [],
    find_verb_pos_orig(Tokens, VerbPos),
    VerbPos > 0,
    nth0(VerbPos, Tokens, V),
    verb_subject(Tokens, VerbPos, SubjTokens),
    SubjTokens \== [],
    VerbEnd is VerbPos + 1,
    first_preposition_pos(Tokens, VerbEnd, PrepPos),
    verb_object(Tokens, VerbEnd, PrepPos, ObjTokens),
    ObjTokens \== [],
    atomic_list_concat(SubjTokens, '_', S),
    atomic_list_concat(ObjTokens, '_', O).

% Caso sin preposición encontrada — objeto hasta el final
parse_svo_clean(Tokens, (S, V, O)) :-
    Tokens \== [],
    find_verb_pos_orig(Tokens, VerbPos),
    VerbPos > 0,
    verb_subject(Tokens, VerbPos, SubjTokens),
    SubjTokens \== [],
    VerbEnd is VerbPos + 1,
    verb_object_to_end(Tokens, VerbEnd, ObjTokens),
    ObjTokens \== [],
    atomic_list_concat(SubjTokens, '_', S),
    atomic_list_concat(ObjTokens, '_', O),
    nth0(VerbPos, Tokens, V).

% Fallback: 3+ tokens sin verbo conocido
parse_svo_clean(Tokens, (S, R, O)) :-
    exclude(is_glue, Tokens, Clean),
    Clean = [S, R|Rest],
    Rest \== [],
    atomic_list_concat(Rest, '_', O).

% verb_subject(+Tokens, +VerbPos, -Subj) — non-glue tokens antes del verbo
verb_subject(Tokens, VerbPos, Subj) :-
    length(Before, VerbPos),
    append(Before, _, Tokens),
    exclude(is_glue, Before, Subj).

% verb_object(+Tokens, +Start, +End, -Obj) — non-glue tokens entre Start y End
verb_object(Tokens, Start, End, Obj) :-
    Drop is Start,
    length(DropPrefix, Drop),
    append(DropPrefix, Rest0, Tokens),
    Len is End - Start,
    length(Mid, Len),
    append(Mid, _, Rest0),
    exclude(is_glue, Mid, Obj).

% verb_object_to_end(+Tokens, +Start, -Obj) — non-glue tokens desde Start al final
verb_object_to_end(Tokens, Start, Obj) :-
    length(DropPrefix, Start),
    append(DropPrefix, Rest, Tokens),
    exclude(is_glue, Rest, Obj).

% parse_svo_single: wrapper para maplist con split_conjunction
parse_svo_single(Tokens, Triple) :-
    parse_svo_clean(Tokens, Triple).

% Preposiciones que inician frases preposicionales (cortan el objeto)
is_prep_phrase(T) :- member(T, [of, to, in, at, on, for, by, with, from, into, through, about, over, under, down, up, out, off, around, along, across, between]).

% ── tokenize_pos_clean ───────────────────────────────────────────────

tokenize_pos_clean(Sentence, Clean) :-
    tokenize_pos(Sentence, Tokens),
    exclude(is_glue, Tokens, Clean).

% ── content_token ─────────────────────────────────────────────────────

content_token(T) :-
    atom(T),
    \+ is_glue(T),
    atom_length(T, L), L >= 2.

% ── symbolize_text ───────────────────────────────────────────────────

symbolize_text(Sentence, Triple) :-
    tokenize_pos(Sentence, Tokens),
    parse_svo_clean(Tokens, Triple).

% ── División de conjunciones ─────────────────────────────────────────

split_conjunction(Tokens, Parts) :-
    append(Left, [and|Right], Tokens),
    Left \== [], Right \== [],
    \+ member(and, Left),
    !,
    split_conjunction(Left, LeftParts),
    split_conjunction(Right, RightParts),
    append(LeftParts, RightParts, Parts).
split_conjunction(Tokens, [Tokens]) :-
    \+ member(and, Tokens).

split_complex(Sentence, SubSentences) :-
    string_lower(Sentence, Lower),
    split_at_conjunctions(Lower, SubSentences0),
    exclude(is_too_short, SubSentences0, SubSentences).

split_at_conjunctions(Text, [Before, After]) :-
    sub_string(Text, BeforeLen, _, AfterLen, " and "),
    BeforeLen > 0, AfterLen > 0,
    sub_string(Text, 0, BeforeLen, _, Before),
    AfterStart is BeforeLen + 5,
    sub_string(Text, AfterStart, _, 0, After),
    !.
split_at_conjunctions(Text, [Text]).

is_too_short(S) :-
    string_length(S, L), L < 5.

symbolize_text_conj(Sentence, Triples) :-
    split_conjunction_tokens(Sentence, SubSents),
    maplist(symbolize_text_single, SubSents, Triples0),
    exclude(==([]), Triples0, Triples).

split_conjunction_tokens(Sentence, [Sentence]) :-
    \+ sub_string(Sentence, _, _, _, " and "), !.
split_conjunction_tokens(Sentence, [Left, Right]) :-
    sub_string(Sentence, _, BeforeLen, AfterLen, " and "),
    BeforeLen > 0, AfterLen > 0,
    sub_string(Sentence, 0, BeforeLen, _, Left),
    AfterStart is BeforeLen + 5,
    sub_string(Sentence, AfterStart, _, 0, RightAtom),
    atom_string(Right, RightAtom),
    !.

symbolize_text_single(Sentence, Triple) :-
    symbolize_text(Sentence, Triple).
