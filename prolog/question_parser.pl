% question_parser.pl
% Preguntas -> forma logica -> motor (hechos y/o reglas) -> respuesta + prueba.
% Tipos: where (directa), who (inversa), does (si/no), why (explicacion).
% Respuestas: answer(Xs, retrieved|reasoned, Proof) | yes | no | unknown.
% Listas completas (nunca singleton-forzado); sin hecho ni regla -> unknown.
:- use_module(library(lists)).

% --- parse de preguntas (tokens via open_vocab) ---
parse_question(Sentence, q_where_lives(X)) :-
    tokenize_en(Sentence, [where, does, X, live]), !.
parse_question(Sentence, q_where_reaches(X)) :-
    tokenize_en(Sentence, [where, does, X, reach]), !.
parse_question(Sentence, q_where_arrives(X)) :-
    tokenize_en(Sentence, [where, does, X, arrive]), !.
parse_question(Sentence, q_where_borrows(X)) :-
    tokenize_en(Sentence, [where, does, X, borrow]), !.
parse_question(Sentence, q_where_provides(X)) :-
    tokenize_en(Sentence, [where, does, X, provide]), !.
parse_question(Sentence, q_where_imports(X)) :-
    tokenize_en(Sentence, [where, does, X, import]), !.
parse_question(Sentence, q_where_uses(X)) :-
    tokenize_en(Sentence, [where, does, X, use]), !.
parse_question(Sentence, q_where_harvests(X)) :-
    tokenize_en(Sentence, [where, does, X, harvest]), !.
parse_question(Sentence, q_who_visits(Y)) :-
    tokenize_en(Sentence, [who, visits, Y]), !.
parse_question(Sentence, q_who_lives(Y)) :-
    tokenize_en(Sentence, [who, lives, in, Y]), !.
parse_question(Sentence, q_yn(X, lives_in, Y)) :-
    tokenize_en(Sentence, [does, X, live, in, Y]), !.
parse_question(Sentence, q_yn(X, visits, Y)) :-
    tokenize_en(Sentence, [does, X, visit, Y]), !.
parse_question(Sentence, q_yn(X, reaches, Y)) :-
    tokenize_en(Sentence, [does, X, reach, Y]), !.
parse_question(Sentence, q_yn(X, arrives, Y)) :-
    tokenize_en(Sentence, [does, X, arrive, in, Y]), !.
parse_question(Sentence, q_why(X, reaches, Y)) :-
    tokenize_en(Sentence, [why, does, X, reach, Y]), !.

% --- motor ---
answer_query(q_where_lives(X), A) :-
    solve_slot(lives_in, X, sub, A).
answer_query(q_where_reaches(X), A) :-
    solve_slot(reaches, X, sub, A).
answer_query(q_where_arrives(X), A) :-
    solve_slot(arrives, X, sub, A).
answer_query(q_where_borrows(X), A) :-
    solve_slot(borrows, X, sub, A).
answer_query(q_where_provides(X), A) :-
    solve_slot(provides, X, sub, A).
answer_query(q_where_imports(X), A) :-
    solve_slot(imports, X, sub, A).
answer_query(q_where_uses(X), A) :-
    solve_slot(uses, X, sub, A).
answer_query(q_where_harvests(X), A) :-
    solve_slot(harvests, X, sub, A).
answer_query(q_who_visits(Y), A) :-
    solve_slot(visits, Y, obj, A).
answer_query(q_who_lives(Y), A) :-
    solve_slot(lives_in, Y, obj, A).
answer_query(q_yn(X, R, Y), yes) :-
    ( memory_relation(X, R, Y, _, _) ; infer_via_rule(X, R, Y, _) ), !.
answer_query(q_yn(_, _, _), no).
answer_query(q_why(X, R, Y), explanation(X, R, Y, Proof)) :-
    proof_for(X, R, Y, Proof).

% solve_slot: retrieval (todo lo almacenado) y luego reglas.
solve_slot(R, Fixed, Role, answer(Xs, retrieved, Facts)) :-
    slot_facts(R, Fixed, Role, Xs, Facts),
    Xs \== [].
solve_slot(R, Fixed, Role, answer(Xs, reasoned, Proofs)) :-
    rule_answers(R, Fixed, Role, Xs, Proofs),
    Xs \== [].
solve_slot(_, _, _, unknown).

slot_facts(R, Fixed, sub, Xs, Facts) :-
    findall(O-F, ( F = (Fixed, R, O),
                   memory_relation(Fixed, R, O, _, _)
                 ),
            OF),
    kvs(OF, Xs, Facts),
    sort(Xs, Xs).
slot_facts(R, Fixed, obj, Xs, Facts) :-
    findall(S-F, ( F = (S, R, Fixed),
                   memory_relation(S, R, Fixed, _, _)
                 ),
            SF),
    kvs(SF, Xs, Facts),
    sort(Xs, Xs).

kvs([], [], []).
kvs([K-V|Ps], [K|Ks], [V|Vs]) :-
    kvs(Ps, Ks, Vs).

rule_answers(R, Fixed, sub, Xs, Proofs) :-
    findall(O-P, ( infer_via_rule(Fixed, R, O, _),
                   rule_proof(R, Fixed, sub, O, P)
                 ),
            OP),
    kvs(OP, Xs, Proofs),
    sort(Xs, Xs).
rule_answers(R, Fixed, obj, Xs, Proofs) :-
    findall(S-P, ( infer_via_rule(S, R, Fixed, _),
                   rule_proof(R, Fixed, obj, S, P)
                 ),
            SP),
    kvs(SP, Xs, Proofs),
    sort(Xs, Xs).

% infer_via_rule: solo reglas constrained inducidas (nada inventado).
infer_via_rule(S, R, O, Sig) :-
    constrained_rule(R, Path, Sig),
    full_bindings(S, O, Path, Full),
    eq_signature(Full, Sig).

rule_proof(R, Fixed, sub, X, [rule(R, Path, Sig)|Fs]) :-
    constrained_rule(R, Path, Sig),
    full_bindings(Fixed, X, Path, Full),
    eq_signature(Full, Sig),
    steps(Full, Path, Fs).
rule_proof(R, Fixed, obj, X, [rule(R, Path, Sig)|Fs]) :-
    constrained_rule(R, Path, Sig),
    full_bindings(X, Fixed, Path, Full),
    eq_signature(Full, Sig),
    steps(Full, Path, Fs).

steps([_], [], []).
steps([A, B|T], [R|Rs], [(A, R, B)|Fs]) :-
    steps([B|T], Rs, Fs).

proof_for(X, R, Y, [fact(X, R, Y)]) :-
    memory_relation(X, R, Y, _, _), !.
proof_for(X, R, Y, [rule(R, Path, Sig)|Fs]) :-
    constrained_rule(R, Path, Sig),
    full_bindings(X, Y, Path, Full),
    eq_signature(Full, Sig),
    steps(Full, Path, Fs).

% --- verbalizacion minima (respuesta interna -> texto) ---
verbalize(answer([X], _, _), q_where_lives(S), Out) :-
    format(string(Out), "~w lives in ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_reaches(S), Out) :-
    format(string(Out), "~w reaches ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_arrives(S), Out) :-
    format(string(Out), "~w arrives in ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_borrows(S), Out) :-
    format(string(Out), "~w borrows ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_provides(S), Out) :-
    format(string(Out), "~w provides ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_imports(S), Out) :-
    format(string(Out), "~w imports ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_uses(S), Out) :-
    format(string(Out), "~w uses ~w.", [S, X]).
verbalize(answer([X], _, _), q_where_harvests(S), Out) :-
    format(string(Out), "~w harvests ~w.", [S, X]).
verbalize(answer([X], _, _), q_who_visits(O), Out) :-
    format(string(Out), "~w visits ~w.", [X, O]).
verbalize(answer([X], _, _), q_who_lives(O), Out) :-
    format(string(Out), "~w lives in ~w.", [X, O]).
verbalize(answer(Xs, _, _), _, Out) :-
    atomic_list_concat(Xs, ', ', L),
    format(string(Out), "Answers: ~w.", [L]).
verbalize(yes, _, "Yes.").
verbalize(no, _, "No.").
verbalize(unknown, _, "I don't know.").
verbalize(explanation(X, R, Y, Proof), _, Out) :-
    format(string(Out), "~w --~w--> ~w via ~w.", [X, R, Y, Proof]).
