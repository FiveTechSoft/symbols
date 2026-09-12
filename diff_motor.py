"""diff_motor.py — compara .pl locales vs origin/master:prolog/*.pl."""
import os
import subprocess
import difflib

LOCAL = 'C:/temp'
CORE = ['memory.pl', 'corpus.pl', 'open_vocab.pl', 'english_graph.pl',
        'conflict.pl', 'composition.pl', 'guided_search.pl',
        'question_parser.pl', 'coreference.pl', 'reuse.pl',
        'continuous.pl', 'multivariable.pl', 'run_corpus1.pl',
        'natural_parse.pl', 'positional.pl', 'language_graph.pl']

print('%-22s %8s %8s %8s' % ('fichero', 'local', 'remoto', 'difflines'))
for f in CORE:
    lp = os.path.join(LOCAL, f)
    if not os.path.exists(lp):
        print('%-22s LOCAL-MISSING' % f)
        continue
    r = subprocess.run(['git', 'show', 'origin/master:prolog/' + f],
                       capture_output=True, cwd=LOCAL)
    if r.returncode != 0:
        print('%-22s REMOTO-MISSING' % f)
        continue
    a = open(lp, encoding='utf-8', errors='replace').read().replace('\r\n', '\n').splitlines()
    b = r.stdout.decode('utf-8', errors='replace').replace('\r\n', '\n').splitlines()
    if a == b:
        print('%-22s %8d %8d %8s' % (f, len(a), len(b), 'IGUAL'))
    else:
        d = list(difflib.unified_diff(a, b, lineterm=''))
        plus = sum(1 for l in d if l.startswith('+') and not l.startswith('+++'))
        print('%-22s %8d %8d %8d' % (f, len(a), len(b), plus))
