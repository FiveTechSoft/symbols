% preflight.pl
% Ritual automatizado antes de publicar: sintaxis, aridad, dinamicas,
% llamadas indefinidas, consults existentes y plantillas findall/bagof/setof
% desconectadas (toda variable nombrada de la plantilla debe aparecer en el
% objetivo; EXP36). Estatico (no ejecuta nada).
% Uso: preflight('experiment19.pl'). Veredicto pass/fail con informe.
% Limites honestos: allowlist pragmatica de builtins/libs; HO aproximado.
:- use_module(library(lists)).

:- dynamic pf_finding/2.

preflight(File) :-
    retractall(pf_finding(_, _)),
    ( catch(preflight_inner(File), E,
            pf_fail(syntax, File, E)) ->
        true
    ; pf_fail(internal, File, 'collection failed')
    ),
    report_preflight(File).

preflight_inner(File) :-
    collect_transitive(File, [File], _, DynAll, HeadsAll, Calls, _Consults,
                       Missing),
    forall(member(M, Missing),
           pf_fail(consult, File, M)),
    check_undefined(File, HeadsAll, DynAll, Calls),
    check_arity(File, HeadsAll, Calls),
    check_dynamics(File, DynAll, Calls),
    check_findall_templates(File).

% ---------- lectura ----------
read_all_terms(File, Terms) :-
    open(File, read, S, [encoding(utf8)]),
    read_terms_loop(S, Terms),
    close(S).

read_terms_loop(S, [T|Ts]) :-
    read_term(S, T, []),
    T \== end_of_file, !,
    read_terms_loop(S, Ts).
read_terms_loop(_, []).

% ---------- recoleccion transitiva (parseo, sin cargar) ----------
collect_transitive(File, Seen, SeenOut, Dyn, Heads, Calls, Consults,
                   Missing) :-
    read_all_terms(File, Terms),
    file_terms_info(Terms, Dyn0, Heads0, Calls0, Consults0),
    file_directory_name(File, Dir),
    resolve_consults(Dir, Consults0, Seen, SeenOut, DynR, HeadsR, Missing),
    append(Dyn0, DynR, Dyn),
    append(Heads0, HeadsR, Heads),
    Calls = Calls0,
    Consults = Consults0.

resolve_consults(_, [], Seen, Seen, [], [], []).
resolve_consults(Dir, [C|Cs], Seen, SeenOut, Dyn, Heads, Missing) :-
    atomic_list_concat([Dir, '/', C], P),
    ( member(P, Seen) ->
        resolve_consults(Dir, Cs, Seen, SeenOut, Dyn, Heads, Missing)
    ; exists_file(P) ->
        collect_transitive(P, [P|Seen], Seen1,
                           Dyn0, Heads0, _, _, _),
        resolve_consults(Dir, Cs, Seen1, SeenOut, DynR, HeadsR, Missing),
        append(Dyn0, DynR, Dyn),
        append(Heads0, HeadsR, Heads)
    ; Missing = [C|MissingR],
      resolve_consults(Dir, Cs, Seen, SeenOut, Dyn, Heads, MissingR)
    ).

file_terms_info(Terms, Dyn, Heads, Calls, Consults) :-
    findall(D, ( member((:- dynamic(Ds)), Terms),
                 comma_list(Ds, D)
               ),
            Dyn0),
    flatten(Dyn0, Dyn1),
    maplist(dyn_indicator, Dyn1, Dyn),
    findall(H, ( member(T, Terms),
                 clause_head(T, H),
                 H \== none
               ),
            Heads0),
    maplist(head_indicator, Heads0, Heads),
    findall(C, ( member(T, Terms),
                 clause_body_calls(T, C)
               ),
            Calls0),
    flatten(Calls0, Calls),
    findall(Cf, ( member((:- consult(Cf)), Terms) ;
                  member((:- ensure_loaded(Cf)), Terms)
                ),
            Consults).

comma_list((A, B), [A|R]) :- !, comma_list(B, R).
comma_list(A, [A]).

dyn_indicator(Pred/Arity, Pred/Arity) :- !.
dyn_indicator(Pred, Name/Arity) :-
    Pred =.. [Name|Args],
    length(Args, Arity).

head_indicator(H, Name/Arity) :-
    functor(H, Name, Arity).

clause_head((H :- _), H) :- !.
clause_head((H --> _), H) :- !.
clause_head((:- _), none) :- !.
clause_head(H, H).

% --- walker de cuerpos: solo posiciones de objetivo ---
clause_body_calls((H :- B), Calls) :- callable(H), !, walk_body(B, Calls).
clause_body_calls((H --> B), Calls) :- callable(H), !, walk_body(B, Calls).
clause_body_calls((:- initialization(G)), Calls) :- !, walk_body(G, Calls).
clause_body_calls(_, []).

walk_body(V, []) :- var(V), !.
walk_body((A, B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body((A; B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body((A -> B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body((A *-> B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body(\+ G, Calls) :- !, walk_body(G, Calls).
walk_body(not(G), Calls) :- !, walk_body(G, Calls).
walk_body(once(G), Calls) :- !, walk_body(G, Calls).
walk_body(ignore(G), Calls) :- !, walk_body(G, Calls).
walk_body(forall(A, B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body(foreach(A, B), Calls) :- !,
    walk_body(A, C1), walk_body(B, C2), append(C1, C2, Calls).
walk_body(findall(_, G, _), Calls) :- !, walk_body(G, Calls).
walk_body(bagof(_, G, _), Calls) :- !, walk_body(G, Calls).
walk_body(setof(_, G, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _, _, _, _, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _, _, _, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _, _, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(call(G, _), Calls) :- !, walk_body(G, Calls).
walk_body(catch(G, _, _), Calls) :- !, walk_body(G, Calls).
walk_body(setup_call_cleanup(S, C, _), Calls) :- !,
    walk_body(S, C1), walk_body(C, C2), append(C1, C2, Calls).
walk_body(include(P, _, _), Calls) :- !, ho_call(P, 1, Calls).
walk_body(exclude(P, _, _), Calls) :- !, ho_call(P, 1, Calls).
walk_body(maplist(P, _, _), Calls) :- !, ho_call(P, 2, Calls).
walk_body(maplist(P, _, _, _), Calls) :- !, ho_call(P, 3, Calls).
walk_body(assertz(_), []) :- !.
walk_body(asserta(_), []) :- !.
walk_body(assert(_), []) :- !.
walk_body(retract(_), []) :- !.
walk_body(retractall(_), []) :- !.
walk_body(abolish(_), []) :- !.
walk_body(_ is _, []) :- !.
walk_body(_ =:= _, []) :- !.
walk_body(_ =\= _, []) :- !.
walk_body(_ < _, []) :- !.
walk_body(_ > _, []) :- !.
walk_body(_ =< _, []) :- !.
walk_body(_ >= _, []) :- !.
walk_body(_ = _, []) :- !.
walk_body(_ \= _, []) :- !.
walk_body(_ == _, []) :- !.
walk_body(_ \== _, []) :- !.
walk_body(_ =.. _, []) :- !.
walk_body(true, []) :- !.
walk_body(fail, []) :- !.
walk_body(!, []) :- !.
walk_body(repeat, []) :- !.
walk_body(G, [Name/Arity]) :-
    callable(G),
    functor(G, Name, Arity).

% ho_call: la closure recibe N args extra al llamarse (include +1, etc).
ho_call(P, N, [Name/Req]) :-
    ( atom(P) -> Name = P, Arity = 0
    ; callable(P), functor(P, Name, Arity)
    ),
    Req is Arity + N, !.
ho_call(_, _, []).

% ---------- modo proyecto: union de cabezas/dinamicas del directorio --
% Los modulos usan lo que les dan sus llamantes (contexto hacia arriba);
% comprobar cada fichero contra la union es la semantica correcta aqui.
preflight_project(Dir) :-
    retractall(pf_finding(_, _)),
    pl_files_recursive(Dir, Pls),
    sort(Pls, Fulls),
    collect_union(Fulls, UH, UD),
    forall(member(Full, Fulls),
           check_against_union(Full, UH, UD)),
    report_preflight(Dir).

% recursive .pl discovery (skips .git); paths relative to CWD for reports
pl_files_recursive(Dir, Pls) :-
    directory_files(Dir, Entries0),
    exclude(dot_entry, Entries0, Entries),
    findall(Q, ( member(E, Entries),
                 atomic_list_concat([Dir, '/', E], P),
                 dir_pl_file(P, Q)
               ),
            Pls).

% dir_pl_file(+Path, -PlFile): el propio fichero o los de dentro.
dir_pl_file(P, P) :-
    pl_file(P), !.
dir_pl_file(P, Q) :-
    exists_directory(P),
    \+ subdir_skip(P),
    pl_files_recursive(P, Sub),
    member(Q, Sub).

dot_entry('.').
dot_entry('..').

subdir_skip(P) :-
    sub_atom(P, _, _, _, '/.git').

pl_file(F) :-
    atom_string(A, F),
    sub_atom(A, _, 3, 0, '.pl'),
    exists_file(F).

collect_union([], [], []).
collect_union([F|Fs], UH, UD) :-
    catch(( read_all_terms(F, Terms),
            file_terms_info(Terms, Dyn, Heads, _, _)
          ),
          _, (Dyn = [], Heads = [])),
    collect_union(Fs, UH0, UD0),
    append(Heads, UH0, UH),
    append(Dyn, UD0, UD).

check_against_union(Full, UH, UD) :-
    catch(read_all_terms(Full, Terms), E,
          ( pf_fail(syntax, Full, E), fail )),
    file_terms_info(Terms, _, _, Calls, _),
    sort(Calls, UC),
    forall(member(N/A, UC),
           ( member(N/A, UH) -> true
           ; member(N/A, UD) -> true
           ; kernel_pred(N/A) -> true
           ; resolvable_builtin(N/A) -> true
           ; pf_fail(undefined, Full, N/A)
           )),
    check_findall_templates(Full).

% ---------- plantillas findall/bagof/setof desconectadas (EXP36) ----------
% Toda variable NOMBRADA de la plantilla debe aparecer en el objetivo.
% findall(X, (memory_relation(_,R,E,_,_),...), Ins) con X ausente devuelve
% [_] por solucion: longitudes "correctas", contenido falso (EXP36: grados
% (6,6)/(15,15) contando toda la memoria). Las anonimas (_) estan exentas
% (idioma de conteo: findall(_, G, L), length(L, N)); SWI las omite en
% variable_names, asi que la exencion es automatica.
check_findall_templates(File) :-
    catch(read_named_terms(File, Terms), E,
          ( pf_fail(syntax, File, E), fail )),
    forall(( member(T-Names, Terms),
             clause_check_body(T, B),
             sub_term(Sub, B),
             collector_sub(Sub, Temp, Goal),
             term_variables(Temp, TVs),
             member(V, TVs),
             \+ goal_var_in(Goal, V),
             member(Nm=VV, Names), VV == V,
             term_to_atom(Temp, TA),
             atomic_list_concat([Nm, ' not in goal of ', TA], Detail)
           ),
           pf_fail(findall_template, File, Detail)).

% Solo cuerpos de reglas (y initialization): los findall literales en
% cabezas (p. ej. walk_body(findall(_,G,_),...) o el propio
% collector_sub(findall(Temp,Goal,_),...)) son patrones, no colectas.
clause_check_body((H :- B), B) :- callable(H), !.
clause_check_body((:- initialization(G)), G) :- !.
clause_check_body(_, none).

read_named_terms(File, Terms) :-
    open(File, read, S, [encoding(utf8)]),
    read_named_loop(S, Terms),
    close(S).

read_named_loop(S, [T-Names|Ts]) :-
    read_term(S, T, [variable_names(Names)]),
    T \== end_of_file, !,
    read_named_loop(S, Ts).
read_named_loop(_, []).

collector_sub(findall(Temp, Goal, _), Temp, Goal).
collector_sub(bagof(Temp, Goal, _), Temp, Goal).
collector_sub(setof(Temp, Goal, _), Temp, Goal).

goal_var_in(Goal, V) :-
    term_variables(Goal, GVs),
    member(VV, GVs), VV == V, !.

% --- asserts: objetivos de modificacion (registrados, no llamadas) ---
assert_targets_in_terms(Terms, Targets) :-
    findall(N/A, ( member(T, Terms),
                   clause_conjuncts(T, Conj),
                   member(B, Conj),
                   assert_goal(B, Inner),
                   callable(Inner),
                   functor(Inner, N, A)
                 ),
            Targets).

clause_conjuncts((H :- B), Bs) :- !,
    ( var(H) -> Bs = [] ; conj_list(B, Bs) ).
clause_conjuncts(_, []).

conj_list((A, B), [A|R]) :- !, conj_list(B, R).
conj_list(A, [A]).

assert_goal(assertz(X), X).
assert_goal(asserta(X), X).
assert_goal(assert(X), X).
assert_goal(retract(X), X).
assert_goal(retractall(X), X).

% ---------- verificacion ----------
check_undefined(File, Heads, Dyn, Calls) :-
    sort(Heads, UH),
    sort(Dyn, UD),
    sort(Calls, UC),
    forall(member(N/A, UC),
           ( member(N/A, UH) -> true
           ; member(N/A, UD) -> true
           ; kernel_pred(N/A) -> true
           ; resolvable_builtin(N/A) -> true
           ; pf_fail(undefined, File, N/A)
           )).

% builtins visibles sin importar (incluye autoload de sistema)
resolvable_builtin(N/A) :-
    functor(Head, N, A),
    predicate_property(Head, built_in), !.
resolvable_builtin(N/A) :-
    functor(Head, N, A),
    predicate_property(Head, autoload), !.
resolvable_builtin(N/A) :-
    functor(Head, N, A),
    predicate_property(Head, imported_from(_)), !.

check_arity(File, Heads, Calls) :-
    sort(Heads, UH),
    sort(Calls, UC),
    forall(member(N/A, UC),
           ( setof(Ar, member(N/Ar, UH), Ars),
             \+ member(A, Ars) ->
               pf_fail(arity, File, N/A-expects-Ars)
           ; true
           )).

check_dynamics(File, Dyn, _Calls) :-
    % exige declarar lo asertado en el propio fichero o deps (transitiva
    % para dynamics, principal para targets: aproximacion documentada).
    current_preflight_terms(File, Terms),
    assert_targets_in_terms(Terms, Targets),
    sort(Dyn, UD),
    sort(Targets, UT),
    forall(member(T, UT),
           ( member(T, UD) -> true
           ; pf_fail(dynamic, File, T)
           )).

% terminos del fichero principal (para asserts; simple, sin transitiva)
current_preflight_terms(File, Terms) :-
    read_all_terms(File, Terms).

% ---------- allowlist pragmatica ----------
kernel_pred(true/0). kernel_pred(fail/0). kernel_pred(!/0).
kernel_pred(repeat/0). kernel_pred(call/1). kernel_pred(once/1).
kernel_pred(is/2).
kernel_pred(member/2). kernel_pred(memberchk/2). kernel_pred(append/3).
kernel_pred(select/3). kernel_pred(select/4). kernel_pred(reverse/2).
kernel_pred(length/2). kernel_pred(sort/2). kernel_pred(keysort/2).
kernel_pred(msort/2). kernel_pred(last/2). kernel_pred(nth0/3).
kernel_pred(nth1/3). kernel_pred(nth1/4). kernel_pred(flatten/2).
kernel_pred(subtract/3). kernel_pred(intersection/3). kernel_pred(union/3).
kernel_pred(ord_union/3). kernel_pred(ord_intersection/3).
kernel_pred(numlist/3). kernel_pred(between/3). kernel_pred(succ/2).
kernel_pred(plus/3).
kernel_pred(findall/3). kernel_pred(bagof/3). kernel_pred(setof/3).
kernel_pred(forall/2). kernel_pred(foreach/2). kernel_pred(include/3).
kernel_pred(exclude/3). kernel_pred(maplist/2). kernel_pred(maplist/3).
kernel_pred(maplist/4). kernel_pred(ignore/1). kernel_pred(catch/3).
kernel_pred(setup_call_cleanup/3). kernel_pred(setup_call_catcher_cleanup/4).
kernel_pred(open/3). kernel_pred(open/4). kernel_pred(close/1).
kernel_pred(read_term/3). kernel_pred(read_term/2). kernel_pred(read/1).
kernel_pred(read/2). kernel_pred(write/1). kernel_pred(write/2).
kernel_pred(writeln/1). kernel_pred(writeln/2). kernel_pred(print/1).
kernel_pred(print/2). kernel_pred(format/2). kernel_pred(format/3).
kernel_pred(nl/0). kernel_pred(nl/1). kernel_pred(tab/1).
kernel_pred(atom_length/2). kernel_pred(atom_chars/2).
kernel_pred(atom_codes/2). kernel_pred(atom_concat/3).
kernel_pred(atom_number/2). kernel_pred(atom_string/2).
kernel_pred(number_chars/2). kernel_pred(number_codes/2).
kernel_pred(number_string/2). kernel_pred(term_to_atom/2).
kernel_pred(term_string/2). kernel_pred(atomic_list_concat/2).
kernel_pred(atomic_list_concat/3). kernel_pred(atomic_concat/3).
kernel_pred(sub_atom/5). kernel_pred(sub_string/5).
kernel_pred(string_length/2). kernel_pred(string_chars/2).
kernel_pred(string_codes/2). kernel_pred(string_lower/2).
kernel_pred(string_upper/2). kernel_pred(split_string/4).
kernel_pred(atomics_to_string/2). kernel_pred(term_variables/2).
kernel_pred(copy_term/2). kernel_pred(arg/3). kernel_pred(functor/3).
kernel_pred('=..'/2). kernel_pred(atom/1). kernel_pred(atomic/1).
kernel_pred(number/1). kernel_pred(integer/1). kernel_pred(float/1).
kernel_pred(var/1). kernel_pred(nonvar/1). kernel_pred(ground/1).
kernel_pred(callable/1). kernel_pred(compound/1). kernel_pred(is_list/1).
kernel_pred(assertz/1). kernel_pred(asserta/1). kernel_pred(assert/1).
kernel_pred(retract/1). kernel_pred(retractall/1). kernel_pred(abolish/1).
kernel_pred('dynamic'/1). kernel_pred('multifile'/1). kernel_pred('discontiguous'/1).
kernel_pred(consult/1). kernel_pred(ensure_loaded/1).
kernel_pred(use_module/1). kernel_pred(use_module/2).
kernel_pred(op/3). kernel_pred(current_op/3).
kernel_pred(current_predicate/1). kernel_pred(current_predicate/2).
kernel_pred(predicate_property/2). kernel_pred(clause/2).
kernel_pred(file_directory_name/2). kernel_pred(exists_file/1).
kernel_pred(directory_files/2). kernel_pred(make_directory/1).
kernel_pred(delete_file/1). kernel_pred(working_directory/2).
kernel_pred(get_time/1). kernel_pred(statistics/2).
kernel_pred(aggregate_all/3).
kernel_pred(flatten/2). kernel_pred(max_list/2). kernel_pred(min_list/2).
kernel_pred(not/1). kernel_pred(throw/1).
kernel_pred(string_concat/3). kernel_pred(char_type/2).
kernel_pred(read_line_to_string/2).
kernel_pred(exclude/4). kernel_pred(include/4).

% ---------- informe ----------
pf_fail(Kind, File, Detail) :-
    assertz(pf_finding(Kind, Detail)),
    format('PREFLIGHT ~w ~w: ~w~n', [Kind, File, Detail]).

report_preflight(File) :-
    findall(K-D, pf_finding(K, D), Fs),
    length(Fs, N),
    ( N =:= 0 ->
        format('PREFLIGHT ~w: PASS (clean)~n', [File])
    ; format('PREFLIGHT ~w: FAIL (~w findings above)~n', [File, N])
    ).
