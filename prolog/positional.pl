% positional.pl
% Simbolizador SIN lexicon: segmentacion puramente estructural SVO.
% Sujeto = primer token, objeto = ultimo, relacion = tramo intermedio
% unido con '_' ("lives in" -> lives_in). Sin listas de verbos, tipos
% ni plantillas: cualquier palabra es opaca para el sistema.
% Limites honestos: requiere orden SVO rigido; la morfologia crea
% relaciones distintas (visited =/= visits); sin adjuntos.
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

% parse_svo(+Tokens, -(S, R, O)): >= 3 tokens.
parse_svo(Tokens, (S, R, O)) :-
    Tokens = [S|Rest],
    append(Mid, [O], Rest),
    Mid \== [],
    atomic_list_concat(Mid, '_', R).

% symbolize_text(+Sentence, -Triple): falla si no hay forma SVO.
symbolize_text(Sentence, Triple) :-
    tokenize_pos(Sentence, Tokens),
    parse_svo(Tokens, Triple).
