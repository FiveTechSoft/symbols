% gaps.pl
% Lo que el sistema NO sabe se GUARDA para aprenderlo despues.
% gap(Id, Question, Status, Created, Extra): Status = open | resolved(Answer).
% - chat/ask registran cada UNKNOWN con la pregunta en superficie.
% - bookbrain conserva gaps+entidades entre ingestas y rechequea al final:
%   si la nueva memoria responde, el hueco pasa a resolved (se informa).
% - Todo persiste en document.knowledge.pl (gapfact/entfact) y va al repo.
% El reintento es generico: gap_recheck(:Try) donde Try = try_answer(Q, Ans)
% lo aporta el llamante (chat.pl/ask.pl) con su propio ask_form.
:- use_module(library(lists)).

:- dynamic gap/5.
:- dynamic gap_seq/1.

gaps_reset :-
    retractall(gap(_, _, _, _, _)),
    retractall(gap_seq(_)),
    assertz(gap_seq(0)).

gap_open(Question, Extra) :-
    % Sin duplicados abiertos identicos.
    gap(_, Question, open, _, _) -> true
    ;
    ( retract(gap_seq(N)) -> true ; N = 0 ),
    N1 is N + 1,
    assertz(gap_seq(N1)),
    get_time(Now),
    assertz(gap(N1, Question, open, Now, Extra)),
    format('GAP open #~w: ~w~n', [N1, Question]).

% gap_recheck(:Try): reintenta todos los abiertos; informa resueltos.
gap_recheck(Try) :-
    findall((Id, Q), gap(Id, Q, open, _, _), Open),
    length(Open, N),
    ( N == 0 -> writeln('GAP recheck: none open.')
    ; forall(member((Id, Q), Open), gap_retry(Try, Id, Q))
    ).

gap_retry(Try, Id, Q) :-
    ( catch(call(Try, Q, Ans), _, fail),
      Ans \== unknown ->
        retract(gap(Id, Q, open, C, E)),
        get_time(Now),
        assertz(gap(Id, Q, resolved(Ans), C, resolved_at(Now, E))),
        format('GAP resolved #~w: ~w -> ~w~n', [Id, Q, Ans])
    ; true
    ).

gap_report :-
    findall(Id, gap(Id, _, open, _, _), O),
    length(O, NO),
    findall(Id, gap(Id, _, resolved(_), _, _), R),
    length(R, NR),
    format('GAP status: ~w open, ~w resolved.~n', [NO, NR]).

% Persistencia (el llamante abre/cierra el stream).
gap_save(Out) :-
    forall(gap(Id, Q, St, C, E),
           format(Out, 'gapfact(~q,~q,~q,~q,~q).~n', [Id, Q, St, C, E])),
    ( gap_seq(N) ->
        format(Out, 'gapseq(~q).~n', [N])
    ; true
    ).

gap_load :-
    forall(gapfact(Id, Q, St, C, E), assertz(gap(Id, Q, St, C, E))),
    forall(gapseq(N), (retractall(gap_seq(_)), assertz(gap_seq(N)))),
    gap_report.
