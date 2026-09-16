:- module(parser_v2,
    [ parse_sentence/2,
      parse_and_attend/4,
      attention/3,
      top_k/3,
      normalize_text/2,
      sentence_relations/2,
      pattern/2,
      query_type/2,
      query_weight/3,
      head_weight/2,
      score_relation/3,
      compatibility/3,
      query_bonus/3,
      article/1,
      content_word/1
    ]).

:- use_module(library(lists)).

:- dynamic known_symbol/1.
:- dynamic known_relation/3.

known_symbol(juan).   known_symbol(maria).
known_symbol(pedro).  known_symbol(ana).
known_symbol(coche).  known_symbol(casa).
known_symbol(libro).  known_symbol(empresa).
known_symbol(manzana). known_symbol(bicicleta).
known_symbol(madrid).  known_symbol(barcelona).
known_symbol(malaga).  known_symbol(marbella).
known_symbol(john).   known_symbol(mary).
known_symbol(peter).  known_symbol(anne).
known_symbol(london). known_symbol(paris).

known_relation(juan, vive_en, madrid).
known_relation(maria, vive_en, barcelona).
known_relation(john, lives_in, london).
known_relation(mary, lives_in, paris).


% ════════════════════════════════════════════════════════════════════
%  TOKENIZER
% ════════════════════════════════════════════════════════════════════

parse_sentence(Text, Relations) :-
    normalize_text(Text, Normalized),
    sentence_relations(Normalized, Relations).

normalize_text(Text, Normalized) :-
    string_lower(Text, Lower),
    split_string(Lower, " ,.!?;:\t\n", " ,.!?;:\t\n", Strings),
    maplist(atom_string, Tokens, Strings),
    normalize_accents(Tokens, Normalized).

normalize_accents([], []).
normalize_accents([H|T], [N|R]) :-
    normalize_atom(H, N),
    normalize_accents(T, R).

normalize_atom(Atom, Normalized) :-
    atom_string(Atom, Str),
    normalize_space(string(Norm), Str),
    ( number_string(Num, Norm) -> Normalized = Num
    ; atom_string(Normalized, Norm)
    ).


% ════════════════════════════════════════════════════════════════════
%  FUNCTION WORDS — bilingual (the ONLY hardcoded set)
% ════════════════════════════════════════════════════════════════════

article(el). article(la). article(un). article(una).
article(los). article(las). article(unos). article(unas).
article(a). article(an). article(the).

preposition(para). preposition(desde). preposition(en). preposition(a).
preposition(for). preposition(from). preposition(in). preposition(to).
preposition(on). preposition(at). preposition(since).

% Language-specific articles (subset of article/1 above, for pattern disambiguation)
article_en(a). article_en(an). article_en(the).
article_es(el). article_es(la). article_es(un). article_es(una).
article_es(los). article_es(las). article_es(unos). article_es(unas).

% Preposition roles (subsets of preposition/1, deduced from usage):
% location: "en/in/on/at" (vive en, in madrid, in barcelona)
% indirect: "para/for/to" (para Maria, for Mary)
% temporal: "desde/from/since" (desde 2020, since 2020)
location_prep(en). location_prep(in). location_prep(on). location_prep(at).
indirect_prep(para). indirect_prep(for). indirect_prep(to).
temporal_prep(desde). temporal_prep(from). temporal_prep(since).

conjunction(y). conjunction(pero).
conjunction(and). conjunction(but).

pronoun(lo). pronoun(la). pronoun(le). pronoun(se).
pronoun(it). pronoun(him). pronoun(her).
pronoun(she). pronoun(he). pronoun(they). pronoun(himself). pronoun(herself).

negation(no). negation(not).

auxiliary(does). auxiliary(do). auxiliary(did).
auxiliary(is). auxiliary(are). auxiliary(was). auxiliary(were).


% ════════════════════════════════════════════════════════════════════
%  DEDUCTION — classify by POSITION, not by hardcoded word lists
% ════════════════════════════════════════════════════════════════════

% ── Content word: anything not a function word ─────────────────────
content_word(X) :- atom(X),
    \+ article(X), \+ preposition(X), \+ conjunction(X),
    \+ pronoun(X), \+ negation(X), \+ auxiliary(X).

% ── Location: after a preposition (deduced from context) ───────────
% The pattern itself tells us: if a token follows a preposition, it's a location.
% We check this by looking at the token's position relative to prepositions.

% ── Temporal: numbers OR last token before end of sentence ─────────
% Numbers are always temporal. Other temporal words are deduced by position.


% ════════════════════════════════════════════════════════════════════
%  PARSER — pattern matching → relation(Type, Predicate, Args)
%  Word types are DEDUCED from position in the pattern, not from lexicons
% ════════════════════════════════════════════════════════════════════

sentence_relations(Tokens, Relations) :-
    all_clause_relations(Tokens, FullRels),
    FullRels \== [],
    \+ compound_steal(Tokens, FullRels), !,
    sort(FullRels, Relations).
sentence_relations(Tokens, Relations) :-
    fallback_relations(Tokens, Relations),
    Relations \== [],
    \+ compound_steal(Tokens, Relations), !.
sentence_relations(Tokens, Relations) :-
    split_conjunctions(Tokens, Clauses),
    findall(R, (
        member(Clause, Clauses),
        all_clause_relations(Clause, Rels),
        member(R, Rels)
    ), AllRels),
    ( Clauses = [Before, After] ->
        findall(R, r7_shared_subject(Before, After, R), Shared)
    ; Shared = [] ),
    append(AllRels, Shared, All0),
    All0 \== [], !,
    sort(All0, Relations).
sentence_relations(Tokens, Relations) :-
    split_conjunctions(Tokens, Clauses),
    findall(R, (
        member(Clause, Clauses),
        fallback_relations(Clause, Rels),
        member(R, Rels)
    ), AllRels2),
    sort(AllRels2, Relations), !.

sentence_relations(_, []).


split_conjunctions(Tokens, [Before, After]) :-
    append(Before, [Conj|After0], Tokens),
    conjunction(Conj),
    Before \== [],
    After0 \== [],
    After = After0, !.
split_conjunctions(Tokens, [Tokens]).


% Cross-conjunction steal guard: reject a relation from the unsplit path
% whose first argument equals the pre-conjunction subject while another
% argument was taken from the post-conjunction tail (r5a object steal,
% e.g. "the sea wrought, and was tempestuous" -> main(wrought,[sea,tempestuous])).
compound_steal(Tokens, Relations) :-
    append(Pre, [Conj|Post], Tokens),
    conjunction(Conj),
    Pre \== [], Post \== [],
    Relations \== [],
    member(relation(_, _, Args), Relations),
    ( Pre = [Art, S|_], article_en(Art) ; Pre = [S|_] ),
    r7_subject(S),
    Args = [S|_],
    member(O, Args),
    member(O, Post),
    content_word(O), !.
compound_steal(_, _).

subject_head([S], S) :- !.
subject_head([_, S|_], S).

% Elliptical shared subject: "[NP] ..., and [Aux ADJ]" -> attribute(NP, ADJ)
% e.g. "the sea wrought, and was tempestuous".
r7_shared_subject(Before, [Aux, ADJ], relation(attribute, attribute, [S, ADJ])) :-
    auxiliary(Aux),
    content_word(ADJ),
    \+ r5_function(ADJ),
    \+ r7_intensifier(ADJ),
    \+ r7_participle(ADJ),
    subject_head(Before, S),
    r7_subject(S).


all_clause_relations(Tokens, Relations) :-
    findall(R, pattern(Tokens, R), Relations), !.

pattern(Tokens, Relation) :- p1(Tokens, Relation).
pattern(Tokens, Relation) :- p1e(Tokens, Relation).
pattern(Tokens, Relation) :- p2(Tokens, Relation).
pattern(Tokens, Relation) :- p3(Tokens, Relation).
pattern(Tokens, Relation) :- p3e(Tokens, Relation).
pattern(Tokens, Relation) :- p4(Tokens, Relation).
pattern(Tokens, Relation) :- p4e(Tokens, Relation).
pattern(Tokens, Relation) :- p5(Tokens, Relation).
pattern(Tokens, Relation) :- p6(Tokens, Relation).
pattern(Tokens, Relation) :- p7(Tokens, Relation).
pattern(Tokens, Relation) :- p8(Tokens, Relation).
pattern(Tokens, Relation) :- p9(Tokens, Relation).
pattern(Tokens, Relation) :- p10(Tokens, Relation).
pattern(Tokens, Relation) :- p10e(Tokens, Relation).
pattern(Tokens, Relation) :- p11(Tokens, Relation).
pattern(Tokens, Relation) :- p12(Tokens, Relation).
pattern(Tokens, Relation) :- p13(Tokens, Relation).
pattern(Tokens, Relation) :- p14(Tokens, Relation).


% ── R5: minimal grammar extension (fallback prefix patterns) ───────
% Fires ONLY when no base pattern (p1-p14) matched the clause.
% R5-A: Art S V -> main(S,V) with first acceptable later token as
% object; R5-B: S V -> main(S,V) with NO object (patient unknown,
% never invented). r5_function = closed classes: then/so/after/behold/
% quite... can never be subject, verb or object.

r5_temporal(now). r5_temporal(then). r5_temporal(today).
r5_temporal(yesterday). r5_temporal(tomorrow). r5_temporal(tonight).
r5_temporal(later). r5_temporal(soon). r5_temporal(already).
r5_temporal(ago). r5_temporal(henceforth).

r5_modal(has). r5_modal(have). r5_modal(had).
r5_modal(will). r5_modal(shall). r5_modal(would).
r5_modal(can). r5_modal(could). r5_modal(may).
r5_modal(might). r5_modal(must). r5_modal(should).

r5_discourse(behold).
r5_discourse(yea). r5_discourse(nay). r5_discourse(verily).
r5_discourse(hath). r5_discourse(doth). r5_discourse(hadst).
r5_discourse(shalt). r5_discourse(wilt). r5_discourse(hast).
r5_discourse(art). r5_discourse(didst). r5_discourse(wast).
r5_discourse(thou). r5_discourse(thee). r5_discourse(thy).
r5_discourse(thine). r5_discourse(ye). r5_discourse(thyself).
r5_discourse(themselves). r5_discourse(myself).
r5_discourse(ourselves). r5_discourse(yourselves). r5_discourse(itself).
r5_discourse(i). r5_discourse(we). r5_discourse(us).
r5_discourse(me). r5_discourse(myself).
r5_discourse(them). r5_discourse(you). r5_discourse(or). r5_discourse(nor).
r5_discourse(who). r5_discourse(whom). r5_discourse(whose).
r5_discourse(which). r5_discourse(what).
r5_discourse(where). r5_discourse(when). r5_discourse(why).
r5_discourse(how). r5_discourse(whence). r5_discourse(wherein).
r5_discourse(whereby). r5_discourse(wherefore). r5_discourse(whether).
r5_discourse(that). r5_discourse(this). r5_discourse(these). r5_discourse(those).
r5_discourse(by). r5_discourse(of). r5_discourse(with).
r5_discourse(unto). r5_discourse(upon). r5_discourse(within).
r5_discourse(without). r5_discourse(among). r5_discourse(between).
r5_discourse(through). r5_discourse(during). r5_discourse(under).
r5_discourse(after). r5_discourse(before).

% Possessive determiners: never subject, verb or object.
r5_possessive(my). r5_possessive(our). r5_possessive(your).
r5_possessive(their). r5_possessive(its). r5_possessive(his).
r5_possessive(theirs). r5_possessive(ours). r5_possessive(yours).

% Spelled-out numbers: never R5 arguments (digit tokens are number/1).
r5_number(one). r5_number(two). r5_number(three). r5_number(four).
r5_number(five). r5_number(six). r5_number(seven). r5_number(eight).
r5_number(nine). r5_number(ten). r5_number(eleven). r5_number(twelve).
r5_number(twenty). r5_number(thirty). r5_number(forty). r5_number(fifty).
r5_number(hundred). r5_number(thousand).

% Ordinals: same family (order words, never R5 arguments).
r5_number(first). r5_number(second). r5_number(third). r5_number(fourth).
r5_number(fifth). r5_number(sixth). r5_number(seventh). r5_number(eighth).
r5_number(ninth). r5_number(tenth). r5_number(eleventh). r5_number(twelfth).

r5_nonverb(all). r5_nonverb(here). r5_nonverb(there).
r5_nonverb(thus). r5_nonverb(so). r5_nonverb(therefore).
r5_nonverb(however). r5_nonverb(nevertheless). r5_nonverb(again).
r5_nonverb(also). r5_nonverb(even). r5_nonverb(only).
r5_nonverb(just). r5_nonverb(still). r5_nonverb(yet).
r5_nonverb(quite). r5_nonverb(such). r5_nonverb(rather).

r5_function(W) :-
    ( article(W) ; preposition(W) ; conjunction(W) ; pronoun(W) ;
      negation(W) ; auxiliary(W) ; r5_temporal(W) ; r5_modal(W) ;
      r5_discourse(W) ; r5_possessive(W) ; r5_number(W) ;
      r5_nonverb(W) ), !.

r5_verb(W) :-
    content_word(W),
    \+ preposition(W),
    \+ conjunction(W),
    \+ r5_function(W).

r5a([Art, S, V|Rest], relation(main, V, [S, O])) :-
    article_en(Art),
    content_word(S),
    \+ r5_function(S),
    r5_verb(V),
    ( r5_first_object(V, Rest, O) -> true ; O = unknown ).

r5_first_object(V, Rest, O) :-
    member(O, Rest),
    content_word(O),
    \+ r5_function(O),
    O \== V, !.

r5b([S, V], relation(main, V, [S, unknown])) :-
    content_word(S),
    \+ r5_function(S),
    r5_verb(V).

fallback_relations(Tokens, Relations) :-
    findall(R, ( r5a(Tokens, R) ; r5b(Tokens, R) ; r5_of(Tokens, R) ;
                 r7_copula(Tokens, R) ), Rs0),
    Rs0 \== [], !,
    sort(Rs0, Relations).
fallback_relations(Tokens, Relations) :-
    findall(R, r5b(Tokens, R), Rs0),
    Rs0 \== [], !,
    sort(Rs0, Relations).
fallback_relations(_, []).

% ── R6: of-PP-attached subject ──────────────────────────────────────
% "the word of the lord came unto jonah" -> came(word, jonah).
% Fires ONLY in the same no-base-pattern regime as R5 (via
% fallback_relations below). Subject zone = [Art, Head, of ... of-chain
% closed greedily]; verb = first r5_verb after the chain closes.
% Pre is restricted to [Art] so the head must sit right after the
% article: kills garbage hits like "belly(out)" (18_1) and
% "cried(belly,i)" (19_2) where the head is not the grammatical
% subject. Chain closure consumes of [Art] N / N / of N greedily.
r5_of([Art, S, of|Rest], relation(main, V, [S, O])) :-
    article_en(Art),
    content_word(S),
    \+ r5_function(S),
    r6_close_of_chain(Rest, Pre1),
    append(Pre1, [V|VRest], Rest),
    r5_verb(V), !,
    ( r5_first_object(V, VRest, O) -> true ; O = unknown ).

r6_close_of_chain(Rest, Pre) :-
    ( Rest = [A|T], article_en(A) ->
        ( T = [X|T2], \+ r5_function(X) ->
            ( T2 = [of|_] -> r6_close_of_chain(T2, P2), Pre = [A, X, of|P2] ; Pre = [A, X] )
        ; Pre = [A] )
    ; Rest = [X|T2], content_word(X), \+ r5_function(X) ->
        ( T2 = [of|_] -> r6_close_of_chain(T2, P2), Pre = [X, of|P2] ; Pre = [X] )
    ; Pre = [] ).

% ── R7: copular attribute ───────────────────────────────────────────
% "the mariners were afraid" -> attribute(mariners, afraid).
% Fires ONLY in the no-base-pattern regime (via fallback_relations).
% Shape: leading skip-tokens (nonverb/temporal/preposition — never
% discourse/possessive/pronoun), optional article, S, be-form, ADJ
% (+ optional closed intensifier before ADJ). Excluded on purpose:
% pronoun subjects, "there was", NP-predicatives (X was a Y — that is
% type, not attribute), participles (-ed len>=5) and infinitive /
% directional continuations (is come up, was like to be broken).
r7_skip(W) :- r5_nonverb(W).
r7_skip(W) :- r5_temporal(W).
r7_skip(W) :- preposition(W).

r7_leading([H|T], T) :- r7_skip(H), !, r7_leading(T, T).
r7_leading(T, T).

r7_intensifier(very). r7_intensifier(exceeding). r7_intensifier(exceedingly).

r7_participle(W) :-
    atom_chars(W, Cs), append(_, [e, d], Cs),
    atom_length(W, L), L >= 5.

r7_infinitive_rest([to|_]).
r7_infinitive_rest([up|_]).
r7_infinitive_rest([down|_]).
r7_infinitive_rest([out|_]).
r7_infinitive_rest([forth|_]).
r7_infinitive_rest([away|_]).

r7_subject(S) :-
    content_word(S),
    \+ r5_function(S),
    \+ pronoun(S),
    \+ r5_possessive(S).

r7_copula(Tokens, relation(attribute, attribute, [S, ADJ])) :-
    r7_leading(Tokens, T1),
    ( T1 = [Art, S, Aux, ADJ|Rest], article_en(Art)
    ; T1 = [S, Aux, ADJ|Rest]
    ),
    r7_subject(S),
    auxiliary(Aux),
    content_word(ADJ),
    \+ r5_function(ADJ),
    \+ r7_intensifier(ADJ),
    \+ r7_participle(ADJ),
    \+ r7_infinitive_rest(Rest).
r7_copula(Tokens, relation(attribute, attribute, [S, ADJ])) :-
    r7_leading(Tokens, T1),
    ( T1 = [Art, S, Aux, Adv, ADJ|Rest], article_en(Art)
    ; T1 = [S, Aux, Adv, ADJ|Rest]
    ),
    r7_subject(S),
    auxiliary(Aux),
    r7_intensifier(Adv),
    content_word(ADJ),
    \+ r5_function(ADJ),
    \+ r7_participle(ADJ),
    \+ r7_infinitive_rest(Rest).

% ── P1: S V Art O Adj Prep Place Time (Spanish: art noun adj) ──────
% Position AFTER article and BEFORE preposition = attribute (deduced)
p1([S,V,Art,O,Adj,Prep,Place,Time], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1([S,V,Art,O,Adj,Prep,Place,Time], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1([S,V,Art,O,Adj,Prep,Place,Time], relation(location, ubicado_en, [O,Place])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1([S,V,Art,O,Adj,Prep,Place,Time], relation(temporal, tiempo, [comprar(S,O),Time])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.


% ── P1e: S V Art Adj O Prep Place Time (English: art adj noun) ─────
p1e([S,V,Art,Adj,O,Prep,Place,Time], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1e([S,V,Art,Adj,O,Prep,Place,Time], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1e([S,V,Art,Adj,O,Prep,Place,Time], relation(location, ubicado_en, [O,Place])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.
p1e([S,V,Art,Adj,O,Prep,Place,Time], relation(temporal, tiempo, [comprar(S,O),Time])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), ( number(Time) ; content_word(Time) ),
    location_prep(Prep), Adj \== O.


% ── P2: S V Prep Place ─────────────────────────────────────────────
p2([S,V,Prep,Place], relation(main, vivir, [S,Place])) :-
    content_word(S), content_word(V), content_word(Place),
    preposition(Prep).


% ── P3: S V Art O Adj Prep Place (Spanish: art noun adj) ───────────
p3([S,V,Art,O,Adj,Prep,Place], relation(main, tener, [S,O])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place),
    location_prep(Prep), Adj \== O.
p3([S,V,Art,O,Adj,Prep,Place], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place),
    location_prep(Prep), Adj \== O.
p3([S,V,Art,O,Adj,Prep,Place], relation(location, ubicado_en, [O,Place])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Place),
    location_prep(Prep), Adj \== O.

% ── P3e: S V Art Adj O Prep Place (English: art adj noun) ──────────
p3e([S,V,Art,Adj,O,Prep,Place], relation(main, tener, [S,O])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), location_prep(Prep), Adj \== O.
p3e([S,V,Art,Adj,O,Prep,Place], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), location_prep(Prep), Adj \== O.
p3e([S,V,Art,Adj,O,Prep,Place], relation(location, ubicado_en, [O,Place])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Place), location_prep(Prep), Adj \== O.


% ── P4: S V Art O Adj Prep Person (Spanish: art noun adj) ──────────
p4([S,V,Art,O,Adj,Prep,Person], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Person),
    indirect_prep(Prep), Adj \== O.
p4([S,V,Art,O,Adj,Prep,Person], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Person),
    indirect_prep(Prep), Adj \== O.
p4([S,V,Art,O,Adj,Prep,Person], relation(indirect, para, [comprar(S,O),Person])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O),
    content_word(Adj), content_word(Person),
    indirect_prep(Prep), Adj \== O.

% ── P4e: S V Art Adj O Prep Person (English: art adj noun) ─────────
p4e([S,V,Art,Adj,O,Prep,Person], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Person), indirect_prep(Prep), Adj \== O.
p4e([S,V,Art,Adj,O,Prep,Person], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Person), indirect_prep(Prep), Adj \== O.
p4e([S,V,Art,Adj,O,Prep,Person], relation(indirect, para, [comprar(S,O),Person])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    content_word(Person), indirect_prep(Prep), Adj \== O.


% ── P5: S no/not V Prep Place ──────────────────────────────────────
p5([S,Neg,V,Prep,Place], relation(negation, negado, [vivir(S,Place)])) :-
    content_word(S), negation(Neg), content_word(V),
    preposition(Prep), content_word(Place).

% ── P5e: S aux not/no V Prep Place (English negation with auxiliary) ─
p5([S,Aux,Neg,V,Prep,Place], relation(negation, negado, [vivir(S,Place)])) :-
    content_word(S), auxiliary(Aux), negation(Neg), content_word(V),
    preposition(Prep), content_word(Place).


% ── P6: S V O Prep Time ────────────────────────────────────────────
p6([S,V,O,Prep,Time], relation(main, visitar, [S,O])) :-
    content_word(S), content_word(V), content_word(O),
    preposition(Prep), ( number(Time) ; content_word(Time) ).
p6([S,V,O,Prep,Time], relation(temporal, tiempo, [visitar(S,O),Time])) :-
    content_word(S), content_word(V), content_word(O),
    preposition(Prep), ( number(Time) ; content_word(Time) ).


% ── P7: S V Art O Conj Pron V Prep Place ───────────────────────────
p7([S,V1,Art,O,Conj,Pron,V2,Prep,Place], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V1), article(Art), content_word(O),
    conjunction(Conj), pronoun(Pron), content_word(V2),
    preposition(Prep), content_word(Place).
p7([S,V1,Art,O,Conj,Pron,V2,Prep,Place], relation(main, llevar, [S,O])) :-
    content_word(S), content_word(V1), article(Art), content_word(O),
    conjunction(Conj), pronoun(Pron), content_word(V2),
    preposition(Prep), content_word(Place).
p7([S,V1,Art,O,Conj,Pron,V2,Prep,Place], relation(location, llevar_destino, [O,Place])) :-
    content_word(S), content_word(V1), article(Art), content_word(O),
    conjunction(Conj), pronoun(Pron), content_word(V2),
    preposition(Prep), content_word(Place).


% ── P8: S V1 Art O Art2 O2 V2 Prep Place ───────────────────────────
p8([S,V1,Art,O,Art2,O2,V2,Prep,Place], relation(main, tener, [S,O])) :-
    content_word(S), content_word(V1), article(Art), content_word(O),
    article(Art2), content_word(O2), O == O2, content_word(V2),
    preposition(Prep), content_word(Place).
p8([S,V1,Art,O,Art2,O2,V2,Prep,Place], relation(location, ubicado_en, [O,Place])) :-
    content_word(S), content_word(V1), article(Art), content_word(O),
    article(Art2), content_word(O2), O == O2, content_word(V2),
    preposition(Prep), content_word(Place).


% ── P9: S V Prep Place Prep2 Time ──────────────────────────────────
p9([S,V,Prep,Place,Prep2,Time], relation(main, trabajar, [S,Place])) :-
    content_word(S), content_word(V), preposition(Prep), content_word(Place),
    preposition(Prep2), ( number(Time) ; content_word(Time) ).
p9([S,V,Prep,Place,Prep2,Time], relation(temporal, desde, [trabajar(S,Place),Time])) :-
    content_word(S), content_word(V), preposition(Prep), content_word(Place),
    preposition(Prep2), ( number(Time) ; content_word(Time) ).


% ── P10: S V Art O Adj (no preposition, Spanish) ───────────────────
p10([S,V,Art,O,Adj], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O), content_word(Adj),
    Adj \== O.
p10([S,V,Art,O,Adj], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_es(Art), content_word(O), content_word(Adj),
    Adj \== O.

% ── P10e: S V Art Adj O (no preposition, English) ──────────────────
p10e([S,V,Art,Adj,O], relation(main, comprar, [S,O])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    Adj \== O.
p10e([S,V,Art,Adj,O], relation(attribute, attribute, [O,Adj])) :-
    content_word(S), content_word(V), article_en(Art), content_word(Adj), content_word(O),
    Adj \== O.


% ── P11: S V Art O (simple) ────────────────────────────────────────
p11([S,V,Art,O], relation(main, tener, [S,O])) :-
    content_word(S), content_word(V), article(Art), content_word(O).


% ── P12: S V O (no article) ────────────────────────────────────────
p12([S,V,O], relation(main, visitar, [S,O])) :-
    content_word(S), content_word(V), content_word(O).


% ── P13: S V O Prep Place ──────────────────────────────────────────
p13([S,V,O,Prep,Place], relation(main, llevar, [S,O])) :-
    content_word(S), content_word(V), content_word(O),
    preposition(Prep), content_word(Place).
p13([S,V,O,Prep,Place], relation(location, llevar_destino, [O,Place])) :-
    content_word(S), content_word(V), content_word(O),
    preposition(Prep), content_word(Place).


% ── P14: S V Prep Place ────────────────────────────────────────────
p14([S,V,Prep,Place], relation(location, ubicado_en, [S,Place])) :-
    content_word(S), content_word(V), preposition(Prep), content_word(Place).


% ════════════════════════════════════════════════════════════════════
%  QUERIES — bilingual, keyword-based (function words only)
% ════════════════════════════════════════════════════════════════════

:- discontiguous query_weight/3.
:- discontiguous query_weight_extra/3.

% Spanish: compound phrases first, then interrogatives, then descriptors
query_type(color_query, Tokens) :- ( member(color, Tokens) ; member(tono, Tokens) ; member(tonos, Tokens) ), \+ ( member(quien, Tokens) ; member(donde, Tokens) ; member(cuando, Tokens) ; member(como, Tokens) ), !.
query_type(size_query, Tokens) :- ( member(grande, Tokens) ; member(pequeno, Tokens) ; member(tipo, Tokens) ; member(amplia, Tokens) ; member(amplio, Tokens) ; member(dimensiones, Tokens) ; member(tamano, Tokens) ), \+ ( member(quien, Tokens) ; member(donde, Tokens) ; member(cuando, Tokens) ; member(que, Tokens) ; member(como, Tokens) ), !.
query_type(who,     Tokens) :- ( member(quien, Tokens) ; member(para, Tokens) ), !.
query_type(what,    Tokens) :- member(que, Tokens), !.
query_type(where,   Tokens) :- ( member(donde, Tokens) ; member(ciudad, Tokens) ; member(ubicacion, Tokens) ; member(sitio, Tokens) ; member(lugar, Tokens) ), !.
query_type(when,    Tokens) :- ( member(cuando, Tokens) ; member(ano, Tokens) ; member(epoca, Tokens) ; member(momento, Tokens) ; member(fecha, Tokens) ), !.
query_type(negation, Tokens) :- member(no, Tokens), !.
query_type(how,    Tokens) :- member(como, Tokens), !.
% English: compound phrases first, then interrogatives, then descriptors
query_type(color_query, Tokens) :- ( member(red, Tokens) ; member(blue, Tokens) ; member(green, Tokens) ; member(black, Tokens) ; member(white, Tokens) ; member(yellow, Tokens) ; member(color, Tokens) ; member(shade, Tokens) ; member(tone, Tokens) ), \+ ( member(who, Tokens) ; member(whom, Tokens) ; member(where, Tokens) ; member(when, Tokens) ; member(how, Tokens) ), !.
query_type(size_query, Tokens) :- ( member(big, Tokens) ; member(small, Tokens) ; member(large, Tokens) ; member(huge, Tokens) ; member(tiny, Tokens) ; member(dimensions, Tokens) ; member(spacious, Tokens) ), \+ ( member(who, Tokens) ; member(whom, Tokens) ; member(where, Tokens) ; member(when, Tokens) ; member(what, Tokens) ; member(how, Tokens) ), !.
query_type(who,     Tokens) :- ( member(who, Tokens) ; member(whom, Tokens) ), !.
query_type(what,    Tokens) :- member(what, Tokens), !.
query_type(where,   Tokens) :- ( member(where, Tokens) ; member(location, Tokens) ; member(city, Tokens) ), !.
query_type(when,    Tokens) :- ( member(when, Tokens) ; member(time, Tokens) ; member(year, Tokens) ; member(period, Tokens) ), !.
query_type(negation, Tokens) :- member(not, Tokens), !.
query_type(how,    Tokens) :- member(how, Tokens), !.
query_type(unknown, _).


% ════════════════════════════════════════════════════════════════════
%  HEAD WEIGHTS
% ════════════════════════════════════════════════════════════════════

head_weight(relation,  0.30).
head_weight(entity,    0.20).
head_weight(position,  0.10).
head_weight(discourse, 0.10).
head_weight(temporal,  0.15).
head_weight(novelty,   0.15).


% ════════════════════════════════════════════════════════════════════
%  QUERY-DEPENDENT WEIGHTS
% ════════════════════════════════════════════════════════════════════

query_weight(what, relation,  0.35).
query_weight(what, entity,    0.25).
query_weight(what, position,  0.05).
query_weight(what, discourse, 0.15).
query_weight(what, temporal,  0.10).
query_weight(what, novelty,   0.10).

query_weight(where, relation,  0.20).
query_weight(where, entity,    0.15).
query_weight(where, position,  0.05).
query_weight(where, discourse, 0.10).
query_weight(where, temporal,  0.10).
query_weight(where, novelty,   0.10).
query_weight_extra(where, location, 0.30).
query_weight_extra(where, main, 0.10).

query_weight(when, relation,  0.20).
query_weight(when, entity,    0.10).
query_weight(when, position,  0.05).
query_weight(when, discourse, 0.10).
query_weight(when, temporal,  0.35).
query_weight(when, novelty,   0.10).

query_weight(who, relation,  0.25).
query_weight(who, entity,    0.35).
query_weight(who, position,  0.05).
query_weight(who, discourse, 0.15).
query_weight(who, temporal,  0.10).
query_weight(who, novelty,   0.10).

query_weight(negation, relation,  0.25).
query_weight(negation, entity,    0.15).
query_weight(negation, position,  0.10).
query_weight(negation, discourse, 0.15).
query_weight(negation, temporal,  0.10).
query_weight(negation, novelty,   0.10).
query_weight_extra(negation, negation, 0.30).

query_weight(color_query, relation,  0.20).
query_weight(color_query, entity,    0.15).
query_weight(color_query, position,  0.10).
query_weight(color_query, discourse, 0.15).
query_weight(color_query, temporal,  0.05).
query_weight(color_query, novelty,   0.10).

query_weight(size_query, relation,  0.20).
query_weight(size_query, entity,    0.15).
query_weight(size_query, position,  0.10).
query_weight(size_query, discourse, 0.15).
query_weight(size_query, temporal,  0.05).
query_weight(size_query, novelty,   0.10).


% ════════════════════════════════════════════════════════════════════
%  SYMBOLIC ATTENTION — query-dependent
% ════════════════════════════════════════════════════════════════════

parse_and_attend(Text, QueryText, Selected, All) :-
    parse_sentence(Text, Relations),
    normalize_text(QueryText, QueryTokens),
    query_type(QueryType, QueryTokens),
    attention(Relations, QueryType, Scored),
    top_k(Scored, 5, Selected),
    All = Scored.

attention(Relations, QueryType, Scored) :-
    maplist(score_relation(QueryType), Relations, Raw),
    predsort(compare_score, Raw, Scored).


score_relation(QueryType, Relation, scored(Relation, Score, Heads)) :-
    relation_head(Relation, R),
    entity_head(Relation, E),
    position_head(Relation, P),
    discourse_head(Relation, D),
    temporal_head(Relation, T),
    novelty_head(Relation, N),
    type_head(Relation, Typ),
    get_weight(QueryType, relation,  R, WR),
    get_weight(QueryType, entity,    E, WE),
    get_weight(QueryType, position,  P, WP),
    get_weight(QueryType, discourse, D, WD),
    get_weight(QueryType, temporal,  T, WT),
    get_weight(QueryType, novelty,   N, WN),
    BaseScore is WR + WE + WP + WD + WT + WN,
    compatibility(QueryType, Typ,Compat),
    ( query_bonus(QueryType, Typ, Bonus) -> true ; Bonus = 0.0 ),
    conflict(QueryType, Typ, Conflict),
    Score is BaseScore + Compat + Bonus - Conflict,
    Heads = heads(relation-R, entity-E, position-P, discourse-D, temporal-T, novelty-N, type-Typ, compat-Compat, bonus-Bonus, conflict-Conflict).


get_weight(QueryType, Head, Value, Weight) :-
    ( query_weight(QueryType, Head, Base) -> true ; head_weight(Head, Base) ),
    Weight is Base * Value.


% ── COMPATIBILITY ──────────────────────────────────────────────────

compatibility(where,     location,    2.0).
compatibility(where,     main,        0.6).
compatibility(where,     temporal,    0.3).
compatibility(where,     attribute,   0.2).
compatibility(where,     negation,    0.4).
compatibility(where,     indirect,    0.2).

compatibility(when,      temporal,    2.0).
compatibility(when,      main,        0.5).
compatibility(when,      location,    0.3).
compatibility(when,      attribute,   0.2).
compatibility(when,      negation,    0.3).
compatibility(when,      indirect,    0.2).

compatibility(what,      main,        1.0).
compatibility(what,      attribute,   0.9).
compatibility(what,      location,    0.5).
compatibility(what,      temporal,    0.4).
compatibility(what,      indirect,    0.6).
compatibility(what,      negation,    0.3).

compatibility(who,       main,        0.8).
compatibility(who,       indirect,    2.0).
compatibility(who,       attribute,   0.5).
compatibility(who,       location,    0.3).
compatibility(who,       temporal,    0.2).
compatibility(who,       negation,    0.2).

compatibility(negation,  negation,    2.0).
compatibility(negation,  main,        0.5).
compatibility(negation,  location,    0.6).
compatibility(negation,  temporal,    0.3).
compatibility(negation,  attribute,   0.2).
compatibility(negation,  indirect,    0.2).

compatibility(color_query, attribute, 2.0).
compatibility(color_query, main,      0.3).
compatibility(color_query, location,  0.1).
compatibility(color_query, temporal,  0.0).
compatibility(color_query, negation,  0.0).
compatibility(color_query, indirect,  0.1).

compatibility(size_query, attribute, 2.0).
compatibility(size_query, main,      0.4).
compatibility(size_query, location,  0.2).
compatibility(size_query, temporal,  0.0).
compatibility(size_query, negation,  0.0).
compatibility(size_query, indirect,  0.1).

compatibility(unknown,   main,        0.7).
compatibility(unknown,   _,           0.5).


% ── QUERY BONUS ────────────────────────────────────────────────────

query_bonus(who, indirect, 0.25).
query_bonus(where, negation, -0.10).
query_bonus(when, main, 0.10).


% ── CONFLICT ───────────────────────────────────────────────────────

conflict(where,     main,        0.5).
conflict(where,     attribute,   0.3).
conflict(where,     temporal,    0.2).
conflict(where,     indirect,    0.2).

conflict(when,      main,        0.5).
conflict(when,      location,    0.3).
conflict(when,      attribute,   0.2).
conflict(when,      indirect,    0.2).

conflict(what,      main,        0.3).
conflict(what,      location,    0.2).
conflict(what,      temporal,    0.2).
conflict(what,      indirect,    0.2).

conflict(who,       attribute,   0.3).
conflict(who,       location,    0.2).
conflict(who,       temporal,    0.2).

conflict(negation,  attribute,   0.3).
conflict(negation,  indirect,    0.2).
conflict(negation,  temporal,    0.2).

conflict(color_query, main,      0.6).
conflict(color_query, location,  0.3).
conflict(color_query, temporal,  0.2).
conflict(color_query, negation,  0.2).
conflict(color_query, indirect,  0.2).

conflict(size_query,  main,      0.5).
conflict(size_query,  location,  0.3).
conflict(size_query,  temporal,  0.2).
conflict(size_query,  negation,  0.2).
conflict(size_query,  indirect,  0.2).

conflict(_,          _,          0.0).


% ── HEAD 0: TYPE ──────────────────────────────────────────────────
type_head(relation(main, _, _), main) :- !.
type_head(relation(attribute, _, _), attribute) :- !.
type_head(relation(location, _, _), location) :- !.
type_head(relation(temporal, _, _), temporal) :- !.
type_head(relation(indirect, _, _), indirect) :- !.
type_head(relation(negation, _, _), negation) :- !.
type_head(_, unknown).

% ── HEAD 1: RELATION ───────────────────────────────────────────────
relation_head(relation(main, _, _), 1.0) :- !.
relation_head(relation(negation, _, _), 0.95) :- !.
relation_head(relation(temporal, _, _), 0.8) :- !.
relation_head(relation(location, _, _), 0.7) :- !.
relation_head(relation(attribute, _, _), 0.7) :- !.
relation_head(relation(indirect, _, _), 0.6) :- !.
relation_head(_, 0.5).

% ── HEAD 2: ENTITY ─────────────────────────────────────────────────
entity_head(relation(_, _, Args), Score) :-
    maplist(entity_known, Args, Scores),
    sum_list(Scores, Total),
    length(Scores, Len),
    Score is Total / Len.

entity_known(X, 1.0) :- atom(X), known_symbol(X), !.
entity_known(X, 1.0) :- compound(X), arg(1, X, A), known_symbol(A), !.
entity_known(X, 0.5) :- atom(X), !.
entity_known(_, 0.3).

% ── HEAD 3: POSITION ──────────────────────────────────────────────
position_head(relation(main, _, _), 1.0) :- !.
position_head(relation(negation, _, _), 0.9) :- !.
position_head(relation(attribute, _, _), 0.8) :- !.
position_head(relation(location, _, _), 0.7) :- !.
position_head(relation(temporal, _, _), 0.6) :- !.
position_head(relation(indirect, _, _), 0.5) :- !.
position_head(_, 0.4).

% ── HEAD 4: DISCOURSE ─────────────────────────────────────────────
discourse_head(relation(main, comprar, _), 1.0) :- !.
discourse_head(relation(main, tener, _), 1.0) :- !.
discourse_head(relation(main, visitar, _), 1.0) :- !.
discourse_head(relation(main, llevar, _), 1.0) :- !.
discourse_head(relation(main, trabajar, _), 1.0) :- !.
discourse_head(relation(main, vivir, _), 0.9) :- !.
discourse_head(relation(negation, _, _), 0.9) :- !.
discourse_head(relation(temporal, _, _), 0.8) :- !.
discourse_head(relation(attribute, _, _), 0.7) :- !.
discourse_head(relation(location, _, _), 0.7) :- !.
discourse_head(_, 0.5).

% ── HEAD 5: TEMPORAL ──────────────────────────────────────────────
temporal_head(relation(temporal, _, _), 1.0) :- !.
temporal_head(relation(main, _, _), 0.4) :- !.
temporal_head(relation(negation, _, _), 0.6) :- !.
temporal_head(_, 0.2).

% ── HEAD 6: NOVELTY ───────────────────────────────────────────────
novelty_head(relation(main, R, Args), 0.2) :-
    Args = [S,O], known_relation(S, R, O), !.
novelty_head(_, 1.0).


% ════════════════════════════════════════════════════════════════════
%  SORT / TOP-K
% ════════════════════════════════════════════════════════════════════

compare_score(Order, A, B) :-
    A = scored(_, ScoreA, _),
    B = scored(_, ScoreB, _),
    compare(Order, ScoreB, ScoreA).

top_k(Items, K, Result) :- take_k(Items, K, Result).

take_k(_, 0, []) :- !.
take_k([], _, []).
take_k([H|T], K, [H|R]) :- K > 0, K1 is K - 1, take_k(T, K1, R).
