% live.pl — Master Algorithm mouth on BookBrain.
% Does NOT redefine chat_line_tokens/3 or chat_identity/1 (that wipes chat.pl).
% Uso: swipl -q -s live.pl -g live
:- consult('chat.pl').
:- dynamic ma_root/1.

ma_set_root :-
    ( ma_root(_) -> true
    ; working_directory(Cwd, Cwd),
      absolute_file_name('..', Root),
      assertz(ma_root(Root))
    ).

ma_theory_path(P) :-
    ma_set_root,
    ma_root(R),
    atomic_list_concat([R, '/motor/archive/theory.pl'], P).

ma_boot :-
    ma_set_root,
    ma_theory_path(Th),
    ( exists_file(Th) -> consult(Th)
    ; format('UNKNOWN: no theory.pl at ~w~n', [Th]), fail
    ),
    ma_sync.

ma_sync :-
    forall(rec(S, Cs),
           ( atomic_list_concat(Cs, '+', A),
             remember_tracked(S, recurrence, A, motor_theory, none) )),
    forall(companion(A, B),
           remember_tracked(A, transfers_to, B, motor_theory, none)),
    forall(true_mod(S, M, Per),
           ( atomic_list_concat([M, '=', Per], P),
             remember_tracked(S, pisano, P, motor_theory, none) )),
    forall(lemma(Id, _T, Txt),
           remember_tracked(Id, lemma, Txt, motor_theory, none)),
    forall(verified(fact(_W, _F, Name, Formula)),
           remember_tracked(Name, verified, Formula, motor_theory, none)),
    forall(rejected(Name, Why),
           remember_tracked(Name, rejected, Why, motor_theory, none)),
    memory_size(N),
    format('SYNC ~w facts from theory.pl~n', [N]).

ma_cmd(tick)    :- ma_tick(5).
ma_cmd(vive)    :- ma_tick(5).
ma_cmd(mejora)  :- ma_tick(8).
ma_cmd(growth)  :- ma_growth.
ma_cmd(creciste):- ma_growth.
ma_cmd(teoria)  :- ma_theory_path(P), format('~w~n', [P]).

ma_tick(K) :-
    ma_root(R),
    number_string(K, Ks),
    process_create(path(python3),
                   ['-m', 'motor', 'tick', '--steps', Ks],
                   [cwd(R), stdout(pipe(Out)), stderr(pipe(Err)), process(P)]),
    read_string(Out, _, So),
    read_string(Err, _, Se),
    close(Out), close(Err),
    process_wait(P, Exit),
    format('tick steps=~w exit=~w~n~w~n', [K, Exit, So]),
    ( Se == "" -> true ; format('stderr: ~w~n', [Se]) ),
    retractall(rec(_,_)), retractall(lemma(_,_,_)),
    retractall(verified(_)), retractall(rejected(_,_)),
    retractall(true_mod(_,_,_)),
    ma_theory_path(Th), consult(Th), ma_sync.

ma_growth :-
    ma_root(R),
    atomic_list_concat([R, '/motor/runs/latest.json'], J),
    ( exists_file(J) ->
        process_create(path(python3),
            ['-c', 'import json;d=json.load(open("motor/runs/latest.json"));print("hechos",d.get("n_verified_facts"));print("tipos",d.get("n_distinct_types"));print("transfer",d.get("transfer_accuracy"));print("reuse",d.get("lemma_reuse_rate"));print("steps",d.get("total_steps"))'],
            [cwd(R), stdout(pipe(O)), process(P)]),
        read_string(O, _, S), close(O), process_wait(P, _),
        write(S)
    ; writeln('UNKNOWN: no latest.json')
    ),
    memory_size(N),
    format('memoria ~w~n', [N]).

ma_say_who :-
    memory_size(N),
    format('Soy el motor Master Algorithm. ~w hechos verificados. UNKNOWN si no está en theory.pl.~n', [N]).

live :-
    gaps_reset,
    clear_memory,
    catch(retractall(prov(_,_,_,_)), _, true),
    ma_boot,
    writeln('MASTER ALGORITHM. tick | mejora | creciste | quit'),
    ma_loop.

ma_loop :-
    write('> '), flush_output,
    catch(read_line_to_string(user_input, L), _, L = end_of_file),
    ( L == end_of_file -> writeln('Bye.')
    ; downcase_atom_line(L, Low),
      member(Low, [quit, exit, bye]) -> writeln('Bye.')
    ; member(Low, ['quien eres', 'quién eres', 'who are you', 'what are you']) ->
        ma_say_who, ma_loop
    ; atom_string(A, Low), ma_cmd(A) -> ma_loop
    ; chat_line(L), ma_loop
    ).

downcase_atom_line(L, Low) :-
    string_lower(L, S),
    split_string(S, " \t", " \t?", Parts),
    atomic_list_concat(Parts, ' ', Low).
