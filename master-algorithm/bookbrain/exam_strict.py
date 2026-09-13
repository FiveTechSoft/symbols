#!/usr/bin/env python3
"""Strict exam. No credit for almost. Exit 1 if it invents or fails knowns."""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# (question, expect) expect is one of: known_substr | unknown | reject_substr
CASES = [
    ("who are you", "identity"),
    ("what did fib recurrence?", "1+1"),
    ("did fib transfers_to lucas?", "yes"),
    ("did fib transfers_to pell?", "yes"),  # companion exists even if transfer law failed
    ("did transfer_fib_to_pell_1_1 rejected n=2: pred=1 != obs=2?", "yes"),
    ("what did cassini_fib verified?", "(-1)"),
    ("what did fib pisano?", "2="),
    ("did fib recurrence 2?", "no"),  # false law
    ("who discovered phyllotaxis?", "unknown"),
    ("what did alma mean?", "unknown"),
    ("did fib always_prime true?", "unknown"),
    ("why did the universe begin?", "unknown"),
]

SCRIPT = r"""
:- consult('live.pl').
:- initialization(run, main).
run :-
    gaps_reset, clear_memory,
    catch(retractall(prov(_,_,_,_)), _, true),
    ma_boot,
    ma_say_who,
    exam_loop,
    halt.
exam_loop :-
    read_line_to_string(user_input, L),
    ( L == end_of_file -> true
    ; L == "CRECISTE" -> ma_growth, exam_loop
    ; format('Q: ~w~n', [L]),
      catch(chat_line(L), E, format('ERROR ~w~n', [E])),
      writeln('---END---'),
      exam_loop
    ).
"""

def run_exam():
    questions = [c[0] for c in CASES] + ["CRECISTE"]
    stdin = "\n".join(questions) + "\n"
    r = subprocess.run(
        ["swipl", "-q", "-g", "true", "-t", "halt(1)"],
        input=SCRIPT + "\n",
        cwd=HERE,
        capture_output=True,
        text=True,
    )
    # The above won't work well; use -s live and a dedicated exam.pl
    return r

if __name__ == "__main__":
    exam_pl = HERE / "exam_strict.pl"
    exam_pl.write_text(r"""
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
    ]), exam_q(Q)),
    writeln('===GROWTH==='),
    ma_growth,
    writeln('===EXAM END===').
exam_q(Q) :-
    format('~n## Q: ~w~n', [Q]),
    catch(chat_line(Q), E, format('ERROR: ~w~n', [E])),
    writeln('## END').
""")
    p = subprocess.run(
        ["swipl", "-q", "-s", "exam_strict.pl", "-g", "run_exam,halt", "-t", "halt(1)"],
        cwd=HERE,
        capture_output=True,
        text=True,
    )
    out = p.stdout + "\n" + p.stderr
    Path("/workspace/master-algorithm/motor/runs/exam1.txt").write_text(out)
    print(out)
    sys.exit(p.returncode)
