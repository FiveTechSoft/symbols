% gen_parse.pl
% Parser GENERAL sin lexicon de contenido (EXP56/55 libro autonomo).
% Frontera declarada: clase funcional CERRADA (articulos, conjunciones,
% preposiciones, pronombres, auxiliares/modales, numerales-palabra) que es
% infraestructura de la lengua, NO conocimiento del dominio.
% Cero listas de verbos/entidades/roles de contenido:
%   - verbos por MORFOLOGIA (-ed/-ing con be/-s/-base tras aux), forma
%     superficial preservada (went != go; los conceptos los agruparan).
%   - entidades por MAYUSCULAS (censo pasado-1) + runs multi-palabra.
%   - roles por POSICION (actor/objeto) + preposiciones cerradas; prep no
%     mapeada => relacion superficie (UNKNOWN preservado, no inventado).
%   - pronombres he/she/it/they => ultima entidad (recencia); i => narrator;
%     you => unknown (contado, sin inventar).
% No redefine nada de otros ficheros: autonomo (sin consults).
:- use_module(library(lists)).

% ===== clase funcional cerrada (lengua, no dominio) =====
gen_closed(the). gen_closed(a). gen_closed(an).
gen_closed(this). gen_closed(that). gen_closed(these). gen_closed(those).
gen_closed(my). gen_closed(your). gen_closed(his). gen_closed(her).
gen_closed(its). gen_closed(our). gen_closed(their).
gen_closed(all). gen_closed(every). gen_closed(each). gen_closed(any).
gen_closed(some). gen_closed(such). gen_closed(own). gen_closed(same).
gen_closed(other). gen_closed(another). gen_closed(much). gen_closed(many).
gen_closed(more). gen_closed(most). gen_closed(few). gen_closed(little).
gen_closed(both). gen_closed(either). gen_closed(neither). gen_closed(no).
gen_closed(none). gen_closed(s).  % posesivo 's
gen_closed(and). gen_closed(but). gen_closed(or). gen_closed(nor).
gen_closed(for). gen_closed(yet). gen_closed(so). gen_closed(as).
gen_closed(if). gen_closed(than). gen_closed(though). gen_closed(although).
gen_closed(while). gen_closed(because). gen_closed(however).
gen_closed(moreover). gen_closed(nevertheless). gen_closed(therefore).
gen_closed(hence). gen_closed(thus). gen_closed(else). gen_closed(otherwise).
gen_closed(of). gen_closed(to). gen_closed(in). gen_closed(on).
gen_closed(at). gen_closed(from). gen_closed(with). gen_closed(by).
gen_closed(about). gen_closed(into). gen_closed(over). gen_closed(after).
gen_closed(before). gen_closed(between). gen_closed(through). gen_closed(under).
gen_closed(against). gen_closed(among). gen_closed(around). gen_closed(without).
gen_closed(within). gen_closed(upon). gen_closed(onto). gen_closed(toward).
gen_closed(towards). gen_closed(along). gen_closed(across). gen_closed(behind).
gen_closed(beyond). gen_closed(beside). gen_closed(except). gen_closed(until).
gen_closed(till). gen_closed(since). gen_closed(during). gen_closed(past).
gen_closed(down). gen_closed(off). gen_closed(out). gen_closed(up).
gen_closed(inside). gen_closed(outside). gen_closed(above). gen_closed(below).
gen_closed(near).
gen_closed(i). gen_closed(you). gen_closed(he). gen_closed(she).
gen_closed(it). gen_closed(we). gen_closed(they).
gen_closed(me). gen_closed(him). gen_closed(us). gen_closed(them).
gen_closed(myself). gen_closed(yourself). gen_closed(himself).
gen_closed(herself). gen_closed(itself). gen_closed(ourselves).
gen_closed(themselves).
gen_closed(who). gen_closed(whom). gen_closed(whose). gen_closed(which).
gen_closed(what). gen_closed(there). gen_closed(here). gen_closed(where).
gen_closed(when). gen_closed(how). gen_closed(why).
gen_closed(be). gen_closed(am). gen_closed(is). gen_closed(are).
gen_closed(was). gen_closed(were). gen_closed(been). gen_closed(being).
gen_closed(have). gen_closed(has). gen_closed(had). gen_closed(having).
gen_closed(do). gen_closed(does). gen_closed(did).
gen_closed(will). gen_closed(would). gen_closed(shall). gen_closed(should).
gen_closed(may). gen_closed(might). gen_closed(must). gen_closed(can).
gen_closed(could).
gen_closed(not). gen_closed(never). gen_closed(ever). gen_closed(very).
gen_closed(too). gen_closed(also). gen_closed(even). gen_closed(still).
gen_closed(now). gen_closed(then). gen_closed(quite). gen_closed(rather).
gen_closed(almost). gen_closed(nearly). gen_closed(hardly). gen_closed(scarcely).
gen_closed(once). gen_closed(twice). gen_closed(always).
gen_closed(one). gen_closed(two). gen_closed(three). gen_closed(four).
gen_closed(five). gen_closed(six). gen_closed(seven). gen_closed(eight).
gen_closed(nine). gen_closed(ten). gen_closed(eleven). gen_closed(twelve).
gen_closed(thirteen). gen_closed(fourteen). gen_closed(fifteen).
gen_closed(sixteen). gen_closed(seventeen). gen_closed(eighteen).
gen_closed(nineteen). gen_closed(twenty). gen_closed(thirty). gen_closed(forty).
gen_closed(fifty). gen_closed(sixty). gen_closed(seventy). gen_closed(eighty).
gen_closed(ninety). gen_closed(hundred). gen_closed(thousand).
gen_closed(million). gen_closed(first). gen_closed(second). gen_closed(third).
gen_closed(oh). gen_closed(ah).

gen_be(be). gen_be(am). gen_be(is). gen_be(are).
gen_be(was). gen_be(were). gen_be(been). gen_be(being).
gen_modal_base(will). gen_modal_base(would). gen_modal_base(shall).
gen_modal_base(should). gen_modal_base(may). gen_modal_base(might).
gen_modal_base(must). gen_modal_base(can). gen_modal_base(could).
gen_modal_base(have). gen_modal_base(has). gen_modal_base(had).
gen_modal_base(do). gen_modal_base(does). gen_modal_base(did).

% prep -> rol (cerrado, declarado). No mapeada => relacion superficie.
gen_role(in, location). gen_role(at, location). gen_role(on, location).
gen_role(upon, location). gen_role(inside, location). gen_role(outside, location).
gen_role(under, location). gen_role(over, location). gen_role(above, location).
gen_role(below, location). gen_role(behind, location). gen_role(near, location).
gen_role(after, time). gen_role(before, time). gen_role(until, time).
gen_role(till, time). gen_role(since, time). gen_role(during, time).
gen_role(from, source). gen_role(with, instrument). gen_role(to, recipient).
gen_role(for, recipient). gen_role(about, topic). gen_role(by, agent_by).

% ===== tokenizador con caso preservado (sin pcre) =====
% gen_tokenize(+Sentence, -Lower, -Raw): dos vistas alineadas por indice.
gen_tokenize(Sentence, Lower, Raw) :-
    string_chars(Sentence, Chars),
    maplist(gen_norm_char, Chars, Normed),
    string_chars(NormStr, Normed),
    split_string(NormStr, " ", " ", Chunks0),
    exclude(gen_empty, Chunks0, Chunks),
    findall(T, (member(C, Chunks), gen_chunk(C, T)), R0),
    exclude(gen_empty, R0, R1),
    maplist(atom_string, Raw, R1),
    maplist(gen_down, R1, L1),
    Lower = L1.

% Normaliza comillas/apostrofos/guiones a ' o espacio (una pasada, chars).
gen_norm_char('’', '\'') :- !.
gen_norm_char('‘', '\'') :- !.
gen_norm_char('“', ' ') :- !.
gen_norm_char('”', ' ') :- !.
gen_norm_char('"', ' ') :- !.
gen_norm_char('á', 'a') :- !.
gen_norm_char('é', 'e') :- !.
gen_norm_char('í', 'i') :- !.
gen_norm_char('ó', 'o') :- !.
gen_norm_char('ú', 'u') :- !.
gen_norm_char('ü', 'u') :- !.
gen_norm_char('ñ', 'n') :- !.
gen_norm_char('ç', 'c') :- !.
gen_norm_char('à', 'a') :- !.
gen_norm_char('è', 'e') :- !.
gen_norm_char('ì', 'i') :- !.
gen_norm_char('ò', 'o') :- !.
gen_norm_char('ù', 'u') :- !.
gen_norm_char('â', 'a') :- !.
gen_norm_char('ê', 'e') :- !.
gen_norm_char('î', 'i') :- !.
gen_norm_char('ô', 'o') :- !.
gen_norm_char('û', 'u') :- !.
gen_norm_char('(', ' ') :- !.
gen_norm_char(')', ' ') :- !.
gen_norm_char('[', ' ') :- !.
gen_norm_char(']', ' ') :- !.
gen_norm_char('—', ' ') :- !.
gen_norm_char('–', ' ') :- !.
gen_norm_char('-', ' ') :- !.
gen_norm_char('*', ' ') :- !.
gen_norm_char(C, C).

gen_down(A, L) :- atom_string(A, S), string_lower(S, LS), atom_string(L, LS).

% Contracciones inglesas -> auxiliares (todos en clase cerrada).
gen_chunk(C, T) :-
    atom_chars(C, Cs),
    drop_punct(Cs, D1),
    reverse(D1, D2rev),
    drop_punct(D2rev, D3rev),
    reverse(D3rev, D3),
    ( D3 == [] -> T = ""
    ; atom_chars(Clean, D3),
      gen_chunk_core(Clean, T)
    ).

drop_punct([C|Cs], Out) :-
    member(C, ['.', ',', ';', ':', '?', '!', '\'']), !,
    drop_punct(Cs, Out).
drop_punct(L, L).

gen_chunk_core(C, T) :- sub_atom(C, _, 3, 0, 'n\'t'), !,
    sub_atom(C, 0, _, 3, Stem),
    ( Stem == '' -> T = 'not' ; (T = Stem ; T = 'not') ).
gen_chunk_core(C, T) :- sub_atom(C, _, 2, 0, Suf), member(Suf, ['\'s', '\'m']), !,
    sub_atom(C, 0, _, 2, Stem),
    ( Stem == '' -> gen_suf_word(Suf, T) ; (T = Stem ; gen_suf_word(Suf, T)) ).
gen_chunk_core(C, T) :- sub_atom(C, _, 3, 0, Suf), member(Suf, ['\'re', '\'ve', '\'ll', '\'d']), !,
    sub_atom(C, 0, _, 3, Stem),
    ( Stem == '' -> gen_suf_word(Suf, T) ; (T = Stem ; gen_suf_word(Suf, T)) ).
gen_chunk_core(C, C).

gen_suf_word('\'s', s). gen_suf_word('\'m', am).
gen_suf_word('\'re', are). gen_suf_word('\'ve', have).
gen_suf_word('\'ll', will). gen_suf_word('\'d', would).

gen_empty("").

% ===== verbos por morfologia (forma superficial) =====
gen_is_verb(Lower, Raw, I) :-
    nth0(I, Lower, W),
    \+ gen_closed(W),
    \+ gen_be(W),
    \+ gen_modal_base(W),
    ( gen_ed(W)
    ; gen_ing(Lower, I, W)
    ; gen_3sg(W)
    ; gen_base_after_aux(Lower, Raw, I, W)
    ), !.

gen_ed(W) :-
    atom_length(W, L), L > 4,
    sub_atom(W, _, 2, 0, 'ed').

gen_ing(Lower, I, W) :-
    atom_length(W, L), L > 5,
    sub_atom(W, _, 3, 0, 'ing'),
    I > 0, J is I - 1,
    nth0(J, Lower, Prev),
    gen_be(Prev).

gen_3sg(W) :-
    atom_length(W, L), L > 3,
    sub_atom(W, _, 1, 0, 's'),
    sub_atom(W, L2, 1, _, _),
    L2 is L - 2,
    sub_atom(W, L2, 1, 0, C),
    \+ member(C, ['s', 'u', 'i', 'x']).

gen_base_after_aux(Lower, Raw, I, W) :-
    I > 0, J is I - 1,
    nth0(J, Lower, Prev),
    gen_modal_base(Prev),
    % "will Alice": el candidato no debe ser nombre propio (salvo inicial).
    nth0(I, Raw, R),
    ( I == 0 -> true
    ; ( atom_chars(R, [C|_]), char_type(C, upper) -> fail ; true )
    ).

% ===== contenido: no cerrado, no verbo =====
gen_content_at(Lower, Raw, I) :-
    nth0(I, Lower, W),
    \+ gen_closed(W),
    \+ gen_is_verb(Lower, Raw, I).

% ===== NPs: runs maximales de contenido (indices) =====
% gen_np_runs(+Lower, +Raw, -Runs): lista de listas de indices.
gen_np_runs(Lower, Raw, Runs) :-
    length(Lower, N),
    N1 is N - 1,
    gen_runs(0, N1, Lower, Raw, [], Runs).

gen_runs(I, Max, _, _, Acc, Runs) :-
    I > Max, !,
    reverse(Acc, Runs).
gen_runs(I, Max, Lower, Raw, Acc, Runs) :-
    ( gen_content_at(Lower, Raw, I) ->
        gen_run(I, Max, Lower, Raw, Run, Next),
        I1 = Next
    ; Run = skip, I1 is I + 1
    ),
    ( Run == skip -> Acc1 = Acc ; Acc1 = [Run|Acc] ),
    gen_runs(I1, Max, Lower, Raw, Acc1, Runs).

gen_run(I, Max, Lower, Raw, [I|Rest], Next) :-
    I =< Max,
    gen_content_at(Lower, Raw, I), !,
    I1 is I + 1,
    gen_run(I1, Max, Lower, Raw, Rest, Next).
gen_run(Next, _, _, _, [], Next).

% NP clasificada: entity(Name) | common(Head).
% Entidad = run con mayuscula no-inicial o en censo (lowercase, _-joined).
gen_np_class(Raw, Lower, Run, Census, entity(Name)) :-
    gen_run_cap(Raw, Run, HasCap),
    HasCap == true,
    gen_run_entity_ok(Raw, Lower, Run, Census), !,
    findall(W, (member(I, Run), nth0(I, Lower, W)), Ws),
    atomic_list_concat(Ws, '_', Name).
gen_np_class(_, Lower, Run, _, common(Head)) :-
    last(Run, LI),
    nth0(LI, Lower, Head).

gen_run_cap(Raw, Run, true) :-
    member(I, Run),
    nth0(I, Raw, R),
    atom_chars(R, [C|_]),
    char_type(C, upper), !.
gen_run_cap(_, _, false).

% Mayuscula valida: no es primer token de frase, o esta en el censo,
% o el run tiene 2+ palabras con 2+ mayusculas.
gen_run_entity_ok(Raw, Lower, Run, Census) :-
    Run = [F|_],
    ( F > 0 -> true
    ; findall(W, (member(I, Run), nth0(I, Lower, W)), Ws),
      atomic_list_concat(Ws, '_', Nm),
      member(Nm, Census)
    ), !.
gen_run_entity_ok(Raw, _, Run, _) :-
    findall(I, (member(I, Run), nth0(I, Raw, R),
                atom_chars(R, [C|_]), char_type(C, upper)), Caps),
    length(Caps, N),
    N >= 2.

% ===== entrada: gen_frames(+Lower, +Raw, +Census, +LastEnt, -Frames, -Stats, -Mentions)
% Frame = frame(S, V, O, PPs) con PPs = [prep-NP,...] crudos.
% Mentions = entidades mencionadas en orden (para el registro documental).
% Resolucion pronominal aqui (he/she/it/they=>LastEnt; i=>narrator).
gen_frames(Lower, Raw, Census, LastEnt, Frames, st(NV, NSkip, NCoref, NUnk), Mentions) :-
    findall(I, gen_is_verb(Lower, Raw, I), VIs0),
    take_n(VIs0, 4, VIs),
    length(VIs0, NV),
    gen_np_runs(Lower, Raw, Runs),
    gen_frames_loop(VIs, Lower, Raw, Census, Runs, LastEnt, Frames,
                    0, NSkip, 0, NCoref, 0, NUnk, [], Mentions).

take_n(_, 0, []) :- !.
take_n([], _, []) :- !.
take_n([H|T], K, [H|R]) :- K1 is K - 1, take_n(T, K1, R).

gen_frames_loop([], _, _, _, _, _, [], NS, NS, NC, NC, NU, NU, M, M).
gen_frames_loop([I|Vs], Lower, Raw, Census, Runs, LastEnt, Frames,
                S0, NS, C0, NC, U0, NU, M0, M) :-
    nth0(I, Lower, V),
    gen_side(Runs, Lower, Raw, Census, I, before, LastEnt, S, C1, U1),
    gen_side(Runs, Lower, Raw, Census, I, after, LastEnt, O, C2, U2),
    ( S \== none, O \== none ->
        gen_pps(Runs, Lower, Raw, Census, I, LastEnt, PPs, C3, U3),
        Frames = [frame(S, V, O, PPs)|Rest],
        NS1 = S0, NC1 is C0 + C1 + C2 + C3, NU1 is U0 + U1 + U2 + U3,
        frame_mentions(S, O, PPs, FM),
        append(FM, MRest, M1)
    ; Frames = Rest,
      NS1 is S0 + 1, NC1 = C0, NU1 = U0, M1 = MRest
    ),
    gen_frames_loop(Vs, Lower, Raw, Census, Runs, LastEnt, Rest,
                    NS1, NS, NC1, NC, NU1, NU, M0, MRest),
    M = M1.

frame_mentions(S, O, PPs, M) :-
    findall(N, (member(X, [S, O]), X = entity(N)), SO),
    findall(N, (member(_-NPV, PPs), NPV = entity(N)), PP),
    append(SO, PP, M).

% Lado: argumento (NP o pronombre) mas cercano antes/despues del verbo.
% Los pronombres compiten por posicion con los NPs (si no, nunca resuelven).
gen_side(Runs, Lower, Raw, Census, I, Dir, LastEnt, Val, NC, NU) :-
    findall(Key-K, gen_arg_cand(Runs, Lower, I, Dir, Key, K), Cands),
    ( Dir == before ->
        keysort(Cands, Sorted),
        last(Sorted, _-Best)
    ; keysort(Cands, [_-Best|_])
    ),
    gen_arg_value(Best, Lower, Raw, Census, LastEnt, Val, NC, NU), !.
gen_side(_, _, _, _, _, _, _, none, 0, 0).

% Candidatos: run->[indice-extremo] y pronombre->[indice].
gen_arg_cand(Runs, Lower, I, before, Key, run-Run) :-
    member(Run, Runs),
    last(Run, LI), LI < I, Key = LI.
gen_arg_cand(Runs, Lower, I, after, Key, run-Run) :-
    member(Run, Runs),
    Run = [F|_], F > I, Key = F.
gen_arg_cand(_, Lower, I, before, Key, pron-P) :-
    nth0(Key, Lower, P),
    Key < I,
    gen_pronoun(P).
gen_arg_cand(_, Lower, I, after, Key, pron-P) :-
    nth0(Key, Lower, P),
    Key > I,
    gen_pronoun(P).

gen_arg_value(run-Run, Lower, Raw, Census, LastEnt, Val, NC, NU) :-
    gen_np_value(Run, Lower, Raw, Census, LastEnt, Val, NC, NU).
gen_arg_value(pron-P, _, _, _, LastEnt, Val, NC, NU) :-
    gen_resolve(P, LastEnt, Val, NC, NU).

% Valor de NP: pronombre resuelto | entidad | comun.
gen_np_value(Run, Lower, Raw, Census, LastEnt, Val, NC, NU) :-
    Run = [F|_],
    nth0(F, Lower, W),
    gen_pronoun(W), !,
    gen_resolve(W, LastEnt, Val, NC, NU).
gen_np_value(Run, Lower, Raw, Census, _, Val, 0, 0) :-
    gen_np_class(Raw, Lower, Run, Census, Cl),
    ( Cl = entity(Name) -> Val = entity(Name)
    ; Cl = common(Head) -> Val = np(Head)
    ).

gen_pronoun(he). gen_pronoun(she). gen_pronoun(him). gen_pronoun(her).
gen_pronoun(it). gen_pronoun(they). gen_pronoun(them).
gen_pronoun(i). gen_pronoun(me). gen_pronoun(you).

gen_resolve(you, _, unknown, 0, 1) :- !.
gen_resolve(i, _, entity(narrator), 0, 0) :- !.
gen_resolve(me, _, entity(narrator), 0, 0) :- !.
gen_resolve(_, none, unknown, 0, 1) :- !.
gen_resolve(_, LastEnt, entity(LastEnt), 1, 0).

% PPs tras el verbo: [prep-ValorNP...] (prep cruda, mapeo en doc_corpus).
% Objeto de prep = NP que empieza justo despues, o pronombre.
gen_pps(Runs, Lower, Raw, Census, I, LastEnt, PPs, NC, NU) :-
    findall(P-NPV-C-U, (nth0(J, Lower, P),
                        J > I,
                        gen_closed(P),
                        gen_is_prep(P),
                        J1 is J + 1,
                        gen_pp_obj(Runs, Lower, Raw, Census, LastEnt, J1, NPV, C, U)),
            Found),
    findall(P-NPV, member(P-NPV-_-_, Found), PPs),
    findall(C, member(_-_-C-_, Found), Cs),
    sum_small(Cs, NC),
    findall(U, member(_-_-_-U, Found), Us),
    sum_small(Us, NU).

gen_pp_obj(Runs, Lower, Raw, Census, LastEnt, J1, NPV, C, U) :-
    member(Run, Runs),
    Run = [F|_], F == J1, !,
    gen_np_value(Run, Lower, Raw, Census, LastEnt, NPV, C, U).
gen_pp_obj(_, Lower, _, _, LastEnt, J1, NPV, C, U) :-
    nth0(J1, Lower, W),
    gen_pronoun(W), !,
    gen_resolve(W, LastEnt, NPV, C, U).

sum_small([], 0).
sum_small([H|T], S) :- sum_small(T, S0), S is S0 + H.

gen_is_prep(P) :-
    gen_role(P, _), !.
gen_is_prep(of).
