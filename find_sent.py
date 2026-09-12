"""find_sent.py — localiza la frase global N con el mismo split del lector."""
import sys

N = int(sys.argv[1])
lines = open('books/alice.txt', encoding='utf-8', errors='replace').read().splitlines()
# cuerpo desde CHAPTER I
i = next(k for k, l in enumerate(lines) if l.strip().startswith('CHAPTER I'))
body = lines[i:]
paras, cur = [], []
for l in body:
    if l.strip() == '':
        if cur:
            paras.append(' '.join(cur))
            cur = []
    else:
        cur.append(l.strip())
if cur:
    paras.append(' '.join(cur))
import re
n = 0
for p in paras:
    for s in re.split(r'[.!?;:,—–]', p):
        s = ' '.join(s.split())
        if not s:
            continue
        n += 1
        if n == N:
            print('frase', N, ':', repr(s[:300]))
            sys.exit()
print('no hay tantas:', n)
