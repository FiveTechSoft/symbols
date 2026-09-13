:- consult('live.pl').
run_exam :-
    gaps_reset, clear_memory,
    catch(retractall(prov(_,_,_,_)), _, true),
    ma_boot,
    writeln('===EXAM START==='),
    ma_say_who,
    forall(member(Q, [
        'what did fib recurrence?',
        'did fib recurrence 1+1?',
        'did fib transfers_to lucas?',
        'did fib transfers_to pell?',
        'what did cassini_fib verified?',
        'what did fib pisano?',
        'did fib recurrence 2?',
        'who discovered phyllotaxis?',
        'what is the soul?',
        'did fib always_prime true?',
        'why did the universe begin?',
        'who are you'
    ]), ignore(exam_q(Q))),
    writeln('===GROWTH==='),
    ignore(ma_growth),
    writeln('===EXAM END===').
exam_q(Q) :-
    format('~n## Q: ~w~n', [Q]),
    catch(ignore(chat_line(Q)), E, format('ERROR: ~w~n', [E])),
    writeln('## END').
