% positional.pl
% Simbolizador SIN lexicon: segmentacion puramente estructural SVO.
% Sujeto = primer token de contenido, objeto = ultimo token de contenido,
% relacion = tramo intermedio unido con '_'.
% P2: strip de artículos/preposiciones para evitar relaciones basura
% como "followed_the_white" en vez de "followed_white".
:- use_module(library(lists)).

tokenize_pos(Sentence, Tokens) :-
    string_lower(Sentence, Lower),
    string_chars(Lower, Chars),
    exclude(pos_punct, Chars, Clean),
    string_chars(CleanStr, Clean),
    split_string(CleanStr, " ", " ", Parts),
    exclude(pos_empty, Parts, NonEmpty),
    maplist(atom_string, Tokens, NonEmpty).

pos_punct('.'). pos_punct(','). pos_punct(';'). pos_punct(':').
pos_punct('?'). pos_punct('!').
pos_empty("").

% ── Glue tokens: artículos, preposiciones, auxiliares, subordinantes ──
% Se eliminan del cálculo de SVO pero se preservan como contexto.
% Artículos
is_glue(the). is_glue(a). is_glue(an).
% Preposiciones
is_glue(of). is_glue(to). is_glue(in). is_glue(at).
is_glue(on). is_glue(for). is_glue(by). is_glue(with).
is_glue(from). is_glue(into). is_glue(through).
is_glue(about). is_glue(over). is_glue(under).
is_glue(down). is_glue(up). is_glue(out). is_glue(off).
is_glue(around). is_glue(along). is_glue(across). is_glue(between).
% Auxiliares / modales (NO incluir not/no — negación en chat.pl)
is_glue(has). is_glue(had). is_glue(have). is_glue(having).
is_glue(was). is_glue(were). is_glue(been). is_glue(been).
is_glue(will). is_glue(would). is_glue(could). is_glue(should).
is_glue(may). is_glue(might). is_glue(must). is_glue(can).
is_glue(do). is_glue(does). is_glue(did).
is_glue(yes).
% Subordinantes / relativos (NO incluir that/which — los usa chat.pl)
is_glue(who). is_glue(whom).
is_glue(whose). is_glue(where). is_glue(when). is_glue(while).
is_glue(if). is_glue(unless). is_glue(because). is_glue(since).
is_glue(although). is_glue(though). is_glue(whether).
is_glue(then). is_glue(else). is_glue(so). is_glue(very).
% Determinantes / cuantificadores (NO incluir this/that — demostrativos)
is_glue(these). is_glue(those).
is_glue(some). is_glue(any). is_glue(every).
is_glue(each). is_glue(all). is_glue(much). is_glue(many).

% ── Tokenización mejorada ────────────────────────────────────────────

% tokenize_pos_clean(+Sentence, -CleanTokens) — sin glue ni puntuación
tokenize_pos_clean(Sentence, Clean) :-
    tokenize_pos(Sentence, Tokens),
    exclude(is_glue, Tokens, Clean).

% ── parse_svo mejorado ───────────────────────────────────────────────

% parse_svo(+Tokens, -(S, R, O)): >= 3 tokens, con strip de glue.
parse_svo(Tokens, (S, R, O)) :-
    Tokens = [S|Rest],
    append(Mid, [O], Rest),
    Mid \== [],
    atomic_list_concat(Mid, '_', R).

% parse_svo_clean(+Tokens, -(S, R, O)): SVO con glue stripping.
% Primero intenta con tokens limpios; si no hay suficientes, usa todos.
parse_svo_clean(Tokens, (S, R, O)) :-
    % Obtener tokens de contenido (sin glue)
    exclude(is_glue, Tokens, Clean),
    Clean = [S|Rest],
    append(Mid, [O], Rest),
    Mid \== [],
    % Unir middle tokens como relación
    atomic_list_concat(Mid, '_', R).
parse_svo_clean(Tokens, Triple) :-
    % Fallback: usar todos los tokens (para frases muy cortas)
    parse_svo(Tokens, Triple).

% ── Detección de contenido ───────────────────────────────────────────

% content_token/1 — true si el token es contenido (no glue, no punct)
content_token(T) :-
    atom(T),
    \+ is_glue(T),
    atom_length(T, L), L >= 2.

% ── symbolize_text mejorado ──────────────────────────────────────────

% symbolize_text(+Sentence, -Triple): SVO limpio con glue stripping.
symbolize_text(Sentence, Triple) :-
    tokenize_pos(Sentence, Tokens),
    parse_svo_clean(Tokens, Triple).

% ── División de conjunciones ─────────────────────────────────────────

% split_conjunction(+Tokens, -Parts) — divide en "A and B" → [A, B]
% Maneja: "S V O and V2 O2" y "S V O and O2"
split_conjunction(Tokens, Parts) :-
    append(Left, [and|Right], Tokens),
    Left \== [], Right \== [],
    \+ member(and, Left),  % solo un "and"
    !,
    split_conjunction(Left, LeftParts),
    split_conjunction(Right, RightParts),
    append(LeftParts, RightParts, Parts).
split_conjunction(Tokens, [Tokens]) :-
    \+ member(and, Tokens).

% ── División de oraciones compuestas ─────────────────────────────────

% split_complex(+Sentence, -SubSentences)
% Divide en oraciones simples por conjunciones y marcadores.
split_complex(Sentence, SubSentences) :-
    string_lower(Sentence, Lower),
    % Buscar "and", "but", "which", "that" como puntos de corte
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

% ── symbolize_text_conj — SVO con división de conjunciones ──────────

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
